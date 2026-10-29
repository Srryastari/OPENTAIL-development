# OpenTail Android / UI recheck — 2026-10-29

**Scope:** Android PR #21, stacked on Studio PR #20. Firmware integration is PR #19. This is an audit of source and CI evidence, **not** a phone screenshot or bench certification. Keep both Android and firmware PRs draft until acceptance.

## Confirmed in the current Kotlin/Compose source

- Exactly one Compose launcher; six top destinations in locked order: Home, Show Modes, Layout, Triggers, Auto Events, Settings. No WebView runtime, bottom tab bar or Internet permission.
- OLED-black and white schemes, pill action buttons/text fields, and retained white Connect button. Home BLE status and unified pairing/authorization action are present.
- Local v4 designer: named zones, zero-based offsets (including manual gaps/reverse), individual painted pixels, trigger assignment editor, optional startup/shutdown definitions, sequence repeat/shuffle/shared palette, local presets, document import/export.
- CORE remains authoritative for effects, vehicle trigger arbitration and physical output writes. BLE live effect previews are authenticated, bounded and temporary; hardware output configuration requires a separate explicit NVS save and controller readback.
- Exact local pixel indexing and optional verified CORE logical RGB888 snapshot capture; no invented 1:1 effect simulation. The capture path is source-implemented but not physically validated on the bench.

## Fixes made during this recheck

1. Removed a GS400-era **623 LED per output** cap from the **active Compose editor and BLE codec**. Codec honors the unsigned 16-bit wire representation; installed firmware alone limits real board memory and may reject a requested mapping. Do not interpret 65535 as supported by a device.
2. Prevented the output editor's Remove zone operation from leaving its reported output total out of agreement with surviving zone lengths. Refreshed positional editor state when zone count changes.
3. Removed an invented hardcoded GPIO pin from BLE configuration decode; output wiring is owned by the firmware/board.
4. Replaced obsolete Java Views source audits with assertions against **shipped Compose** pages and their real actions, plus new universal geometry regression guards.
5. Invalidated stale captured CORE frames on local project edits. Live preview/STOP and GATT capture paths remain capability-gated.
6. Restored the previous project on failed local import persistence and applied zone bounds to pixel-clear operations.

## Not yet accepted; do NOT label these complete

- **Visual 1:1 UI:** native Material3 Slider is not yet proven visually identical to the approved glowing capsule design. Exact icons, spacing, scroll ergonomics, dialog behavior and white-mode polish require on-phone screenshot comparison with the locked reference. Static source checks alone cannot certify visual identity.
- **Trigger test UI:** Start/Stop currently tracks local selection state; it is not a real CORE-rendered animation preview, despite the offline selection message. Physical test controls must be separated from local authoring and must never imply verified hardware playback.
- **Full CORE parity:** optional snapshot proves individual logical RGB frames only. Continuous 53–120 FPS playback mirroring, optical light output, WLED 1:1 golden frames for every mode, flare/spark/particle equivalence, and dual-output synchronization are not yet certified.
- **Physical integration:** Android BLE owner enrollment/reconnect, supported effects, low-MTU multi-chunk RGB snapshots, STOP priority, Output 1/2 NVS readback, eight 12V input priorities, actual LED color order and measured FPS need actual device evidence.
- **Broader product behavior:** hardware live pixel/zone geometry edits need a separate authorized volatile-preview protocol; automatic Startup/Shutdown ignition transitions and controller power-off are firmware projects, not completed by the Android UI.
- **Data/UI acceptance:** import/export interchange against a real Studio v4 file, Android restart persistence, edge counts, TalkBack/touch targets, narrow-screen scrolling, and actual approved navigation/style screenshots are still release gates.

## CI and merge policy

Use the **latest head** of PR #21 for Kotlin/Gradle/Android unit tests, source contract checks, Web Studio regression and APK integrity. Older successful runs cannot certify later commits. Firmware PR #19 has its own build, host, snapshot and physical release gates. No automated check should claim optical/automotive safety or pixel-perfect UI rendering. **No merge or flash until the corresponding approval and physical acceptance.**
