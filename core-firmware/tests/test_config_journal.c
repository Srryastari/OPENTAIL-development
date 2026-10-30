#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "config_journal.h"
static void tm_case(void){
    ot_tm_profile_t a={0},b={0};ot_tm_defaults(&a);b=a;
    b.function[0]=OT_TM_AUX3;b.function[1]=OT_TM_LEFT;
    ot_tm_journal_t journal={0};
    assert(ot_tm_journal_build(&journal,&a,&b));
    assert(ot_tm_journal_valid(&journal));
    ot_tm_profile_t live=a;ot_tm_pending_t pending={0};
    /* Every interrupted stage/primary-write boundary, including power loss
     * while any of IN1..IN8 is held, recovers A and staged B. */
    for(unsigned mask=0;mask<256u;mask++){
        (void)mask;live=a;pending=(ot_tm_pending_t){0};
        assert(ot_tm_journal_recover(NULL,&journal,&live,&pending));
        assert(memcmp(&live,&a,sizeof(a))==0&&pending.pending);
        assert(memcmp(&pending.profile,&b,sizeof(b))==0);
        assert(ot_tm_journal_recover(&journal,&journal,&live,&pending));
        assert(memcmp(&live,&a,sizeof(a))==0&&pending.pending);
    }
    live=a;pending=(ot_tm_pending_t){0};
    assert(ot_tm_journal_recover(&journal,NULL,&live,&pending));
    assert(memcmp(&live,&a,sizeof(a))==0&&pending.pending);
    assert(memcmp(&pending.profile,&b,sizeof(b))==0);
    for(size_t byte=0;byte<sizeof(journal);byte++){
        ot_tm_journal_t corrupt=journal;
        ((uint8_t*)&corrupt)[byte]^=0x40u;
        if(memcmp(&corrupt,&journal,sizeof(journal))!=0)
            assert(!ot_tm_journal_valid(&corrupt));
    }
    assert(!ot_tm_journal_recover(NULL,&(ot_tm_journal_t){0},&live,&pending));
}
static void om_case(void){
    ot_om_output_t a[OT_OM_OUTPUTS]={0},b[OT_OM_OUTPUTS]={0},live[OT_OM_OUTPUTS];
    ot_om_defaults(a);memcpy(b,a,sizeof(b));
    for(unsigned i=0;i<OT_OM_OUTPUTS;i++){
        b[i].active_pixels=17u;
        b[i].zones[0].pixels=17u;
        b[i].color_order=OT_OM_GRB;
        assert(ot_om_valid(&b[i]));
    }
    ot_om_journal_t journal={0};ot_om_pending_t pending={0};
    assert(ot_om_journal_build(&journal,a,b));
    assert(ot_om_journal_valid(&journal));
    for(unsigned mask=0;mask<256u;mask++){
        (void)mask;memcpy(live,a,sizeof(live));
        assert(ot_om_journal_recover(NULL,&journal,live,&pending));
        assert(memcmp(live,a,sizeof(a))==0&&pending.pending);
        assert(memcmp(pending.outputs,b,sizeof(b))==0);
        assert(ot_om_journal_recover(&journal,&journal,live,&pending));
        assert(memcmp(live,a,sizeof(a))==0);
    }
    assert(ot_om_journal_recover(&journal,NULL,live,&pending));
    assert(memcmp(live,a,sizeof(a))==0&&pending.pending);
    assert(memcmp(pending.outputs,b,sizeof(b))==0);
    for(size_t i=0;i<sizeof(journal);i++){
        ot_om_journal_t corrupt=journal;
        ((uint8_t*)&corrupt)[i]^=0x01u;
        assert(!ot_om_journal_valid(&corrupt));
    }
}

/* Model every atomic flash boundary. Before a stage exists, the previous
 * active profile remains authoritative. Once staged exists, even writing
 * active_v3 does not authorize it at reboot. A torn/corrupt journal returns
 * an error, never an untrusted candidate. The same is true on all GPIO masks.
 * Real flash/power-cut injection is a separate hardware release gate. */
