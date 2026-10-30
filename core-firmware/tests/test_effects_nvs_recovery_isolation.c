/* Effects-owned cross-contract: real CORE journals + real FX zone bank.
 * Saved-but-pending vehicle remaps may not retarget live FX or replace a
 * safety zone under an asserted input. Pure host test; not a power-cut
 * simulation of ESP32 flash, raw pin sampling or a live RMT device.
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "config_journal.h"
#include "fx_zone_bank.h"

static void geometry(ot_om_output_t out[OT_OM_OUTPUTS]){
    ot_om_defaults(out);
    for(unsigned ch=0;ch<OT_OM_OUTPUTS;ch++){
        out[ch].zone_count=2;
        out[ch].active_pixels=16u;
        out[ch].zones[0].pixels=8u;
        out[ch].zones[0].role=OT_OM_DECORATIVE;
        out[ch].zones[1].pixels=8u;
        out[ch].zones[1].role=(ch==0)?OT_OM_BRAKE:OT_OM_REVERSE;
        snprintf(out[ch].zones[1].name,OT_OM_NAME_LEN,"Safety %u",ch);
        assert(ot_om_valid(&out[ch]));
    }
}
static ot_ble_fx_packet_t effect(uint8_t chain){
    ot_ble_fx_packet_t p={0};
    p.effect_id=0u;p.chain_mask=chain;p.zone_mask=1u;p.ttl_sec=60u;
    p.brightness=24u;
    return p;
}
int main(void){
    ot_om_output_t active[OT_OM_OUTPUTS],staged[OT_OM_OUTPUTS];
    geometry(active);memcpy(staged,active,sizeof(staged));
    /* Same total pixels but different safety-role boundaries and order:
     * an unsafe early swap would relocate physical brake/reverse pixels. */
    staged[0].zones[0].pixels=12u;staged[0].zones[1].pixels=4u;
    staged[0].zones[1].role=OT_OM_REVERSE;
    staged[1].zones[0].pixels=4u;staged[1].zones[1].pixels=12u;
    staged[1].color_order=OT_OM_BRG;
    for(unsigned ch=0;ch<OT_OM_OUTPUTS;ch++)
        assert(ot_om_valid(&staged[ch]));
    uint16_t prev[OT_OM_OUTPUTS],next[OT_OM_OUTPUTS];
    for(unsigned ch=0;ch<OT_OM_OUTPUTS;ch++){
        prev[ch]=(uint16_t)(180u+ch);
        next[ch]=(uint16_t)(181u+ch);
    }
    ot_om_journal_t journal={0};
    assert(ot_om_journal_build_revisions(&journal,active,staged,prev,next));
    assert(ot_om_journal_valid(&journal));

    ot_tm_profile_t previous_tm={0},next_tm={0};
    ot_tm_defaults(&previous_tm);next_tm=previous_tm;
    next_tm.function[0]=OT_TM_REVERSE; /* physical input remap pending */
    ot_tm_journal_t tm_journal={0};
    assert(ot_tm_journal_build_revisions(&tm_journal,&previous_tm,
                                        &next_tm,301u,302u));
    assert(ot_tm_journal_valid(&tm_journal));

    /* During a live show, a pending journal alone must not mutate the bank.
     * Each physical lane has independent FX generation/start/TTL. */
    ot_fx_zone_bank_t running;
    ot_fx_bank_reset(&running);
    ot_ble_fx_packet_t p=effect(1u);
    assert(ot_fx_bank_apply_configured(&running,&p,active,100u,false));
    p=effect(2u);
    assert(ot_fx_bank_apply_configured(&running,&p,active,200u,false));
    const ot_fx_zone_bank_t before=running;
    assert(before.zones[0][0].generation!=before.zones[1][0].generation);
    assert(before.zones[0][0].started_ms==100u);
    assert(before.zones[1][0].started_ms==200u);
    assert(!before.zones[0][1].configured && !before.zones[1][1].configured);

    unsigned recoveries=0u,wire_safety=0u;
    for(unsigned mask=0;mask<256u;mask++){
        for(unsigned stage=0;stage<3u;stage++){
            /* Stage present, active and staged simultaneously present, or
             * stage already erased during interrupted finalization. */
            const ot_om_journal_t *on_disk_stage=
                 stage<2u?&journal:NULL;
            const ot_om_journal_t *on_disk_active=
                 stage>0u?&journal:NULL;
            const ot_tm_journal_t *tm_stage=
                 stage<2u?&tm_journal:NULL;
            const ot_tm_journal_t *tm_active=
                 stage>0u?&tm_journal:NULL;
            ot_om_output_t live[OT_OM_OUTPUTS]={0};
            ot_om_pending_t om_pending={0};
            ot_tm_profile_t tm_live={0};
            ot_tm_pending_t tm_pending={0};
            assert(ot_om_journal_recover(on_disk_active,on_disk_stage,
                                         live,&om_pending));
            assert(ot_tm_journal_recover(tm_active,tm_stage,
                                         &tm_live,&tm_pending));
            assert(om_pending.pending&&tm_pending.pending);
            assert(!memcmp(live,active,sizeof(active)));
            assert(!memcmp(om_pending.outputs,staged,sizeof(staged)));
            assert(!memcmp(&tm_live,&previous_tm,sizeof(previous_tm)));
            assert(!memcmp(&tm_pending.profile,&next_tm,sizeof(next_tm)));
            assert(!memcmp(&running,&before,sizeof(running)));
            for(unsigned ch=0;ch<OT_OM_OUTPUTS;ch++){
                assert(ot_om_layout(&live[ch],&(ot_layout_t){0}));
                assert(ot_om_section(&live[ch],7u)==0);
                assert(ot_om_section(&live[ch],8u)==1);
                assert(ot_fx_slot_active(&running.zones[ch][0],201u));
                assert(!ot_fx_slot_active(&running.zones[ch][1],201u));
                const uint8_t role=live[ch].zones[1].role;
                const uint8_t signal=(ch==0)?OT_SIG_BRAKE:OT_SIG_REVERSE;
                ot_rgb_t color={0};
                const bool hit=ot_om_trigger_overlay(role,(uint8_t)mask,
                                                     true,32u,&color);
                assert(hit==((mask&signal)!=0u));
                if(hit){
                    assert(ch==0?(color.r==32u&&color.g==0u&&color.b==0u):
                                 (color.r==32u&&color.g==32u&&color.b==32u));
                    ++wire_safety;
                }
            }
            ++recoveries;
        }
    }

    /* Saved scenes cannot auto-start through journal recovery. Effects
     * own a distinct volatile bank which is reset at firmware startup. */
    ot_fx_zone_bank_t boot={0};
    ot_fx_bank_reset(&boot);
    assert(!ot_fx_bank_any_active(&boot,201u));
    /* A STOP during pending edits must invalidate every running effect
     * without changing active layout or pending desired geometry. */
    ot_fx_bank_stop_all(&running);
    assert(!ot_fx_bank_any_active(&running,201u));
    assert(ot_om_valid(&active[0])&&ot_om_valid(&staged[0]));
    assert(recoveries==3u*256u&&wire_safety>0u);
    printf("Effects/NVS cross-contract: %u journal phases/input masks, %u physical Brake/Reverse safety overrides; independent zones, pending geometry, STOP and no boot FX auto-start PASS (host only)\n",
           recoveries,wire_safety);
    return 0;
}
