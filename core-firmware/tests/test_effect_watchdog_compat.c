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
#include <stdio.h>
#include <string.h>
#include "effects_1d.h"
#include "ot_single_watchdog.h"
#include "output_manager.h"

typedef struct {
    uint32_t before;
    ot_wled_rgb_t pixels[OT_OM_MAX_PIXELS_PER_OUTPUT];
    uint32_t after;
} guarded_lane_t;

static guarded_lane_t lane[2];
static ot_effects_1d_state_t state[2];

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
                        assert(ot_effects_1d_render(&cfg, &state[0], frame_ms,
                            lane[0].pixels, OT_OM_MAX_PIXELS_PER_OUTPUT));
                        assert(ot_effects_1d_render(&cfg, &state[1], frame_ms,
                            lane[1].pixels, OT_OM_MAX_PIXELS_PER_OUTPUT));
                        assert(lane[0].before == 0x13579bdfu &&
                               lane[0].after == 0x2468ace0u);
                        assert(lane[1].before == 0xabcdef01u &&
                               lane[1].after == 0x10203040u);

                        /* Normal case: both independently submitted RMT lanes
                         * return ESP_OK from async_done; the render WDT user
                         * is therefore eligible for a real feed. */
                        assert(ot_sw_render_progress_eligible(true, true));
                        now_ms += transfer_ms > cadences[ci]
                            ? transfer_ms : cadences[ci];
                        assert(now_ms - last_feed_ms < twdt_timeout_ms);
                        last_feed_ms = now_ms;

                        /* One-lane fault: a completed surviving lane is genuine
                         * progress, but it cannot satisfy full-frame stability. */
                        assert(ot_sw_render_progress_eligible(false, true));
                        now_ms += transfer_ms;
                        assert(now_ms - last_feed_ms < twdt_timeout_ms);
                        last_feed_ms = now_ms;

                        /* Neither transfer completed: queued work is not progress. */
                        assert(!ot_sw_render_progress_eligible(false, false));
                        rendered_frames++;
                    }
                }
            }
        }
    }

    /* A fully stalled renderer has no eligible feed and reaches the configured
     * 15-second timeout; this is a deterministic policy model, not a live WDT. */
    const uint64_t stalled_at = now_ms;
    assert(!ot_sw_render_progress_eligible(false, false));
    now_ms += twdt_timeout_ms;
    assert(now_ms - stalled_at == twdt_timeout_ms);

    printf("PASS HOST_SIMULATED Effects/#115 watchdog compatibility: %zu/140 effects, "
           "%u dual-lane frames, Speed/Intensity 0/255, 8/33ms cadence, "
           "%llu-pixel lane wire bound %llums vs %llums TWDT; "
           "single-lane progress and zero-lane timeout policy verified; "
           "RMT/TWDT hardware NOT_RUN\n",
           count, (unsigned)rendered_frames,
           (unsigned long long)OT_OM_MAX_PIXELS_PER_OUTPUT,
           (unsigned long long)transfer_ms,
           (unsigned long long)twdt_timeout_ms);
    return 0;
}
