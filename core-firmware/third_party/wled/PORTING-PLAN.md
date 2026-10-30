# OpenTail + WLED: canonical renderer integration plan

**Status: architecture contract; integration remains incomplete.** The current ESP-IDF CORE compatibility renderer is not pixel-identical to WLED. This plan makes upstream WLED the single canonical effect renderer and uses OpenTail as its automotive control/overlay layer.

**Pinned upstream:** https://github.com/wled/WLED/tree/20e7a717664295eefc6ad77972cee1888d2da072  
**Core sources:** `wled00/FX.cpp`, `FX.h`, `FX_fcn.cpp`, `FXparticleSystem.cpp`, `colors.cpp`, WLED Segment runtime, LED bus/output drivers, and the matching palette/transition dependencies.  
**License:** EUPL-1.2-or-later. Keep the pin fixed until a reviewed upgrade, preserve required notices, and complete redistribution review.

## Non-negotiable renderer decision

- **One renderer only:** the upstream WLED runtime owns effect IDs, effect state, frame timing, random state, palettes, metadata defaults, color blending, transitions, particles, channel conversion, and pixel frame generation. Do not run WLED and the compatibility renderer in parallel and blend their decorative frames.
- **OpenTail control plane:** Studio/Android owns the approved user experience, project editing, owner authorization, saved OpenTail vehicle profiles, and assignment of effects to physical outputs/zones. It sends validated segment/effect configuration to the native WLED runtime; it does not reimplement WLED algorithms.
- **OpenTail safety overlay:** vehicle trigger arbitration runs after the native WLED frame is rendered. Apply brake/turn/hazards/reverse only to configured zone spans using the approved priorities; when a trigger releases, expose the already-running WLED effect state without restarting its phase. The overlay must not defeat global Stop/off or brightness caps.
- **Retain custom OpenTail effects:** original OpenTail effects (including Embers and approved startup/shutdown effects) can be registered as native WLED user effects that follow WLED's effect API and metadata conventions. They appear in the same unified Effects catalog, not a second renderer picker.
- **No false parity:** the current ESP-IDF `wled_1d.c` compatibility recipes and six-effect `wled_fx_compat.c` module are migration scaffolding only. Do not claim native parity from matching names/IDs.

## WLED effect metadata is the UI contract

The deployed pinned WLED runtime is the authority for `/json/eff`, `/json/fxdata`, `/json/pal`, and `/json/info`. Studio/Android must consume the actual compiled firmware's capability and metadata data rather than hardcoding controls for every effect.

Parse each `/json/fxdata` entry using WLED's documented semicolon-delimited metadata format:
`<parameter labels>;<color slots>;<palette flag>;<capability flags>;<defaults>`.

- **Parameters:** map WLED's supported speed, intensity, custom sliders `c1..c5` where available in the pinned version, and option checkboxes `o1..o3` to the exact labels/defaults from metadata. Verify actual parameter fields against the pinned source/API; never assume every effect uses the same sliders.
- **Color slots:** preserve the exact native order and labels for foreground/Fx, background/Bg, accent/Cs, or custom names. Show only the slots declared for the selected effect. Keep the full color values per segment/zone; don't flatten two-color effects to one RGB value. Effects whose metadata declares no color slots must not show fake controls.
- **Palettes:** show palette selection only when metadata says the effect uses a palette, and use native palette IDs and values from the deployed firmware. Preserve palette IDs in saved scenes and BLE transport.
- **Capability flags:** use the native metadata flags to classify 1D, 2D/matrix and audio-dependent effects. Expose only effects actually compiled into this firmware and supported by the selected output/runtime. A metadata name or numeric ID is not proof an effect is available.
- **Defaults:** initialize effect controls from the metadata defaults when selected, while retaining explicit saved per-zone values according to the OpenTail persistence contract. Selecting one zone/effect must not overwrite unrelated zones.
- **Dynamic capability source:** BLE capability discovery must return versioned effect IDs plus the associated metadata, palette capabilities, and output-profile capabilities (or provide a safe, authenticated, bounded readback path). Android must negotiate MTU and support long/offset reads; browser preview must never pretend it can directly control BLE hardware.

WLED's effect metadata describes effect UI controls and dimensional/audio capability. It does **not** by itself define every LED bus hardware profile. LED protocol, channel count, RGB order, GPIO, bus driver, CCT/RGBW capabilities, and physical output limits must come from WLED's actual configured bus/device information and the OpenTail Output Manager profile, with cross-validation.

