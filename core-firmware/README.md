# OpenTail CORE v0.1.24-alpha — physical BOOT BLE bench enrollment

> **Current PR #12:** For a free Windows ESP32-S3 build, follow [BUILD-PR12-WINDOWS.md](BUILD-PR12-WINDOWS.md) and run [BUILD-CORE-WINDOWS.ps1](BUILD-CORE-WINDOWS.ps1). This packages the flashable images but never flashes hardware. The older PREPARE-AND-BUILD instructions below describe the historical v0.1.24 enrollment bench release.

This is a test build based on working v0.1.23: same HTTP Studio, output/trigger NVS profiles, BLE shows, BLE Output Manager read/write and Wi-Fi fallback.

## BLE onboarding

After the controller is running, hold the board's **BOOT (GPIO0)** button while Android v0.1.12 taps **Pair with BOOT** in Layout > Bluetooth Output Manager. It reads a 128-bit bench owner secret only while GPIO0 is held low. The secret is generated once and stored in separate NVS namespace `ot_blepair`; later BOOT enrollment re-reads the same secret (physical possession remains required). Android saves the key encrypted via Android Keystore. On later BLE connections the app reads a fresh 16-byte random challenge and proves knowledge of the stored key by truncated HMAC-SHA256. The owner key is **not transmitted during ordinary reconnection**. The existing temporary Wi-Fi token path remains supported as an explicitly requested fallback.

**SECURITY WARNING:** The initial enrollment secret is delivered via unauthenticated BLE GATT while BOOT is held. A nearby attacker capable of intercepting that enrollment exchange may obtain it. This is **not production-grade MITM-resistant pairing**. Do not expose on a public product or use as sole security for road-lighting. Future production commissioning needs authenticated OOB provisioning/LE Secure Connections with MITM protection, key rotation/revocation, and independent road-lighting failover. Physical input wiring/driver electrical safety are not validated by this build.

## Build and rollback

See INSTALL-WINDOWS.md. Build via ESP-IDF 5.5.5 on Windows with `PREPARE-AND-BUILD.ps1`; confirm successful compilation before flashing (no `erase-flash`). The script attempts to copy the custom AP password from previous projects without displaying it. Keep the known-working v0.1.23 folder for rollback. Studio remains v0.19.51: BLE pairing is exposed through Android, not Studio in a normal desktop browser.

Validation: 40 host gates, plus native Android static/syntax checks in the companion project. No ESP-IDF target compiler, Android SDK build, or physical BLE testing has been performed here.
