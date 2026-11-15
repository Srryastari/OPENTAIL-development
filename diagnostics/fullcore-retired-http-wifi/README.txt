OpenTail FULL-CORE Diagnostic — RETIRE HTTP runtime / Wi-Fi memory
(physical bench validation PENDING, not a production release)

BASE: owner's working Output 1 non-DMA full-CORE test
      SHA da4f6a7005ad5d8109be1a4bc066fbe20b181597
TARGET: this artifact's exact Git SHA; v0.1.33-alpha build metadata is in release_manifest.json

CONFIRMED STARTING POINT:
  - GPIO16 / Output 1 physically lights the WS2815 strip with RMT non-DMA,
    64 symbols. BLE connects and Output 1 Pixel Probe works.
  - Owner serial shows BLE advertising, but secondary Wi-Fi AP initialization
    fails: internal free 21975 < REQUIRED 24576, causing FAST RED fault LED.
  - HTTP diagnostic server then fails because Wi-Fi did not start.
  - Pixel-shifter onboard LED remained OFF in the earlier successful test.

EXPERIMENTAL, NOT A PRODUCTION PATCH:
  1. HTTP diagnostic server is no longer STARTED or BOUND; no HTTP listener,
     dashboard or HTTP OTA upload is exposed. Legacy handler source remains
     temporarily compiled to minimize change scope; scheduled for removal
     only after the owner verifies behavior.
  2. BLE + completed RMT frames + watchdog remain OTA boot-validity gates;
     obsolete HTTP readiness no longer blocks rollback confirmation.
  3. Wi-Fi static RX buffers 6->4, AMPDU RX BA window 6->4, management short
     buffers 8->6, matching ESP-IDF permitted low-memory configuration.
     Wi-Fi preflight thresholds are UNCHANGED and still fail closed.
  4. Output 1 non-DMA/64, Output 2 non-DMA/64, timing, Panel v4,
     effects, saves, triggers, BLE owner flow, brightness cap and Android
     are unchanged.

IMPORTANT LIMITATION: Wi-Fi AP is retained, but no Wi-Fi control transport
currently exists without HTTP. It is NOT yet a functional fallback for BLE.
Future secondary transport requires its own approved secure protocol.
USB COM9 remains the diagnostics/flash path; HTTP OTA upload unavailable.

COM9 TEST PROCEDURE (ONLY after exact-SHA CI PASS):
  1. Download the exact-SHA PASS EasyFlash ZIP. Extract to a new folder.
     Keep a copy of the currently PHYSICALLY WORKING non-DMA build to restore.
  2. Close COM9 serial monitor, turn OFF 12V LED power while flashing; keep
     all wiring untouched.
  3. Run 01_FLASH_COM9.bat. It verifies SHA256 and asks before flashing.
     DO NOT run factory reset or erase. NVS at 0x9000 is preserved; OTA-data
     at 0xf000 is reinitialized by the standard four-image USB flashing.
  4. After reboot, turn 12V LED power on, connect existing Android, and repeat
     SAME Output 1 Pixel Probe. Confirm the strip remains physically lit.
  5. Run 02_CAPTURE_BOOT_LOG_COM9.bat if included, or capture serial at
     COM9/115200 while pressing EN/RESET. Observe:
      - RMT_OUTPUT_READY lane=1 gpio=16 dma=0 symbols=64
      - BLE advertising ACTIVE
      - Wi-Fi AP pre-start internal/DMA heap free values
      - Secondary Wi-Fi AP started or preflight failure
      - HTTP diagnostic server DISABLED (obsolete)
      - Whether LED indicator is still fast red
  6. Report: STRIPS (LIT/DARK/PARTIAL), BLE (CONNECTED/FAILED),
     RED STATUS (YES/NO), Wi-Fi AP (STARTED/FAILED), and relevant logs.
  7. If regression occurs, restore the known-good full-CORE non-DMA package.

NO hardware acceptance is asserted by GitHub CI. Do not merge PR #127.
