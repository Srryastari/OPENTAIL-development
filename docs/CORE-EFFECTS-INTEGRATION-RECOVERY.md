# CORE v0.1.27-alpha integration and recovery gate

## Source baseline

This integration is based on the existing CORE v0.1.25 development line `feature/wled-1d-fade-wipe-sweep`, not the older v0.1.24 baseline used by the regressed v0.1.26-alpha build. It retains the newer BLE effect transport, independent per-output/per-zone effects, eight saved-scene slots, Output Manager NVS, trigger manager/arbitration, and the onboard status LED driver.

## Unified effect catalog

- Studio v0.19.62 keeps one Effects selector. CORE BLE capabilities enumerate the 120-entry WLED-compatible 1D catalog plus the original OpenTail Embers effect (121 IDs total).
- The six existing source-derived WLED effects (Solid, Blink, Wipe, Sweep, Fade, Strobe) remain on their source-derived implementations.
- Remaining 1D entries use deterministic bounded compatibility recipes in `wled_1d.c`. This is a working firmware implementation, but **not pixel-identical parity with the full upstream WLED engine**. 2D/matrix and audio-dependent effects are excluded from the 1D catalog.
- The BLE capability characteristic uses offset-aware long reads; Android negotiates ATT MTU 247 before service discovery where supported. Capability payload is 244 bytes at 121 IDs.

## Required color and LED-profile parity

- Dual-color behavior is a first-class requirement: expose foreground and background colors for every effect that supports them, plus accent color where the native WLED effect supports it. Preserve native WLED parameter semantics rather than translating every effect into generic RGB recipes.
- Color settings are independently stored per output/zone with the effect assignment, and must survive selection changes, BLE disconnect/reconnect, saved-scene save/load, and reboot. Updating one zone must not overwrite another zone's colors, palette, or phase.
- Each physical output needs an explicit LED profile: protocol/driver, RGB channel order, supported channel format, brightness behavior, and any applicable white-balance/gamma settings. At minimum, validate the actual WS2815 RGB profile and channel orders used by OpenTail, including GRB and BRG where configured. Profile selection must match real hardware and native WLED output configuration.
- Support RGBW, CCT/dual-white, or other channel profiles only when the selected output hardware and driver physically support those channels. Do not simulate a nonexistent white or CCT channel by silently remapping RGB data. Current WS2815 strips are RGB-only; RGBW/CCT parity requires compatible hardware, driver support, and explicit profile validation.
- Palette IDs, palette lookup, color order, brightness scaling, gamma/white correction and per-effect color parameters must follow the pinned upstream WLED implementation for the selected profile.
- Parity acceptance compares raw output RGB/RGBW frames and channel values against the same pinned WLED source, using identical effect ID, effect parameters, palette, foreground/background/accent colors, brightness, LED profile/order, pixel count, and deterministic frame timing. Compare both uniform test colors and representative frames from every supported effect. Document effects that are inherently nondeterministic and provide seeded/reproducible tests where possible.

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
5. Native WLED renderer and color-profile parity tests pass for every claimed supported effect/profile; catalog names alone are not proof of implementation.
6. Hardware acceptance before any release flash: status LED patterns, BLE authorization and complete effect-ID readback, saved-scene reload, independent effects and color settings across both outputs/zones, physical IN1..IN8 trigger overrides, Stop/restore, and measured FPS at configured pixel counts.

A passing software build is not evidence of physical pixel output, trigger safety, hardware failover, or achieved 62 FPS. Do not install as sole road-lighting hardware; the current prototype has no independent hardware failover.
