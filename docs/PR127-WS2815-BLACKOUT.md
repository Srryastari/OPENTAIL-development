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
