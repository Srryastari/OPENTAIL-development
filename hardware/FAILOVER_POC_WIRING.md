# OpenTail — Rev B two-output failover POC build sheet

**Status:** scoped electrical **proof-of-concept design proposal**, not a completed/fabrication-ready schematic. The previous 80-net matrix and universal 12-output targets remain the full controller design. This milestone makes **PIX1/PIX2 hardware bus ownership** testable without pretending that a 12-output board or a backup firmware image exists.

**Frozen software reference:** PR #51 `bc92d886560dab88c6a53316d8e4cddc5d2fb8ef`. No change to that code/image. `OT_CHAIN_COUNT=2`, and the separately configured pixel capacity defaults to 2048 per output. The existing Primary supports a left 623-pixel chain and right 623-pixel chain **in software capacity**, not yet proven with installed strips.

## Immediate owned-hardware path (before any new circuit)

**User directive: use already owned hardware; custom PCB procurement on hold.** Start with 2 of the 6 hiBCTR ESP32-S3 boards, 2 of the 8-channel PC817 modules, the received Adafruit #6066 2-channel shifter, the fused bench supply, WS2815 test strips, existing suitable connectors and wiring. Two separate MCU tests can be performed with the same #6066 by **powering down completely and moving the two data-input wires**, never joining two MCU/shifter outputs or switching them while powered. This proves only A/B standalone rendering, not automatic electrical failover. For scope, inventory limitations and exact no-go conditions, use [`ON_HAND_FIRST_BENCH_PLAN.md`](./ON_HAND_FIRST_BENCH_PLAN.md) and the inventory-reconciled `FAILOVER_POC_BOM.csv`.

**The component shortlist and schematic concept below are explicitly later-stage unowned engineering candidates**, not a request to buy parts or a claim they are available on the bench. No custom PCB will be ordered without physical A/B evidence and circuit review. The GLEDOPTO 4D-EXMU is a separate WLED baseline only, not a two-processor data selector. Keep full vehicle lighting independent of bench prototypes.

Adafruit #6066 actual **owned two-channel bench hookup** is documented in [`ON_HAND_BENCH_WIRING.md`](./ON_HAND_BENCH_WIRING.md). Its two level-shifted paths are `DAT → D5` and `CLK → C5`; clockless WS2815 uses the latter **as a second DATA signal**, subject to testing. `!D5` is inverted and must not be used. Neither this shifter nor its terminals are an automatic source selector or a high-current LED distribution connector. **No WAGO connectors are owned or assumed.**

## POC topology: exclusive 5 V bus drivers for two outputs

```
             PRIMARY ESP32-S3                             BACKUP ESP32-S3
             GPIO16 / GPIO17                              GPIO16 / GPIO17 (proposed)
                    |  |                                         |  |
                    v  v                                         v  v
              U_P SN74LV4T125-Q1                         U_B SN74LV4T125-Q1
              VCC = P_5V                                  VCC = B_5V
              A1 A2 -> Y1 Y2                              A1 A2 -> Y1 Y2
               |  |      (Y1,Y2 default Hi-Z)              |  |       (Y1,Y2 default Hi-Z)
               +--+---- series parts TBD -------------------+--+-------> PIX1, PIX2
                               (never tie active drivers)
             /OE_P1,/OE_P2                              /OE_B1,/OE_B2
                 ^  ^                                         ^  ^
       passive pullups to P_5V                         passive pullups to B_5V
                 |                                              |
    SUPERVISOR-controlled isolated                      SUPERVISOR-controlled isolated
       pull-down transistors                               pull-down transistors
                 ^                                              ^
              GRANT_P                                        GRANT_B
                 \                                             /
            [EXTERNAL SUPERVISOR + two window watchdogs + rail monitors]
                   cannot grant both; BREAK -> dead interval -> MAKE

  TRIG1..8 -> separately protected P and B opto/conditioning banks
  Ctrl VIN protected -> independent P regulator, B regulator, supervisor supply
  WS2815 +12V separately fused/injected; common signal reference at power entry
```

