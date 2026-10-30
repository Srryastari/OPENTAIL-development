/* Simulated NVS blob recovery, corruption and held-input arbitration.
 * No claim of physical flash power-cut, actual GPIO, or optical validation. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ot_config_journal.h"

static void test_trigger_reboot_while_held(void){
    ot_tm_profile_t old,newer,recovered,requested,live;
    ot_tm_defaults(&old);
    newer=old;
    newer.function[0]=OT_TM_REVERSE;
    ot_cfg_tm_journal_t flash={0};
    assert(ot_cfg_tm_pack(&old,&newer,&flash));
    assert(ot_cfg_tm_unpack(&flash,&recovered,&requested));
    ot_tm_pending_t deferred={0};
    live=recovered;
    assert(live.function[0]==OT_TM_PARK);
    assert(ot_tm_stage(&deferred,&requested));
    assert(deferred.pending && live.function[0]==OT_TM_PARK);
    assert(!ot_tm_apply_if_released(&live,&deferred,1u,1u));
    assert(!ot_tm_apply_if_released(&live,&deferred,0u,1u));
    assert(ot_tm_apply_if_released(&live,&deferred,0u,0u));
    assert(!deferred.pending && live.function[0]==OT_TM_REVERSE);
    /* Even after activation, an input held across a subsequent reboot
     * must never activate the snapshot that was written during the save. */
    assert(ot_cfg_tm_unpack(&flash,&recovered,&requested));
    assert(recovered.function[0]==OT_TM_PARK);
    assert(requested.function[0]==OT_TM_REVERSE);
    flash.requested.function[1]=0xffu;
    assert(!ot_cfg_tm_unpack(&flash,&recovered,&requested));
}
static void test_output_reboot_and_corruption(void){
    ot_om_output_t old[OT_OM_OUTPUTS],newer[OT_OM_OUTPUTS];
    ot_om_output_t recovered[OT_OM_OUTPUTS],requested[OT_OM_OUTPUTS];
    ot_om_defaults(old);
    memcpy(newer,old,sizeof(newer));
    newer[0].zones[0].role=OT_OM_BRAKE;
    ot_cfg_om_journal_t flash={0};
    assert(ot_cfg_om_pack(old,newer,&flash));
    assert(ot_cfg_om_unpack(&flash,recovered,requested));
    assert(recovered[0].zones[0].role==OT_OM_OFF);
    assert(requested[0].zones[0].role==OT_OM_BRAKE);
    ot_om_pending_t deferred={0};
    assert(ot_om_stage(&deferred,requested));
    assert(!ot_om_apply_if_released(recovered,&deferred,2u,0u));
    assert(ot_om_apply_if_released(recovered,&deferred,0u,0u));
    assert(recovered[0].zones[0].role==OT_OM_BRAKE);
    assert(ot_cfg_om_unpack(&flash,recovered,requested));
    assert(recovered[0].zones[0].role==OT_OM_OFF);
    flash.previous[0].zones[0].role=255;
    assert(!ot_cfg_om_unpack(&flash,recovered,requested));
}
static void test_atomic_blob_old_or_new(void){
    ot_tm_profile_t p0,p1,p2,active,desired;
    ot_tm_defaults(&p0);
    p1=p0;p2=p0;
    p1.function[0]=OT_TM_BRAKE;
    p2.function[0]=OT_TM_REVERSE;
    ot_cfg_tm_journal_t committed={0},writing={0};
    assert(ot_cfg_tm_pack(&p0,&p1,&committed));
    assert(ot_cfg_tm_pack(&p1,&p2,&writing));
    /* NVS atomic commit returns either the previous complete blob or
     * the next complete blob, never a mix. Both restore the OLD mapping. */
    assert(ot_cfg_tm_unpack(&committed,&active,&desired));
    assert(active.function[0]==OT_TM_PARK);
    assert(ot_cfg_tm_unpack(&writing,&active,&desired));
    assert(active.function[0]==OT_TM_BRAKE);
    writing.version++;
    assert(!ot_cfg_tm_unpack(&writing,&active,&desired));
    /* Missing/malformed/torn records must not be promoted by decode. */
    assert(!ot_cfg_tm_unpack(NULL,&active,&desired));
    assert(!ot_cfg_tm_unpack(&committed,NULL,&desired));
}
int main(void){
    test_trigger_reboot_while_held();
    test_output_reboot_and_corruption();
    test_atomic_blob_old_or_new();
    puts("PASS durable config journal host recovery tests");
    return 0;
}
