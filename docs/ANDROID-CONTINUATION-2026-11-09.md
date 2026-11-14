# Android continuation checkpoint — 2026-11-09

Owner requested all Android followups and subsequently asked to conserve remaining usage. Continue draft PR #111 from verified head 9943dd7d784a76045237860ac87dce6a3d4a2ac7; no merge or hardware release is authorized by this checkpoint.

## Completed in this increment

Issue #122: native BLE output configuration now enforces 1–2000 pixels per output, with cumulative zone totals matching output capacity. Zone placeholders may still be zero. Both encode and device readback enforce the budget. JSON fractions, strings and overflowing numbers are rejected before integer coercion. Wire version 1, 261-byte layout, uint16 fields and CRC are unchanged. The existing native output editor already uses the shared codec limit; no removed WebView editor was restored.

Validation: JDK 17.0.17, JUnit 4.13.2, org.json 20240303: four new codec tests passed. `node android/qa/test_native_source.cjs` passed. No new APK, BLE/device, LED, vehicle or physical brownout evidence was produced.

## Remaining authorized work — not completed

- Renderer: synchronize JNI with Effects #110 head 09eb762e826b97dcdaa5a29a1f83c913ab7fc052 for explicit-black Aurora and Firmware #127 head 62166aafda1e8230ce996ab424ee4a1e871adb1e for stable zone state ownership. Update source provenance and run the independent 140-effect JNI oracle. Do not merely change capability lists: Aurora needs palette 4 and Popcorn/Aurora need the exact `fx-tertiary13-v1` token.
- Geometry: transport/transaction APIs already exist, but Layout is not wired to the acknowledged device transaction lifecycle. Preserve drafts versus confirmed geometry, assigned roles and atomic MTU-aware updates; review overlapping boundary edits and session checks before wiring UI. Use latest verified Firmware #127 contract; do not invent Fill-to-Index opcodes or hand off a final protocol while reconciliation is incomplete.
- Wi-Fi: integrate authenticated same-device STOP and link policy from PR #119 head 1e91d1bde356ad6efaf692ad75b4fe8d455b9272. Full command parity requires firmware endpoints beyond existing STOP; gate unsupported operations and never fall back to unauthenticated mutations. Keep the six-screen native UI.
- Reconcile the firmware owner-approved 0xFE lifecycle, five-second geometry lease, separate diagnostic lease and 64-event ISR queue. Firmware work was paused when the owner switched to Android; it is not completed by this Android increment.

## Workspace preservation

`/workspace/OPENTAIL-android`, branch `codex/android-continuation`, contains this Android work. `/workspace/OPENTAIL-development` contains older, unfinished firmware edits atop local d497a09777ed6323780abb0df18e8abfe331885e. Remote Firmware #127 advanced independently; do not push that old local branch or overwrite its work. Audit the current remote before resuming. Keep both PRs draft/unmerged and retain all physical evidence gates.
