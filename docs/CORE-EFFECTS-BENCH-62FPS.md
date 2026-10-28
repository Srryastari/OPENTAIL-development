# CORE native effects — 62 FPS bench target (unverified hardware)

This branch expands the original five native show modes without importing WLED code or altering the approved OpenTail Studio styling, saved output NVS schema, trigger map, or BLE wire IDs. The first five enum values are preserved for existing Android/BLE clients. Ten new modes are Wi-Fi show API controlled until the BLE protocol is versioned.

## Native catalog

Existing: Solid, Chase, Breathe, Wipe, Rainbow.

New: Strobe, Scan, Pulse, Fade, Blink, Plasma Sweep, Sparkle, Meteor, Color Waves, Twinkle.

These are original, bounded, deterministic 1D effect recipes, **not** a complete port of the WLED effect library. The existing original `effects/Effects1D.h` particle implementation is not integrated into the physical render task by this patch.

## Bench controls

Studio > CORE Alpha > Home > CORE · Live bench tests > CORE Native Effects. The same card appears in native Show Modes if rendered. Choose firmware-reported effect, output and zone, capped bench brightness and run time, then Start or Stop. Durations: 15/30/60 seconds on older firmware; 2 and 3 minutes only when `/api/v1/caps` reports `native_show_max_ms` supporting them. Real physical inputs continue to override effects using existing arbitration; restoring the effect when released is governed by the selected show deadline. This branch **does not implement per-zone trigger/show compositing** or animation sequences.

## FPS feasibility

**Target:** 62 rendered frames per second; **observed on hardware:** not yet measured for this branch.

At 800 kHz WS2815-compatible timing, 522 RGB pixels need approximately 15.66 ms of wire transmission per output, excluding reset and software overhead. The previous renderer called synchronous `led_strip_refresh()` for GPIO16 then GPIO17, limiting two full outputs to a theoretical ~31.9 combined frames/sec before overhead. This patch pins `led_strip==3.1.1` and calls `led_strip_refresh_async()` for both RMT channels, then `led_strip_refresh_async_done()` for both before buffer reuse. **Parallel transmission is a candidate improvement, not proof of 62 FPS.** The wire-time ceiling for a single full chain is about 63.9 FPS before reset and CPU overhead, leaving only ~0.47 ms of margin at 62 FPS. `target_fps:62` remains a declared objective and `measured_fps` reports actual throughput. Test both physical outputs and check for corruption/jitter at full pixel count; no actual FPS measurement has been made.


## Validation plan

1. Run `bash core-firmware/tests/run-host-tests.sh` in CI; this checks effect IDs, deterministic frames, brightness bounds, masks and existing arbitration tests.
2. Run ESP-IDF target build and Studio JS parsing checks; do not call passing host tests proof of an ESP-IDF build.
3. Flash bench unit only after backing up the currently working firmware, especially because the checked-in starting point reports `0.1.24-alpha` while a later bench firmware has been reported separately.
4. Open Studio; check **Home > CORE · Live bench tests** for native effects. Confirm an old firmware only exposes the names it advertises.
5. Choose Rainbow for 120 s (new firmware), trigger IN2 → GPIO5 Brake while active, inspect physical pixels, release brake, confirm return of the still-running show. Repeat for IN3 → GPIO6 and IN4 → GPIO7. Check Start/Stop/timeout and outputs GPIO16/17.
6. Record `measured_fps`, strip lengths, both active outputs, host voltage, board firmware/version, and whether show/trigger priority behaved as observed. **No bench, automotive safety, fallback, or 62 FPS PASS is claimed in this change.**

## Scope held for later

Full WLED effect parity, configurable native effect editor parity, arbitrary custom palettes, persistent sequences, per-zone show/trigger compositing, verified stable dual-output 62 FPS timing, backup controller/watchdog, and automotive protections are separate milestones. Preserve previous release artifacts.
