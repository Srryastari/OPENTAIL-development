#include "core_logic.h"
#include "arbitration_logic.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define TEST_STABLE_SAMPLES 4u
#define MS(value) ((uint64_t)(value)*1000u)

static void edge(ot_signal_debouncer_t *state,bool raw,uint64_t at_us) {
    ot_signal_debouncer_transition(state,raw,at_us);
}
static bool sample_at(ot_signal_debouncer_t *state,uint64_t now_us) {
    return ot_signal_debouncer_sample(state,now_us,TEST_STABLE_SAMPLES);
}
static bool sample_ms(ot_signal_debouncer_t *state,uint64_t now_ms) {
    return sample_at(state,MS(now_ms));
}

static void assertion_keeps_four_sample_qualification(void) {
    ot_signal_debouncer_t d;
    ot_signal_debouncer_init(&d,false);
    edge(&d,true,MS(0u));
    assert(!sample_ms(&d,0u));
    assert(!sample_ms(&d,10u));
    assert(!sample_ms(&d,20u));
    assert(sample_ms(&d,30u));
    assert(d.asserted&&!d.release_pending);
}

static void release_hold_bridges_short_gaps(void) {
    const uint64_t gaps_ms[]={100u,200u,300u,399u};
    for(unsigned i=0;i<sizeof(gaps_ms)/sizeof(gaps_ms[0]);i++) {
        ot_signal_debouncer_t d;
        const uint64_t fall=MS(1000u+(uint64_t)i*1000u);
        const uint64_t rise=fall+gaps_ms[i]*1000u;
        ot_signal_debouncer_init(&d,true);
        edge(&d,false,fall);
        assert(sample_at(&d,fall+gaps_ms[i]*500u));
        edge(&d,true,rise);
        assert(sample_at(&d,rise));
        assert(d.asserted&&!d.release_pending);
        edge(&d,false,rise+1000u);
        const uint64_t deadline=rise+1000u+OT_TURN_RELEASE_HOLD_US;
        assert(sample_at(&d,deadline-1u));
        assert(!sample_at(&d,deadline));
    }
}

static void absolute_deadline_and_exact_boundary(void) {
    ot_signal_debouncer_t d;
    ot_signal_debouncer_init(&d,true);
    edge(&d,false,MS(2000u));
    assert(sample_ms(&d,2399u));
    assert(!sample_ms(&d,2400u)); /* exact 400ms expiry */
    assert(!d.asserted&&!d.release_pending);

    ot_signal_debouncer_init(&d,true);
    edge(&d,false,MS(3000u));
    edge(&d,true,MS(3400u)); /* equality expires old assertion */
    assert(!sample_ms(&d,3400u));
    assert(!sample_ms(&d,3410u));
    assert(!sample_ms(&d,3420u));
    assert(sample_ms(&d,3430u)); /* new assertion qualifies after four samples */

    ot_signal_debouncer_init(&d,true);
    edge(&d,false,MS(4000u));
    edge(&d,true,MS(4401u)); /* late pulse cannot resurrect expired assertion */
    assert(!d.asserted&&!d.release_pending);
    assert(!sample_ms(&d,4401u));
}

static void scheduler_jitter_uses_edge_timestamp(void) {
    ot_signal_debouncer_t d;
    ot_signal_debouncer_init(&d,true);
    edge(&d,false,MS(500u));
    edge(&d,true,MS(899u));     /* event is replayed before the delayed tick */
    assert(sample_ms(&d,912u)); /* actual inactive gap was only 399ms */
    assert(d.asserted&&!d.release_pending);

    ot_signal_debouncer_init(&d,true);
    edge(&d,false,MS(500u));
    edge(&d,true,MS(901u)); /* actual gap was 401ms */
    assert(!sample_ms(&d,912u));
    assert(!d.asserted&&!d.release_pending);
}

