#include "core_logic.h"
#include "arbitration_logic.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define TEST_STABLE_SAMPLES 4u

static bool sample_us_with_sequence(ot_signal_debouncer_t *state, bool raw,
                                      uint32_t now_us, uint32_t raw_previous_edge_us,
                                      uint32_t raw_edge_us, uint32_t raw_edge_sequence) {
    return ot_signal_debouncer_update(state,raw,now_us,raw_previous_edge_us,
                                      raw_edge_us,raw_edge_sequence,TEST_STABLE_SAMPLES);
}
static bool sample_us(ot_signal_debouncer_t *state, bool raw,
                      uint32_t now_us, uint32_t raw_edge_us) {
    uint32_t sequence=state->last_edge_sequence;
    if(raw!=state->last_raw_asserted)sequence+=2u;
    return sample_us_with_sequence(state,raw,now_us,0u,raw_edge_us,sequence);
}
static bool sample_at(ot_signal_debouncer_t *state, bool raw,
                      uint32_t now_ms, uint32_t raw_edge_ms) {
    return sample_us(state,raw,now_ms*1000u,raw_edge_ms*1000u);
}
static bool sample(ot_signal_debouncer_t *state, bool raw, uint32_t now_ms) {
    return sample_at(state,raw,now_ms,now_ms);
}

static void assertion_keeps_four_sample_qualification(void) {
    ot_signal_debouncer_t d;
    ot_signal_debouncer_init(&d,false,0u);
    assert(!sample(&d,true,0u));
    assert(!sample(&d,true,10u));
    assert(!sample(&d,true,20u));
    assert(sample(&d,true,30u));
    assert(d.asserted&&!d.release_pending);
}

static void release_hold_bridges_short_gaps(void) {
    const uint32_t gaps[]={100u,200u,300u,399u};
    for(unsigned i=0;i<sizeof(gaps)/sizeof(gaps[0]);i++) {
        ot_signal_debouncer_t d;
        ot_signal_debouncer_init(&d,true,0u);
        const uint32_t start=1000u+(uint32_t)i*1000u;
        assert(sample(&d,false,start));
        assert(sample(&d,false,start+gaps[i]/2u));
        assert(sample(&d,true,start+gaps[i]));
        assert(d.asserted&&!d.release_pending);
        /* Repeated flash cycles must not publish a falling edge or reset
         * the consumer's phase while every inactive gap is <400ms. */
        assert(sample(&d,false,start+gaps[i]+10u));
        assert(sample(&d,false,start+gaps[i]+409u));
        assert(sample(&d,false,start+gaps[i]+410u)==false);
    }
}

static void release_occurs_at_400ms_and_reassertion_requalifies(void) {
    ot_signal_debouncer_t d;
    ot_signal_debouncer_init(&d,true,0u);
    assert(sample(&d,false,2000u));
    assert(sample(&d,false,2100u));
    assert(sample(&d,false,2200u));
    assert(sample(&d,false,2300u));
    assert(sample(&d,false,2399u));
    assert(!sample(&d,false,2400u)); /* exactly 400ms continuous inactive */
    assert(!d.asserted&&!d.release_pending);

    /* A raw return observed at the deadline is not "before" expiry: the old
     * assertion ends, and the new assertion must qualify for four samples. */
    ot_signal_debouncer_init(&d,true,0u);
    assert(sample(&d,false,3000u));
    assert(!sample(&d,true,3400u));
    assert(!sample(&d,true,3410u));
    assert(!sample(&d,true,3420u));
    assert(sample(&d,true,3430u));

    ot_signal_debouncer_init(&d,true,0u);
    assert(sample(&d,false,4000u));
    assert(!sample(&d,false,4401u)); /* longer than the release hold */
}

static void reassertion_before_deadline_cancels_timer(void) {
    ot_signal_debouncer_t d;
    ot_signal_debouncer_init(&d,true,0u);
    assert(sample(&d,false,100u));
    assert(sample(&d,false,200u));
    assert(sample(&d,true,499u)); /* 399ms inactive */
    assert(d.asserted&&!d.release_pending);
    assert(sample(&d,false,500u));
    assert(sample(&d,false,899u));
    assert(sample(&d,false,900u)==false); /* new 400ms window */
}

