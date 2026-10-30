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
    assert(memcmp(&live,&b,sizeof(b))==0&&!pending.pending);
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
    assert(memcmp(live,b,sizeof(b))==0&&!pending.pending);
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
                assert(memcmp(&got,&b,sizeof(b))==0);
                assert(!pending.pending);
            }
        }
        /* NVS set/commit failure leaves stage; corrupt/missing stage fails
         * closed rather than promoting the newer active key. */
        ot_tm_journal_t damaged=stage;damaged.crc^=1u;
        assert(!ot_tm_journal_recover(&stage,&damaged,&got,&pending));
    }
}

int main(void){tm_case();om_case();crash_matrix();puts("PASS: durable journal CRC and all 256 recovery masks, both profiles");return 0;}
