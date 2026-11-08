# Effects team — Firmware #115 final compatibility and sign-off

**Certification target (immutable product source):** Firmware watchdog PR #115 HEAD
`f97bceec7445a958f735df17ca0842b97bf8e062`

**Compared renderer baseline:** reconciled Effects PR #110 HEAD
`6180e2a5ccbfbf8567513504669c6840b68c045b`

**Integrated Android/Firmware lineage:** PR #111 HEAD
`1a9cfc4b25ef57bda38a7861ac5548c3747d9605`

## Scope and change decision

**NO PRODUCTION CODE CHANGE REQUIRED.** The complete authoritative Effects engine at #115 is byte-identical to #110 across the 12 renderer/metadata source and header files listed below. Android's pure-C JNI renderer is also byte-identical to CORE. Firmware #115 did not alter the Effects renderer, palette engine, proprietary animation source, or 1D dispatcher. No renderer rewrite or corrective product patch is justified by the reproduced evidence.

This sign-off branch is based directly on the exact #115 commit. Its only additions are a host-only integration stress test, a source-order guard, this report, and a dedicated compatibility workflow. No CORE runtime, Android UI/JNI, BLE protocol, NVS, trigger arbitration, firmware version or partition source is changed.

## Immutable source identity

The following files have the same Git blob SHA at both #110 and #115:

| Source | Git blob SHA |
|---|---|
| `wled_fx_compat.c` | `2bd896bc5fd886a156285ac867fc2b8750f4117e` |
| `wled_fx_compat.h` | `32f575faee4f41b3cda5398a1c93d8ede84a25e5` |
| `ot_auto_effects.c` | `1287d00121470b09f0a2f5becae47117038604b5` |
| `ot_auto_effects.h` | `1f0aa8dd9d64dc558e39c692f270676c6c86d6bd` |
| `effects_1d.c` | `7f682dd5f7c2a20179779f2f172cd1f5887c79cf` |
| `effects_1d.h` | `75288a12563792996be882ffea351ec8ae3c576c` |
| `wled_1d.c` | `f736ce2f5b92af2c19879f7ba71f3e9383502ac5` |
| `wled_1d.h` | `836c0f1293c49d5b25ac1a588c918f12da6a858c` |
| `palette_1d.c` | `a147f424a4a59f96b4e217240fdb500bd2a8f036` |
| `palette_1d.h` | `60e5bebdb8a2e300763be51adb5ab1f8eea13a33` |
| `particle_system_1d.c` | `f1502b75651b723242eeeab5e44411ee94a5ceb9` |
| `particle_system_1d.h` | `d92936de24670a9bf744b256afcc87b1dd12828b` |

Android `SOURCE.json` remains pinned to Effects #110 and validates its 13 native source SHA256 values. The #115/PR #112 capability contract advertises **13 source-proven and transport-admitted third-color effects**; the six unsupported source roles remain IDs 10, 11, 40, 48, 60 and 91. The new JNI parity job explicitly compares all 13 consumers against the authoritative CORE renderer.

## Firmware #115 watchdog/RMT behavior reviewed

Source inspection at exact #115 confirms this production order:

1. Configure both independent output lanes and render their buffers.
2. Submit both asynchronous RMT transfers before waiting for either lane.
3. Wait for each submitted lane's `led_strip_refresh_async_done()`.
4. Only a successful completion sets `any_output_completed=true`; a failed completion clears that lane's frame-submitted flag and receives no completion credit.
5. Call `ot_sw_idf_render_cycle(full_frame, any_output_completed, ...)` after the completion loop.
6. The render TWDT user is reset only if `ot_sw_render_progress_eligible()` reports at least one successfully completed lane. No queued-but-incomplete transfer, failed lane, or render-loop iteration can forge progress.
7. A surviving output lane may continue to feed render progress during a single-lane fault. The 60-second recovery-stability window and OTA confirmation still require complete dual-lane frames, fresh input samples and no output/configuration faults.

ESP-IDF TWDT is configured for 15,000ms with both CPU idle tasks watched (`idle_core_mask=3`); interrupt and boot watchdogs remain enabled. The output task uses the same watchdog progress path for every effect and for non-Show physical output; watchdog progress is not conditional on effect ID or Show Mode.

At the current default 2,048 pixels/output, nominal 800-kHz RGB wire time is approximately 61.74ms per lane including the 300us latch allowance. The two lanes overlap. This is a protocol-derived upper wire-time estimate, **not a measured ESP32 render/DMA completion time**. The 15-second timeout is over 240 times that nominal wire interval; actual MCU timing and idle-task scheduling still require the powered-board test.

## Evidence matrix

Status definitions: PASS = exact-HEAD automated software evidence; NOT_RUN = requires physical/device measurement; FAIL = an actual software defect. Historical runs are identified explicitly and are not relabeled as new post-#115 tests.

