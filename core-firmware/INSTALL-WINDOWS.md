# OpenTail CORE v0.1.24-alpha — Windows install and bench test

1. Extract into `Desktop\OpenTail` alongside your known-working `OpenTail-CORE-Alpha-v0.1.23-BLE-Outputs` folder.
2. Open **ESP-IDF 5.5.5 PowerShell**:

```powershell
cd "$env:USERPROFILE\Desktop\OpenTail\OpenTail-CORE-Alpha-v0.1.24-BLE-Enrollment"
.\PREPARE-AND-BUILD.ps1
```

3. Confirm the build succeeded. Fix any compiler errors before flashing. Flash the *correct current* COM port, e.g. `idf.py -p COM9 flash`. Never use `erase-flash`.
4. Verify CORE status reports `0.1.24-alpha` at `http://192.168.4.1/` when on the CORE AP; existing outputs and zones should remain.
5. Install **Android v0.1.12**. You do **not** need to join CORE Wi-Fi for Bluetooth onboarding. Open Android **Layout > Bluetooth Output Manager**; choose **Find BLE**, select OpenTail CORE, then **hold the physical BOOT button after startup** and tap **Pair with BOOT**. Do not hold BOOT during a reset or power-on; it can put ESP32 into download mode.
6. Wait for `OK AUTH`, release BOOT, then **Reload Output** on GPIO17; check the previous 30-pixel configuration. Disconnect BLE/reconnect to test owner-key automatic reauthorization without Wi-Fi.
7. Verify GPIO16 saved config unchanged and existing Wi-Fi Studio still works. Do not edit the 522-pixel assembly during first onboarding tests.

**Security:** BOOT presence restricts the enrollment read but does not cryptographically authenticate the first exchange. This is bench-only, not production-safe pairing. The firmware and app have not been cross-compiled or run on hardware in this environment. Roll back to v0.1.23 if pairing prevents normal use; leave NVS intact.
