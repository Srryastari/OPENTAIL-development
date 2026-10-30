# On-hand bench wiring — no WAGO connectors

**Scope:** one car's two data chains; test ONE hiBCTR ESP32-S3 at a time, then repeat with second ESP32 after full power removal. `ON_HAND_FIRST_BENCH_PLAN.md` is the source of safety/limitations. **Frozen source:** PR #51 SHA `bc92d886560dab88c6a53316d8e4cddc5d2fb8ef`. All wiring below is a **proposed test fixture**, not an assembled schematic or actual verified pinout. Requires inspection of the user's exact board/module/strip markings before energizing.

## Correct Adafruit #6066 two-signal pinout

Official Adafruit source: https://learn.adafruit.com/adafruit-pixel-shifter/pinouts

Adafruit #6066 is an **unrelated DATA channel and CLOCK channel**, each independently shifted to ~5 V. For **clockless WS2815**, the CLOCK channel may be tested as a **second data signal** when driven by a second independent 800-kbit/s ESP32 output; this is an engineering use of the independent clock level shifter, not a second WS2815 clock. Confirm phase/timing with actual strips. The available output terminals are `D5` (shifted DAT), `C5` (shifted CLK), `!D5` (INVERTED DAT — **do not use** for WS2815), and `G`. **Do not mistake `!D5` for channel 2.**

| Owner/source | Adafruit #6066 input | Adafruit output | Destination | Conditions |
| --- | --- | --- | --- | --- |
| Selected ESP32 GPIO16 | `DAT` | `D5` | Left WS2815 **DI** | Normal non-inverted 5 V data |
| Selected ESP32 GPIO17 | `CLK` | `C5` | Right WS2815 **DI** | A second DATA stream via the second level-shifting path; **not a clock** |
| Selected ESP32 **3V3** supply | `V` | internally powers #6066 | No LED +12 V | Adafruit says V requires 3–5 V; power shifter from verified stable logic rail after confirming board current budget |
| Selected ESP32 GND | `G` | `G` | 12 V strip **signal reference ground** | Tie grounds deliberately for single-ended data; LED high-current return must not flow through shifter terminals |
| Qualified fused +12 V LED source | **NEVER #6066 V** | no connection to #6066 outputs | Strip +12 V and any qualified injection points | Fuse/connector/wire selected by current and mechanical rating |
| Qualified LED return wiring | no high-current return through #6066 | no return through ESP32 | Strip GND and rated supply return | Maintain a deliberate shared signal reference, but do not use GPIO jumpers as power return |

Adafruit #6066 has its **own integral screw terminals for 18–26 AWG signal wires**; the user explicitly has **NO WAGO connectors**. Do not purchase WAGO connectors or claim they are on hand. The #6066 terminals are **NOT** suitable for the main WS2815 strip's high-current +12 V/ground distribution. If there are no mechanically secure, correctly rated existing LED power connections, **do not energize the strip power** until proper terminals/connectors are sourced; improvised twisted/open wires are not acceptable. Reuse existing fused power wiring only when its actual connectors and conductors are qualified. No specific fuse size is approved without current and wire ratings.

## Bench layout and removable selected owner

```
                Primary hiBCTR ESP32-S3 (only when selected)
                     GPIO16 -----------.        
                     GPIO17 ----------. |          Adafruit #6066:
                                       | +-------> DAT ---- D5 --------> LEFT WS2815 DI
                                       +---------> CLK ---- C5 --------> RIGHT WS2815 DI
 Primary 3V3 (verified) ------------------------> V     !D5 (DO NOT USE)
 Primary GND -----------------------------------> G ------> LED signal GND reference

    LED +12V supply ---- rated source fuse/branches ------------> LEFT/RIGHT +12V
    LED supply GND ----- rated return wiring -------------------> LEFT/RIGHT GND

  Backup hiBCTR ESP32-S3: completely disconnected from the #6066 DAT/CLK during
  primary test. After all power is OFF and safely discharged, move DAT/CLK/V/G
  to backup and repeat as a separate test. DO NOT wire two GPIO sources in parallel.
```

**Power-down owner swap is only an A/B standalone-capability test.** Not live handover, automatic arbitration, safe road lighting, or demonstrated independence from a shared shifter/LED rail. Keep vehicle factory road lamps connected during lab evaluation.

## Independently connected on-hand input modules

- Use PC817 bank **P** OUT1..OUT8 -> Primary GPIO4..GPIO11 and bank **B** OUT1..OUT8 -> Backup GPIO4..GPIO11, each powered/referenced only to the respective validated 3.3 V side.
- A PC817 board's optocouplers do not automatically make its entire module suitable for raw 5–24 V vehicle signals. Before using any 12 V trigger source, measure/review **actual on-board input resistors, active polarity, input current, reverse protection and isolation/ground copper layout**. The two isolated input-side harness feeds must be branch protected. Do not apply 12 V directly to any ESP GPIO.
- For one-input-at-a-time tests, begin with a **verified, current-limited, low-energy bench stimulus compatible with the module**, and record raw/debounced masks. Do not assume the module works with 24 V inputs.
- Neither bank can replace the independent input-power and fault-tolerant supervisor circuit; real autonomous failover requires hardware **not documented on hand**.

## Run order / acceptance recording

1. **Zero power:** check selected ESP and #6066 pin labels, ground continuity (as appropriate), data versus power separation, independent board disconnection, secure terminations, strip DI polarity and existing fuse rating.
2. **Logic only:** selected ESP over validated USB/logic supply. Confirm #6066 V uses verified 3–5 V logic (prefer verified ESP 3V3) and G reference; no 12 V LED supply yet. Confirm data output logic with appropriate instrument if available; do not claim oscilloscope proof without one.
3. **Short strip, limited power:** connect already secure/fused 12 V strip supply independently; start with low brightness, measure/observe left and right separately; validate GPIO16→DAT→D5 and GPIO17→CLK→C5.
4. **Full left/right installation:** configure O1=623, O2=623 in the approved CORE profile and test five independent contiguous zones only if source firmware and power limits permit. Existing 20 A bench fuse/source is **not** a proved 1,246-pixel full-white supply. Record strip power current, voltage and refresh FPS.
5. **Power OFF completely**, disconnect Primary from #6066, move DAT/CLK/V/G solely to Backup and repeat standalone tests with an appropriate **separate authorized backup test firmware image**. Frozen #51 does not provide an implemented autonomous backup image/owner arbitration handshake.
6. Record results in `ON_HAND_BENCH_RESULTS.csv`; until physically measured, every row stays `NOT_RUN`. Keep `REV_B_ACCEPTANCE.csv` and PR #51 mandatory release evidence gates BLOCKED.

### Source references

- Adafruit official pinout / terminal naming: https://learn.adafruit.com/adafruit-pixel-shifter/pinouts
- Adafruit product 6066 voltage/terminal AWG guidance: https://www.adafruit.com/product/6066
- Frozen PR #51: https://github.com/Srryastari/OPENTAIL-development/pull/51
