#include "fx_zone_bank.h"
#include <string.h>
void ot_fx_bank_reset(ot_fx_zone_bank_t *bank){
    if(bank)memset(bank,0,sizeof(*bank));
}
bool ot_fx_slot_active(const ot_fx_zone_slot_t *slot,uint32_t now_ms){
    return slot&&slot->configured&&
        (slot->standalone|| (uint32_t)(now_ms-slot->refreshed_ms) <
                                 (uint32_t)slot->request.ttl_sec*1000u);
}
bool ot_fx_bank_apply(ot_fx_zone_bank_t *bank,const ot_ble_fx_packet_t *request,
                      uint32_t now_ms,bool standalone){
    if(!bank||!request||request->chain_mask==0||
       (request->chain_mask&~((1u<<OT_OM_OUTPUTS)-1u))!=0||
       request->zone_mask==0||request->ttl_sec<1||request->ttl_sec>60)return false;
    for(unsigned output=0;output<OT_OM_OUTPUTS;output++){
        if(!(request->chain_mask&(1u<<output)))continue;
        for(unsigned z=0;z<OT_OM_ZONES;z++){
            if(!(request->zone_mask&(1u<<z)))continue;
            ot_fx_zone_slot_t *slot=&bank->zones[output][z];
            /* Editing a running effect is not replacement: preserve source
             * state, phase, spark/flare overlays and zone generation. Renew
             * the bounded preview TTL independently of animation time. */
            if(ot_fx_slot_active(slot,now_ms) &&
               slot->request.effect_id==request->effect_id){
                slot->request=*request;
                slot->standalone=standalone;
                slot->continuous=false;
                slot->refreshed_ms=now_ms;
                continue;
            }
            uint32_t gen=++bank->next_generation;
            if(gen==0)gen=++bank->next_generation;
            *slot=(ot_fx_zone_slot_t){
              .configured=true,.standalone=standalone,.request=*request,
              .started_ms=now_ms,.refreshed_ms=now_ms,.generation=gen
            };
        }
    }
    return true;
}
/* Resolve broad BLE masks against actual configured zones. Avoid acknowledging
 * phantom/disabled zones, and never reset a different zone's live effect.
 * A two-output target may contain differently sized independent layouts. */
bool ot_fx_bank_apply_configured(ot_fx_zone_bank_t *bank,
 const ot_ble_fx_packet_t *request,const ot_om_output_t outputs[OT_OM_OUTPUTS],
 uint32_t now_ms,bool standalone){
 if(!bank||!request||!outputs||request->chain_mask==0||
    (request->chain_mask&~((1u<<OT_OM_OUTPUTS)-1u))!=0||
    request->zone_mask==0||request->ttl_sec<1||request->ttl_sec>60)return false;
 uint8_t masks[OT_OM_OUTPUTS]={0};
 bool selected=false;
 for(unsigned o=0;o<OT_OM_OUTPUTS;o++){
  if(!(request->chain_mask&(1u<<o)))continue;
  const ot_om_output_t *out=&outputs[o];
  if(out->zone_count>OT_OM_ZONES)return false;
  for(unsigned z=0;z<out->zone_count;z++){
   if((request->zone_mask&(1u<<z))&&out->zones[z].pixels>0&&
      out->zones[z].role!=OT_OM_OFF){
     masks[o]=(uint8_t)(masks[o]|(1u<<z));
     selected=true;
   }
  }
 }
 if(!selected)return false;
 for(unsigned o=0;o<OT_OM_OUTPUTS;o++){
  if(!masks[o])continue;
  ot_ble_fx_packet_t effective=*request;
  effective.chain_mask=(uint8_t)(1u<<o);
  effective.zone_mask=masks[o];
  if(!ot_fx_bank_apply(bank,&effective,now_ms,standalone))return false;
 }
 return true;
}
/* A live sparkle/flare parameter change must NOT restart the underlying
 * WLED/native effect. Require at least one already-active, configured zone;
 * validate the entire selection before modifying anything. The caller holds
 * state_lock and controls authentication/session lifetime. */
