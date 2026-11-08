/* Effects/Firmware #115 integration stress — HOST_SIMULATED only.
 *
 * Exercise the authoritative 140-effect renderer at Speed/Intensity extremes
 * on two independent output states while applying the real pure-C watchdog
 * progress eligibility policy. The source-order companion test proves that
 * production calls this policy only after led_strip_refresh_async_done()
 * succeeds. This cannot execute ESP-IDF TWDT, DMA, RMT or vehicle hardware.
 */
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "effects_1d.h"
#include "ot_single_watchdog.h"
#include "ot_frame_contract.h"
#include "output_manager.h"

typedef struct {
    uint32_t before;
    ot_wled_rgb_t pixels[OT_OM_MAX_PIXELS_PER_OUTPUT];
    uint32_t after;
} guarded_lane_t;

static guarded_lane_t lane[2];
static ot_effects_1d_state_t state[2];

typedef enum {
    WAIT_COMPLETED,
    WAIT_RETURNED_ERROR,
    WAIT_BLOCKED
} modeled_wait_result_t;

/* Production waits for O1 then O2 before it calls render_cycle(). A returned
 * error advances to the next lane; a blocked async_done never returns, so the
 * render cycle (and its WDT feed) is not reached, even if O1 already completed. */
static bool modeled_completion_cycle(modeled_wait_result_t o1,
                                     modeled_wait_result_t o2,
                                     bool *full_frame,
                                     bool *any_completed) {
    const modeled_wait_result_t result[2] = {o1,o2};
    bool completed[2] = {false,false};
    for (unsigned lane_index=0; lane_index<2u; lane_index++) {
        if (result[lane_index] == WAIT_BLOCKED) {
            *full_frame = ot_frame_all_submitted(completed,2u);
            *any_completed = completed[0] || completed[1];
            return false; /* render_cycle/feed is unreachable */
        }
        completed[lane_index] = result[lane_index] == WAIT_COMPLETED;
    }
    *full_frame = ot_frame_all_submitted(completed,2u);
    *any_completed = completed[0] || completed[1];
    return ot_sw_render_progress_eligible(*full_frame,*any_completed);
}

static bool modeled_render_feed(uint64_t cycle_start_ms,
                                uint64_t last_feed_ms,
                                uint32_t render_work_ms,
                                uint32_t completion_wait_ms,
                                uint32_t frame_pacing_ms,
                                bool all_lanes_completed,
                                bool any_lane_completed,
                                uint64_t timeout_ms,
                                uint64_t *feed_at_ms) {
    if (!feed_at_ms ||
        !ot_sw_render_progress_eligible(all_lanes_completed, any_lane_completed))
        return false;
    /* Render work happens before the wait. Use the longer of the RMT wait
     * and frame pacing delay, then add it to the injected render budget. */
    const uint32_t wait_ms = completion_wait_ms > frame_pacing_ms
        ? completion_wait_ms : frame_pacing_ms;
    const uint64_t completed_at =
        cycle_start_ms + (uint64_t)render_work_ms + wait_ms;
    if (completed_at - last_feed_ms >= timeout_ms)
        return false;
    *feed_at_ms = completed_at;
    return true;
}

