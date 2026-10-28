# OpenTail / WLED native renderer integration

**Status: architecture and migration candidate; full BLE integration and physical validation remain incomplete.**

The owner requirement is the **actual upstream WLED renderer**, effect-specific metadata, dual-color semantics and validated LED output profiles. The current ESP-IDF compatibility renderer does not satisfy pixel-identical parity.

## Single-renderer architecture

- **Canonical renderer:** pinned upstream WLED source at commit `20e7a717664295eefc6ad77972cee1888d2da072`. Keep WLED's effect runtime, Segment state, palettes, transitions, random state, particle system, color conversion, and bus output code intact where possible.
- **OpenTail control plane:** existing approved Studio/Android UI, authenticated BLE commands, Output Manager and saved vehicle profiles map to native WLED segments. Keep one unified Effects catalog; no duplicate WLED/native effect picker.
- **OpenTail trigger layer:** use the native WLED post-render hook for brake/turn/hazards/reverse on configured zone spans. Preserve the underlying effect state and phase; reveal it again when the trigger releases. Respect Stop/global off and brightness protection.
- **OpenTail effects:** register original effects such as Embers and optional startup/shutdown animations as native WLED effects with proper metadata, so one engine and one catalog own them.
- **No parallel decorative renderers:** do not blend output frames from WLED and the old ESP-IDF compatibility renderer. The compatibility code is migration/rollback scaffolding, not an alternate parity implementation.

## Effect metadata is the source of truth

Use metadata from the exact pinned and compiled WLED firmware, including `/json/eff`, `/json/fxdata`, `/json/pal`, and `/json/info`. Do not hardcode generic sliders or color controls for every effect.

The WLED `/json/fxdata` entry follows the native semicolon-separated format:
`<parameter labels>;<color slots>;<palette flag>;<capability flags>;<defaults>`.

- Parse the actual effect labels and supported parameters/options; preserve native IDs, defaults and parameter ranges.
- Parse the color-slot section and display the exact declared slots in native order, including foreground/Fx, background/Bg, accent/Cs or custom names. A dual-color effect must retain both colors independently. Only show slots the effect actually declares.
- Use native palette metadata and IDs, not approximated palette names or remapped values.
- Use native capability flags to distinguish 1D effects, matrix-only effects and audio-dependent effects. A catalog name or ID alone does not prove the effect is compiled or supported.
- Return metadata and palette capabilities through an authenticated, bounded BLE readback mechanism. Support ATT MTU negotiation and offset/long reads. Preserve compatibility/versioning and fail closed for truncated or unknown metadata.
- Keep per-zone effect ID, parameters, options, all declared colors, palette, reverse/mirror and other supported segment state independent through save/load, disconnect/reconnect and reboot.

**Important distinction:** WLED effect metadata describes controls and effect capabilities. It is not the complete LED hardware profile. Actual bus/device configuration and OpenTail Output Manager must agree on protocol, driver, channel count, color order, GPIO, pixel count, brightness/current limits and supported corrections.

## LED profiles and color handling

- Current bench hardware: WS2815 12V RGB strips. Verify the configured GRB/BRG order with known RGB frames.
- Use WLED's native color conversion, channel ordering, brightness, gamma/white correction and palette functions exactly once.
- Model LED output profiles from the actual configured WLED bus. Fail closed when a saved profile disagrees with the bus.
- RGBW, RGB+CCT/dual-white and white-only outputs may be exposed only when the physical LEDs and selected WLED driver actually support those channels. Do not simulate white/CCT channels on RGB-only WS2815 by silently remapping values.
- Keep the OpenTail bench brightness cap separate from WLED bus brightness/current limiting; neither UI values nor CI prove safe electrical current.
- Define overlay color inputs as logical colors and route them through the same native WLED conversion path; avoid applying a second conflicting color transform.

## Firmware integration route

1. Build the pinned upstream WLED PlatformIO/Arduino firmware for ESP32-S3 N16R8 (16MB flash, 8MB PSRAM), including this usermod and the configured physical output buses.
2. Implement authenticated OpenTail BLE/profile transport in the WLED firmware and map it to native segment state. The current JavaScript JSON bridge is a reference mapping adapter, **not** the BLE firmware bridge.
3. Reconcile NVS namespaces and schema migration, partition layout, pairing/authorization, rollback, and status LED ownership before any flash. Preserve the currently working CORE image as a rollback artifact; do not erase user settings.
4. Expose native WLED compiled effect IDs, metadata and palette capabilities to Studio/Android. Keep one metadata-driven Effects editor.
5. Apply zone-local vehicle overlays after native rendering without resetting effect phase. Keep the locked priority rules and ensure Stop/global off cannot be overridden.
6. Add golden-frame parity tests against the same pinned WLED build for each supported effect/profile. Fix effect ID, parameters, palette, all colors, brightness, pixel count, LED profile/order and deterministic timing; compare every output channel. Use seeded random state where possible and explicitly classify inherently nondeterministic cases.
7. Bench validate BLE, dual colors, palette round trips, both outputs, zones, trigger overrides/resume, black-frame Stop, RGB color order, level shifting, fused power and measured FPS.

## Existing prototype

- `opentail-wled-bridge.mjs` maps OpenTail 1D zones into WLED `/json/state` segment data and preserves effect IDs, palette IDs, speed, intensity, colors, brightness, reverse and mirror. Host tests: `node --test wled-native/tests/native-bridge.test.mjs`.
- `usermods/opentail_native/opentail_native.cpp` is a minimal WLED v2 usermod with an overlay hook. It is disabled by default and **does not yet implement authenticated OpenTail BLE/profile transport**.
- `build-native.sh` builds the pinned upstream source in a separate directory; it never flashes or erases a controller.
- Existing ESP-IDF CORE, saved Output Manager data and GPIO assignments remain the rollback baseline until migration and acceptance are proven.
- WLED distribution must comply with EUPL-1.2-or-later licensing and attribution obligations.

## Release gates

- [x] Pinned-source architecture and host-tested JSON mapping adapter.
- [ ] Native WLED ESP32-S3 build containing the OpenTail integration.
- [ ] Authenticated BLE bridge, metadata/capability long reads and Output Manager profile mapping.
- [ ] One metadata-driven Studio/Android catalog with exact per-effect color slots, defaults, parameters and palettes.
- [ ] Dual-color and output-profile round trips across independent zones and saved scenes.
- [ ] Golden-frame parity for every claimed effect/profile.
- [ ] Physical WS2815 test, both outputs, trigger overlay/phase restoration, Stop/off, brightness and measured FPS.
- [ ] Licensing and rollback/migration review.

Do not describe the current ESP-IDF firmware or this usermod stub as a completed 1:1 WLED port. Do not flash this candidate until target build, migration, BLE and bench gates pass. Current prototype is not independently fail-safe and must not be the sole road-lighting controller.
