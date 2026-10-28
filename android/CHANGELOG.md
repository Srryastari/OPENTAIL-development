# OpenTail Android Changelog

## v0.1.28 — unified 1D effects and status-light restoration
- Sync the approved OpenTail Studio v0.19.62 asset with web Studio and CORE's embedded page.
- CORE BLE effect capability list supports the full 120-entry 1D catalog plus OpenTail Embers.
- Negotiate ATT MTU before BLE service discovery so the expanded effect list can be read in one transfer where supported.
- Preserve the v0.1.25 CORE BLE effects, independent zones, saved scenes, Output Manager, trigger logic, and approved UI.
- Restore approved onboard status patterns: startup white flash, Wi-Fi cyan pulse, connected green double pulse, disconnected red double pulse, 1.4-second Show Mode rainbow, and 90/90ms red fault blink.
- Software integration candidate only; actual pixel behavior and 62 FPS remain unverified on hardware.
