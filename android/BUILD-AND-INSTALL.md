# Build OpenTail Android v0.1.15 on Windows

1. Extract the ZIP; open `OpenTail-Android-v0.1.15`.
2. Run `BUILD-APK.bat`. Confirm **BUILD SUCCESSFUL** and a generated APK before installation.
3. Install the new APK over the existing OpenTail Android app (same applicationId and higher versionCode).
4. Open app > Layout > Bluetooth Output Manager to pair using BOOT on CORE v0.1.24.
5. See PHONE-TESTS.md for staged tests. Only flash CORE firmware after its own successful ESP-IDF build. Preserve your existing CORE settings; do not erase flash.

The ZIP is source, not a precompiled APK.
