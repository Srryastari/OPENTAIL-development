# OpenTail — **USE OWNED PARTS FIRST** bench hardware plan

**Priority override (2026-10-30):** User has already obtained physical OpenTail bench hardware. Do **not** turn POC into a new parts-shopping exercise or a custom PCB order. Prototype the two independent MCU render/trigger paths with on-hand hardware first; isolate the missing truly automatic takeover circuit as a separate, not-owned gate. One compact enclosure per car remains the final mechanical requirement. Frozen Software PR #51: `bc92d886560dab88c6a53316d8e4cddc5d2fb8ef`.

## Verified user-reported inventory vs assumptions

| Part | User-reported status | Immediate role | What it cannot prove |
|---|---|---|---|
| **6 × hiBCTR ESP32-S3 N16R8** | Owned | 2 for each car (primary + backup), 2 spares; begin with 2 on one bench | Not electrically independent without separate power, triggers and output selection |
| **2 × 8-channel PC817 optocoupler boards** | Owned | Two *separate* sampling modules for one dual-MCU bench test, one per MCU | Two entire finished cars need **four** independent 8-channel banks; 2 total is not enough for full dual-bank redundancy on two cars |
| **Adafruit Pixel Shifter #6066** | At least one physically received and tested; exact extra-unit quantity not confirmed | Supports two 3.3 V to 5 V pixel-data channels, GPIO16/17 for the selected MCU | **Not a 2-source selector, watchdog, or automatic handover. Do not tie P and B outputs together on it** |
| **AITRIP nominal fixed-5 V mini modules (10-pack)** | Owned | Future individual low-current MCU supply experiments after **unit-by-unit 5V verification**, power-path review and 5V headroom verification | Not automatically a proven 5–24 V input automotive regulator; VIN=5 V dropout, thermals and transients remain open |
| **Adjustable buck converters** | Owned; exact variant/count unknown | Bench-only alternative power path **after measured setpoint** | Not a step-up / automotive-qualified supply |
| **Inline automotive fuseholders** | Owned; quantity/fuse values not confirmed | Protected, limited-current LED branch experiments; use appropriately rated wire and terminations | Fuse and conductor ratings must be verified; not a connector substitute |
| **WS2815 12 V BTF-LIGHTING strips** | Owned/installed; 144 px/m reported | Two real GS400 623-pixel chains (left and right), using the app-provided zone map | Not a second independent lamp chain / luminous fallback |
| **GLEDOPTO Elite 4D-EXMU** | Owned | Separate known-good WLED output/color baseline on its own test circuit | 4-output WLED controller, **not** a failsafe data multiplexer or a substitute for OpenTail's backup MCU; manufacturer states 15 A *maximum combined output current*, don't route the 20 A bench source through it |
| **12 V bench supplies** | 12 V/4 A and a later fused ~20 A bench setup have both been reported; actual hardware labeling and configuration should be checked before test | Low-brightness, fused and current-limited LED testing | No verification of enough current for 1,246 pixels all-white |
| **Plastic enclosure and existing leads/connectors** | Owned; exact internal usable dimensions, clearance and pinout unmeasured | Fit checks and removable bench mockup | Not yet validated for two processors, high-current harness, thermal / EMC clearance |

**No automatic data-selector IC, supervisor/watchdog gate, dual-output OE logic, qualified auto-grade power frontend, 10-channel AUX driver bank, full 12-lane level-shifter bank, or independent light emitter is documented as owned.** Mark as *missing for final safety requirement*, not as ordered or installed.

## Use only owned components for first validation

### Phase 1: primary/backup **separately** on a single car bench

