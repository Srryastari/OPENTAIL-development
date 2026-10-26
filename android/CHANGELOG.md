# OpenTail Android v0.1.15 — approved compact live status chips

- Replaced the visually large/text-only header indicator with the **user-approved small, bordered OLED pill** (near-black backing, 12px icon, 11px text). All other navigation and existing screen structure remain unchanged.
- Green only when verified Bluetooth authorization or actual Wi-Fi transport is active; red when Bluetooth is disabled/disconnected; amber while scanning, connecting, pairing, authorizing, saving or reading.
- Bluetooth off takes priority even if Wi-Fi is available; Wi-Fi still appears separately on its own card.
- BLE in-progress states now take priority over Wi-Fi fallback in the header (no false green Wi-Fi while BLE is pairing).
- Matching compact status chip inside Bluetooth Output Manager; full details remain visible below.
- Native Android Bluetooth and Wi-Fi event callbacks remain unchanged; status stays event-driven, no additional polling.
- No CORE firmware, BLE wire protocol, NVS format, trigger logic, Live Lighting Preview, or navigation changes.
- Static/host tests only; APK compilation and actual-device state transitions remain to be verified.
