# OpenTail unified effects software candidate — #26–#35

**Integration scope:** only WLED-derived OpenTail 1D effects and pinned-source RGB engines, overlays (spark/flare/particles), fixed/dynamic palette sampling, the single global random palette producer, and opt-in source gamma calibration helpers. No WLED firmware image is used. Pinned source: `wled/WLED@20e7a717664295eefc6ad77972cee1888d2da072`.

**Parent:** effects/source-port-rolling-balls-48 at `e1278ae073e7057fc62c5f6cb71410310c374096`. Candidate is the exact cumulative source tree at effects/source-harmonic-global-rng-20261029 `3ae5c88e8d99801ac25feef5cd8eb5b070c1909f`, plus this scope manifest and a separate exact-HEAD CI verification workflow. One consolidated review compares this candidate directly to the parent of #26 and absorbs the branches from #26, #27, #28, #30, #31, #34 and #35 without separate merges.

## Acceptance
1. `effects-exact-head-host` checks out `github.event.pull_request.head.sha` explicitly, rejects synthetic-merge or wrong revisions, then runs **all** `core-firmware/tests/run-host-tests.sh` tests at that exact commit: all 120 direct upstream RGB cases / 391,750,848 exact RGB comparisons, fixed and custom palette references, 240 seeded harmonic random palette traces/all five harmony families, eight and 16 bit math/timing/cadence, flare/spark compositing, two-output independent frame snapshots, universal zones, Stop, trigger regression, BLE protocol ABI and fail-closed behavior.
2. `effects-exact-head-espidf` checks out **the same exact commit SHA**, asserts it, builds ESP32-S3 with ESP-IDF v5.5.5 and checks bootloader, partition, CORE app and flash manifest files (without flashing).
3. The existing change-impact, Studio/Android compatibility, standard host, and ESP-IDF jobs continue to run independently. Existing Windows packaging and release-evidence gates **must not be bypassed** for draft PR or absent physical evidence.
4. Review the aggregate diff against the parent before marking software candidate complete. Source effect code and tests are allowed; **no changes** to vehicle trigger mapping, Android UI, BLE palette transport/NVS persistence, hardware pins, external WLED runtime or backend protocols.

## Limitations/next owner
- **#36 firmware-owned custom palette BLE transfer/NVS is explicitly NOT included.** It is stacked on #35 and handled by firmware team, not in this effects candidate.
- Live custom palette 73–255 access needs an upstream validated caller-owned palette snapshot; unprovided snapshots fail closed.
- Source gamma helper stays opt-in. Enabling hardware gamma, driver pixel timing, electrical behavior, device heap headroom, full-loaded FPS and actual optical parity require owner review and physical measurements. Gamma helper test is source-LUT reference only.
- Source RNG parity is exercised with deterministic test bytes; on-device `esp_random` and real-world entropy are not deterministic golden sources.
- Host 1:1 RGB evidence pertains to the enumerated source test profiles and is **not** a system-wide 1:1 optical/timing certificate. Road use is prohibited by CORE bench-only constraints.
- No merge or flash requested for this **candidate**. Draft PR status should remain until the software tests and device/owner gates are reviewed.
