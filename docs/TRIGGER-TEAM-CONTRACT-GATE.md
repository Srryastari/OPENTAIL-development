# OpenTail Trigger Integration — team contract gate
**Status:** BLOCKED — source/ABI reconciliation incomplete. **Physical testing is deferred until 100% of cross-team contracts below are agreed and reconciled.**

**Engineering rules:** Simplicity first; solve root causes; minimum necessary code impact. Trigger/audit owner may merge **only trigger or audit PRs**. Cross-team comments are handoffs, **not permission to merge** firmware, effects, Android, Studio, or their mixed-scope PRs. No physical acceptance can be inferred from a host check or an earlier flashed build.

## Source pins inspected (2026-10-29)

| Workstream | Exact PR head inspected | Relevant contract |
|---|---|---|
| CORE / firmware | PR #19 `183f39ecf8823a3089c935b2ea29f0cb2fefa092` | `docs/OPENTAIL-LOCKED-RELEASE-CONTRACT.md`, CORE frame snapshot v1 |
| Trigger integration | PR #24 `4d617193dc5a3b85e5c2c7081df9df721c889462` | `docs/TRIGGER-BLE-CONTRACT-V1.md`, 11-domain source audit |
| Native Android | PR #21 `2436af18dd1f965b40ff67650d0ab470a3dd4d33` | `android/LIVE-EDIT-CONTRACT.md`, `android/EXACT-LED-PREVIEW-CONTRACT.md` |
| Spark/flare engine | PR #22 `2f08e6717ae2ea71bac9a999719f51b6b929f79d` | Zone overlay after FX render, before trigger arbitration |
| Borrowed effects and palettes | PR #23 `c291c2c642e667fbf7badb8cab1f6ca34afd26f5` | Source-derived modes, selected-palette compatibility |

These are **independent draft heads**, not a reconciled integrated image. Changes to those heads require repeating only the affected *source contract review* before any combined tests.

## Required agreements (a complete source check is not an owner sign-off)

| ID | Boundary to preserve | Source cross-check | Open decision / blocker | Owning team |
|---|---|---|---|---|
| TC-CORE-01 | CORE alone owns physical IN1–IN8 GPIO4–11, fixed input polarity, 10ms × 4 stable-sample debounce, Stop/BLE disconnect independence, user-configured zone roles; rendering remains a single owner | Source-reviewed in #24 and #19 | Reconcile latest CORE input-init failure propagation, BOOT owner-enrollment GPIO fail-closed path, BLE advertising readiness, OTA boot confirmation and output runtime faults into the **same** candidate as trigger deferred-apply hooks | Firmware #19 |
| TC-PERSIST-01 | Both trigger mapping and output-zone saves serialize through existing mutex. Successful NVS write followed by asserted raw/debounced pin must preserve **old active state** until release; no `state_lock` held across NVS. No second save while pending | Pure helper/source contract present on #24 | Define whether `saved_on_device` denotes durable NVS or matching *active* profile. Currently #24 reports false when pending even though flash commit succeeded. Agree pending readback and reboot policy, especially a reboot *while an input is held*, before client changes | Trigger #24 + firmware #19 |
| TC-BLE-01 | Optional owner-auth trigger mapping GATT suffix `0x2c`: 11-byte write, 15-byte readback, LE revision, valid function IDs 0–8; existing output BLE config v2 wire is 261 bytes | #24 trigger wire and #21 output client reviewed | #21's `BleDiscovery.cfgResponse` immediately compares output snapshot to requested bytes. #24's **durable-but-pending** save legitimately reads back previous active output, causing Android to report an undifferentiated failure. Need capability-safe pending indication / deferred verification / explicit fail-closed messaging, without inventing opcodes or silently retrying flash | Android #21 + firmware #19 + trigger #24 |
| TC-ANDROID-01 | Compose trigger authoring is offline/local until the connected CORE explicitly supports an authenticated trigger-mapping characteristic; local Start/Stop is simulation, not physical override | Current Compose Triggers page and `BleDiscovery` checked | #21 does **not** currently implement a live trigger-map BLE client. Agree whether offline-only remains the milestone scope; do not advertise a deployed edit from local project changes | Android #21 |
| TC-FX-01 | Effect → additive spark/flare overlay → physical trigger per zone role → logical CORE frame snapshot → wire color-order swizzle. Trigger-owned colors are not calibrated or recolored by FX | PR #22 overlay source precedes its `ot_om_trigger_overlay`; PR #19 and #24 snapshot source follows trigger overlay before swizzle | PR #22 does not include PR #19's later frame snapshot; PR #24 does not include #22 overlays. Agree one unified compositor on a reconciled SHA, preserving effect phase under Brake/Hazards/Reverse/AUX and independent zones | Effects #22 + firmware #19 |
| TC-FX-02 | Unknown or incomplete borrowed effects, palettes and particles never bypass physical trigger priority or relabel approximate renderer parity as physical parity | #23 separately owns source ports and metadata; #24 uses role-local final trigger overlay | Confirm selected effect/palette fast paths and future particles do not skip final arbitration or output fault handling; remain unsupported/fail-closed when not implemented | Effects #23 |
| TC-OUTPUT-01 | Universal output geometry is user-defined; firmware owns strip resize/reconfigure/refresh/fault handling, and must not transmit a partial or stale buffer on change | #24 render task reconfigures by `active_pixels`; newer #19 source additionally checks per-output transfer/write faults and avoids incomplete-frame credit | Preserve the **newer #19 fault-handling/OTA-readiness** while incorporating #24 deferred geometry activation. Do not overwrite #19 render code with #24's older `ESP_ERROR_CHECK` path | Firmware #19 + trigger #24 |
| TC-RELEASE-01 | Only the final reconciled CORE candidate can be judged by CI/ESP-IDF/phone/physical evidence; keep release-evidence gate and bench-only warnings intact | All PRs draft, release evidence missing | Agree integrated commit SHA and exact feature/protocol capabilities before scheduling any physical test. Prior CI success on other SHAs is historical evidence, not sign-off | All workstreams |

## Ordered gate for physical tests

1. **100% team-contract agreement:** each owning team confirms all of its row(s) with a dated PR comment, any source changes needed are completed, and no blocker remains. A handoff request does not count as agreement.
2. **100% merged-source cross-check without production merge:** identify a reproducible integrated *candidate* commit/merge ref and inspect every affected boundary together. Keep other workstreams' PRs under their respective merge authorization.
3. **Only then** execute software CI/ESP-IDF/ABI regression on that exact candidate, retaining the release evidence gate. Passing CI is not hardware acceptance.
4. **Only after the above** conduct physical GPIO polarity/isolation, 8-input combinations, live LED arbitration/timing, BLE readback/reconnect and NVS power-cycle tests with exact flashed SHA and measurements.

**Gate status today: NOT 100%.** Source review documented; cross-team approvals and a single reconciled candidate are missing. Physical testing, flashing and merge approval are **ON HOLD**. No new tests or production-code edits are required merely to record this gate.

## Existing handoffs

- Firmware #19: https://github.com/Srryastari/OPENTAIL-development/pull/19#issuecomment-6511396746
- Android #21: https://github.com/Srryastari/OPENTAIL-development/pull/21#issuecomment-6511397769
- Overlay #22: https://github.com/Srryastari/OPENTAIL-development/pull/22#issuecomment-6511398264
- Effects #23: https://github.com/Srryastari/OPENTAIL-development/pull/23#issuecomment-6511398802