static void multiple_unseen_pulses_are_replayed_in_order(void) {
    ot_signal_debouncer_t d;
    ot_signal_debouncer_init(&d,true);
    edge(&d,false,MS(1001u));

    /* All four transitions occur between two 10ms sampler wakes. The first
     * rising edge bridges the original deadline; later pulses each restart
     * the deadline. Latest-edge-only snapshots cannot reproduce this result. */
    edge(&d,true,1400500u);
    edge(&d,false,1400700u);
    edge(&d,true,1402000u);
    edge(&d,false,1405000u);
    assert(sample_at(&d,MS(1410u)));
    assert(d.asserted&&d.release_pending);
    assert(d.release_deadline_us==1805000u);
    assert(sample_at(&d,1804999u));
    assert(!sample_at(&d,1805000u));

    /* A pulse after the original deadline expires the old assertion. Since
     * the pulse ends before four stable samples, it cannot reassert the lane. */
    ot_signal_debouncer_init(&d,true);
    edge(&d,false,MS(1001u));
    edge(&d,true,MS(1402u));
    edge(&d,false,MS(1405u));
    assert(!sample_ms(&d,1410u));
    assert(!d.asserted&&!d.release_pending);
}

static void overflow_resync_is_explicit_and_requalifies(void) {
    ot_signal_debouncer_t d;
    ot_signal_debouncer_init(&d,true);
    edge(&d,false,MS(1000u));
    ot_signal_debouncer_resync(&d,false);
    assert(!d.asserted&&!d.release_pending);
    ot_signal_debouncer_resync(&d,true);
    assert(!sample_ms(&d,1010u));
    assert(!sample_ms(&d,1020u));
    assert(!sample_ms(&d,1030u));
    assert(sample_ms(&d,1040u));
}

static void independent_turns_and_hazards(void) {
    ot_signal_debouncer_t left,right;
    ot_signal_debouncer_init(&left,true);
    ot_signal_debouncer_init(&right,true);
    edge(&left,false,MS(1000u));
    edge(&right,false,MS(1000u));
    edge(&left,true,MS(1100u));
    assert(sample_ms(&left,1400u));
    assert(!sample_ms(&right,1400u));
    uint8_t mask=sample_ms(&left,1400u)?OT_SIG_LEFT:0u;
    ot_arbitration_t a=ot_arbitrate(mask,false,false);
    assert(!a.hazards);

    /* Right reasserts only after its own release; Left remains independent. */
    edge(&right,true,MS(1410u));
    assert(sample_ms(&left,1410u));
    assert(!sample_ms(&right,1410u));
    assert(!sample_ms(&right,1420u));
    assert(!sample_ms(&right,1430u));
    assert(sample_ms(&right,1440u));
    mask=(sample_ms(&left,1440u)?OT_SIG_LEFT:0u)|
         (sample_ms(&right,1440u)?OT_SIG_RIGHT:0u);
    a=ot_arbitrate(mask,false,false);
    assert(a.hazards);
}

static void one_turn_can_expire_while_other_continues(void) {
    ot_signal_debouncer_t left,right;
    ot_signal_debouncer_init(&left,true);
    ot_signal_debouncer_init(&right,true);
    edge(&left,false,MS(1000u));
    edge(&right,false,MS(1000u));
    edge(&right,true,MS(1200u));
    assert(!sample_ms(&left,1400u));
    assert(sample_ms(&right,1400u));
    edge(&right,false,MS(1420u));
    assert(sample_ms(&right,1819u));
    assert(!sample_ms(&right,1820u));
}

