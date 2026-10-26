# Rev A staged review and acceptance checklist

## Schematic review — do not send to fab before every blocking item passes
- [ ] Actual hiBCTR ESP32-S3 N16R8 development board header count, row spacing, pinout, height and USB location obtained from actual board measurements; socket footprint verified.
- [ ] ESP32 5V input AND backfeed behavior confirmed by board documentation; external buck output independently checked before closure of JP_LOGIC_5V_VERIFY (no meter available to user: qualified helper or independently verified 5V source may be needed).
- [ ] GPIO4..11 correspond to isolated 3.3V logic input signals. No unisolated vehicle 12V on ESP32.
- [ ] GPIO16 and GPIO17 stay PIX1/PIX2 for v0.1.15 compatibility.
- [ ] No GPIO26..37 allocated; check GPIO38 vs onboard RGB and GPIO43/44 UART0 conflict.
- [ ] Exact 74AHCT245 datasheet, package direction, power sequencing, default OE and unused pins verified.
- [ ] Fused / protected branches with correct real fuse values, connectors, wire ampacity and electrical isolation.
- [ ] No full LED current through carrier PCB: separate fused bus, harness return and protective ground reference documented.
- [ ] 10 switched outputs genuinely high side (+12V), not low side (GND switched); smart-switch datasheet thermal/current/fault connections reviewed.
- [ ] ESP32 native USB GPIO19/20 available, antenna keep-out clear, GPIO boot straps unaffected.
- [ ] Audio pins GPIO42,43,47 reserved, check native UART0 and I2S driver feasibility.
- [ ] Backup-data BI pads left disconnected pending exact WS2815 and first-pixel BI wiring confirmation.
- [ ] ERC warnings individually resolved/documented, no missing power pins or unintended mixed-ground short.
- [ ] Enclosure internal dimensions physically measured; external nominal 152×64×38mm is insufficient for placement guarantee.

## Bench validation plan
- [ ] With MCU USB powered and LED power separate, check 8 trigger inputs through isolated module; brake+both hazards arbitration result reproducible.
- [ ] Pixel 1 and 2 physically control short WS2815 strips at low brightness; verify BRG and segment boundaries.
- [ ] Implement parallel 12-output driver (existing two-RMT-output code does NOT implement 12).
- [ ] Demonstrate 12 distinct data streams with known test patterns; record each output channel.
- [ ] Test combined 522, 1044 and 1246 pixels as assemblies become available; document supply/current limit and power injections.
- [ ] Validate separate configured per-channel LED counts and maximum total 6,000 with measured frame duration and MCU free heap/PSRAM.
- [ ] Demonstrate safe switch output driver control on limited loads and verify all outputs default OFF when device is unpowered/resetting.
- [ ] Thermal check at realistic enclosure ambient and harness loads; verify no hot connector or PCB trace.
- [ ] Before any road-lighting substitution, develop and test independent road-light fallback and automotive power protection separately; Rev A is not road-ready.

## Release gates
**G1**: Schematic + verified footprints/pinouts + power boundary correct
**G2**: ERC + placement + DRC + manufactured assembly review
**G3**: Low-current two-output physical test
**G4**: Full 12 output physical test
**G5**: Power, thermal, electrical and fail-safety vehicle-specific certification strategy (not included in this design package)