int main(void) {
    uint16_t ids[160] = {0};
    const size_t count = ot_effects_1d_supported_ids(ids, 160u);
    assert(count == 140u);

    const uint8_t speeds[] = {0u, 255u};
    const uint8_t intensities[] = {0u, 255u};
    const uint16_t cadences[] = {8u, 33u};
    const uint64_t wire_us =
        (uint64_t)OT_OM_MAX_PIXELS_PER_OUTPUT * 30u + 300u;
    const uint64_t transfer_ms = (wire_us + 999u) / 1000u;
    const uint64_t twdt_timeout_ms = 15000u;
    const uint32_t injected_render_work_ms = 250u;

    /* 800-kHz RGB wire-time budget for one maximum-length lane. Both RMT
     * submissions overlap; the longest lane bounds normal completion time. */
    assert(OT_OM_MAX_PIXELS_PER_OUTPUT >= 1u);
    assert(transfer_ms > 0u && transfer_ms < twdt_timeout_ms);

    lane[0].before = 0x13579bdfu;
    lane[0].after = 0x2468ace0u;
    lane[1].before = 0xabcdef01u;
    lane[1].after = 0x10203040u;

    uint64_t now_ms = 0u;
    uint64_t last_feed_ms = 0u;
    uint32_t rendered_frames = 0u;

    for (size_t id_index = 0; id_index < count; ++id_index) {
        for (size_t si = 0; si < sizeof(speeds); ++si) {
            for (size_t ii = 0; ii < sizeof(intensities); ++ii) {
                for (size_t ci = 0; ci < sizeof(cadences)/sizeof(cadences[0]); ++ci) {
                    ot_effects_1d_reset(&state[0],
                        0x13570000u ^ (uint32_t)ids[id_index] ^ (uint32_t)(si << 8));
                    ot_effects_1d_reset(&state[1],
                        0x24680000u ^ (uint32_t)ids[id_index] ^ (uint32_t)(ii << 8));
                    memset(lane[0].pixels, 0, sizeof(lane[0].pixels));
                    memset(lane[1].pixels, 0, sizeof(lane[1].pixels));

                    ot_wled_fx_config_t cfg = {
                        .effect_id = ids[id_index],
                        .speed = speeds[si],
                        .intensity = intensities[ii],
                        .brightness = 32u,
                        .frame_time_ms = cadences[ci],
                        .palette_id = 0u,
                        .foreground = {255u, 48u, 12u},
                        .background = {0u, 0u, 0u},
                        .tertiary = {0u, 96u, 255u},
                        .tertiary_set = true
                    };
                    const ot_wled_audio_peak_t audio = {220.0f, 1800.0f};
                    cfg.audio_peak = &audio;

                    for (uint32_t frame = 0; frame < 3u; ++frame) {
                        const uint32_t frame_ms = frame * cadences[ci];
                        /* Independent per-output state: rendering O1 cannot
                         * mutate O2's state, and vice versa. */
                        const ot_effects_1d_state_t o2_before = state[1];
                        assert(ot_effects_1d_render(&cfg, &state[0], frame_ms,
                            lane[0].pixels, OT_OM_MAX_PIXELS_PER_OUTPUT));
                        assert(memcmp(&state[1], &o2_before, sizeof(state[1])) == 0);
                        const ot_effects_1d_state_t o1_after = state[0];
                        assert(ot_effects_1d_render(&cfg, &state[1], frame_ms,
                            lane[1].pixels, OT_OM_MAX_PIXELS_PER_OUTPUT));
                        assert(memcmp(&state[0], &o1_after, sizeof(state[0])) == 0);
                        assert(lane[0].before == 0x13579bdfu &&
                               lane[0].after == 0x2468ace0u);
                        assert(lane[1].before == 0xabcdef01u &&
                               lane[1].after == 0x10203040u);

                        /* Normal case: add a synthetic renderer delay AND the
                         * RMT completion wait before evaluating the 15s deadline.
                         * Both lanes complete; this models the exact feed policy. */
                        uint64_t next_feed_ms = 0u;
                        assert(modeled_render_feed(now_ms,last_feed_ms,
                            injected_render_work_ms,(uint32_t)transfer_ms,
                            cadences[ci],true,true,twdt_timeout_ms,&next_feed_ms));
                        assert(next_feed_ms - last_feed_ms < twdt_timeout_ms);
                        now_ms = next_feed_ms;
                        last_feed_ms = next_feed_ms;

                        /* One-lane fault: the surviving completion can feed,
                         * but the production stable-window source guard still
                         * requires all outputs and fresh inputs. */
                        assert(modeled_render_feed(now_ms,last_feed_ms,
                            injected_render_work_ms,(uint32_t)transfer_ms,
                            cadences[ci],false,true,twdt_timeout_ms,&next_feed_ms));
                        assert(next_feed_ms - last_feed_ms < twdt_timeout_ms);
                        now_ms = next_feed_ms;
                        last_feed_ms = next_feed_ms;

                        /* Neither transfer completed: no feed, even when the
                         * modeled render + wait interval reaches the deadline. */
                        assert(!modeled_render_feed(now_ms,last_feed_ms,
                            15000u,0u,0u,false,false,twdt_timeout_ms,&next_feed_ms));
                        rendered_frames++;
                    }
                }
            }
        }
    }

    /* Exercise returned errors separately from a blocked async_done call.
     * The production loop waits O1 then O2 before render_cycle(): a returned
     * error continues, but a hang on either lane prevents the feed call. */
    bool full=false, any=false;
    assert(modeled_completion_cycle(WAIT_COMPLETED,WAIT_COMPLETED,&full,&any));
    assert(full&&any);
    /* A returned O1 error followed by successful O2 completion can feed
     * partial progress, but never counts as a full stable frame. */
    assert(modeled_completion_cycle(WAIT_RETURNED_ERROR,WAIT_COMPLETED,&full,&any));
    assert(!full&&any);
    assert(!ot_sw_can_mark_stable(60000u,1000u,1000u,true,false));
    /* A returned O2 error after O1 completed has the same partial result. */
    assert(modeled_completion_cycle(WAIT_COMPLETED,WAIT_RETURNED_ERROR,&full,&any));
    assert(!full&&any);
    assert(!ot_sw_can_mark_stable(60000u,1000u,1000u,true,false));
    assert(!modeled_completion_cycle(WAIT_RETURNED_ERROR,WAIT_RETURNED_ERROR,&full,&any));
    assert(!full&&!any);
    /* A blocked O1 wait prevents reaching O2 and render_cycle(). */
    assert(!modeled_completion_cycle(WAIT_BLOCKED,WAIT_COMPLETED,&full,&any));
    assert(!full&&!any);
    /* If O1 completed but O2 async_done blocks, render_cycle() is still
     * unreachable; the earlier completion does not falsely feed the WDT. */
    assert(!modeled_completion_cycle(WAIT_COMPLETED,WAIT_BLOCKED,&full,&any));
    assert(!full&&any);
    assert(!modeled_completion_cycle(WAIT_BLOCKED,WAIT_BLOCKED,&full,&any));
    assert(!full&&!any);

    /* Exercise both sides of the deadline boundary. A 14,900ms render
     * plus 62ms completion can feed before 15s; a 14,940ms render plus 62ms
     * completion is overdue and must be rejected. Neither result is hardware. */
    uint64_t boundary_feed = 0u;
    assert(modeled_render_feed(0u,0u,14900u,(uint32_t)transfer_ms,33u,
                               true,true,twdt_timeout_ms,&boundary_feed));
    assert(boundary_feed == 14900u + transfer_ms);
    assert(!modeled_render_feed(0u,0u,14940u,(uint32_t)transfer_ms,33u,
                                true,true,twdt_timeout_ms,&boundary_feed));
    assert(!modeled_render_feed(0u,0u,15000u,0u,0u,false,false,
                                twdt_timeout_ms,&boundary_feed));

    printf("PASS HOST_SIMULATED Effects/#115 watchdog compatibility: %zu/140 effects, "
           "%u dual-lane frames, Speed/Intensity 0/255, 8/33ms cadence, "
           "synthetic render delay %ums + max(%llums completion, 33ms pacing) < %llums TWDT; "
           "14,900+62ms deadline PASS, 14,940+62ms overdue FAIL-CLOSED; "
           "RMT/TWDT hardware NOT_RUN\n",
           count, (unsigned)rendered_frames,
           (unsigned)injected_render_work_ms,
           (unsigned long long)transfer_ms,
           (unsigned long long)twdt_timeout_ms);
    return 0;
}
