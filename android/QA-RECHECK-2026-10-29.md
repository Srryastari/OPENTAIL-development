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
7. Consolidated Layout and other editors onto a shared native **borderless soft-capsule Slider** (12dp track, rounded accent fill, subtle 22dp thumb glow), without replacing Material3 drag/keyboard/accessibility semantics. Corrected the color field's weighted row layout and gave action pills 48dp minimum height.
8. Made action rows horizontally scroll on narrow phones, reset vertical scroll when switching top destinations, and added exact-address 8/16/32/64 LED inspection zoom. No pixel interpolation or animated-frame approximation was introduced.
9. Added a local trigger-color swatch to Start/Stop instead of a misleading simulated animation; the test explicitly says CORE LEDs are unchanged.
10. Added a confirmation before a local preset replaces the active project. Restored Settings **Preview Offline** beside the locked white Connect action. This opens a local-only Show Modes session, clears old CORE frames, suppresses new effect/snapshot BLE commands, and issues STOP only when a previous temporary hardware preview was already armed. Leaving Show Modes restores normal authorized preview behavior.
11. Expanded active Compose regression checks for unified slider, touch targets, screen-scroll reset, zoom, preview isolation, and destructive-action confirmation.
12. Serialized CORE output reads and writes against BLE owner pairing/authorization and effect-capability reads. A disabled adapter explicitly cancels frame capture, and connection teardown clears pending authorization/writes.
13. Cleared cached CORE output readbacks and rendered pixel snapshots when controller authorization ends; disconnected devices can no longer appear to have a fresh output draft in the editor.
14. Added native vector top-navigation glyphs using the locked four meanings: sparkle (Show Modes), squares (Layout), lightning (Triggers), sun (Auto Events). No third-party icon font, emoji, or additional Android dependency.
15. Corrected the Home/Settings Bluetooth status pill to use the real BLE connection phase; transient diagnostic messages remain separate explanatory text.
16. Made Auto Events' **Preview offline** enter the same explicitly BLE-isolated session as Settings' **Preview Offline**, and accurately described that offline pixel inspection does not generate CORE animation.
17. Stopped preset snapshots from recursively embedding the entire saved preset collection. Loading an old design preserves today's preset library, including newer saved presets; malformed presets now show a local error instead of silently doing nothing.
18. Closed a GATT race by guarding owner pairing/authentication against other active BLE operations and resetting pending authentication on disconnect/reconnect.
19. Made native v4 preset-library updates **atomic** on disk commit failure; an unsuccessful save restores the prior in-memory collection instead of presenting an unsaved preset as persisted.

## Not yet accepted; do NOT label these complete

- **Visual 1:1 UI:** Shared native capsule slider styling is implemented across editors, but exact icons, optical glow strength, spacing, scrolling, dialogs and white-mode appearance still require on-phone screenshot comparison with the locked reference. Static checks cannot certify visual identity.
- **Trigger test UI:** Start/Stop now shows the selected trigger's **local color swatch**, not a full CORE-rendered effect animation or a physical trigger simulation. Full motion or hardware-backed trigger tests still require the CORE render protocol and bench evidence.
- **Full CORE parity:** optional snapshot proves individual logical RGB frames only. Continuous 53–120 FPS playback mirroring, optical light output, WLED 1:1 golden frames for every mode, flare/spark/particle equivalence, and dual-output synchronization are not yet certified.
- **Physical integration:** Android BLE owner enrollment/reconnect, supported effects, low-MTU multi-chunk RGB snapshots, STOP priority, Output 1/2 NVS readback, eight 12V input priorities, actual LED color order and measured FPS need actual device evidence.
- **Broader product behavior:** hardware live pixel/zone geometry edits need a separate authorized volatile-preview protocol; automatic Startup/Shutdown ignition transitions and controller power-off are firmware projects, not completed by the Android UI.
- **Data/UI acceptance:** import/export interchange against a real Studio v4 file, Android restart persistence, edge counts, TalkBack/touch targets, narrow-screen scrolling, and actual approved navigation/style screenshots are still release gates.

## CI and merge policy

Use the **latest head** of PR #21 for Kotlin/Gradle/Android unit tests, source contract checks, Web Studio regression and APK integrity. Older successful runs cannot certify later commits. Firmware PR #19 has its own build, host, snapshot and physical release gates. No automated check should claim optical/automotive safety or pixel-perfect UI rendering. **No merge or flash until the corresponding approval and physical acceptance.**