static void all_256_boot_masks_and_arbitration(void) {
    for(unsigned held=0u;held<256u;held++) {
        ot_inputs_t sampler={0};
        bool levels[OT_IN_COUNT]={false};
        for(unsigned i=0;i<OT_IN_COUNT;i++) {
            levels[i]=(held&(1u<<i))!=0u;
            sampler.settled[i]=levels[i];
            sampler.pending[i]=levels[i];
            sampler.ticks[i]=TEST_STABLE_SAMPLES;
        }
        ot_signal_debouncer_t left,right;
        ot_signal_debouncer_init(&left,(held&OT_SIG_LEFT)!=0u);
        ot_signal_debouncer_init(&right,(held&OT_SIG_RIGHT)!=0u);
        uint8_t physical=ot_debounce(&sampler,levels,TEST_STABLE_SAMPLES);
        physical=(uint8_t)(physical&(uint8_t)~(OT_SIG_LEFT|OT_SIG_RIGHT));
        if(sample_ms(&left,100u))physical|=OT_SIG_LEFT;
        if(sample_ms(&right,100u))physical|=OT_SIG_RIGHT;
        assert(physical==(uint8_t)held);

        const ot_arbitration_t a=ot_arbitrate(physical,false,true);
        assert(a.signals==(uint8_t)held);
        assert(a.hazards==((held&(OT_SIG_LEFT|OT_SIG_RIGHT))==
                           (OT_SIG_LEFT|OT_SIG_RIGHT)));
        assert(a.brake_over_parking==((held&(OT_SIG_BRAKE|OT_SIG_PARK))==
                                      (OT_SIG_BRAKE|OT_SIG_PARK)));
        if(held&OT_SIG_BRAKE)
            assert(ot_arb_zone(&a,0,OT_TOP,3u,31u)==OT_ZONE_BRAKE);
        if(held&OT_SIG_REVERSE)
            assert(ot_arb_zone(&a,0,OT_REVERSE,3u,31u)==OT_ZONE_REVERSE);
        if(held&OT_SIG_LEFT)
            assert(ot_arb_zone(&a,0,OT_TURN,3u,31u)==OT_ZONE_LEFT_TURN);
        if(held&OT_SIG_RIGHT)
            assert(ot_arb_zone(&a,1,OT_TURN,3u,31u)==OT_ZONE_RIGHT_TURN);
    }
}

static void non_turn_inputs_keep_existing_four_sample_debounce(void) {
    const unsigned channels[]={0u,1u,4u,5u,6u,7u};
    for(unsigned c=0;c<sizeof(channels)/sizeof(channels[0]);c++) {
        ot_inputs_t d={0};
        bool levels[OT_IN_COUNT]={false};
        const unsigned bit=channels[c];
        levels[bit]=true;
        for(unsigned i=0;i<TEST_STABLE_SAMPLES-1u;i++)
            assert((ot_debounce(&d,levels,TEST_STABLE_SAMPLES)&(1u<<bit))==0u);
        assert((ot_debounce(&d,levels,TEST_STABLE_SAMPLES)&(1u<<bit))!=0u);
        levels[bit]=false;
        for(unsigned i=0;i<TEST_STABLE_SAMPLES-1u;i++)
            assert((ot_debounce(&d,levels,TEST_STABLE_SAMPLES)&(1u<<bit))!=0u);
        assert((ot_debounce(&d,levels,TEST_STABLE_SAMPLES)&(1u<<bit))==0u);
    }
}

static void safety_priorities_remain_unchanged(void) {
    const uint8_t mask=(uint8_t)(OT_SIG_LEFT|OT_SIG_RIGHT|OT_SIG_BRAKE|
                                 OT_SIG_PARK|OT_SIG_REVERSE);
    const ot_arbitration_t a=ot_arbitrate(mask,false,true);
    assert(a.hazards&&a.brake_over_parking);
    assert(ot_arb_zone(&a,0,OT_TOP,3u,31u)==OT_ZONE_BRAKE);
    assert(ot_arb_zone(&a,0,OT_REVERSE,3u,31u)==OT_ZONE_REVERSE);
    assert(ot_arb_zone(&a,0,OT_TURN,3u,31u)==OT_ZONE_LEFT_TURN);
    assert(ot_arb_zone(&a,1,OT_TURN,3u,31u)==OT_ZONE_RIGHT_TURN);
}

int main(void) {
    assertion_keeps_four_sample_qualification();
    release_hold_bridges_short_gaps();
    absolute_deadline_and_exact_boundary();
    scheduler_jitter_uses_edge_timestamp();
    multiple_unseen_pulses_are_replayed_in_order();
    overflow_resync_is_explicit_and_requalifies();
    independent_turns_and_hazards();
    one_turn_can_expire_while_other_continues();
    all_256_boot_masks_and_arbitration();
    non_turn_inputs_keep_existing_four_sample_debounce();
    safety_priorities_remain_unchanged();
    puts("PASS Issue121 SignalDebouncer: ISR transition history, multiple inter-sample pulses, absolute 400ms deadline, strict equality expiry, four-sample requalification, independent Left/Right, hazards, 256 boot masks, unchanged non-turn inputs, overflow resync and arbitration; HOST_SIMULATED");
    return 0;
}
