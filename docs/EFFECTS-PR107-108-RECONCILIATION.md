# Effects P0 — exact PR #107/#108 reconciliation

Reconciliation source of truth:
- Parent **PR #107 exact latest HEAD** `4ac067323f9f18fce1939506b2154aeea1e712bd`.
- PR #108 head `79cd072000eb85334d460ab566f7b52f0207d51b` was based on older #107 `b2daac4bd194b7e64e69b9469deee977403d3ccd`. GitHub compare confirms **diverged**.
- This branch originates **directly** at latest #107, so no change from #107 can be lost by rebasing an older feature.

## Source identity and reconciliation, checked by immutable Git blob SHA

| Contract | File | Exact Git blob SHA retained |
|---|---|---|
| #107 latest (unchanged) | `core-firmware/tests/test_wled_source_parity.cpp` | `282fbdf6ae062a5058fdff660c740c20f82121ee` |
| #107 latest (unchanged) | `docs/EFFECTS-ISSUE99-FRAME-ACCEPTANCE.md` | `0f7aacca467293a5a2ca1dc369dd05358de6e145` |
| #107 latest (unchanged) | `core-firmware/tests/test_issue99_frame_quality.c` | `43c8477e62a200242e9858348fb8cf5cbeea51be` |
| #107 latest (unchanged) | `core-firmware/main/ot_auto_effects.c` | `1287d00121470b09f0a2f5becae47117038604b5` |
| #107 latest (unchanged) | `core-firmware/tests/test_proprietary_loop_continuity.c` | `c02a13dafc6a0de1b7615ea374190cac035a84f0` |
| #108 migrated verbatim | `core-firmware/main/wled_fx_compat.c` | `2bd896bc5fd886a156285ac867fc2b8750f4117e` |
| #108 migrated verbatim | `core-firmware/main/wled_fx_compat.h` | `32f575faee4f41b3cda5398a1c93d8ede84a25e5` |
| #108 migrated verbatim | `core-firmware/tests/run-host-tests.sh` | `dcf25fca8a9ef3a890174e1a9c50d7fdef2a1c1c` |
| #108 migrated verbatim | `core-firmware/tests/test_issue102_third_roles.c` | `152e684c2ea9add3cd9e6608ac5d131a733715e9` |
| #108 migrated verbatim | `docs/EFFECTS-ISSUE93-140-ACCEPTANCE.csv` | `ca9440f92f822e37e42628fab423a3bf631e7fd5` |
| #108 migrated verbatim | `docs/EFFECTS-ISSUE102-SOURCE-ROLE-VERDICT.md` | `f1a8773d83ea2e1300eb5a89961fa1df67d53a61` |

The overlapping `test_issue99_frame_quality.c` file was byte-identical between both latest heads; it was **not overwritten**. The latest #107 strict parity label and #99 acceptance report are preserved. Git compare of the reconciled tree against latest #107 shows only the **six** intentional #108 files above (three modified code/test files, one added test and two documentation files); no unrelated firmware, BLE, NVS, Arduino, or Android UI changes.

## Behavioral acceptance contract

- All **140** registered effects and original Show Mode loops remain unchanged in registry/IDs.
- All 120 WLED-source modes retain strict source-golden comparisons except **explicitly identified #99 authorized display-policy corrections** (speed 255 Blink/Strobe, true-black Chase base, BPM modular wrap, TV scene duration). Do not misstate such deviations as source-byte parity.
- All 12 proprietary continuous effects have pinned golden vectors; Fusion Core's deliberately updated radial ring is the source of truth, Event Horizon stays a one-shot with terminal foreground. Separate intentional Startup/Shutdown one-shots remain clamped.
- **13 proven third-color consumers:** original 11 plus conditional Aurora #38, and source per-particle Popcorn #95. Retain six unsupported default 1D third-color roles (Scan 10/11, Scanner 40/60, Rolling Balls 48, Bouncing Balls 91). Metadata/role grammar is **not** BLE capability evidence.
- Assert color-slot #000000 remains truly black, palette source authenticity, zone isolation, show STOP, OEM Brake/Turn/Reverse priority and staged NVS through existing CORE/Trigger regressions.
- Do not claim Breathe/Fade/Fireworks1D **phone smoothness** is proven by deterministic host traces.

## Integration and review gates

Run the full `core-firmware/tests/run-host-tests.sh` including Issue #99 + #102 tests on this **one combined** HEAD; verify ESP-IDF v5.5.5 ESP32-S3 image and manifest; Android source metadata; independently run Trigger-host safety on exact same SHA. **Do not reuse prior #107/#108 passing run IDs as combined evidence.**

Request independent Codex review of shared code, source-authored third-color roles, auth/trigger isolation and test exceptions. Genuine findings must be resolved by scoped commits and tests; do not assert review approval before a separate reviewer supplies it.

Issue #103 downstream: integrate this exact reviewed shared renderer with Android #106 and Firmware #96 through a separate reviewable successor branch, repin JNI source hashes and run genuine combined exact-head tests. Preserve owner-approved native six-screen UI exactly; neither add BLE opcode 11 admission nor modify physical safety on an Effects branch.

**Release gates:** both Effects PRs stay draft and unmerged. No physical or phone testing, no 22-case hardware acceptance, no failover or vehicle release certification, no firmware version bump.
