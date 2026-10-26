# CORE v0.1.24-alpha — BENCH BLE owner enrollment

- Added BOOT/GPIO0-held owner enrollment GATT characteristic and persistent per-controller 128-bit owner key in independent NVS namespace.
- Added random single-use challenge + truncated HMAC-SHA256 session authorization, five failed attempts maximum per BLE session.
- Preserved old Wi-Fi-issued temporary BLE AUTH command for compatibility.
- Preserved output/trigger manager storage and firmware behavior, GPIO16/GPIO17, Studio v0.19.51 and BLE config transfer.
- Important: initial GATT enrollment is not MITM-resistant and is *not* production-secure pairing.
- Device/APK compile and live Bluetooth verification still required.
