# OpenTail GPIO16 bare-bones LED test (80% brightness)

This is a **temporary diagnostic firmware**, separate from the normal OpenTail application. It uses ESP-IDF **5.5.5** on the single ESP32-S3 N16R8, GPIO16 (connected Output 1), original #6066 shifter and WS2815 chain. GPIO17 / Output 2 is not connected and is not tested.

## Pattern and scope

The code sends GRB red, green, blue and white, **204/255 = 80% per illuminated channel**, to **only the FIRST EIGHT pixels**. It repeats that four-color sequence three times, two seconds per color, and then sends OFF. The deliberately small eight-pixel footprint avoids lighting the entire chain at 80% while isolating the physical data path.

It uses native **RMT non-DMA** with 10 MHz timing, WS2815 bit0 300/900ns and bit1 900/300ns, with low idle between frames for reset/latch. It has no Panel v4, zones, NVS calls, Bluetooth, Wi-Fi, app, effects, or vehicle inputs. Do not run on a vehicle.

## Windows COM9 runner

1. Open the GitHub Actions run for **GPIO16 bare-bones RMT smoke diagnostic** on branch `diagnostic/gpio16-rmt-smoke-20261115`. Download its **opentail-gpio16-smoke-80pct** artifact after build success.
2. Extract the ZIP. Close any COM9 monitor and turn **OFF** the separate 12V LED supply during flashing; keep wiring unchanged.
3. Double-click **RUN-GPIO16-SMOKE.bat**, which runs the included **Flash-GPIO16-Smoke.ps1**.
4. The runner verifies the exact-source manifest, all image SHA-256 values, flash addresses, ESP-IDF build package and local esptool installation before asking you to type `FLASH COM9`. If esptool is missing, install it with `py -3 -m pip install esptool`.
5. The runner writes bootloader at 0x0, partition table at 0x8000, OTA-data at 0xf000 and app at 0x20000. **NVS at 0x9000 is not erased or written.** The OTA selection is reinitialized to boot the temporary image. This temporarily replaces the production firmware.
6. Once the controller reboots normally, turn the separate 12V LED supply on. Watch the first eight LEDs cycle red / green / blue / white three times. The shifter DAT NeoPixel may also respond when powered. Enter YES / NO / PARTIAL in the runner after the test, and share **GPIO16_SMOKE_RESULT.txt**.
7. **Restore normal OpenTail firmware** afterward using its previously verified COM9 EasyFlash package, not a factory reset. Keep Android and firmware from older revisions from saving the current layout during this test.

If the lights illuminate, the minimal GPIO16 path is functional and the integrated firmware requires focused debugging. If dark, the test isolates that the problem exists **outside Panel v4**, but software still cannot distinguish absent electrical signaling from shifter/power without a physical measurement. GitHub CI is not physical optical proof.

## Build and boundaries

The file `.github/workflows/gpio16-smoke.yml` runs ESP-IDF 5.5.5 in GitHub CI, verifies the isolated 8-pixel/80% contract and the exact production partition offsets, and packages the actual compiled images with the Windows runner. No firmware is flashed by CI. Production Firmware PR #127, Android PR #111, and their sources are not modified by this standalone diagnostic branch.
