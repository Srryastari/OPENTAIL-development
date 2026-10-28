# CORE v0.1.27-alpha integration and recovery gate

## Source baseline

This integration is based on the existing CORE v0.1.25 development line `feature/wled-1d-fade-wipe-sweep`, not the older v0.1.24 baseline used by the regressed v0.1.26-alpha build. It retains the newer BLE effect transport, independent per-output/per-zone effects, eight saved-scene slots, Output Manager NVS, trigger manager/arbitration, and the onboard status LED driver.

## Unified effect catalog

- Studio v0.19.62 keeps one Effects selector. CORE BLE capabilities enumerate the 120-entry WLED-compatible 1D catalog plus the original OpenTail Embers effect (121 IDs total).
- The six existing source-derived WLED effects (Solid, Blink, Wipe, Sweep, Fade, Strobe) remain on their source-derived implementations.
- Remaining 1D entries use deterministic bounded compatibility recipes in `wled_1d.c`. This is a working firmware implementation, but **not pixel-identical parity with the full upstream WLED engine**. 2D/matrix and audio-dependent effects are excluded from the 1D catalog.
- The BLE capability characteristic uses offset-aware long reads; Android negotiates ATT MTU 247 before service discovery where supported. Capability payload is 244 bytes at 121 IDs.

## Restored onboard status LED patterns

- Startup: white flash.
- Wi-Fi ready: cyan pulse, 1.0-second cycle.
- Connected: green double pulse, ~2.2-second cycle.
- Disconnected: red double pulse, ~2.2-second cycle.
- Show Mode: smooth rainbow, 1.4-second cycle.
- Fault: red 90 ms on / 90 ms off; fault remains visible when ordinary status indications are muted.

The host status tests assert these exact timing/color rules.

## Frame scheduling

- Both RMT output refreshes are started asynchronously before either completion is awaited.
- Buffers are not reused until both transfers report completion.
- Firmware reports `measured_fps` and a separate `target_fps: 62`; **62 FPS remains an unverified hardware target**, especially at 522 pixels on each output.

## Required release gates

1. Host tests: all 120 catalog entries resolve, render deterministic bounded frames, and keep brightness caps; status patterns, BLE parsing, zone/preset/output/trigger regression suites pass.
2. ESP-IDF 5.5.5 target build succeeds with led_strip 3.1.1.
3. Studio v0.19.62 web and Android assets are byte-identical and match the pinned SHA-256.
4. Android v0.1.28 debug APK compiles.
5. Hardware acceptance before any release flash: status LED patterns, BLE authorization and complete 121-ID readback, saved-scene reload, independent effects across both outputs/zones, physical IN1..IN8 trigger overrides, Stop/restore, and measured FPS at configured pixel counts.

A passing software build is not evidence of physical pixel output, trigger safety, hardware failover, or achieved 62 FPS. Do not install as sole road-lighting hardware; the current prototype has no independent hardware failover.