bool ot_fx_bank_set_overlay(ot_fx_zone_bank_t *bank,
 const ot_om_output_t outputs[OT_OM_OUTPUTS],uint8_t chain_mask,
 uint8_t zone_mask,uint32_t now_ms,const ot_fx_overlay_config_t *overlay){
 if(!bank||!outputs||!ot_fx_overlay_valid(overlay)||!chain_mask||
    (chain_mask&~((1u<<OT_OM_OUTPUTS)-1u))!=0u||!zone_mask)return false;
 bool active=false;
 for(unsigned o=0;o<OT_OM_OUTPUTS;o++){
  if(!(chain_mask&(1u<<o)))continue;
  if(outputs[o].zone_count>OT_OM_ZONES)return false;
  for(unsigned z=0;z<outputs[o].zone_count;z++)
   if((zone_mask&(1u<<z))&&outputs[o].zones[z].pixels>0u&&
      outputs[o].zones[z].role!=OT_OM_OFF &&
      ot_fx_slot_active(&bank->zones[o][z],now_ms))
    active=true;
 }
 if(!active)return false;
 for(unsigned o=0;o<OT_OM_OUTPUTS;o++){
  if(!(chain_mask&(1u<<o)))continue;
  for(unsigned z=0;z<outputs[o].zone_count;z++)
   if((zone_mask&(1u<<z))&&outputs[o].zones[z].pixels>0u&&
      outputs[o].zones[z].role!=OT_OM_OFF &&
      ot_fx_slot_active(&bank->zones[o][z],now_ms))
    bank->zones[o][z].overlay=*overlay;
 }
 return true;
}
bool ot_fx_bank_any_active(const ot_fx_zone_bank_t *bank,uint32_t now_ms){
    if(!bank)return false;
    for(unsigned o=0;o<OT_OM_OUTPUTS;o++)
        for(unsigned z=0;z<OT_OM_ZONES;z++)
            if(ot_fx_slot_active(&bank->zones[o][z],now_ms))return true;
    return false;
}
void ot_fx_bank_stop_temporary(ot_fx_zone_bank_t *bank){
    if(!bank)return;
    for(unsigned o=0;o<OT_OM_OUTPUTS;o++)
        for(unsigned z=0;z<OT_OM_ZONES;z++)
            if(!bank->zones[o][z].standalone)bank->zones[o][z].configured=false;
}
void ot_fx_bank_stop_all(ot_fx_zone_bank_t *bank){
    if(!bank)return;
    for(unsigned o=0;o<OT_OM_OUTPUTS;o++)
        for(unsigned z=0;z<OT_OM_ZONES;z++)
            bank->zones[o][z].configured=false;
}

bool ot_fx_bank_apply_continuous(ot_fx_zone_bank_t *bank,
 const ot_ble_fx_packet_t *request,const ot_om_output_t outputs[OT_OM_OUTPUTS],
 uint32_t now_ms){
 if(!bank||!request||!outputs||!request->chain_mask||
    (request->chain_mask&~((1u<<OT_OM_OUTPUTS)-1u))||
    !request->zone_mask||request->ttl_sec<1||request->ttl_sec>60)return false;
 uint8_t masks[OT_OM_OUTPUTS]={0};
 bool any=false;
 for(unsigned o=0;o<OT_OM_OUTPUTS;o++){
  if(!(request->chain_mask&(1u<<o)))continue;
  if(outputs[o].zone_count<1||outputs[o].zone_count>OT_OM_ZONES||
     outputs[o].active_pixels<1||outputs[o].active_pixels>OT_OM_MAX_PIXELS_PER_OUTPUT)return false;
  unsigned total=0;
  for(unsigned z=0;z<outputs[o].zone_count;z++){
   if(outputs[o].zones[z].role>OT_OM_DECORATIVE)return false;
   total+=outputs[o].zones[z].pixels;
  }
  if(total!=outputs[o].active_pixels)return false;
  for(unsigned z=0;z<outputs[o].zone_count;z++)
   if((request->zone_mask&(1u<<z))&&outputs[o].zones[z].pixels&&
      outputs[o].zones[z].role!=OT_OM_OFF){masks[o]|=(uint8_t)(1u<<z);any=true;}
 }
 if(!any)return false;
 /* All rejection paths precede mutation. Drop only previous continuous
  * targets; explicit stored scenes on unrelated zones remain independent. */
 for(unsigned o=0;o<OT_OM_OUTPUTS;o++)for(unsigned z=0;z<OT_OM_ZONES;z++)
  if(bank->zones[o][z].continuous && !(masks[o]&(1u<<z)))
   bank->zones[o][z].configured=false;
 for(unsigned o=0;o<OT_OM_OUTPUTS;o++){
  if(!masks[o])continue;
  ot_ble_fx_packet_t effective=*request;
  effective.chain_mask=(uint8_t)(1u<<o);effective.zone_mask=masks[o];
  if(!ot_fx_bank_apply(bank,&effective,now_ms,true))return false;
  for(unsigned z=0;z<OT_OM_ZONES;z++)
   if(masks[o]&(1u<<z))bank->zones[o][z].continuous=true;
 }
 return true;
}
void ot_fx_bank_stop_continuous(ot_fx_zone_bank_t *bank){
 if(!bank)return;
 for(unsigned o=0;o<OT_OM_OUTPUTS;o++)for(unsigned z=0;z<OT_OM_ZONES;z++)
  if(bank->zones[o][z].continuous)bank->zones[o][z].configured=false;
}
