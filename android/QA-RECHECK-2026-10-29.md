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

## Native palette engine and locked styling (October 29 continuation)

- **Android-only change:** Native Compose PalettePicker now replaces the sequence's unlabelled numeric palette slider and appears in Show Modes Effects and per-sequence-step editing. Same existing OLED/white Tone, pill controls and fields; no new tab, WebView, Internet permission or other UI theme.
- Per-step palette is stored as `sequence[].settings.pal`, interoperable with Studio's v4 sequence settings. Shared palette remains `sequenceOptions.sharedPaletteEnabled/sharedPaletteId`, default OFF; toggling OFF restores stored step palette without overwriting it. The visible step summary reports the effective preference and clearly limits it to palette-compatible effects.
- Native catalog identifies default IDs 0–4; other IDs remain honest numbered choices unless a project supplies `paletteNames`. Gradient swatches are drawn **only** from well-formed project `palettePreviews` fixed RGB stop entries with proper 0–255 offsets; no fabricated preview for a dynamic/unknown palette. Existing Studio v4 exports do not include those optional preview fields, so most palette swatches will correctly say “Preview unavailable.”
- Standalone effect palette choice currently persists in **local Android preferences** like its existing speed/color settings; it is not yet a firmware scene or a portable Show Mode profile. Selecting a CORE-supported palette now routes through the existing authenticated **temporary** BLE FX palette byte, while unsupported palettes stay local and STOP any stale physical preview. There is no separate palette-write opcode or new firmware protocol. Sequence palette authoring is locally exported/imported with the v4 project.
- The locked LED viewer remains the zero-based design strip or an authenticated single CORE RGB888 frame. A palette swatch is **not** a CORE-rendered animation; physical palette acceptance is limited to CORE's implemented subset and requires independent bench proof.
- Native palette source/shape regression guards were added. **Current-head Kotlin build, Android unit tests, screenshots, TalkBack, BLE, and real palette parity require independent acceptance.** Keep Android PR #21 draft; no cross-domain merges.

## Palette import fidelity and immediate sequence redraw (October 29 continuation)

- No changes to approved screen structure, navigation, controls, fonts, colors, or capsule styling. No generated UI interpretation was used.
- Sequence model explicitly observes its Compose revision on the active screen, so saved step palettes and shared-palette toggle/readback immediately update without changing the editor's appearance.
- v4 imported and newly saved sequence steps reject invalid palette IDs rather than rendering a clamped number while exporting bad data. `sequenceOptions.sharedPaletteId` uses the same integer 0–255 validation. Rejection leaves the existing local project unchanged.
- Only genuine supplied fixed stop data can produce palette swatches. The parser rejects malformed/fractional RGB values instead of silently truncating them into different colors.
- Added executable JVM coverage of palette-byte boundaries, plus Android source contract regressions for v4 import, Compose model revision and exact palette stop parsing. Verify latest-head CI separately.
- Existing limitations remain: full actual effect animation parity requires CORE frames; only palette 0 for any supported effect and palettes 2–4 for Embers are accepted by the existing FX BLE command. No other palette selection may be claimed as physical LED playback.

## Unsupported palette/effect preview preflight (October 29 continuation)

- **Root cause:** selecting a palette unsupported by the installed CORE stopped the prior transient show, but a subsequent speed/color/effect gesture silently substituted palette 0 in the BLE packet via `corePreviewPalette()`. This misrepresented the selected UI palette and could restart a misleading physical demo.
- **Fix:** every effect-edit gesture now validates that the connected CORE advertises the selected effect and that the selected palette is accepted **before** arming the existing transient BLE preview. An unsupported selection stays editable locally, cancels any armed show and never sends a substituted default packet.
- The palette picker now calls the same common preview path as sliders and effect selection; duplicate palette-specific STOP handling was removed. The existing locked UI geometry, navigation, themes, controls, authenticated packet, brightness cap, TTL and trigger ownership were left intact.
- Effect IDs reported by a prior CORE are discarded when BLE is disconnected/unauthorized; reconnecting cannot inherit stale capabilities from another controller.
- Static architecture/live-edit regressions assert that the preflight runs before BLE arm, that STOP handles stale demos, and that capabilities are invalidated on disconnect. This is a source-level proof only; phone/BLE/physical-LED observation remains outstanding.