static void timer_wrap_and_scheduler_jitter(void) {
    ot_signal_debouncer_t d;
    const uint32_t start=UINT32_MAX-199999u;
    ot_signal_debouncer_init(&d,true,0u);
    assert(sample_us(&d,false,start,start));
    assert(sample_us(&d,false,start+101000u,start));
    assert(sample_us(&d,false,start+250000u,start));
    assert(sample_us(&d,false,start+399000u,start));
    assert(!sample_us(&d,false,start+400000u,start)); /* wrap-safe 400ms */

    ot_signal_debouncer_init(&d,true,0u);
    assert(sample(&d,false,500u));
    const uint32_t jittered[]={507u,523u,547u,609u,800u,899u};
    for(unsigned i=0;i<sizeof(jittered)/sizeof(jittered[0]);i++)
        assert(sample_at(&d,false,jittered[i],500u));
    assert(!sample_at(&d,false,900u,500u));

    /* The sampler may run after a raw reassertion. Use the ISR edge timestamp
     * to distinguish a 399ms gap from a real 401ms inactive interval. */
    ot_signal_debouncer_init(&d,true,0u);
    assert(sample_at(&d,false,500u,500u));
    assert(sample_at(&d,true,912u,899u)); /* sampled 412ms later; edge was 399ms */
    assert(d.asserted&&!d.release_pending);
    ot_signal_debouncer_init(&d,true,0u);
    assert(sample_at(&d,false,500u,500u));
    assert(!sample_at(&d,true,912u,901u)); /* edge was 401ms: prior state expired */
}

static void restart_hold_after_pulse_between_sampler_wakes(void) {
    ot_signal_debouncer_t d;
    ot_signal_debouncer_init(&d,true,0u);
    assert(sample(&d,false,1000u)); /* falling edge starts first interval */
    const uint32_t sequence=d.last_edge_sequence+4u; /* unseen rise + fall */
    assert(sample_us_with_sequence(&d,false,1410u*1000u,1395u*1000u,1405u*1000u,sequence));
    assert(d.inactive_since_us==1405u*1000u);
    assert(sample_us_with_sequence(&d,false,1804u*1000u,1395u*1000u,1405u*1000u,sequence));
    assert(!sample_us_with_sequence(&d,false,1805u*1000u,1395u*1000u,1405u*1000u,sequence));

    /* A pulse that begins after the original deadline cannot resurrect
     * the old assertion if both edges occur between sampler wakes. */
    ot_signal_debouncer_init(&d,true,0u);
    assert(sample(&d,false,1001u));
    assert(sample(&d,false,1400u)); /* 399ms: old assertion still held */
    const uint32_t late_pulse_sequence=d.last_edge_sequence+4u;
    assert(!sample_us_with_sequence(&d,false,1410u*1000u,1402u*1000u,
                                    1405u*1000u,late_pulse_sequence));
    assert(!d.asserted&&!d.release_pending);

    /* A timestamp newer than its sample is a torn snapshot and must not
     * turn unsigned subtraction into an immediate false release. */
    ot_signal_debouncer_init(&d,true,0u);
    assert(sample(&d,false,2000u));
    const uint32_t future_sequence=d.last_edge_sequence+2u;
    assert(sample_us_with_sequence(&d,false,2000u*1000u,0u,2001u*1000u,
                                   future_sequence));
    assert(d.asserted&&d.release_pending);

    /* The GPIO level may become active before its ISR timestamp is published.
     * Multiple sampler wakes must wait rather than compare a stale falling
     * timestamp and falsely expire the pending hold. */
    ot_signal_debouncer_init(&d,true,0u);
    assert(sample(&d,false,3000u));
    const uint32_t rising_sequence=d.last_edge_sequence+2u;
    assert(sample_us_with_sequence(&d,true,3390u*1000u,0u,3000u*1000u,
                                   d.last_edge_sequence));
    assert(d.asserted&&d.release_pending&&d.awaiting_edge_record);
    assert(sample_us_with_sequence(&d,true,3410u*1000u,0u,3000u*1000u,
                                   d.last_edge_sequence));
    assert(d.asserted&&d.release_pending&&d.awaiting_edge_record);
    assert(sample_us_with_sequence(&d,true,3410u*1000u,0u,3390u*1000u,
                                   rising_sequence));
    assert(d.asserted&&!d.release_pending&&!d.awaiting_edge_record);
}

