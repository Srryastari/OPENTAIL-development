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
 a.zone_mask=0;assert(!ot_fx_bank_apply(&bank,&a,10,false));
 a=make(1,3,255,1);
 assert(ot_fx_bank_apply(&bank,&a,UINT32_MAX-500u,false));
 assert(ot_fx_bank_any_active(&bank,UINT32_MAX-200u));
 assert(ot_fx_bank_any_active(&bank,100u));
 assert(!ot_fx_bank_any_active(&bank,800u));
 return 0;
}