### Tentatively selected devices (verify exact ordered suffix, ratings and package pins before ERC)

| Function | Candidate | Quantity, POC / per 12-output final box | Why / verification |
|---|---|---|---|
| 3.3->5 V signal buffer with individually controlled Hi-Z | **TI SN74LV4T125-Q1** in TSSOP-14 | 2 / 6, three four-lane chips for each MCU | 3.3->5-V translation and AEC-Q100. **Verify Ioff in the exact Q1 datasheet and unpowered IO isolation** before electrically tying buffer outputs. 2 used channels per chip initially, others disabled |
| Independent window watchdog | **TI TPS3430-Q1** | 2 / 2 | One per MCU, fed from qualified progress signal only; compare watchdog window with actual longest WS2815 frame and boot/OTA states |
| Rail-valid monitor | **TI TPS3704-Q1 family** | TBD / TBD | Select exact orderable variant and thresholds only after independently measured P, B, supervisor and LED rails |
| Front-end reverse-polarity/OV cut-off candidate | **TI LM7480-Q1** with back-to-back MOSFETs and appropriately rated TVS | TBD / TBD | Controller front end; external FETs and TVS need complete ISO pulse design, not direct schematic assumption |
| 5 V logic regulation across 5–24 V target | **TI LM51772-Q1** external-switch 4-FET buck-boost, **shortlist only** | 2 / 2 plus independent supervisor regulator TBD | Hardware-CFG selectable; high external BOM and FET/EMI design overhead. **Not frozen** until smaller regulator alternative evaluated and startup works at 5V, 12V, 24V without processor intervention |
| Baseline development comparison only | Existing Adafruit #6066 two-channel shifter | available / not a final failover component | Can validate the existing 2-output strip electrical path, but **does not** supply two independent gated driver banks |

**Design correction to Rev A:** A pair of SN74AHCT245 chips is only 12 channels for **one** processor and its unpowered-output isolation has not been demonstrated. POC evaluates an explicitly high-Z, translated driver topology instead. Do not assume passive tying is safe merely because there are OE pins.

### Candidate net and resistor-level decisions for the first test PCB / carrier

| Test net | Electrical connection | Evidence required before physical assembly |
|---|---|---|
| `P_DATA1`, `P_DATA2` | Primary GPIO16/17 to U_P A1/A2 | Actual S3 board pinout; 3.3 V signal; ensure no boot strap |
| `B_DATA1`, `B_DATA2` | Backup GPIO16/17 to U_B A1/A2 | Backup essential renderer must send compatible 800 kbit/s WS2815 timing without primary |
| `P_OE_N`, `B_OE_N` | Separate per-rail pull-up to 5 V; independent supervisor-switched **open-drain pull-down** requests; all unused OE pins held disabled | Reset/brownout/power-off HIGH at each chip /OE. No cross-feeding 5 V into 3.3 V arbiter GPIO. Pull-up and FET values pending datasheet and leakage tests |
| `PIX1_BUS`, `PIX2_BUS` | U_P Y1/Y2 and U_B Y1/Y2 to one *shared* data bus per lane **only after** independent Hi-Z, Ioff and interlock tests; provisional per-branch series pads and common output ESD/series pad | Scope both enabled and one/both unpowered. **No permanent short of two active push-pull drivers.** Resistor values intentionally TBD pending cable and timing measurements |
| `P_REG_OK`, `B_REG_OK`, `SUP_REG_OK` | Power-good and UV/OV monitor signals into supervisor sequencer | Must reflect actual upstream/regulator faults, not soft GPIO claims |
| `P_FRAME_TICK`, `B_FRAME_TICK` | Proposed qualified frame-completion output from each MCU to **separate** window watchdog | Current PR #51 does **not** export an accepted failover handshake; firmware owner must design a future candidate. Never fake watchdog via independent periodic task |
| `P_VALID`, `B_VALID` | Latched validated essential profile state; B validation is wholly independent | Stale/pending/corrupt config cannot arm backup |
| `CTRL_VIN` and `LED_12V` | Separate input connectors; controller target 5–24 V, LED rail regulated to strip rating | No 24 V on strip; no unverified 5 V regulator on either S3; no full strip current through logic PCB |