| Requirement | Status | Evidence / boundary |
|---|---|---|
| 140 stable effect IDs (120 WLED + 20 OpenTail originals) | PASS | #115 full CORE host suite; registry and deterministic render tests |
| WLED source authenticity / palettes / custom gradients / black | PASS | #115 source parity gate: 391,750,848 RGB samples across 120 modes; palette reference and custom stop tests; documented #99 policy exceptions |
| Speed/Intensity extremes | PASS | #115 Issue #93 2,100 profiles; 140-mode deterministic frame scan |
| High-speed Blink/Strobe | PASS | #115 Issue #99 30/60/120fps toggle traces; crisp Strobe edge tests at Speed 255 |
| BPM / Breathe / Fade / Fireworks | PASS (host source-frame behavior) | #115 Issue #99 BPM wrap, Breathe/Fade bounded-delta and deterministic Fireworks cadence tests; perceptual smoothness remains device-only |
| Fusion Core / Event Horizon / TV Simulator / Chase black gaps | PASS | #115 Issue #99 and 12 proprietary continuous-loop golden tests; TV scene progression and true-black boundaries |
| Proprietary continuous and one-shot transitions | PASS | #115 twelve continuous RGB goldens, seven fixed one-shot endpoints, five Startup/Shutdown native one-shot frames |
| 13 third-color consumers / six unsupported roles | PASS | #115 Issue #102 host role tests, 140 catalog and BLE capability manifest; no invented sixth-role support |
| Brake / Turn / Hazards / Reverse / STOP / zone isolation | PASS (software) | #115 Effects/OEM matrix: 193,536 safety overrides, 165,888 decorative frames, all 256 input masks, six wire orders; Trigger and NVS host regressions |
| O1/O2 submit/complete ordering and watchdog feed eligibility | PASS (source + host model) | #115 watchdog structural guard and frame-completion contracts; this branch adds an explicit Effects/WDT source-order guard and two-lane 140-effect host simulation |
| No false TWDT reset during normal heavy Effects playback | PASS (host/source model); physical NOT_RUN | 140-effect max Speed/Intensity/two-lane stress plus 15s progress-policy model; real ESP-IDF TWDT/DMA timing is not simulated by host |
| Android JNI source identity and BLE/source contract | PASS | #115 `android-unchanged-contract` and #115 active-source Android tests; this branch adds byte-exact 140 JNI-vs-CORE parity against #110 |
| Android JUnit / debug APK compile | PASS | #115 exact-HEAD active-source Android job; rerun in this branch's dedicated workflow |
| ESP-IDF v5.5.5 ESP32-S3 build | PASS | #115 dedicated watchdog workflow exact-head build; rerun in this branch's dedicated workflow |
| Four-image ZIP, OTA data at 0xF000, SHA/CRC, downloaded artifact recheck | PASS | #115 dedicated watchdog workflow build/package and independent downloaded-artifact verification |
| Independent Trigger host guard | PASS | #115 exact-head independent Trigger workflow/job |
| Powered-board RMT async completion / actual TWDT no-false-reset timing | NOT_RUN | Requires owner-controlled powered ESP32-S3 bench |
| Physical LED optics, independent O1/O2 output timing and 22 mandatory hardware checks | NOT_RUN | Requires the actual installed LED harness and test procedure |
| Owner phone display FPS, perceptual Breathe/Fade/Fireworks smoothness, BLE latency | NOT_RUN | Requires the owner Android device and live controller |
| Road/vehicle release or independent essential-light fallback | NOT CERTIFIED | No hardware failover or independent electrical OEM fallback is proven by this software work |

### Existing #115 exact-HEAD receipts

- [Issue #113 dedicated watchdog run 42455015190](https://github.com/Srryastari/OPENTAIL-development/actions/runs/42455015190): all seven jobs PASS — exact source boundary, full 140 Effects host suite, Android unchanged contract, independent Trigger guard, compile-time fault-injection build, ESP-IDF v5.5.5/four-image package, and downloaded ZIP integrity.
- [Independent Trigger integration run 42455015167](https://github.com/Srryastari/OPENTAIL-development/actions/runs/42455015167): PASS on exact #115 SHA.
- [Active-sources run 42455015157](https://github.com/Srryastari/OPENTAIL-development/actions/runs/42455015157): all software jobs (CORE host, Android JUnit/APK, ESP-IDF build and installer check) PASS. Its final release-evidence gate FAIL is expected and remains in place because physical acceptance and full release provenance are not complete. It is not counted as an Effects test failure or a release pass.

## New exact-head revalidation

Workflow `.github/workflows/verify-effects-firmware115-compatibility.yml` runs on this documentation/test-only successor, while asserting exact #115 ancestry and byte identity with #110. It reruns:

- Full CORE host regressions and the new 140-effect/two-lane watchdog-progress host stress.
- RMT async completion ordering, render WDT eligibility, dual-lane stable-window and 15-second timing source guard.
- 140/140 real Android JNI frames against the independent exact #110 CORE oracle, including all 13 third-color consumers.
- Android source/JUnit/debug APK build, independent Trigger regressions, ESP-IDF v5.5.5 build and four-image ZIP verification.

**Final team sign-off is conditional on that new exact-head workflow and independent review completing successfully.** No physical, phone, optical or vehicle-release result will be inferred from these jobs.
