# PR #127 WS2815 blackout correction

Owner evidence before this change: BLE authorization, Pixel Probe, nonblack
software frame and RMT completion passed; actual WS2815 illumination and the
Adafruit #6066 DAT pixel failed. Replacement shifter/strips/harness were separately
verified by the owner with BlueGhozt. These hardware tests are not repeated here.

## Stage A: timing correction and bounded routing evidence

Starting GitHub HEAD: `17a2762064fe1a96f5adc4477d665449c1c7990e`.
The exact dependency remains `espressif/led_strip==3.1.1`.

The generic WS2812 profile in this driver uses 300/900 ns for zero, 900/600 ns for
one, and 280 us reset. CORE now selects `LED_MODEL_CUSTOM` with WS2815 timing:

| Symbol | High / low | Encoded at 10 MHz |
|---|---|---|
| bit0 | 300 / 900 ns | level1:3, level0:9 |
| bit1 | 900 / 300 ns | level1:9, level0:3 |
| reset | 320 us continuously low | level0:1600, level0:1600 |

**Every Stage A test retains the locked baseline:** GPIO16 DMA enabled / 1024
symbols; GPIO17 DMA disabled / 64 symbols; both 10 MHz. No all-DMA or all-non-DMA
experiment is included. Technical fallback capacity remains 2048 pixels per
output; fresh saved geometry remains 0 active LEDs, 0 panels, 0 zones.

On the render task, the first nonblack authorized Pixel Probe for each driver
lifetime logs `PIXEL_PROBE_ROUTE` and calls `gpio_dump_io_configuration(stdout,
(1ULL << 16) | (1ULL << 17))` immediately before submission. A successful resize
re-arms this one-shot. The log associates lane, GPIO, live strip handle, capacity,
byte count and DMA settings. It checks output enable, IOMUX function, inversion,
open-drain state, RMT signal range 81..84 and equality to the signal captured after
that handle's initialization. The dump reports both GPIOs' matrix ownership.
No GPIO is changed by the diagnostic. A route mismatch is reported, not hidden
by reconfiguring the pins. No diagnostic runs in an ISR or on every frame.

The actual upstream source is compiled by `tests/test_ws2815_driver.py` against
mock RMT peripherals, with SHA-256 guards on the 3.1.1 driver, encoder and type
header. CI repeats it against the component-manager-resolved sources, rather
than substituting a model of the driver. It verifies exact symbols, two separate
6144-byte buffers, nonzero white after the production brightness limiter and all
six color permutations, correct `pixel_buf` pointer/byte count at transmit,
write/delete rejection while in flight, completion errors, and teardown/recreate
with a different capacity. Existing BRG driver storage plus CORE's GRB permutation
is preserved; color calibration and brightness ceiling 32 are unchanged.

Driver source findings: `led_strip_set_pixel()` writes `pixel_buf[index*3 +
component_position]`; `rmt_transmit()` receives that same trailing buffer and
`strip_len * bytes_per_pixel`; `refresh_async_done()` waits for completion,
disables the channel, then releases the buffer state. CORE waits for both lanes
before resizing or rewriting. No successful completion is counted on an error.

Stage A deliberately leaves pixel/encoder allocation and RMT cache-safe settings
unchanged, so timing can be tested independently. It does not prove optical
illumination or establish DMA as the cause of the blackout.

## Owner-controlled check

After separately choosing and installing a package, reconnect the already
authorized phone. Preserve saved output geometry. With physical inputs inactive,
request one white Pixel Probe on GPIO16/output 1, then GPIO17/output 2 using the
existing UI. Observe the actual strip and DAT pixel during each bounded probe.
Record which package/SHA was used and whether either LED physically illuminated.
Serial route logs are available using existing COM9 tooling; no new equipment
is required. Driver completion alone is not optical PASS.

No automatic flash, NVS erase, wiring change or physical acceptance is performed.
If both remain dark, any GPIO16-DMA-off diagnostic is a **separate** owner-controlled
A/B with GPIO17 still non-DMA and every other RMT setting identical. Restore the
approved asymmetric baseline unless evidence supports a change.

## Stage B: separate internal-SRAM and cache-off safety change

The timing-only verification candidate is
`884a144d28359e0a36a7f064fa4a9face72936c9` (the timing implementation is
`6856a69d8c44f0cb232362dce3be89d04b62d9d2`; the follow-ups repair the test harness and accept standard
YAML quoting without changing firmware). Keep its package for the first optical test.

