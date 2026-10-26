# OpenTail CORE — Rev A schematic/netlist architecture

## Functional schematic
```
12V VIN (fused at source)
  |-- F_LOGIC --> AITRIP fixed-5V module --> JP5V (OPEN until independently verified) --> ESP32 5V input
  |                                               |--> two SN74AHCT245 data buffers (5V)
  |-- F_LED branches --> separately rated 12V distribution BUS (enclosure) --> LED cable +12V pins
  |                               LED return BUS (enclosure) --> LED cable GND pins
  |-- F_SWITCH --> protected switch power rail --> three TPS4H160-Q1 candidate devices --> 10x +12V switched outputs

12V vehicle trigger inputs --> existing 8ch isolated input MODULE --> ESP32 GPIO4..11
ESP32 12 direct GPIOs --> 2x SN74AHCT245 --> 12x PIX_DOUT signal headers --> LED cable DATA pins
ESP32 GPIO1/2 --> MCP23017 I2C --> 10x control signals --> switched-output drivers
ESP32 GPIO44 --> transistor interlock --> /OE gates for both data buffers
ESP32 GPIO19/20 --> native USB (do not repurpose)
ESP32 onboard RGB GPIO48 is PROVISIONAL (check actual hiBCTR variant)
```

## Target / constraints
- One removable primary ESP32-S3 N16R8 per board; 2 separate prototype boards overall.
- 12 independent pixel signals. Target 6,000 LEDs distributed among outputs; distribution configurable; **not a guaranteed 60–120 fps**. Existing CORE Alpha 0.1.15 two-channel RMT implementation must be redesigned/benchmarked for 12 outputs.
- 8 isolated conventional automotive input channels. Preserve known core v0.1.15 pin assignments.
- 10 positive-switched 12V output channels, not arbitrary high-current load ratings.
- Wi-Fi and Bluetooth are firmware goals; audio I2S microphone planned, CAN module deferred.
- No second ESP32 or autonomous fallback populated on this Rev A. The car's road lamps must be kept electrically independent until a genuine fail-safe architecture is tested.

## Existing board connections
| Signal | Proposed connection | Validation |
|---|---|---|
| ESP32 5V | Verified dev-board 5V input (NOT 3V3) via normally-open selection link | Check exact board's voltage input and USB/external isolation |
| ESP32 GND | Logic GND | Check source/buck/isolated input reference scheme |
| GPIO4..11 | Isolated triggers IN1..8 | From CORE v0.1.15 board_config.h |
| GPIO16,17 | PIXEL DATA 1,2, respectively | From CORE v0.1.15 |
| GPIO12..15,18,21,38..41 | PIXEL DATA 3..12 | Proposed; verify accessible, not onboard RGB, no bus/boot conflict |
| GPIO1,2 | I2C SDA,SCL for MCP23017 | Proposed |
| GPIO44 | LED driver buffer /OE enable through transistor | Proposed; conflicts with UART0 RX |
| GPIO42,43,47 | Optional audio clock, LR clock, data | Proposed; GPIO43 conflicts with UART0 TX; not implemented |
| GPIO48 | Onboard RGB status LED | PROVISIONAL per v0.1.15 source; verify variant |
| GPIO19,20 | USB D-/D+ | reserved, do not route user GPIO |
| GPIO33..37 | OPI PSRAM / Flash | FORBIDDEN on N16R8 |
| GPIO0,3,45,46 | Boot strapping-sensitive | Avoid external driven loads |
| GPIO26..32 | flash / PSRAM bus | FORBIDDEN |

## Signal buffering
Use two 5V SN74AHCT245 bidirectional 8-bit buffers locked to **A to B** direction; driving from 3.3V outputs to 5V data. Enable only once MCU ready. U_BUF1 powers channels 1..8, U_BUF2 channels 9..12; its other 4 channels unused. Verify the exact TI datasheet pin names, package, 0.1uF local ceramic bypass, proper VCC range, logic thresholds, and allowed cable capacitance. Add reviewed small series resistors, ESD treatment and connector strain relief. The Adafruit #6066 module owned by the user covers TWO channels for early bench tests and is NOT 12-channel hardware.

## Power interface and regulator module
- On-hand AITRIP nominal fixed 5V, advertised 5-30V in, up to 3A out. Do not assume demonstrated 3A at enclosure temperature or current module voltage correctness.
- Back-of-module arrow left: right top `IN-`, right bottom `IN+`, left top `OUT-`, left bottom `OUT+` (based on user's earlier photo); this is **not** an official physical module footprint.
- Physically hold regulator on an insulating carrier/clip with strain relief, and wires to 4-pin PCB connector. Module pads are ELECTRICAL pads, not mounting holes.
- `JP_LOGIC_5V_VERIFY` must be left OPEN unless output independently verified. Avoid simultaneous external 5V and USB until the dev-board supply arrangement is validated.
- Keep +12V LED pass-through out of logic PCB. A single input to the enclosure can split to separate fusible distribution; verify connector, fuse, conductor, power injection, and return paths for the actual loads before building.
- Reverse battery, automotive transients/load dump, ESD, thermal, fault isolation still REQUIRE hardware design review. Do not claim a module with a 30V advertised maximum input alone is adequately automotive protected.

## 10 switched 12V high-side output bank
Candidate: 3 x TPS4H160-Q1, each with 4 high-side switch channels. First 10 populated outputs, last 2 disabled. Feed ten control input signals from MCP23017 (16 GPIO expander). At ~2.5A per channel rated output current candidate, this is for properly characterized modest auxiliary loads, **not 80A total LED pass-through**. Use TI datasheet reference circuits, validate current limit resistors, power dissipation, thermal pad land patterns and copper, fault outputs and protected supply; retain 0-current load/drive status by default until firmware configures expander outputs.

## Wiring harness boundary
The carrier PCB's `PIX_DOUT_1..12` connection conveys SIGNAL ONLY. At the enclosure's physical LED output connector, combine:
- `DATA` from buffered signal header
- `+12V` from fused high-current bus / correctly rated branch wire
- `GND` from return bus; bond logic ground at a deliberate low-noise common reference near the power entry as required for single-ended WS2815 signal integrity
Optional `BI` per output gets a RESERVED disconnected testpad only, pending exact strip BI wiring and scheme. Do not connect spare MCU outputs to BI without protocol review.

## Enclosure target
152 x 64 x 38 mm EXTERNAL nominal. The fitted usable PCB dimensions cannot be assumed. Preliminary PCB routing will wait until board height, ESP header width, connector end-face arrangement and cooling clearances are measured. Keep Wi-Fi antenna edge free of copper/metal and large cable bundles.

## Priority milestones
1. Schematic: exact dev board/mezzanine geometry known, all logic nets completed, no incompatible GPIO assignments, only separately approved +12V distribution.
2. PCB: placement with USB access / antenna keepout, proper input/output connector footprints, fuse and harness mating, DRC/ERC clean with all exceptions documented.
3. Bench: 2 pixel channels current setup; then 12 channel physical driver and independent configuration; no road use until external fallback/failure behavior is separately validated.