## Existing CORE palette BLE support restored accurately (October 29 continuation)

- Verified current firmware `core-firmware/main/ble_fx_command.c` and `palette_1d.c`: existing `showFx` 17-byte command **does** contain palette ID byte 15. CORE permits ID 0 for all supported effects; effect 32768 Embers additionally permits 2 (Color 1), 3 (Colors 1&2) and 4 (Color Gradient). Palette 1 and other custom IDs are not physically supported.
- The native palette engine now exposes a single restricted compatibility check. When the user selects a CORE-supported palette, Android sends it through the already-existing authorized, bounded 5-second FX preview with 24/255 brightness cap. All other choices remain locally editable and do not issue an unsupported palette packet.
- Switching to an unsupported palette explicitly stops any armed temporary hardware FX preview and invalidates the previous captured CORE frame, avoiding a false on-device match. Other FX gestures preserve their existing BLE behavior; unsupported palette preferences fall back to direct/default ID 0 in transmitted packets.
- Updated existing source regression checks and added executable Kotlin/JUnit cases for 0, 2–4, invalid IDs and non-Embers effects; no new controls, routes, styling, firmware or effects code.
- **Not physically proven:** the guarded packet and tests establish source compatibility, not that a connected CORE bench emits optical output matching Android's local palette choice or continuous 1:1 rendering.

## CORE logical-frame preview race regression (October 29 continuation)

- **Root cause:** the native RGB888 frame listener could publish a previously requested capture after an effect/color/palette/layout edit, offline-preview switch, disconnection, hardware output readback or explicit STOP had cleared the prior frame. A stale frame could thus be displayed under a current-state label.
- **Android-only fix:** `invalidateCoreFrame()` now advances a UI revision and disarms pending captures whenever the design or physical preview context changes. The listener requires the initiating revision, selected output, owner capability and non-offline mode to remain valid before it may publish the bytes.
- BLE `captureFrame` now returns a truthful started/rejected result; a short callback-delivery barrier prevents starting the next request between transport completion and delivery of its prior UI callback. Failed/busy requests do not claim to be capturing.
- Added source regression assertions for stale-callback rejection, output routing and the BLE handoff. Existing authenticated snapshot CRC/offset/sequence verification remains unchanged. **Still not a physical LED frame/optical parity test.** An Android APK CI success alone does not certify transport race timing on a device.
- No CORE, trigger, WLED source, Web Studio styling or unrelated PR was changed.

## Not yet accepted; do NOT label these complete

- **Visual 1:1 UI:** Shared native capsule slider styling is implemented across editors, but exact icons, optical glow strength, spacing, scrolling, dialogs and white-mode appearance still require on-phone screenshot comparison with the locked reference. Static checks cannot certify visual identity.
- **Trigger test UI:** Start/Stop now shows the selected trigger's **local color swatch**, not a full CORE-rendered effect animation or a physical trigger simulation. Full motion or hardware-backed trigger tests still require the CORE render protocol and bench evidence.
- **Full CORE parity:** optional snapshot proves individual logical RGB frames only. Continuous 53–120 FPS playback mirroring, optical light output, WLED 1:1 golden frames for every mode, flare/spark/particle equivalence, and dual-output synchronization are not yet certified.
- **Physical integration:** Android BLE owner enrollment/reconnect, supported effects, low-MTU multi-chunk RGB snapshots, STOP priority, Output 1/2 NVS readback, eight 12V input priorities, actual LED color order and measured FPS need actual device evidence.
- **Broader product behavior:** hardware live pixel/zone geometry edits need a separate authorized volatile-preview protocol; automatic Startup/Shutdown ignition transitions and controller power-off are firmware projects, not completed by the Android UI.
- **Data/UI acceptance:** import/export interchange against a real Studio v4 file, Android restart persistence, edge counts, TalkBack/touch targets, narrow-screen scrolling, and actual approved navigation/style screenshots are still release gates.

## CI and merge policy

Use the **latest head** of PR #21 for Kotlin/Gradle/Android unit tests, source contract checks, Web Studio regression and APK integrity. Older successful runs cannot certify later commits. Firmware PR #19 has its own build, host, snapshot and physical release gates. No automated check should claim optical/automotive safety or pixel-perfect UI rendering. **No merge or flash until the corresponding approval and physical acceptance.**
