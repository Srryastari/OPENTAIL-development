# OpenTail Studio v0.19.37 / CORE Alpha v0.1.9

## Automatic status-light interface

- Removed user status RGB brightness and enable/disable controls from CORE Diagnostics.
- Display read-only status label, concise animation timing, and hardware availability warning. No animation tester.
- State indicator reflects Wi-Fi, Studio connected/disconnected, show-mode, and fault states.
- Fixed firmware brightness at the previous default **65%**; previously saved experimental brightness/off preferences are no longer read.
- All six previously approved status patterns, including the 1.0-second Wi-Fi pulse and continuous 90ms fault blink, are unchanged.
- `GET /api/v1/status-led` and the Studio connection heartbeat remain available; `POST /api/v1/status-led` configuration was removed.
- Maintains exactly the same OLED black/white theme, segmented tabs, sliders used elsewhere, and glowing rounded-rectangle LED preview.

## Compatibility and testing

Browser UI: 32 focused simulated tests and 55 previous regression tests passed; no browser exceptions. Firmware: 25 CORE logic and 30 RGB animation host assertions passed. ZIP has matching embedded gzip Studio content. **v0.1.9 has not been compiled with ESP-IDF or flashed/verified on the physical ESP32-S3.** The RGB GPIO48 assignment remains provisional for the user board. The firmware is bench-only and has no automotive lighting fallback.

## Flash instructions (user's known COM9 port)

Extract `OpenTail-CORE-Alpha-v0.1.9-Auto-Status-LED-source.zip` to the Desktop `OpenTail` folder. In ESP-IDF PowerShell, change to the extracted project folder that contains `CMakeLists.txt`, then copy your previous private `sdkconfig` (do not distribute it), build, and flash:

```powershell
cd "$env:USERPROFILE\Desktop\OpenTail\OpenTail-CORE-Alpha-v0.1.9"
Copy-Item "..\OpenTail-CORE-Alpha-v0.1.1\sdkconfig" ".\sdkconfig" -Force
idf.py build
if ($LASTEXITCODE -eq 0) { idf.py -p COM9 flash monitor }
```

After flash, connect to `OpenTail-CORE-Alpha` and visit `http://192.168.4.1/studio`.