static void independent_turns_and_hazards(void) {
    ot_signal_debouncer_t left,right;
    ot_signal_debouncer_init(&left,true,0u);
    ot_signal_debouncer_init(&right,true,0u);
    assert(sample(&left,false,1000u));
    assert(sample(&right,false,1000u));

    assert(sample(&left,true,1100u));  /* left flash returns before deadline */
    assert(sample(&right,false,1100u));
    uint8_t mask=(sample(&left,true,1400u)?OT_SIG_LEFT:0u) |
                 (sample(&right,false,1400u)?OT_SIG_RIGHT:0u);
    assert(mask==OT_SIG_LEFT);
    ot_arbitration_t a=ot_arbitrate(mask,false,false);
    assert(!a.hazards);

    /* Reassert right after its own expiry: left remains independently active. */
    assert(sample(&left,true,1410u));
    assert(!sample(&right,true,1410u));
    assert(!sample(&right,true,1420u));
    assert(!sample(&right,true,1430u));
    assert(sample(&right,true,1440u));
    mask=(sample(&left,true,1440u)?OT_SIG_LEFT:0u) |
         (sample(&right,true,1440u)?OT_SIG_RIGHT:0u);
    a=ot_arbitrate(mask,false,false);
    assert(a.hazards);
}

static void one_turn_can_expire_while_other_continues(void) {
    ot_signal_debouncer_t left,right;
    ot_signal_debouncer_init(&left,true,0u);
    ot_signal_debouncer_init(&right,true,0u);
    assert(sample(&left,false,1000u));
    assert(sample(&right,false,1000u));
    assert(sample(&left,false,1200u));
    assert(sample(&right,true,1200u)); /* right flash returns; left stays off */
    assert(!sample(&left,false,1400u));
    assert(sample(&right,true,1400u));
    const uint8_t mask=(sample(&left,false,1410u)?OT_SIG_LEFT:0u) |
                       (sample(&right,true,1410u)?OT_SIG_RIGHT:0u);
    assert(mask==OT_SIG_RIGHT); /* no false hazard, right phase continues */
    assert(sample(&right,false,1420u));
    assert(sample(&right,false,1819u));
    assert(!sample(&right,false,1820u));
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
        ot_signal_debouncer_init(&left,(held&OT_SIG_LEFT)!=0u,0u);
        ot_signal_debouncer_init(&right,(held&OT_SIG_RIGHT)!=0u,0u);
        uint8_t physical=ot_debounce(&sampler,levels,TEST_STABLE_SAMPLES);
        physical=(uint8_t)(physical&(uint8_t)~(OT_SIG_LEFT|OT_SIG_RIGHT));
        if(sample(&left,levels[2],100u))physical|=OT_SIG_LEFT;
        if(sample(&right,levels[3],100u))physical|=OT_SIG_RIGHT;
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
    release_occurs_at_400ms_and_reassertion_requalifies();
    reassertion_before_deadline_cancels_timer();
    timer_wrap_and_scheduler_jitter();
    restart_hold_after_pulse_between_sampler_wakes();
    independent_turns_and_hazards();
    one_turn_can_expire_while_other_continues();
    all_256_boot_masks_and_arbitration();
    non_turn_inputs_keep_existing_four_sample_debounce();
    safety_priorities_remain_unchanged();
    puts("PASS Issue121 SignalDebouncer: four-sample assertion, 100/200/300/399ms gaps, exact 400ms release, pre/post-deadline unseen pulses, independent turns/hazards, one-side expiry, 256 boot masks, safety arbitration, non-turn timing, wrap and scheduler jitter; HOST_SIMULATED");
    return 0;
}
