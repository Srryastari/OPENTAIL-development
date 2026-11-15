# OpenTail Windows one-click bench (BLE-only controller)

**Start here:** From Windows 10/11, double-click **OpenTail-Bench.bat** in the repository root.

The controller is commanded **only over Bluetooth through the Android OpenTail app**. USB-C is used for phone ADB installation/logcat and ESP32-S3 receive-only serial logs. The runner **never uses controller Wi-Fi, HTTP, or a dashboard**. The Windows PC needs Internet only for GitHub Actions artifacts.

## One-time prerequisites

- Windows PowerShell 5.1 (included with Windows).
- Android SDK Platform Tools ADB installed. The runner detects ADB on PATH **or** in Android Studio's standard local SDK directory, without editing system PATH. If unavailable, it provides an official installation link. Enable Developer Options / USB debugging on the phone, attach a data USB-C cable, and approve USB debugging. ADB should report exactly one authorized 'device'.
- GitHub CLI gh (for verified Actions artifacts). **The launcher now checks for it, including standard Windows installation folders.** If missing, it offers to run the official WinGet installer only after you type INSTALL; if not signed in, it offers GitHub's browser authorization only after you type LOGIN. Both are one-time setup steps. If WinGet is unavailable, install GitHub CLI from https://cli.github.com/ and rerun. No account tokens are copied into the launcher or logs.
- ESP32-S3 N16R8 USB-C cable and USB serial/JTAG driver. Device Manager must show a COM port. The runner recognizes ESP32 VID_303A, USB-JTAG, CP210, CH340 and FTDI, and refuses to guess when multiple ports qualify.
- Only for optional **explicit firmware flashing**: Python Windows launcher 'py -3' and 'py -3 -m pip install esptool'. Not needed for serial logging.

## If you previously saw gh not installed or not on PATH

That was a **preflight error**, not a Bluetooth, Pixel Probe, Save or RMT test. The old report's zeros mean **no capture ran**. Download a **fresh repository ZIP** or update the checkout, then run OpenTail-Bench.bat again. The new launcher guides GitHub CLI installation and login with approval, refreshes its own process PATH after WinGet finishes, and marks unexecuted hardware checks **NOT RUN**.

You can also install GitHub CLI manually with 'winget install --id GitHub.cli --exact --source winget', open a fresh PowerShell window, and run 'gh auth login'.

## Normal one-click session

1. Double-click **OpenTail-Bench.bat** with both USB devices connected.
2. The runner reads the current **draft/unmerged** Android PR #111 and Firmware PR #127 SHAs. It checks successful exact-head Android unit/geometry/JNI/140-effect and Firmware build workflows, plus the successful debug-APK build job.
3. It downloads the matching unexpired GitHub Actions artifacts, verifies firmware release_manifest.json (exact SHA and ESP32-S3 N16R8) and SHA256SUMS.txt for the app image and flashing metadata.
4. 'adb install -r' installs the debug APK **without uninstalling, wiping app data, downgrading, or touching controller NVS**.
5. Logcat and ESP32 serial logging start together, and the Android app opens. During the default 120-second session, connect and authorize OpenTail via phone BLE, choose Layout Output 1, tap **Check first LED**, and optionally try Output 2, Show Modes and unsaved live changes. Save only if **you explicitly choose**.
6. Open the timestamped folder **bench-reports/bench-YYYYMMDD-HHMMSS**: REPORT.md, report.json, android-logcat.txt, esp32-serial.txt, and optional flash.log.

The report distinguishes **automated exact-head software regressions** from **live physical observations**. Missing log evidence is **NOT VERIFIED**, not PASS. Firmware PIXEL_PROBE_ACCEPT, PIXEL_PROBE_RMT_DONE, PIXEL_PROBE_NONBLACK_FRAME indicate progressively deeper software/RMT execution but **never prove light actually illuminated**.

## Optional commands from the repo root

- Safe no-hardware runner test:
  powershell.exe -NoProfile -File .\scripts\windows\OpenTail-Bench.ps1 -SelfTest
- Five-minute capture:
  powershell.exe -NoProfile -File .\scripts\windows\OpenTail-Bench.ps1 -CaptureSeconds 300
- Choose ambiguous devices manually:
  powershell.exe -NoProfile -File .\scripts\windows\OpenTail-Bench.ps1 -EspPort COM9 -AndroidSerial PHONE_ADB_SERIAL
- Guided existing BLE Pixel Probe (not an independent USB command):
  powershell.exe -NoProfile -File .\scripts\windows\OpenTail-Bench.ps1 -DirectLedProbe
- Optional firmware app-partition flash: **OpenTail-Bench.bat flash** or the PowerShell runner with -FlashFirmware. It displays the exact SHA and requires typing **FLASH COM9** (or the detected port). Only then will esptool write the existing app partition at **0x20000**. It does not run erase-flash, erase NVS at 0x9000, factory_reset.py, 03_FACTORY_RESET_COM9.bat, partition-table writes, or OTA data writes. The existing bootloader and partition layout must already be compatible.

## Direct RMT diagnostic limitation

Existing Firmware #127 has a white pixel-0 diagnostic via the authenticated **BLE Pixel Probe** and USB serial logs for RMT completion. It does **not** have a verified USB serial command to drive GPIO16/17 independently. This runner will not spoof pairing, extract owner keys, bypass BLE, inject a new unreviewed UART endpoint, or label an Android BLE probe as independent of Android/BLE. A true transport-independent RMT test would require a separately reviewed **development-only** firmware interface and explicitly authorized flash; it is not part of this tooling-only change.

## Safety / troubleshooting

- No production Android UI, ESP-IDF code, GPIOs, trigger arbitration, effect renderer, security, BLE contracts, RMT, or NVS is modified.
- If ADB shows unauthorized, approve the phone USB debugging prompt. If multiple phone/COM devices exist, select them with the PowerShell options instead of guessing.
- Close other serial monitor programs before starting. Serial capture disables DTR/RTS and sends zero bytes, but on some hardware opening a USB port can still affect boot timing.
- GitHub's overall release-evidence job may remain red pending physical tests. The runner checks **specific passing exact-head jobs**, not false full hardware approval.
- Serial and phone logs can include private device identifiers. They remain local. Review/redact before sharing; there is **no automatic upload**.
- Physical illumination, GATT 133 reproduction, Save persistence across controller reboot, and real-world continuity are **not automatically certified**. The runner captures evidence while the owner operates the authorized BLE phone interface.

Development-only; Bluetooth is the only controller command transport.
