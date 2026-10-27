# OpenTail Android v0.1.26 — CORE output UI parity and navigation cleanup

- Kept the six primary tabs in one sticky, horizontally scrollable row on narrow phones. No wrapping, fixed bottom bar, or duplicated navigation.
- Removed the nonfunctional development-only Output Control card. Actual independently saved outputs remain editable through the BLE Output Manager.
- Restyled the BLE Output Manager to pure black/neutral OLED cards and pill controls, preserving active/connected status colors and the white theme.
- Simplified Home to one contextual Connect/Authorize/Manage button and eliminated its hidden duplicate output button.
- Home now uses BLE-confirmed saved outputs for pixel counts and zones when CORE Wi-Fi is not connected; unverified physical lighting is never represented as telemetry.
- Preserved Output 1/2 NVS save/readback, original GPIO16/17 mappings, BLE wire protocol, Studio deployment copies, and trigger behavior.
- Android static/host regression checks pass locally. Native APK compilation and owner visual/physical acceptance must be independently confirmed by CI and on-device testing.

## Prior v0.1.25 changes

- Fixed Home navigation/content ordering, added whole-output BLE effect targets, optimized simulated preview repaint, integrated the one-button BOOT enrollment path, and restyled the native Bluetooth picker.

---

# OpenTail Android v0.1.15 — approved compact live status chips

- Replaced the visually large/text-only header indicator with the **user-approved small, bordered OLED pill** (near-black backing, 12px icon, 11px text). All other navigation and existing screen structure remain unchanged.
- Green only when verified Bluetooth authorization or actual Wi-Fi transport is active; red when Bluetooth is disabled/disconnected; amber while scanning, connecting, pairing, authorizing, saving or reading.
- Bluetooth off takes priority even if Wi-Fi is available; Wi-Fi still appears separately on its own card.
- BLE in-progress states now take priority over Wi-Fi fallback in the header (no false green Wi-Fi while BLE is pairing).
- Matching compact status chip inside Bluetooth Output Manager; full details remain visible below.
- Native Android Bluetooth and Wi-Fi event callbacks remain unchanged; status stays event-driven, no additional polling.
- No CORE firmware, BLE wire protocol, NVS format, trigger logic, Live Lighting Preview, or navigation changes.
- Static/host tests only; APK compilation and actual-device state transitions remain to be verified.
