# OpenTail CORE — corrected Effects #80 integration checkpoint

**Status:** Software integration branch; **not** a vehicle-release or physical hardware approval.

## Immutable source selection

- Effects-approved renderer **PR #80:** `7c0fcc450a7619a57603aabf833dea855a18bcd8`
- Firmware foundation **PR #69:** `05509c894ad6dbd48566c3784466aa005fb61263`, including #62 BLE recovery, #54 boot-stack correction, #51 four-owner source integration and #49 durable NVS.
- Target Android JNI source **PR #79:** `3f7d7dc0b3d3248bb5b04ec6a2eb4a425adb6e13` (pending independent Android test correction and approval).
- Integration branch starts *at the exact Effects #80 commit*, carrying its unmodified production implementation. No cherry-picked approximations, no UI changes, no modifications of approved Effects algorithms.

## Behavior and safety requirements

- All 140 identifiers and source metadata; 120 source-pinned WLED effect RGB goldens.
- Twelve continuous originals 32774–32785 wrap exactly across phase boundaries; live Speed edits preserve per-zone phase. Seven one-shot startup/shutdown effects remain terminal; full trigger priority and SHOW→STOP semantics retained.
- Logical render sequence: resolved palette/effects/particle overlays, then physical OEM override (Brake, Turn, Hazards, Reverse), then decorative-only gamma/calibration, immutable RGB capture and physical color swizzle.
- HMAC owner enrollment/authentication, 140-item long-read ATT behavior, BLE reconnection, config staging/revision semantics and NVS recovery remain #69 exact.
- Backup/failover software interfaces and physical-release gates are unchanged. Actual dual-MCU failover, electrical brownouts, GPIO safety latency and optical WS2815 measurements are **not certified**.

## Android parity and explicit blocker

The 13 native C renderer/header files in Android #79 were compared against the same paths in Effects #80 and matched **13/13 Git blobs**. This proves source identity only; it does not prove JNI/Java operation or user-phone displayed frames.

Android #79 CI run 41210752844 failed in `android/qa/test_full_offline_preview.py`: the test still asserts old pin `1767b144e193ed6978816899ebb5eba237ee6e2d` and its older checksums, while the JNI sources were updated to #80. Android Architecture owns updating the oracle/provenance fixture and rerunning all 140/140 JNI output/golden tests. Firmware will not edit Android files, source manifest, or approved UI. Until Android CI is green, mark cross-platform frame parity **UNVERIFIED**, not PASS.

## Acceptance sequence

The integration CI asserts the complete upstream Effects #80 runtime is unchanged, runs full CORE host + Triggers/Effects tests, confirms 13/13 Android source blobs against exact #79, builds ESP32-S3 under IDF 5.5.5 and verifies the bootloader/partition/app/flash manifest. Separate independent Trigger and Android acceptance are required against this final integration SHA. No merge or flash until applicable checks, reviews and physical gates are satisfied. No direct `main` changes.
