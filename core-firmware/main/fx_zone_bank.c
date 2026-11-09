#include "fx_zone_bank.h"
#include <string.h>
void ot_fx_bank_reset(ot_fx_zone_bank_t *bank){
    if(bank)memset(bank,0,sizeof(*bank));
}
bool ot_fx_slot_active(const ot_fx_zone_slot_t *slot,uint32_t now_ms){
    return slot&&slot->configured&&
        (slot->standalone||slot->continuous|| (uint32_t)(now_ms-slot->refreshed_ms) <
                                 (uint32_t)slot->request.ttl_sec*1000u);
}
static bool panel_has_identity(const ot_pz_output_t *panel,
                                 uint16_t panel_id,uint16_t zone_id){
    if(!panel||!panel_id||!zone_id)return false;
    for(unsigned z=0;z<panel->zone_count&&z<OT_OM_ZONES;z++)
        if(panel->zones[z].panel_id==panel_id&&panel->zones[z].id==zone_id)
            return true;
    return false;
}
bool ot_fx_bank_resolve_zone_identity(const ot_pz_output_t *panel_output,
 ot_fx_zone_bank_t *bank,unsigned output,uint32_t now_ms,
 uint8_t resolved_indices[OT_OM_ZONES]){
 if(!panel_output||!bank||!resolved_indices||output>=OT_OM_OUTPUTS||
    panel_output->zone_count>OT_OM_ZONES)return false;
 memset(resolved_indices,0xff,OT_OM_ZONES*sizeof(resolved_indices[0]));
 for(unsigned bank_z=0;bank_z<OT_OM_ZONES;bank_z++){
  ot_fx_zone_slot_t *slot=&bank->zones[output][bank_z];
  if(!ot_fx_slot_active(slot,now_ms)){
   slot->owner_panel_id=0u;slot->owner_zone_id=0u;
   continue;
  }
  /* Compatibility migration for slots created by the legacy index API. Once
   * owned, a slot keeps that identity until an identity-aware mutation binds
   * a different current target. Generation/refresh changes alone never move it. */
  if((!slot->owner_panel_id||!slot->owner_zone_id)&&
     bank_z<panel_output->zone_count){
   slot->owner_panel_id=panel_output->zones[bank_z].panel_id;
   slot->owner_zone_id=panel_output->zones[bank_z].id;
  }
 }
 for(unsigned zone=0;zone<panel_output->zone_count;zone++){
  const ot_pz_zone_t *zone_cfg=&panel_output->zones[zone];
  for(unsigned bank_z=0;bank_z<OT_OM_ZONES;bank_z++){
   const ot_fx_zone_slot_t *slot=&bank->zones[output][bank_z];
   if(slot->owner_panel_id==zone_cfg->panel_id&&
      slot->owner_zone_id==zone_cfg->id&&ot_fx_slot_active(slot,now_ms)){
    resolved_indices[zone]=(uint8_t)bank_z;break;
   }
  }
 }
 return true;
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
                slot->tertiary=(ot_ble_fx_tertiary_t){0}; /* legacy command restores legacy third default */
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
static bool resolve_requested_slots(ot_fx_zone_bank_t *bank,
 const ot_om_output_t *output_cfg,const ot_pz_output_t *panel_cfg,
 unsigned output,uint8_t requested_mask,uint32_t now_ms,bool require_active,
 uint8_t *bank_mask,uint8_t resolved[OT_OM_ZONES]){
 if(!bank||!output_cfg||!panel_cfg||!bank_mask||!resolved||
    output>=OT_OM_OUTPUTS||output_cfg->zone_count>OT_OM_ZONES||
    panel_cfg->zone_count>OT_OM_ZONES)return false;
 if(!ot_fx_bank_resolve_zone_identity(panel_cfg,bank,
                                      output,now_ms,resolved))return false;
 *bank_mask=0u;
 for(unsigned zone=0;zone<output_cfg->zone_count;zone++){
  if(!(requested_mask&(1u<<zone)))continue;
  if(output_cfg->zones[zone].pixels==0u||
     output_cfg->zones[zone].role==OT_OM_OFF)continue;
  unsigned bank_z=resolved[zone];
  if(bank_z>=OT_OM_ZONES){
   if(require_active)continue;
   bool found=false;
   for(unsigned candidate=0;candidate<OT_OM_ZONES;candidate++){
    if(*bank_mask&(1u<<candidate))continue;
    ot_fx_zone_slot_t *slot=&bank->zones[output][candidate];
    const bool owner_live=panel_has_identity(panel_cfg,slot->owner_panel_id,
                                             slot->owner_zone_id);
    if(!ot_fx_slot_active(slot,now_ms)||!owner_live){
     bank_z=candidate;found=true;break;
    }
   }
   if(!found)return false;
   resolved[zone]=(uint8_t)bank_z;
  }
  if(*bank_mask&(1u<<bank_z))return false;
  *bank_mask=(uint8_t)(*bank_mask|(1u<<bank_z));
 }
 return *bank_mask!=0u;
}
static void bind_requested_slots(ot_fx_zone_bank_t *bank,
 const ot_pz_output_t *panel,unsigned output,uint8_t requested_mask,
 const uint8_t resolved[OT_OM_ZONES]){
 for(unsigned zone=0;zone<panel->zone_count;zone++){
  if(!(requested_mask&(1u<<zone)))continue;
  const unsigned bank_z=resolved[zone];
  if(bank_z>=OT_OM_ZONES)continue;
  ot_fx_zone_slot_t *slot=&bank->zones[output][bank_z];
  slot->owner_panel_id=panel->zones[zone].panel_id;
  slot->owner_zone_id=panel->zones[zone].id;
 }
}
bool ot_fx_bank_apply_configured_identity(ot_fx_zone_bank_t *bank,
 const ot_ble_fx_packet_t *request,const ot_om_output_t outputs[OT_OM_OUTPUTS],
 const ot_pz_output_t panel_outputs[OT_OM_OUTPUTS],uint32_t now_ms,bool standalone){
 if(!bank||!request||!outputs||!panel_outputs||!request->chain_mask||
    (request->chain_mask&~((1u<<OT_OM_OUTPUTS)-1u))||!request->zone_mask||
    request->ttl_sec<1u||request->ttl_sec>60u)return false;
 uint8_t masks[OT_OM_OUTPUTS]={0};
 uint8_t resolved[OT_OM_OUTPUTS][OT_OM_ZONES];
 bool any=false;
 for(unsigned o=0;o<OT_OM_OUTPUTS;o++){
  if(!(request->chain_mask&(1u<<o)))continue;
  if(!resolve_requested_slots(bank,&outputs[o],&panel_outputs[o],o,
      request->zone_mask,now_ms,false,&masks[o],resolved[o]))continue;
  any=true;
 }
 if(!any)return false;
 for(unsigned o=0;o<OT_OM_OUTPUTS;o++){
  if(!masks[o])continue;
  ot_ble_fx_packet_t effective=*request;
  effective.chain_mask=(uint8_t)(1u<<o);effective.zone_mask=masks[o];
  if(!ot_fx_bank_apply(bank,&effective,now_ms,standalone))return false;
  bind_requested_slots(bank,&panel_outputs[o],o,request->zone_mask,resolved[o]);
 }
 return true;
}
bool ot_fx_bank_apply_continuous_tertiary_identity(ot_fx_zone_bank_t *bank,
 const ot_ble_fx_packet_t *request,const ot_om_output_t outputs[OT_OM_OUTPUTS],
 const ot_pz_output_t panel_outputs[OT_OM_OUTPUTS],
 const ot_ble_fx_tertiary_t *tertiary,uint32_t now_ms){
 if(!bank||!request||!outputs||!panel_outputs||!request->chain_mask||
    (request->chain_mask&~((1u<<OT_OM_OUTPUTS)-1u))||!request->zone_mask||
    request->ttl_sec<1u||request->ttl_sec>60u)return false;
 uint8_t masks[OT_OM_OUTPUTS]={0};
 uint8_t resolved[OT_OM_OUTPUTS][OT_OM_ZONES];
 bool any=false;
 for(unsigned o=0;o<OT_OM_OUTPUTS;o++){
  if(!(request->chain_mask&(1u<<o)))continue;
  if(outputs[o].zone_count<1u||outputs[o].zone_count>OT_OM_ZONES||
     outputs[o].active_pixels<1u||
     outputs[o].active_pixels>OT_OM_MAX_PIXELS_PER_OUTPUT)return false;
  unsigned total=0u;
  for(unsigned z=0;z<outputs[o].zone_count;z++){
   if(outputs[o].zones[z].role>OT_OM_DECORATIVE)return false;
   total+=outputs[o].zones[z].pixels;
  }
  if(total!=outputs[o].active_pixels)return false;
  if(resolve_requested_slots(bank,&outputs[o],&panel_outputs[o],o,
      request->zone_mask,now_ms,false,&masks[o],resolved[o]))any=true;
 }
 if(!any)return false;
 for(unsigned o=0;o<OT_OM_OUTPUTS;o++)for(unsigned z=0;z<OT_OM_ZONES;z++)
  if(bank->zones[o][z].continuous&&!(masks[o]&(1u<<z))){
   bank->zones[o][z].configured=false;
   bank->zones[o][z].owner_panel_id=0u;
   bank->zones[o][z].owner_zone_id=0u;
  }
 for(unsigned o=0;o<OT_OM_OUTPUTS;o++){
  if(!masks[o])continue;
  ot_ble_fx_packet_t effective=*request;
  effective.chain_mask=(uint8_t)(1u<<o);effective.zone_mask=masks[o];
  if(!ot_fx_bank_apply(bank,&effective,now_ms,false))return false;
  for(unsigned z=0;z<OT_OM_ZONES;z++)if(masks[o]&(1u<<z)){
   bank->zones[o][z].continuous=true;
   if(tertiary)bank->zones[o][z].tertiary=*tertiary;
  }
  bind_requested_slots(bank,&panel_outputs[o],o,request->zone_mask,resolved[o]);
 }
 return true;
}
bool ot_fx_bank_set_overlay_identity(ot_fx_zone_bank_t *bank,
 const ot_om_output_t outputs[OT_OM_OUTPUTS],
 const ot_pz_output_t panel_outputs[OT_OM_OUTPUTS],uint8_t chain_mask,
 uint8_t zone_mask,uint32_t now_ms,const ot_fx_overlay_config_t *overlay){
 if(!bank||!outputs||!panel_outputs||!ot_fx_overlay_valid(overlay)||
    !chain_mask||(chain_mask&~((1u<<OT_OM_OUTPUTS)-1u))||!zone_mask)return false;
 bool any=false;
 for(unsigned o=0;o<OT_OM_OUTPUTS;o++){
  if(!(chain_mask&(1u<<o)))continue;
  uint8_t mask=0u,resolved[OT_OM_ZONES];
  if(!resolve_requested_slots(bank,&outputs[o],&panel_outputs[o],o,
      zone_mask,now_ms,true,&mask,resolved))continue;
  any=true;
  for(unsigned z=0;z<outputs[o].zone_count;z++)
   if((zone_mask&(1u<<z))&&resolved[z]<OT_OM_ZONES)
    bank->zones[o][resolved[z]].overlay=*overlay;
 }
 return any;
}
void ot_fx_bank_prune_orphans(ot_fx_zone_bank_t *bank,
 const ot_pz_output_t panel_outputs[OT_OM_OUTPUTS]){
 if(!bank||!panel_outputs)return;
 for(unsigned o=0;o<OT_OM_OUTPUTS;o++)
  for(unsigned z=0;z<OT_OM_ZONES;z++){
   ot_fx_zone_slot_t *slot=&bank->zones[o][z];
   if(slot->owner_panel_id&&slot->owner_zone_id&&
      !panel_has_identity(&panel_outputs[o],slot->owner_panel_id,
                          slot->owner_zone_id)){
    slot->configured=false;slot->owner_panel_id=0u;slot->owner_zone_id=0u;
   }
  }
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
            if(!bank->zones[o][z].standalone&&!bank->zones[o][z].continuous){
                {bank->zones[o][z].configured=false;
             bank->zones[o][z].owner_panel_id=0u;
             bank->zones[o][z].owner_zone_id=0u;}
                bank->zones[o][z].owner_panel_id=0u;
                bank->zones[o][z].owner_zone_id=0u;
            }
}
void ot_fx_bank_stop_all(ot_fx_zone_bank_t *bank){
    if(!bank)return;
    for(unsigned o=0;o<OT_OM_OUTPUTS;o++)
        for(unsigned z=0;z<OT_OM_ZONES;z++)
            bank->zones[o][z].configured=false;
}

bool ot_fx_bank_apply_continuous_tertiary(ot_fx_zone_bank_t *bank,
 const ot_ble_fx_packet_t *request,const ot_om_output_t outputs[OT_OM_OUTPUTS],
 const ot_ble_fx_tertiary_t *tertiary,uint32_t now_ms){
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
  if(!ot_fx_bank_apply(bank,&effective,now_ms,false))return false;
  for(unsigned z=0;z<OT_OM_ZONES;z++)
   if(masks[o]&(1u<<z)){
      bank->zones[o][z].continuous=true;
      if(tertiary)bank->zones[o][z].tertiary=*tertiary;
   }
 }
 return true;
}
bool ot_fx_bank_apply_continuous(ot_fx_zone_bank_t *bank,
 const ot_ble_fx_packet_t *request,const ot_om_output_t outputs[OT_OM_OUTPUTS],
 uint32_t now_ms){
 return ot_fx_bank_apply_continuous_tertiary(bank,request,outputs,NULL,now_ms);
}
void ot_fx_bank_stop_continuous(ot_fx_zone_bank_t *bank){
 if(!bank)return;
 for(unsigned o=0;o<OT_OM_OUTPUTS;o++)for(unsigned z=0;z<OT_OM_ZONES;z++)
  if(bank->zones[o][z].continuous){
   bank->zones[o][z].configured=false;
   bank->zones[o][z].owner_panel_id=0u;
   bank->zones[o][z].owner_zone_id=0u;
  }
}

void ot_fx_bank_prune_expired(ot_fx_zone_bank_t *bank,uint32_t now_ms){
 if(!bank)return;
 for(unsigned o=0;o<OT_OM_OUTPUTS;o++)for(unsigned z=0;z<OT_OM_ZONES;z++){
  ot_fx_zone_slot_t *slot=&bank->zones[o][z];
  if(slot->configured&&!slot->standalone&&!slot->continuous&&
     !ot_fx_slot_active(slot,now_ms)){
   slot->configured=false;slot->owner_panel_id=0u;slot->owner_zone_id=0u;
  }
 }
}
