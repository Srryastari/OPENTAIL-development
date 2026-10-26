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
