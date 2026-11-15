OPENTAIL FULL-CORE / OUTPUT 1 NON-DMA ISOLATION — NOT PRODUCTION

Source branch: diagnostic/full-core-o1-nondma-20261115
Base Firmware PR #127 SHA: f13379ca5498195d076c7ce4e43f5fe24d194527
Diagnostic change: Output 1 GPIO16 RMT DMA=0, symbols=64.
Output 2 GPIO17 was already DMA=0, symbols=64, and has no physical strip connected.
All other production firmware logic, Panel v4 behavior, timing, pixel count fallback,
effects, LED color ordering, brightness ceiling, BLE, Wi-Fi, NVS, Android are UNCHANGED.

BACKGROUND
Physical bench test PASS using the SEPARATE bare-bones GPIO16 test:
8 WS2815 LEDs correctly showed R, G, B, W at 80% brightness.
The pixel-shifter board's onboard LED stayed OFF during that successful test.
This full-firmware test does NOT raise brightness to 80%, and does NOT force
the LEDs to light at boot. Instead it reuses the SAME Pixel Probe as before,
so the Output 1 RMT driver choice is the only material variable.

TEST STEPS — Windows / COM9
1. Download the exact-SHA PASS artifact named FULLCORE-o1-NODMA from GitHub Actions.
   Extract it to a NEW folder (not over any previous EasyFlash).
2. Keep the currently working hardware wiring; close any COM9 serial monitor.
   Save a copy of the previous working/installed firmware artifact for restoration.
3. Double-click 01_FLASH_COM9.bat in the extracted ZIP. It verifies SHA-256 hashes
   and asks for confirmation before writing anything.
   It writes bootloader, partition, OTA init at 0xF000, app at 0x20000;
   it does NOT erase NVS at 0x9000. Avoid factory reset.
4. Once booted, connect with the UNCHANGED Android app and run the SAME
   Output 1 Pixel Probe that previously reported RMT_DONE but stayed dark.
   Do NOT Save/Reload/reconfigure layouts simply to make this test pass.
5. Observe only the attached WS2815 LEDs: do they light physically?
   Record YES / NO / PARTIAL, and serial boot/Pixel Probe logs if convenient.
   Ignore the shifter onboard LED when scoring, since it stayed OFF even
   during the successfully illuminated eight-pixel bare-bones test.
6. If test fails or causes a new issue, restore your previous verified
   production firmware package; do not erase NVS or merge this diagnostic branch.

This build is bench-only; do not run on a road vehicle.
Software CI PASS cannot certify physical optical output.