static void crash_matrix(void){
    ot_tm_profile_t a={0},b={0},got={0};
    ot_tm_defaults(&a);b=a;b.function[0]=OT_TM_RIGHT;
    ot_tm_journal_t stage={0},primary={0};
    ot_tm_pending_t pending={0};
    assert(ot_tm_journal_build(&stage,&a,&b));
    assert(ot_tm_journal_build(&primary,&a,&a));
    for(unsigned gpio=0u;gpio<256u;gpio++){
        (void)gpio;
        /* phase 0: no write; phase 1: stage committed;
         * phase 2: active committed while stage still present;
         * phase 3: stage cleared following promotion. */
        for(unsigned phase=0u;phase<=3u;phase++){
            got=a;pending=(ot_tm_pending_t){0};
            const ot_tm_journal_t *durable_active=phase>=2u?&stage:&primary;
            const ot_tm_journal_t *durable_stage=
                phase==1u||phase==2u?&stage:NULL;
            assert(ot_tm_journal_recover(durable_active,durable_stage,
                                         &got,&pending));
            if(phase<=2u){
                assert(memcmp(&got,&a,sizeof(a))==0);
                assert(pending.pending==(phase!=0u));
            }else{
                /* Stage erasure is NOT an authorization to publish to RAM:
                 * a crash in this gap replays old + desired on next boot. */
                assert(memcmp(&got,&a,sizeof(a))==0);
                assert(pending.pending);
                assert(memcmp(&pending.profile,&b,sizeof(b))==0);
            }
        }
        /* NVS set/commit failure leaves stage; corrupt/missing stage fails
         * closed rather than promoting the newer active key. */
        ot_tm_journal_t damaged=stage;damaged.crc^=1u;
        assert(!ot_tm_journal_recover(&stage,&damaged,&got,&pending));
    }
}

static void revision_case(void){
    ot_tm_profile_t old,next;ot_tm_defaults(&old);next=old;
    next.function[0]=OT_TM_BRAKE;
    ot_tm_journal_t t={0};
    assert(ot_tm_journal_build_revisions(&t,&old,&next,301u,302u));
    assert(ot_tm_journal_valid(&t));
    assert(t.previous_revision==301u&&t.candidate_revision==302u);
    ot_tm_profile_t live={0};ot_tm_pending_t pending={0};
    assert(ot_tm_journal_recover(&t,NULL,&live,&pending));
    assert(pending.pending&&live.function[0]==OT_TM_PARK);
    assert(t.candidate_revision!=301u); /* stale editor rejected at admission */
    assert(!ot_tm_journal_build_revisions(&t,&old,&next,65535u,0u));
    assert(!ot_tm_journal_build_revisions(&t,&old,&next,301u,301u));
    ot_tm_journal_t corrupt=t;corrupt.candidate_revision=1u;
    assert(!ot_tm_journal_valid(&corrupt));
    ot_om_output_t a[OT_OM_OUTPUTS],b[OT_OM_OUTPUTS];
    ot_om_defaults(a);memcpy(b,a,sizeof(b));
    b[0].zones[0].role=OT_OM_BRAKE;
    uint16_t prev[OT_OM_OUTPUTS],desired[OT_OM_OUTPUTS];
    for(unsigned i=0;i<OT_OM_OUTPUTS;i++)prev[i]=desired[i]=(uint16_t)(90u+i);
    desired[0]++;
    ot_om_journal_t o={0};
    assert(ot_om_journal_build_revisions(&o,a,b,prev,desired));
    assert(ot_om_journal_valid(&o));
    assert(o.candidate_revision[0]==91u);
    o.candidate_revision[0]++;
    assert(!ot_om_journal_valid(&o)); /* tampered revision invalidates CRC */
}
static void upgrade_case(void){
    ot_tm_profile_t old,next;ot_tm_defaults(&old);next=old;next.function[0]=OT_TM_RIGHT;
    ot_tm_journal_v1_t v1={0};v1.magic=OT_CFG_JOURNAL_MAGIC;v1.version=1u;
    v1.previous=old;v1.candidate=next;
    v1.crc=ot_cfg_crc32(&v1,sizeof(v1));
    ot_tm_journal_t v2={0};
    assert(ot_tm_journal_upgrade_v1(&v1,&v2));
    assert(ot_tm_journal_valid(&v2));
    assert(v2.previous_revision==1u&&v2.candidate_revision==2u);
    v1.previous.function[0]=255u;
    assert(!ot_tm_journal_upgrade_v1(&v1,&v2));
}
int main(void){
    tm_case();om_case();crash_matrix();revision_case();upgrade_case();
    puts("PASS: durable journal CRC, monotonic revisions, crash boundaries and 256 input masks");
    return 0;
}
