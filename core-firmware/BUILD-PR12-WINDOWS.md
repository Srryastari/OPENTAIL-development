# OpenTail PR #12 — Local Windows firmware build

This is a local ESP32-S3 firmware build for the draft OpenTail PR #12. It uses no GitHub Actions minutes, and the script does not flash any device.

## One-time setup

1. Install Espressif ESP-IDF **v5.5.5 for Windows**, matching the earlier passing CI build. Official installer: https://dl.espressif.com/dl/esp-idf/
2. While signed in to GitHub, open https://github.com/Srryastari/OPENTAIL-development/tree/feature/wled-1d-fade-wipe-sweep . Click **Code > Download ZIP** and extract the ENTIRE repository to a short folder such as C:\OpenTail. Private repositories require your GitHub sign-in.
3. Launch the **ESP-IDF 5.5 PowerShell / IDF_PowerShell** shortcut installed by Espressif, not regular Windows PowerShell.

## Build commands

In the ESP-IDF PowerShell window, change directory to YOUR extracted repository's core-firmware subfolder:

    cd "C:\OpenTail\OPENTAIL-development-feature-wled-1d-fade-wipe-sweep\core-firmware"
    Set-ExecutionPolicy -Scope Process Bypass -Force
    .\BUILD-CORE-WINDOWS.ps1

If you extracted to a different folder, substitute that path. The script's default is two compiler workers; use -Jobs 1 if your PC is resource-constrained.

The build script:
- Verifies idf.py is active and warns if it is not v5.5.5.
- Reuses an existing custom CORE Wi-Fi password from the local ignored sdkconfig or a recognized adjacent previous CORE firmware folder.
- If none exists, generates a random bench Wi-Fi password, prints it once for you to save, and stores it only in the gitignored local sdkconfig (NOT the output ZIP).
- Runs ESP-IDF's reconfigure step and then `ninja -C build -j 2`, matching the **working Windows fix**. Avoid `idf.py -j 2 build`: the user's ESP-IDF 5.5.5 `idf.py` does not accept that flag.
- Verifies every binary and offset specified in the generated flasher_args.json.
- Creates a ZIP with the bootloader, partition table, firmware app, flasher metadata and SHA256 hashes. It does not flash or erase your board.

## Where your built firmware appears

In the extracted repository's core-firmware folder:
- build\opentail_core_alpha.bin — firmware app
- build\bootloader\bootloader.bin — bootloader
- build\partition_table\partition-table.bin — partitions
- build\flasher_args.json — exact flash offsets
- dist\opentail-core-esp32s3-pr12-YYYYMMDD-HHMMSS.zip — full package

After the script prints **SUCCESS**, the firmware ZIP exists on your Windows PC. You may upload it here for inspection. If the build fails, share the console's last 30–50 lines, including the FIRST compiler error.

## Cautions

- The alpha is **BENCH ONLY**. It does not have independently validated road-lighting fallback, hardware redundancy, or WS2815 electrical safety.
- This script does not flash; no board or NVS data is changed. Back up known-working firmware before separate flashing. Do not use erase-flash.
- The generated AP password, if one was needed, will change the device's Wi-Fi access password after later flashing. Keep it private.
- The firmware binary built locally is new and must pass its own build and physical validation. Do not rely on CI success for a different commit as proof of this build.
- Existing PREPARE-AND-BUILD.ps1 and INSTALL-WINDOWS.md describe older v0.1.24 enrollment-only releases; use this guide for the current PR.

Official references: https://dl.espressif.com/dl/esp-idf/ and https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/build-system.html


## Existing pre-GitHub installation: proven steps and fixes

- Your established local workspace is under `C:\Users\astari\Desktop\OpenTail`; the ESP32-S3 was previously on **COM9**, which must still be confirmed when you actually flash later.
- Keep all earlier CORE version folders intact. Download this PR #12 source as a **new, separate folder**, never overwrite the last working release.
- ESP-IDF **v5.5.5-dirty** was already installed and working. Use its ESP-IDF PowerShell shortcut.
- Before building, run `dir CMakeLists.txt, main, sdkconfig.defaults` inside **core-firmware**. A missing `CMakeLists.txt` means you entered the wrong extracted level. Do not run the project build inside `C:\Espressif\frameworks\esp-idf-v5.5.5`.
- If the old custom AP password is not detected, you can explicitly reference the previous release's **local** configuration file without copying the entire old SDK configuration: `.\BUILD-CORE-WINDOWS.ps1 -PreviousSdkconfig "C:\Path\To\Your\Previous\sdkconfig"`. The script reads it locally and does not print the password.
- The previous embedded Studio linker-symbol mismatch is already corrected in PR #12 source (`_binary_studio_htm_gz_start` / `_end`).
- If Windows Ninja gets stuck reconfiguring its manifest, verify that it isn't a source-code compile error; then use the earlier timestamp-normalization recovery selectively rather than erasing the SDK or wiping NVS.
- **Only after a successful build**: independently confirm the board is still on COM9 using `[System.IO.Ports.SerialPort]::GetPortNames()`. Flash later from the project folder using `idf.py -p COM9 -b 460800 flash`, and read logs with `idf.py -p COM9 -b 115200 monitor`. The download/build instructions intentionally stop before this.
