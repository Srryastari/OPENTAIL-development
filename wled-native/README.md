# OpenTail unified rendering integration

**Status: integration in progress.** This specification makes OpenTail the owner of the complete rendering lifecycle while combining the pinned WLED renderer and OpenTail-native effects in one output pipeline.

## Ownership model: one controller, one render pipeline

OpenTail owns the unified renderer coordinator: effect selection and scheduling, per-zone state, saved profiles/scenes, color/parameter state, frame timing, trigger arbitration, output-profile validation, brightness/current limits, and final output authorization.

The coordinator combines two rendering providers:
1. **Upstream WLED provider** — execute the exact effect implementations from pinned WLED source `20e7a717664295eefc6ad77972cee1888d2da072`, preserving native effect state, palettes, transitions, random state, particle simulation, and native color handling.
2. **OpenTail-native provider** — run original effects (including Embers and materialize/startup/shutdown effects) under the same coordinator and zone-state contract. They must not be a second independent output task or compete with WLED's LED bus.

Both providers publish their effect metadata to **one catalog** and render only the zones assigned by OpenTail. The final arbitration/output stage applies vehicle triggers to assigned zones and commits frames through the configured output driver. There is one owner of physical pixel writes, not two renderers racing over the LEDs.

For WLED effects, use the upstream WLED runtime as the provider to preserve pixel parity. For original OpenTail effects, preserve their algorithm but adapt them to the shared frame/output interface. A fail-closed build patch now inserts `opentailNativePostRender(_pixels, totalLen)` at the pinned WLED compositor seam in `wled00/FX_fcn.cpp`, after WLED segment blending and before gamma/color conversion and bus mapping. This is a real integration hook, but it currently enables the trigger-overlay seam only: the native-effect provider, authenticated BLE bridge and profile-driven zone setup are still not connected. The build script rejects unexpected source drift rather than patching a different function.

## OpenTail automotive control plane

OpenTail remains authoritative for:
- authenticated BLE control and session authorization;
- vehicle trigger inputs, priority arbitration and trigger-specific zones;
- Output Manager profiles: physical outputs, zones, counts, pixel ranges, protocol, color order and supported channel types;
- saved effects/scenes, parameters, options, every declared color slot, palette and transition state;
- trigger overlays after the underlying effect has rendered, preserving the effect phase so it resumes unchanged;
- Stop/global-off, fault state, brightness cap, output validation and fail-closed behavior.

Locked priority remains: Reverse overrides show on reverse-assigned zones; hazards drive both turn zones; Brake overrides Park; show effects can continue on unassigned zones; releasing triggers restores the underlying effect without resetting its phase. Stop/global off must never be overridden by an overlay.

The existing CORE firmware remains the recovery reference until the WLED-native image has a verified BLE/profile migration path. Do not attempt to run two independent LED-bus drivers against the same physical pixels.

## Metadata-driven unified Effects catalog

Read metadata from the exact compiled WLED image: `/json/eff`, `/json/fxdata`, `/json/pal`, and `/json/info`. Metadata is runtime data, not a hand-maintained imitation.

- Preserve native effect IDs and compiled 1D/audio capability flags; exclude unsupported matrix-only effects from 1D output.
- Parse each effect's declared parameter labels, options, defaults, palette support and color slots. Dual-color effects retain both colors independently; never assume all effects use the same number or meaning of colors.
- Preserve OpenTail-native effect metadata in the same catalog and namespace without collisions with native WLED IDs.
- Keep per-zone effect state independent across save/load, BLE disconnect/reconnect and reboot.
- The bridge parser in `opentail-wled-bridge.mjs` is a host/reference adapter. It is not the firmware BLE bridge and is not by itself a complete effect UI.
- Never silently drop a metadata field that the active transport cannot encode. Extend the versioned BLE payload or reject that operation explicitly.

## LED profile and color pipeline

Current physical bench strips are WS2815 12 V RGB. Validate the selected bus, GPIO, count and GRB/BRG order using known-color frames. WLED must perform its native bus conversion once; OpenTail overlays must use logical colors and pass through the same final color/bus conversion exactly once.

RGBW, RGB+CCT/dual-white and white-only channel controls are permitted only when both the physical LEDs and selected output driver support those channels. Do not fake unsupported channels on RGB-only WS2815. Keep the bench brightness cap distinct from WLED brightness/current limiting. Reject profile/bus mismatches before output.

## Automatic verification

`.github/workflows/wled-native-integration.yml` now schedules checks on relevant PRs and pushes:
- Node host tests for metadata parsing and native WLED state mapping;
- strict C++17 host tests for trigger overlays;
- a no-flash build of the pinned WLED source with the OpenTail usermod and actual compositor hook;
- a source-patch self-test for idempotence and fail-closed behavior;
- an unflashed firmware artifact upload if the build succeeds.

CI success proves only the software checks/build. It cannot prove exact pixel parity, safe current, status LED timing, BLE behavior, or physical trigger arbitration. Those require golden-frame comparisons and the bench acceptance tests below.

## Required integration stages

1. [partial] Patch the pinned WLED compositor to call OpenTail after segment blending and before final color/bus conversion. Next, wire the coordinator/native provider into that frame stage and register OpenTail-native effects; keep the provider disabled until the bridge/profile path is complete.
2. Integrate authenticated OpenTail BLE transport and profile/zone/scene state into that same firmware.
3. Connect the existing trigger manager/arbitration to post-render zone overlays and phase-preserving restoration.
4. Generate the unified catalog from compiled WLED metadata plus registered OpenTail-native metadata; wire all supported color slots and parameters into Studio and Android.
5. Reconcile partition/NVS schema, authorization and rollback before any flash. Keep a recoverable CORE image and preserve user settings.
6. Add deterministic golden-frame tests against the same pinned WLED source for every claimed WLED effect/profile; classify nondeterministic effects explicitly and compare every output channel.
7. Bench validate status LED patterns, BLE readback/authorization, dual colors, palettes, saved scenes, both outputs, IN1–IN8 arbitration, trigger phase resume, Stop/global off, color order, current limits and measured FPS.

## Current verified scope and release gates

- [x] Upstream WLED commit pinned.
- [x] OpenTail-to-WLED zone/state mapping adapter exists.
- [x] Metadata parser and host tests cover declared color slots, dual colors, defaults, palette/capability flags and unsupported controls.
- [x] Automatic CI workflow added for host checks and the native WLED build.
- [x] Build script applies a narrow, idempotent compositor patch and fails closed if the pinned source anchor changes.
- [ ] Verify the new compositor-hook commit passes CI and the patched native image compiles.
- [ ] Confirm CI runs green on this exact branch head.
- [ ] OpenTail-owned render coordinator wired into the actual WLED render loop and shared physical output stage.
- [ ] OpenTail-native effects registered into the shared catalog/coordinator.
- [ ] Authenticated BLE bridge and Output Manager/profile/scene integration in the WLED firmware.
- [ ] Full parameter/color/options/palette round-trip in Studio and Android.
- [ ] Golden-frame pixel parity for every effect/profile claimed as supported.
- [ ] Physical bench validation and migration/rollback review.

Do not describe the combined runtime as complete until these gates pass. Do not flash the experimental image until the native build, BLE, settings migration and bench gates pass. This alpha is not independently verified as a safety-rated or fail-safe road-lighting controller.
