# Android v0.1.15 — phone test checklist

1. Build APK with `BUILD-APK.bat` and install over existing v0.1.14; header/Settings should reflect v0.1.15.
2. With BLE authorized, top-right shows a **small** black/green `Connected` pill; compare visually to approved reference.
3. Turn off Bluetooth in Quick Settings; it turns **red `Bluetooth off` immediately**, without navigation or refreshing.
4. With Wi-Fi connected and Bluetooth off, header remains red `Bluetooth off` while separate Wi-Fi card shows Wi-Fi connected.
5. Enable Bluetooth, begin connection; pill turns amber during scanning/connecting/pairing/authorizing then green when authorized.
6. Force disconnect; verify red `Disconnected` and no lingering green badge.
7. Read then save GPIO17 over BLE; pill shows amber `Syncing…` / `Saving…` until finished, then green; read back and verify no regressions.
8. Navigate Home / Shows / Triggers / Layout / Settings; layout and Live Lighting Preview remain unchanged.
9. Confirm the pill does not overlap the heading at narrow screen widths.

These are test instructions, not claims that physical phone tests already passed.
