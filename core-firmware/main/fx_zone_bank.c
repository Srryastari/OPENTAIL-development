#include "fx_zone_bank.h"
#include <string.h>
void ot_fx_bank_reset(ot_fx_zone_bank_t *bank){
    if(bank)memset(bank,0,sizeof(*bank));
}
bool ot_fx_slot_active(const ot_fx_zone_slot_t *slot,uint32_t now_ms){
    return slot&&slot->configured&&
        (slot->standalone|| (uint32_t)(now_ms-slot->started_ms) <
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
            uint32_t gen=++bank->next_generation;
            if(gen==0)gen=++bank->next_generation;
            bank->zones[output][z]=(ot_fx_zone_slot_t){
              .configured=true,.standalone=standalone,.request=*request,
              .started_ms=now_ms,.generation=gen
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
    request->zone_mask==0)return false;
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