**External hardware ownership sequence:** (1) both buffer banks OE=HIGH on POR, brownout and unknown state; (2) supervisor selects a qualified owner; (3) any change forces both banks disabled *before* any new owner is enabled; (4) preserve a **measured** non-overlap interval and WS2815 reset gap; (5) enable only the selected bank after qualifications remain valid. Primary gets preference after an independently stable recovery period; no automatic handback oscillation. Use a dedicated hardware sequencer/latch with explicit initialization: **one combinational NOT gate or independent firmware enable wires are not sufficient**. The included software reference model under `hardware/tests` proves only truth-table intent.

### POC execution in dependency order

**P0 — low-voltage isolated lab test**: Use two 3.3 V logic generators (or two USB-powered dev boards whose USB/backfeed is independently verified), two *separately current-limited verified* 5 V rails, U_P/U_B candidates, and a scope/logic analyzer. **LED +12 V disconnected**. Measure both /OE defaults, Hi-Z/current leakage, VCC-off IOFF and bus contention over all P/B rail and selection combinations. STOP if overlapping push-pull outputs, backfeeding or unexpected data pin voltage.

**P1 — interlocked ownership**: Build independent rail monitors and a deterministic break-before-make supervisor circuit. Inject P reset, P frame stall, P data high/low, B reset and both absent. Capture raw GRANT_P/GRANT_B, P/B /OE, output DI and P/B rails under all states; measure *real* deadtime, not assumed millisecond numbers. Firmware owner must add a qualified runtime interface in a new SHA before hardware watchdog validation can be completed.

**P2 — qualified two-strip pixel test**: Drive two short WS2815 strips on independently fused 12 V supply with each processor separately, then test automatic backup ownership with physical input emulators/isolated inputs. After short strips pass, test 623 LEFT and 623 RIGHT at a deliberately capped supply/current; physical color order can differ by strip, app profile owns the map. Full 1,246 worst-case all-white could exceed the existing **20 A** lab source; never treat cap 32/255 as a measured current limit.

**P3 — protected vehicle-range logic power**: Replace the bench-only supplies with reviewed, independent, automotive-protected 5–24 V startup-capable regulator chains. Verify 5/12/24V operation, rail faults, transient, unpowered OE, USB sequencing, wake/power-off current, physical 8 independent input branches, selected MCU reset and thermal tests.

**P4 — expansion**: Replicate the *validated per-lane ownership topology* across PIX3..PIX12 and prove 12 parallel output timing with **new separately approved firmware**. Add 10 x protected high-side channels with per-channel fault/default-off tests and a physically measured 6,000-pixel power architecture. No 12-output firmware feature is claimed in frozen PR #51.

## Hard blockers

- The exact hiBCTR S3 module footprint, rail/current/backfeed behavior and pinout remain unverified.
- No schematic ERC, PCB DRC, oscilloscope captures, built watchdog sequencer, high-current harness or independent backup firmware exists in the repo.
- Two MCUs sharing the *same* LED strip and regulated LED supply do not survive shared first-pixel/power/ground/mux faults. Keep factory road-lighting operational; implement and prove an **independent luminous path** before replacing it.
- Neither code-model verification nor a new parts shortlist proves this controller is safe for road lighting.

## Part references

- SN74LV4T125-Q1: https://www.ti.com/product/SN74LV4T125-Q1
- TPS3430-Q1: https://www.ti.com/product/TPS3430-Q1
- TPS3704-Q1: https://www.ti.com/product/TPS3704-Q1
- LM7480-Q1: https://www.ti.com/product/LM7480-Q1
- LM51772-Q1: https://www.ti.com/product/LM51772-Q1
