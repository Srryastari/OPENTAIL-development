#include <assert.h>
#include "fx_zone_bank.h"
static ot_ble_fx_packet_t make(unsigned fx,unsigned out,unsigned zones,unsigned ttl){
 ot_ble_fx_packet_t p={0};
 p.effect_id=(uint16_t)fx;p.chain_mask=(uint8_t)out;
 p.zone_mask=(uint8_t)zones;p.ttl_sec=(uint8_t)ttl;
 return p;
}
int main(void){
 ot_fx_zone_bank_t bank;
 ot_fx_bank_reset(&bank);
 assert(!ot_fx_bank_any_active(&bank,100));
 ot_ble_fx_packet_t a=make(3,1,1,60),b=make(32768,2,4,10);
 assert(ot_fx_bank_apply(&bank,&a,100,false));
 uint32_t gen_a=bank.zones[0][0].generation;
 assert(ot_fx_bank_apply(&bank,&b,200,false));
 assert(bank.zones[0][0].request.effect_id==3);
 assert(bank.zones[1][2].request.effect_id==32768);
 assert(bank.zones[0][0].generation==gen_a);
 assert(ot_fx_slot_active(&bank.zones[0][0],1100));
 assert(!ot_fx_slot_active(&bank.zones[1][2],10200));
 assert(ot_fx_bank_any_active(&bank,10200));
 b=make(23,2,4,30);
 assert(ot_fx_bank_apply(&bank,&b,10300,true));
 assert(bank.zones[1][2].standalone);
 assert(bank.zones[0][0].generation==gen_a);
 ot_fx_bank_stop_temporary(&bank);
 assert(!ot_fx_slot_active(&bank.zones[0][0],10301));
 assert(ot_fx_slot_active(&bank.zones[1][2],600000));
 /* Saved preset targeting one zone must not delete another saved zone. */
 a=make(0,1,2,1);
 assert(ot_fx_bank_apply(&bank,&a,10350,true));
 assert(ot_fx_slot_active(&bank.zones[0][1],600000));
 assert(ot_fx_slot_active(&bank.zones[1][2],600000));
 ot_fx_bank_stop_all(&bank);
 assert(!ot_fx_bank_any_active(&bank,600000));
 /* Different physical outputs may have entirely different active layouts.
  * Broad ALL must skip disabled/nonexistent zones, and a phantom-only mask
  * must return an error without changing any live slot. */
 ot_om_output_t outputs[OT_OM_OUTPUTS]={0};
 outputs[0].zone_count=2;
 outputs[0].zones[0].pixels=12;outputs[0].zones[0].role=OT_OM_DECORATIVE;
 outputs[0].zones[1].pixels=10;outputs[0].zones[1].role=OT_OM_OFF;
 outputs[1].zone_count=3;
 outputs[1].zones[0].pixels=8;outputs[1].zones[0].role=OT_OM_DECORATIVE;
 outputs[1].zones[1].pixels=8;outputs[1].zones[1].role=OT_OM_OFF;
 outputs[1].zones[2].pixels=8;outputs[1].zones[2].role=OT_OM_DECORATIVE;
 ot_ble_fx_packet_t all=make(12,3,255,60);
 assert(ot_fx_bank_apply_configured(&bank,&all,outputs,500,false));
 assert(ot_fx_slot_active(&bank.zones[0][0],501));
 assert(!ot_fx_slot_active(&bank.zones[0][1],501));
 assert(ot_fx_slot_active(&bank.zones[1][0],501));
 assert(!ot_fx_slot_active(&bank.zones[1][1],501));
 assert(ot_fx_slot_active(&bank.zones[1][2],501));
 uint32_t old=bank.zones[1][2].generation;
 ot_ble_fx_packet_t phantom=make(23,2,128,10);
 assert(!ot_fx_bank_apply_configured(&bank,&phantom,outputs,600,false));
 assert(bank.zones[1][2].generation==old);
 ot_ble_fx_packet_t single=make(3,2,4,10);
 assert(ot_fx_bank_apply_configured(&bank,&single,outputs,600,false));
 assert(bank.zones[1][2].request.effect_id==3);
 assert(bank.zones[0][0].request.effect_id==12);
 a.zone_mask=0;assert(!ot_fx_bank_apply(&bank,&a,10,false));
 a=make(1,3,255,1);
 assert(ot_fx_bank_apply(&bank,&a,UINT32_MAX-500u,false));
 assert(ot_fx_bank_any_active(&bank,UINT32_MAX-200u));
 assert(ot_fx_bank_any_active(&bank,100u));
 assert(!ot_fx_bank_any_active(&bank,800u));
 return 0;
}