1. Use two hiBCTR ESP32-S3 boards (Primary and Backup). Run each independently over USB initially, keeping USB/external 5 V isolation precautions. Frozen PR #51 is a two-physical-output **primary** candidate; a separate backup essential-light firmware image **does not exist yet**. For now the second board can be checked for reset/boot, input read and basic output using a separately authorized test image, but not claimed as autonomous takeover.
2. Use GPIO16 and GPIO17 from **only one board at a time** into the owned Adafruit #6066, then to two *short*, separately fused 12 V WS2815 test strips (data signal and a deliberate common signal-reference ground; never put pixel load through shifter). Verify supply and polarity before connecting. No direct primary–backup output linking or hot swaps.
3. Reassign ownership **only with supplies disconnected**: physically move the two shifter signal-input wires from Primary to Backup and restart. Validate each MCU can generate independent two-output data. This is a **manual A/B functional test**, not hardware failover, and provides zero guarantee of uninterrupted taillights.
4. Use the **two owned PC817 eight-channel boards** as separate Primary and Backup *bench* input banks, with a separately protected/current-limited bench trigger source. Each bank's logic outputs go to only its own MCU GPIO4..11. An input source shared on the high-voltage side is a common failure point, but its logic outputs must not be cross-connected. Verify actual pull-ups, polarity, input current and voltages on exact module first.
5. Treat the GLEDOPTO as a **known-good separately wired optical reference only**. Do not splice its signal output onto an active OpenTail data line. Review its published 15 A *total* output rating; it cannot approve main 1,246-pixel power.
6. Progress to **O1 left 623 and O2 right 623** only after individual short-chain tests. Use low brightness and a measured LED current ceiling. Exact frozen PR #51 accepts a configurable pixel buffer (2048 default per physical output); do not hardcode 623 as a hardware default. Measure actual WS2815 FPS; at 800 kbit/s a 623-pixel frame has ≥18.69ms wire time.
7. Keep the existing original road lighting independent. A USB-powered bench experiment must never become the sole brake/turn lamp controller.

### Phase 2: minimum genuine hardware takeover addition (not currently on hand)

Once individual render and both input banks pass, the **smallest missing function** is externally enforced **exclusive selection** of the two 5 V pixel outputs, with all unselected paths high-Z and failure-safe when either MCU rail is OFF, plus qualified watchdog/rail monitoring. It may eventually use a compact board integrated inside the one enclosure, but do **not** call an Adafruit shifter an arbiter. Exact parts **not selected or purchased**; previous `SN74LV4T125-Q1 / TPS3430-Q1 / LM7480-Q1` suggestions are candidates only, **not user inventory**, and not a purchase instruction.

True autonomous takeover additionally requires backup-owned output firmware, a frame-progress diagnostic interface, mutually validated essential zone profiles and independent rails. Current frozen PR #51 has **no approved backup-controller electrical grant protocol**. When designing failover circuit, do not change PR #51 silently.

### Phase 3: final two cars / 12 outputs

Allocate 4 of the 6 ESP32 boards and keep 2 spares. Procure/add **only proven missing items** *after* phase 1: extra 8-channel isolation coverage for the second car, independent per-MCU protected supplies and regulator monitoring, mutually-exclusive two-then-12-lane data selection, correctly rated 12 V LED power routing and controlled 10 AUX channels. None are assumed already owned. Validate real 12-output firmware later; this is a future candidate, not a feature of PR #51. Keep app layouts universal.

## Stop / no-go conditions

- Do **not** connect outputs of two ESP32s or two #6066 shifters directly together, even for one test. Do not rewire energized outputs.
- Do **not** feed 12 V triggers directly to GPIO or an unqualified shifter; use verified optocoupler output-side 3.3 V levels and independent grounding.
- Do **not** close a fixed-5V regulator verification jumper or connect buck output to a dev board without independent voltage and USB-backfeed checks.
- Do **not** rely on color brightness cap as a *measured* strip-current limiter; full white/1,246 px may exceed bench power ratings.
- Do **not** mark automatic failover, safe road illumination, 5–24 V controller qualification, or 12 outputs PASS based on these manual tests.

## Technical references for user-owned devices

- Adafruit #6066: https://www.adafruit.com/product/6066 — two independent shifter channels (and inverter) but no failover selector.
- GLEDOPTO official Elite 4D-EXMU model GL-C-618WL: https://www.gledopto.com/h-pd-64.html — four digital outputs, 15 A *combined* current maximum.
