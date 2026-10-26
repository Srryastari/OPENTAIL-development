# OpenTail Studio v0.19.35 — Unified chat-preview styling + themes

Date: 2026-10-24

## Changes
- Reuses the approved short vertical pill-shaped LED strip visualization exactly; neither circular LED dots nor a forced vehicle-shaped map replace it.
- Uses the familiar black, thin-border, rounded-container design with neutral-gray selected segmented navigation (desktop/main and Show Modes). Keeps mobile bottom navigation.
- Adds "I Hate Dark Mode" to Connect > Appearance, with a shortcut in the main header; selected white/OLED theme remembered per browser through localStorage (`opentail-ui-theme-v1`). CORE and WLED render the exact same Studio stylesheet and controls; browser settings are not automatically shared across devices.
- Refines slider look and makes numeric outputs in the Universal Effect Editor and standard slider labels clickable for exact-value entry, with clamping and step matching. Slider behavior retains existing controller/edit callbacks.
- Adds honest Live/Synced/Simulated segmented control above the existing strip preview. Live requires actual WLED pixel telemetry; Synced requires a connected WLED device and is labeled "SYNCED · ESTIMATE" because its animation is local, not a measured frame. Simulated never sends LED commands. Current CORE Alpha can simulate the effects but does not provide pixel telemetry or firmware effects.
- Theme switching is UI-only; it never sends controller commands or changes LED strip brightness or color values.

## Tested
- JavaScript syntax verified using Node.js.
- All 27 baseline v0.19.34 browser checks pass.
- All 55 new interaction/browser checks pass on a simulated controller, including CORE/WLED routing, live/synced/simulated mode guards, theme switching and startup selection, modal slider exact-value edit, and widths 320, 390, 420, 768 and 1280 in both themes.
- 25 CORE host-unit assertions pass.
- CORE v0.1.7 bundle's `studio.htm.gz` decompresses byte-for-byte to standalone `opentail-v0.19.35.htm`; WLED deployment `opentail.htm` matches too.
- The CORE v0.1.7 source has NOT been compiled/flashed on physical ESP32-S3 in this environment. No controller-hardware or road-lighting tests occurred.

## Known limitations
- Local Studio simulation is an approximation of an effect, not an authentic controller render.
- Full synchronized animation generation, live CORE pixel-stream telemetry, failover/road-lighting safety remain outside the CORE Alpha scope.
- The theme preference is local to each browser/origin. Using WLED Studio and CORE Studio from two different addresses can have separate preferences, although the UI and toggles are the same.

## Deployment
- WLED: Upload `opentail.htm` using `/edit`; existing `/opentail.htm` URL remains unchanged.
- CORE: extract source ZIP as `OpenTail-CORE-Alpha-v0.1.7` under Desktop/OpenTail, copy working sdkconfig from your current v0.1.1/v0.1.6 project into new folder, run `idf.py build`, then (only if successful) `idf.py -p COM9 flash monitor`. Connect to CORE AP and visit `http://192.168.4.1/studio`.