## Dual-color, palettes and LED output profiles

- Use WLED's native segment color slots and conversion code end-to-end: foreground, background, accent/custom slots, palette selection, brightness, color blending, and effect-specific defaults.
- Keep a separate effect configuration for every OpenTail output/zone: native effect ID, parameters/options, all declared color slots, palette ID, reverse/mirror and other supported segment settings, and native phase/state where persistence supports it.
- Maintain an explicit per-output LED profile that maps to a real configured WLED bus: protocol/driver, RGB channel order, channel format, pixel count, GPIO and supported correction capabilities. Validate it against the actual bus before applying saved settings.
- Current bench strips are WS2815 RGB. Verify GRB and BRG profiles against known RGB test frames. RGBW and CCT must only be enabled when both physical LEDs and the configured WLED bus support those channels; do not fabricate white channels by remapping RGB.
- Keep OpenTail's independent low-brightness bench cap as a separate guardrail from WLED bus brightness/current limiting. Never claim a current limit based solely on the UI cap.
- Use WLED's native channel conversion, gamma/white correction, color order and brightness paths for rendered frames. Do not apply a second incompatible color transform in the OpenTail overlay; define whether overlay colors are logical RGB or bus-channel values and convert exactly once.

## Integration route

1. Build the pinned upstream WLED Arduino/PlatformIO ESP32-S3 image for the hiBCTR ESP32-S3 N16R8 target (16MB flash, 8MB PSRAM), with an OpenTail usermod and output-bus configuration. Do not try to import WLED effect functions into ESP-IDF and assume the runtime dependencies are interchangeable.
2. Register OpenTail custom effects in WLED's real registry with native metadata. Use the actual compiled effect and palette tables as the single catalog source.
3. Implement an authenticated, bounded OpenTail BLE adapter that translates owner-authorized Studio commands and saved Output Manager profiles into WLED segment state. Reconcile BLE ownership, pairing, NVS namespaces, partition layout, migration, rollback and firmware update behavior before any flash. Do not rely on unsecured HTTP vehicle-trigger writes.
4. Map each physical chain to a stable WLED bus and each OpenTail zone to a validated segment/span. Keep effect segments independent. Implement trigger overlays in a documented post-render hook and preserve effect phase when triggers release.
5. Keep one unified Effects editor in web Studio and Android. Metadata drives effect-specific labels, color slots, palette controls and defaults. Native WLED remains the canonical renderer; the browser's offline preview must be explicitly marked simulated unless it runs the same source-derived renderer.
6. Add golden-frame comparison against the pinned upstream build for every supported effect/profile. Compare all output channels for fixed inputs, exact effect ID, parameters, colors, palette, brightness, LED profile/order, pixel count and deterministic time. Seed or isolate random state where possible; document nondeterministic effects that need statistical rather than byte-exact comparison.
7. Validate native target build, BLE authorization/metadata long reads, zone isolation, scene persistence, status LED behavior, physical triggers, Stop black-frame, low-brightness WS2815 output on both chains, signal-level shifting, power/current and measured frame rate.

## Acceptance gates

- [ ] Pinned native WLED ESP32-S3 image builds with the OpenTail usermod.
- [ ] Authenticated BLE bridge returns exact compiled WLED IDs and metadata; no hardcoded fake effect availability.
- [ ] Studio and Android use the same metadata-driven Effects editor and one unified catalog.
- [ ] Dual-color and accent slots match native WLED semantics for every effect that declares them.
- [ ] Palette IDs, defaults, effect options and independent zone settings round-trip through BLE and saved scenes.
- [ ] WS2815 RGB GRB/BRG test frames match the reference; unsupported RGBW/CCT profiles fail closed.
- [ ] Trigger overlay preserves native effect phase and zone-local arbitration; Stop renders black and remains off.
- [ ] Pixel/channel golden-frame tests pass for every claimed effect/profile; measured hardware FPS and thermals are recorded.
- [ ] WLED EUPL-1.2-or-later distribution obligations reviewed.

Until these gates pass, the native WLED engine is the target architecture, not a completed or physically validated release. Do not flash over the working rollback image or install this alpha as sole road-lighting hardware; independent hardware failover and conventional-lamp fallback remain unverified.
