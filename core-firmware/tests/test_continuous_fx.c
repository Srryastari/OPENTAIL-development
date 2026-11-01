#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "fx_zone_bank.h"
int main(void){
 ot_om_output_t outputs[OT_OM_OUTPUTS];ot_om_defaults(outputs);
 for(unsigned i=0;i<OT_OM_OUTPUTS;i++)outputs[i].zones[0].role=OT_OM_DECORATIVE;
 ot_fx_zone_bank_t bank={0},before;
 ot_ble_fx_packet_t req={.effect_id=3,.chain_mask=3,.zone_mask=1,.ttl_sec=1,.brightness=24};
 assert(ot_fx_bank_apply_continuous(&bank,&req,outputs,100));
 assert(bank.zones[0][0].continuous&&bank.zones[0][0].standalone);
 uint32_t gen=bank.zones[0][0].generation;
 assert(ot_fx_slot_active(&bank.zones[0][0],1000000u));
 ot_fx_bank_stop_temporary(&bank); /* link loss/graceful release */
 assert(ot_fx_slot_active(&bank.zones[0][0],1000000u));
 req.speed=200;
 assert(ot_fx_bank_apply_continuous(&bank,&req,outputs,1000001u));
 assert(bank.zones[0][0].generation==gen&&bank.zones[0][0].started_ms==100);
 before=bank;
 req.zone_mask=2;
 assert(!ot_fx_bank_apply_continuous(&bank,&req,outputs,1000002u));
 assert(memcmp(&bank,&before,sizeof(bank))==0);
 req.zone_mask=1;req.chain_mask=1;req.effect_id=9;
 assert(ot_fx_bank_apply_continuous(&bank,&req,outputs,1000003u));
 assert(bank.zones[0][0].generation!=gen);
 assert(!bank.zones[1][0].configured); /* dropped old targets do not leak */
 assert(ot_fx_slot_active(&bank.zones[0][0],UINT32_MAX-2));
 ot_fx_bank_stop_continuous(&bank); /* runtime fault */
 assert(!ot_fx_bank_any_active(&bank,1000004u));
 assert(ot_fx_bank_apply_continuous(&bank,&req,outputs,1000005u));
 ot_fx_bank_stop_all(&bank);
 assert(!ot_fx_bank_any_active(&bank,1000006u));
 assert(ot_fx_bank_apply(&bank,&req,100u,false)); /* old bench contract */
 assert(!ot_fx_slot_active(&bank.zones[0][0],1100u));
 ot_fx_bank_reset(&bank); /* reboot never auto starts */
 assert(!ot_fx_bank_any_active(&bank,0));
 puts("PASS continuous lifetime/disconnect/edits/target replacement/STOP/fault/reboot; legacy TTL bounded");
 return 0;
}
