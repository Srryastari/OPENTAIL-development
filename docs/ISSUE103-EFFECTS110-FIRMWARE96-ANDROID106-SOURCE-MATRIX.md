# Issue #103 — Reviewable Effects #110 / Firmware #96 / Android #106 integration successor

## Ancestry and scope

- **Anchor / exact existing Android #106 HEAD:** `db7bcdff187197c0493b5c38991870224f2c244a`. This preserves the six user-approved Native Compose screens, monotonic vsync preview/O1/O2 treatment, JNI and BLE client source.
- **Authoritative reconciled Effects #110 HEAD:** `6180e2a5ccbfbf8567513504669c6840b68c045b` — reconciled exact #107 `4ac067323f9f18fce1939506b2154aeea1e712bd` plus all six #108 corrections. The CORE source and Android JNI embedded C copies are pinned byte-identically.
- **Firmware #96 final package-corrected HEAD:** `df27209b9876630b2b10f3a189f36817a2764009`. Android #106 already descends from Firmware #96 original runtime; this successor imports **only** the new flash packaging script/tests, documentation and package-verification workflow from #96's later head.
- **Trigger #98:** independent existing safety PR remains unmerged; its exact-head safety expectations must be revalidated against this new integrated SHA via independent Trigger CI. Do not silently merge PR #98.

## Exact source hash/transport evidence

| Source | Git blob SHA | JNI SHA256 (where embedded) |
|---|---|---|
| CORE and JNI `wled_fx_compat.c` | `2bd896bc5fd886a156285ac867fc2b8750f4117e` | `b0a0f2e6f89aac5e6c3cdb94e0332ca6016fe70e4ee2abb334608cd631d3b009` |
| CORE and JNI `wled_fx_compat.h` | `32f575faee4f41b3cda5398a1c93d8ede84a25e5` | `cd1f8176be2ad867dceffabc3425e86404a884241b1517cf59c4a0835cc614f0` |
| CORE and JNI `ot_auto_effects.c` | `1287d00121470b09f0a2f5becae47117038604b5` | `16fe4997a9d4be91375b3ac4e4339b3f6f99be8a734831c9624cdb1e42663f05` |
| `effects_1d.c` (unchanged from 106) | `7f682dd5f7c2a20179779f2f172cd1f5887c79cf` | Verified at runtime in `android/qa/test_native_source.cjs` |
| `effects_1d.h` (unchanged from 106) | `75288a12563792996be882ffea351ec8ae3c576c` | Verified at runtime in `android/qa/test_native_source.cjs` |
| Firmware #96 flash-packager | `49cd0f255408d5cc2c5cfa2995c6c00eff66f0c3` | source-only (not JNI) |
| Firmware #96 package regression | `48be3922655aebef87d3001bbc3197991fd8f931` | source-only (not JNI) |
| Firmware #96 package CI | `9c6771d9a908ff2fe90d027426df7c9f5d11eb17` | source-only (not JNI) |

**Actual capability contract:** 140 stable effects; 13 *renderer*-source-proven third-color consumers, six unsupported third roles. Existing authenticated Firmware #96 opcode11 whitelist still only 11 approved IDs; Android's deployed BLE admission must not claim 13 until Firmware/Trigger approve and add those two IDs separately. Aurora #38 requires source-authentic Colors1/2/3 palette; Popcorn #95 honors source RNG indices. No new BLE/NVS wire contracts or vehicle priority changes in this branch.

## Exact-HEAD gates

- The new `.github/workflows/verify-issue103-exact-head.yml` checks source git blobs, pure JNI SHA256, all 140 CORE host/WLED/proprietary goldens, exact JNI-vs-CORE independent oracle including 13 third slots and live parameter traces, ESP-IDF v5.5.5 ESP32-S3 build, no-fabricated-OTA four-image ZIP creation, verification *and downloaded ZIP re-verification*.
- Updated Android exact-head workflow pins Effects #110 rather than mixing Effects50/80/95 snapshots. Legacy original CORE #49 and Trigger #48 read-only compatibility remain unchanged for historical ABI checks.
- Independent Trigger workflow must PASS on the actual integrated HEAD. Additional core safety tests (OEM override matrices, gamma, all input masks, STOP, journal staged/active, and outage boundaries) remain active. No auto-merge.
- The older immutable CORE integration provenance job expects a different SHA and can intentionally report FAIL; do not weaken that gate or treat the successor as release approved. It requires explicit independent owner review.

## Explicitly NOT CERTIFIED

Device radio latency, 30/60/120 FPS on owner phone, physical O1/O2 optics and output stutter, backup hardware takeover, 22 mandatory physical acceptance checks, real OTA flashing, road-use vehicle release, and remaining Breathe/Fade/Fireworks1D perceptual reports. All remain pending.

**Simplicity First · No Laziness · Minimal Impact.** No Compose design changes, no unrelated firmware protocol rewrite, no firmware version bump, no merge to main/master.
