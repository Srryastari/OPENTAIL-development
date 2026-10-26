# Upgrade OpenTail CORE Alpha to v0.1.1 and open Studio

**You currently have a working v0.1.0 firmware on COM9.** It boots, offers an onboard test page and exposes `/api/v1/status`. Keep the ESP32-S3 connected only to USB. Do not connect vehicle signals or LEDs yet.

## Files
- `OpenTail-CORE-Alpha-v0.1.1-Studio-Bridge-source.zip` — ESP-IDF **source**, not a `.bin` firmware image.
- `opentail-v0.19.29.htm` — stand-alone versioned Studio copy. The *same* file is included, compressed, in the CORE v0.1.1 source package.
- `opentail.htm` — stable deployment copy for WLED at `10.0.0.13/opentail.htm` when using WLED Wi-Fi.

## Step 1 — Extract source on Windows

Extract the v0.1.1 ZIP under your Desktop `OpenTail` folder. In the **ESP-IDF PowerShell** terminal, enter the extracted project folder, the one that contains `CMakeLists.txt`, `main`, `README.md` and `sdkconfig.defaults`. A typical path is:

```powershell
cd "C:\Users\astari\Desktop\OpenTail\OpenTail-CORE-Alpha-v0.1.1"
dir
```

If Extract All created two nested directories, enter the inner directory until `dir` shows `CMakeLists.txt` and `main`.

## Step 2 — Preserve the Wi-Fi password you already configured

Run this **from inside the new v0.1.1 project directory**:

```powershell
Copy-Item "C:\Users\astari\Desktop\OpenTail\OpenTail-CORE-Alpha-v0.1.0\OpenTail-CORE-Alpha-v0.1.0\sdkconfig" ".\sdkconfig"
```

If the old path differs, find the original `sdkconfig` under your successfully built v0.1.0 project. The file contains your private access-point password: do not share it. You can instead use `idf.py menuconfig` to set an AP password if copying fails.

## Step 3 — Build and flash

Keep the COM USB cable connected and open ESP-IDF PowerShell:

```powershell
idf.py build
idf.py -p COM9 flash monitor
```

Do **not** run `idf.py set-target esp32s3` again if you successfully copied `sdkconfig`: it may reset configuration. Verify the build target remains `esp32s3`. The flash monitor uses `Ctrl+]` to exit.

You need to see successful build completion and boot logs showing `CORE 0.1.1-alpha`. The new firmware is still bench-only and unverified until you build and flash it.

## Step 4 — Use the new Studio interface

On your iPhone, join Wi-Fi **OpenTail-CORE-Alpha** using your already configured password. Open:

**http://192.168.4.1/studio**

Studio should open on **Connect → OpenTail CORE**, auto-connect, and show live CORE diagnostics. Try **Brake** (2 seconds), **Red** (3 seconds), and **Stop CORE bench tests**. Without the level shifters/LEDs, verify that the status JSON changes temporarily; no lights need to be attached.

The simple firmware console remains at **http://192.168.4.1/**.

## WLED remains supported

In Studio, choose **WLED** under Connect when using a reachable WLED controller. WLED's previously hosted Studio remains at `http://10.0.0.13/opentail.htm` when on the WLED network. Your iPhone generally cannot access both devices' separate access points simultaneously.

## Expected capabilities, not yet available on CORE

CORE Alpha does **not** execute saved Studio effects, effect editor params, palettes, sequences, Startup/Shutdown animations, or fault-tolerant road lighting. The adapter only exposes temporary bench demos, simulated inputs and firmware status.

## Troubleshooting

- `/studio` is 404: the board is still running v0.1.0; flash the v0.1.1 project.
- AP works but Studio won't connect: open `/api/v1/capabilities` and `/api/v1/status` on `192.168.4.1`; check the serial monitor and the firmware version.
- `idf.py` says `CMakeLists.txt` missing: you are in the wrong extracted folder; enter the folder with project `CMakeLists.txt`.
- No Wi-Fi AP: check USB power, serial boot messages and the copied `sdkconfig` configuration.
- WLED mode unavailable while on CORE's access point: this is a network routing limitation, not a failure of the Studio UI.
