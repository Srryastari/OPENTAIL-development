# OpenTail repository regression audit — 2026-10-28

**Scope:** draft PR #19, `integration/core-v0.1.25-unified-effects-status`; CORE ESP-IDF, native WLED, Studio, Android, NVS, CI, trigger/status/effect contracts. **This is a source and CI audit, not a new physical bench test.** All findings below are distinguished from historical physical observations in `docs/verified-bench-regression-matrix.md`.

## Confirmed defects found and changes committed

| Severity | Evidence | Fix | Verification state |
| --- | --- | --- | --- |
| High / blocking CI | [Verify OpenTail active sources run 40791785923](https://github.com/Srryastari/OPENTAIL-development/actions/runs/40791785923), `core-host-tests`: `test_ble_config.c:11` asserted BRG, but `ot_om_defaults()` now correctly initializes an unconfigured output to GRB. The job aborted (exit 134). | `eda8fad`: correct stale test expectation, preserve BRG as configurable option. | Previous CI failure confirmed; replacement candidate CI pending. |
| High / approved behavior regression | `core-firmware/main/status_led.c` selected purple BLE-only and cyan dual-transport modes, contrary to locked green double-pulse Connected pattern. `test_status_logic.c` explicitly expected those divergent colors. | `bcc5e84`, `82729a7`, `c861838`, `d960a60`: unify connected mode/color for Wi-Fi, BLE, and both; add host assertions. | Source fix committed; physical LED color/timing still unverified. |
| Medium / version identity | `core-firmware/main/ble_status.c` advertised `OpenTail CORE 0.1.25` while REST and build identify `0.1.27-alpha`. | `ot_version.h` shared by REST and BLE; `5f97fd3`, `960cbf6`; version identity host regression `79e0e61`, `b27d3c5`. | Static/compile validation pending. |
| High / unsafe artifact qualification | `.github/workflows/verify-active-sources.yml` uploaded a flashable CORE ZIP in the ESP-IDF job even if host/UI/Android jobs failed on the same commit (as in run 40791785923). | `2fe989f`: CORE build/artifact job now depends on host, web Studio and Android jobs. | CI gate added; separate WLED-native parity/physical gates still not included. |
| Medium / stale build and QA versions | `BUILD-AND-FLASH-OpenTail-CORE-v0.1.27-alpha-COM10.ps1` and `PREPARE-AND-BUILD.ps1` still used Studio v0.19.62; `android/qa/check_release.py` expected Studio 0.19.62 and Android 0.1.27 / code 28, despite approved Studio 0.19.118 and Android 0.1.29 / code 29. | `ac62e6c`, `5053eb8`, `6d4826c`, `56b19ed`: align filenames, QA expectations and pinned Studio SHA256. | QA script added to Android CI; final run pending. |
| High / build-time embed regression | `core-firmware/tools/sync_studio_asset.py` checked for nonexistent `ot-core-output-advanced`, while approved Studio v0.19.118 uses `ot-core-advanced` and the `CORE hardware output mapping · Advanced` disclosure. Running the newly added embed step would reject the correct approved UI. | `40a06b3`: validate the actual approved disclosure markers instead of the obsolete selector. | Source fix committed; CI check pending. |
| Medium / embedded UI provenance | ESP-IDF embeds `core-firmware/main/assets/studio.htm.gz`; web/Android source parity alone does not prove the image was built with the approved Studio source. CMake has a configure-time regeneration step, but CI did not explicitly assert embed parity. | `6e0ec5b`: regenerate and check the gzip asset before compiling CORE; run full Android QA. | Build-time check pending. |

## Release blockers still open — not fixed by these commits

### P0 — no single production/unified renderer firmware

- `core-firmware/main/opentail_core.c` still calls the internal `ot_effects_1d_render()` compatibility renderer; `core-firmware/main/wled_1d.c` contains deterministic approximations for most catalog entries, not upstream WLED pixel parity. The CORE image and native WLED image remain **separate binaries**.
- `wled-native/usermods/opentail_native/opentail_native.cpp` has a compositor hook and internal configuration ABIs but `loop()` does not connect the CORE BLE/GATT, saved scenes, authenticated profile transport, or GPIO sampler. `wled-native/README.md` confirms `ot_vehicle_input_sampler.h` is not wired to hardware.
- `wled-native/usermods/opentail_native/ot_render_coordinator.h` is a tested coordinator **contract**, not the currently instantiated, sole hardware-output path. Bus count, protocol, pixel profile, color order, zone mapping, NVS migration, and fail-closed physical trigger integration are not connected to the WLED runtime.
- The native effect set contains Embers, Materialize Sparks, Random Materialize, Smooth Fade, and Plasma Sweep, but they are disabled by default and not activated from the authenticated CORE Studio/Android control plane. The remaining requested custom effects and editable parameters are not proven complete.
- No golden-frame, per-effect/per-palette, per-profile comparison against pinned upstream WLED has been demonstrated. Catalog visibility or a successful experimental WLED build is not 1:1 parity.

### P0 — no sole-taillight failover or electrical safety validation

- `core-firmware/main/opentail_core.c` explicitly states no watchdog, independent backup, or OEM fallback; the current firmware is **bench-only**. A second ESP32-S3, synchronized saved profile, independent input sensing, secondary pixel generator, protected takeover, and failure-detection tests are not implemented.
- No verified hardware current limiter, power-cut/deep-sleep wake chain, safe shutdown sequence, brownout recovery, or power-injection validation for road use. The brightness cap 32 is a software channel cap, not a measured current limit.
- Physical input polarity/pin ownership on the final board, GPIO16/17 output independence, NVS restart persistence, Stop/restore, BLE reconnect, and status LED timing must be retested on the candidate image. Historical tests do not prove this new image.

### P1 — user-visible gaps and regression risks

- `docs/OPENTAIL-LOCKED-RELEASE-CONTRACT.md` records missing output-specific test/preview controls in Studio. The Advanced card's disclosure persistence has source-level checks but no verified browser/device interaction.
- Android BLE requests MTU 247 before service discovery, but the source has no bounded timeout fallback if an MTU request succeeds without an `onMtuChanged` callback; intermittent BLE behavior remains a device test risk, not a reproduced defect.
- `core-firmware/main/opentail_core.c` defines `OT_FX_ENTRY_FADE_MS 0`; adjustable entry transitions, complete materialization/shutdown controls, and full custom effect coverage remain incomplete.
- The current physical implementation is 2 outputs, 8 inputs, up to 522 pixels/output, and 8 zones/output. Future 12-pixel/10-switch expansion, fiber-optic grouped/addressable expansion, and modular CAN/audio are not implemented by this PR.
- `android/qa/check_release.py` was not previously executed by active-source CI; the new job may expose additional stale assumptions. Do not interpret its static checks as on-device UI proof.

## CI evidence and acceptance rule

- Prior head `a87c211`: active-source workflow [40791785923](https://github.com/Srryastari/OPENTAIL-development/actions/runs/40791785923) **failed** in CORE host tests (stale GRB/BRG assertion); web Studio, Android APK, and CORE ESP-IDF jobs **passed**. Unified WLED integration and experimental native build workflows passed for that head. These are *software results*, not hardware verification.
- Subsequent audit fixes are on the same integration branch. At time of this report, the new candidate's three workflows were queued/running; **do not claim them green without checking their final conclusions**.
- Keep PR #19 draft; do not flash the separate WLED-native image or label any artifact a fully unified release. Before releasing a new bench ZIP, require all active-source and WLED workflows green on the exact final code commit, plus the locked source checks. Before road use, require the physical failover/electrical/trigger tests in the locked contract.

## Next engineering actions

1. Finish the authenticated CORE ↔ pinned WLED bridge with one physical pixel-write owner and verified local trigger arbitration; then test one output and one zone end-to-end before scaling.
2. Golden-frame test real WLED effect IDs, palettes, foreground/background/accent colors and selected RGB channel order against pinned upstream. Clearly mark unsupported profiles/effects.
3. Run all software gates on the final candidate commit, then perform targeted hardware acceptance of GPIO4–11, GPIO16/17, NVS persistence, BLE authorization/reconnect, Stop, status LED, and measured FPS. Keep redundant hardware failover as a separate mandatory road-use gate.
