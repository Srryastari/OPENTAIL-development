# OpenTail CORE — Flux.ai Rev A schematic starting package
2026-10-24 | Design planning package, not manufactured PCB / not a validated schematic

## What this is
This package captures a proposed **single ESP32-S3 N16R8 socketed carrier/controller architecture** for:
- 12 independently buffered 800kHz addressable LED data outputs, design goal 6,000 total LEDs
- 8 isolated conventional 12V vehicle signal inputs (external existing 8-channel opto module)
- 10 programmable high-side switched 12V outputs (candidate 3 × TPS4H160-Q1, 2 spare channels)
- existing AITRIP 12V-to-fixed-5V buck module, physically secured as a module; not an assumed verified 5V source
- removable ESP32 dev board and serviceable plastic enclosure
- fused LED power distribution physically **inside the enclosure**, but routed on **properly sized separate bus/harness**, not through normal thin PCB traces
- optional WS2815 backup data breakout(s), pending correct protocol/wiring confirmation
- reserved audio-reactive pins, CAN deliberately deferred.

## Grounded baseline
Based on OpenTail CORE Alpha v0.1.15 source supplied by the user:
- tested/legacy *firmware* trigger inputs: GPIO4..11 (active-low configuration presently enabled)
- present LED data outputs: GPIO16 and GPIO17, two 30-pixel RMT channels
- status LED: provisional GPIO48
- current output manager: two outputs and five zones per output; 12-output driver NOT built
- User-reported on-bench working WLED left main housing (522 pixels); this does not validate CORE physical output.

## Files
- `FLUX_COPILOT_PROMPT.md` — paste into Flux project's AI chat in small sections
- `DESIGN_SPEC.md` — block-by-block proposed schematic and architecture, power and safety constraints
- `GPIO_ALLOCATION.csv` — proposed pins, risks, baseline preservation
- `NET_PLAN.csv` — conceptual netlist for schematic implementation; NOT an importable Flux netlist
- `BOM_CANDIDATES.csv` — on-hand items vs prospective ICs
- `REVIEW_CHECKLIST.md` — mandatory schematic/electrical/mechanical checks

## Important unverified inputs
1. Exact hiBCTR  ESP32-S3 N16R8 header pitch, row spacing, hole pattern, orientation, 5V input power-path, and available GPIO pins.
2. Actual 8-channel optocoupler module pin labels, output voltage, polarity under physical signals, and mounting dimensions.
3. Exact AITRIP board current derating and operating voltage on this specific module; product listing indicates nominal 5V, but unit output is NOT measured.
4. Pixel output driver method for 12 channels. WLED documents ESP32-S3 parallel I2S capability, but CORE Alpha's present two-channel RMT driver cannot simply be multiplied to 12.
5. 12V power current distribution, fused branches, thermal behavior, terminal/contact ratings, strain relief.

## Critical safety boundary
This is a BENCH / DESIGN schematic starting package only. Not automotive qualified, fail-safe, or safe as the sole road-lighting controller. The proposed single-ESP32 revision does NOT meet the previously specified independent processor backup/fallback requirement. Keep the car's road lights independent. There is no USB/external 5V simultaneous power-path guarantee. With no voltage tester available, do NOT connect a new regulator's output to the ESP32 until an independent verified-voltage check is performed. USB power may be used for isolated controller bench development.

## Primary source links
- Flux editor + AI: https://docs.flux.ai/reference/copilot
- Flux project workflow: https://docs.flux.ai/Introduction/flux-walkthrough-project
- ESP32-S3 memory-pin cautions: https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32s3/schematic-checklist.html
- TI 74AHCT245: https://www.ti.com/product/SN74AHCT245
- Microchip MCP23017: https://www.microchip.com/en-us/product/mcp23017
- TI TPS4H160-Q1: https://www.ti.com/product/TPS4H160-Q1
- WLED multi-strip: https://kno.wled.ge/features/multi-strip/
