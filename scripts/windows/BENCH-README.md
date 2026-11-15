# OpenTail FAST USB + BLE bench runner (Windows)

**Default:** double-click `OpenTail-Bench.bat` for a **30-second Bluetooth-only** capture. There is **no automatic controller command**, LED activation, Save, reset or firmware flash. Keep the Android phone on USB debugging and the ESP32 serial adapter on COM9 (or another detected unambiguous COM port).

## One-click playback diagnosis

For Show Modes, **run `OpenTail-Playback.bat`** from the project root. Its `-Mode playback -CaptureSeconds 30` invocation never passes `-FlashFirmware`. Firmware #127 PR lookup, firmware CI, and firmware artifact download are omitted: the runner observes the **installed** ESP32 firmware. When capture begins, tap Android **Show Modes → Solid Red → Play → Stop**. The runner does not send BLE commands or activate physical LEDs. `OK FX` proves controller acceptance only, not illumination.

If your console still says **"OpenTail one-click USB + BLE bench session"**, your local copy is outdated. Replace the runner scripts and launcher from the verified patch artifact. The corrected launcher says **"OpenTail Playback Diagnostics v2026.11.15.2"**. Never mix the newer launcher with older scripts.

## One launcher, four targeted sessions

| Command | Scope | Default duration |
|---|---|---|
| `OpenTail-Bench.bat` / `OpenTail-Bench.bat ble` | Bluetooth connection, OS bond, owner enrollment/challenge/authorization | 30 s |
| `OpenTail-Bench.bat layout` | Layout and optional owner-controlled Pixel Probe | 30 s |
| `OpenTail-Bench.bat save` | Owner-initiated Save/Reload | 30 s |
| `OpenTail-Playback.bat` / `OpenTail-Bench.bat playback` | Owner-initiated Show Modes / STOP, no firmware CI/flash | 30 s |
| `OpenTail-Bench.bat extended` | Bluetooth capture | 120 s |
| `OpenTail-Bench.bat layout extended` | Layout capture | 120 s |
| `OpenTail-Bench.bat selftest` | Offline PowerShell 5.1 regressions; **no hardware** | N/A |

Advanced direct call: `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\windows\OpenTail-Bench.ps1 -Mode bluetooth -CaptureSeconds 300 -EspPort COM9`. Only durations 30–1800 seconds are supported. `OpenTail-Bench.bat flash` remains **explicit firmware flashing only**, still separately requiring typed `FLASH COMx` confirmation; it does not erase NVS.

## Fast diagnostics

- Every attempt uses the current **draft/unmerged** Android #111 SHA and, for identification, Firmware #127 SHA. Android exact-head and JNI workflow gates must be successful. Firmware CI/build/download is **not required** for read-only Bluetooth diagnostics while Codex is working on Firmware #127. An explicit firmware flash still requires independently verified firmware CI and a checksum-valid package.
- Artifacts are cached under `bench-cache/` by **source SHA and GitHub artifact ID**. Cached files are SHA-256 checked before reuse. No download is repeated for an unchanged verified artifact. The firmware binary is downloaded only if an explicit flash is requested.
- The installer records the installed package's Android `lastUpdateTime` with the phone ADB ID and exact APK checksum. Matching known installs are **not repeated**. Other installs attempt safe `adb install -r` first.
- **Signing mismatch never triggers automatic uninstall.** If Android reports `INSTALL_FAILED_UPDATE_INCOMPATIBLE`, the runner explains that uninstalling deletes the Android app's local projects and secure owner key; the user must type exactly `ERASE APP DATA` before any `adb uninstall`. Press Enter to keep the existing app and produce a **BLOCKED** report. Phone app data deletion does not delete controller ownership in NVS, so accepting may cause owner-enrollment refusal later.
- Capture exits early only for an explicitly observed error: owner enrollment refusal, unavailable challenge, rejected auth, GATT failure, command rejection, Save failure, renderer failure, or RMT failure. Missing success messages are **NOT RUN/BLOCKED**, never assumed failure.
- USB COM9 capture is **receive-only**, with DTR and RTS disabled. The raw log remains unchanged. `esp32-timeline.txt` separately timestamps received serial lines in **host UTC**, about 120 ms poll granularity plus USB buffering. Android clock skew is sampled through read-only ADB `date` calls when available; correlation includes an uncertainty bound and never claims sub-millisecond synchronization.

## What appears in the report

Each attempt creates `bench-reports/bench-YYYYMMDD-HHMMSS/`:

- `REPORT.md`: compact stage table and immediate failure category.
- `RESULTS.json`: machine-readable status, requested/actual duration, exact source/artifact identity, observed installed firmware identity, clock correlation, capped correlated event timeline, and per-stage **PASS, FAIL, BLOCKED, NOT RUN**.
- `android-logcat.txt`, `esp32-serial.txt`, `esp32-timeline.txt`: raw observations and UTC receive-time overlay.
- `report.json`: backwards-compatible JSON for older tooling.

The firmware **downloaded SHA is not the installed firmware SHA**. The actual controller version/boot ID is reported only if it appears in a USB boot log (or explicitly in a BLE information message); otherwise the installed identity is **unavailable**. The script does not reset the ESP32 to force a banner. A `PIXEL_PROBE_RMT_DONE` report is software/RMT evidence, not proof the LED illuminated.

## Setup and security

Requirements: Windows PowerShell 5.1, Android Platform Tools (ADB), USB-debugging-authorized phone, ESP32 COM serial, GitHub CLI authenticated for Actions downloads. The existing runner can guide GitHub CLI installation and login with explicit permission, and can locate Android Studio's installed ADB. For multiple phones or serial ports, specify `-AndroidSerial` / `-EspPort`; it never guesses ambiguous hardware.

Never use an Android APK uninstall as a general BLE owner-recovery procedure. Persistent owner registration in ESP32 NVS can survive phone app deletion. The bench runner does not bypass bonded owner auth, issue Bluetooth commands, flash automatically, erase NVS, clear owner credentials, or modify physical GPIOs.

**Future read-only diagnostics contract (defer until Codex's current firmware fix completes):** request an authenticated or explicitly safe passive controller identity/boot digest and reason-coded owner rejection over firmware BLE/USB telemetry. Do **not** interrupt or duplicate Firmware #127 implementation; until available, missing fields remain unknown.

Logs stay on the PC; review device identifiers before sharing. Physical Save persistence, real optical illumination, and road readiness are not certified by this tool.