Stage B retains **exactly the same timing, GPIO16 DMA/1024, GPIO17 non-DMA/64,
10 MHz, calibration, brightness and geometry**. It changes allocation and cache
safety in a separate commit:

- `cmake/led_strip_internal.cmake` verifies hashes of the exact 3.1.1 RMT driver
  and composite encoder, then adds a forced allocator include to **only those two
  translation units**. The managed dependency files remain unchanged. A dependency
  change stops the build and requires another audit.
- The existing `calloc` calls become `heap_caps_calloc(count, size,
  MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)` through the scoped header. No external-RAM
  fallback is allowed; allocation failure returns the driver's normal no-memory
  error. Runtime allocation logs confirm the entire returned block is internal.
- Both 2048-pixel RGB buffers consume **12,288 bytes combined**, plus strip-object
  and encoder overhead. The GPIO16 GDMA symbol buffer remains a **separate** 4096
  byte allocation; DMA itself never established pixel-buffer residency.
- The composite encoder and its reset symbol are internal. IDF's child bytes/copy
  encoders use `rmt_alloc_encoder_mem` with internal capabilities. Channel state,
  queues, descriptors and DMA buffers use IDF's internal allocation paths. The
  GPIO16 GDMA ISR is explicitly registered cache-safe by IDF when TX cache safety
  is selected. No application RMT completion callback is installed.
- `CONFIG_RMT_TX_ISR_CACHE_SAFE=y` selects IDF's IRAM handler/encoder and internal
  object requirements. `CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=1024` is unchanged.
- `check_rmt_cache_safe.py` checks the generated config and compiler commands,
  verifies that the allocator include reached exactly the two audited files,
  checks IDF allocation/ISR source paths, and inspects the linked ISR/encoder
  symbols and direct call targets for IRAM/ROM placement. The disassembly and
  symbol table are retained under `rmt-evidence/` in the firmware artifact.

The host driver test repeats timing, buffer and resize assertions with the actual
scoped allocator header, checks internal capability selection, and injects a
no-memory failure. This is separate from Stage A's ordinary-allocation test.
Physical cache-disabled NVS/BLE/Wi-Fi stress remains unobserved; the build audit
does not substitute for it. No DMA-mode A/B has been performed.

## Exact Windows COM9 package procedure (owner controlled)

1. Download the **Stage A EasyFlash artifact** linked in PR #127 and extract its
   contents to `C:\OpenTail\WS2815-A\`. The files below must sit directly in that
   directory. Check `release_manifest.json` has Git SHA
   `884a144d28359e0a36a7f064fa4a9face72936c9` and `erase_nvs: false`.
2. Close any existing COM9 monitor. In PowerShell:

   ```powershell
   Set-Location 'C:\OpenTail\WS2815-A'
   py -3 verify_sha256.py
   .\01_FLASH_COM9.bat
   ```

   The script rechecks checksums and requires typing `FLASH`. If Python tooling
   is absent, the included `INSTALL_ESPTOOL.bat` installs it. No controller button
   is part of this procedure; the existing COM9 serial bootloader control is used.
3. Reconnect the phone and perform the two white Pixel Probes above. This is the
   timing-only optical check. Do not claim success from `RMT_DONE` alone.
4. For the separate cache-safety stage, extract its EasyFlash artifact to
   `C:\OpenTail\WS2815-B\`, verify the final SHA in the PR report, then use the same
   commands from that directory. Record results separately. Both packages retain
   the approved asymmetric DMA baseline.

The normal flash script writes these exact package paths/offsets:

| Offset | Relative file path |
|---|---|
| `0x0` | `bootloader\bootloader.bin` |
| `0x8000` | `partition_table\partition-table.bin` |
| `0xf000` | `ota_data_initial.bin` |
| `0x20000` | `opentail_core_alpha.bin` |

Live NVS at `0x9000` is excluded. Use **only `01_FLASH_COM9.bat`**, not the legacy
factory-reset utility also present in the existing package. Never use `erase_flash`.
A monitor can be opened afterward using existing pyserial, for example
`py -3 -m serial.tools.miniterm COM9 115200 --raw --dtr 0 --rts 0`; trigger the probe
from the phone while it is open. The one-shot dump can show actual routing only
when run on the controller; no live routing capture was fabricated here.
