# OpenTail Android v0.1.15 — compact live status badges

Matching release: CORE v0.1.24-alpha. Extract and run **BUILD-APK.bat** on Windows. This release is **source**, not a compiled APK; Android SDK compilation and physical BLE testing must be completed on your phone.

## BLE onboarding without CORE Wi-Fi

Turn Bluetooth on and open **Layout > Bluetooth Output Manager** in the native app (offline Studio is fine). Tap Find BLE and select OpenTail CORE. With CORE already running, **hold its physical BOOT button**, then tap **Pair with BOOT**. When `BLE · OK AUTH` appears, release the button. The Android app encrypts the owner key in Android Keystore and reauthenticates with a new HMAC challenge when BLE reconnects. **Authorize** attempts that stored-key path. Legacy Wi-Fi auth is still an explicit fallback button.

Existing 261-byte BLE configuration read/write with save/readback, 30-pixel GPIO17 test path, Studio v0.19.51, haptics, and Wi-Fi Studio remain present. Default fresh-install auto-Wi-Fi is disabled; manually selecting Wi-Fi remains possible.

**BENCH-ONLY SECURITY LIMITATION:** Initial BOOT enrollment shares the secret on an unauthenticated BLE link and is vulnerable to eavesdropping/MITM during enrollment. Do not use for road lighting or claim production-grade secure pairing. Full secure commissioning, trigger input BLE editing, revocation, and independent hardware failsafe remain future work.

See BUILD-AND-INSTALL.md and PHONE-TESTS.md. No Android APK or ESP-IDF device binary was built here.
