# OpenTail CORE Rev B — dual-controller electrical design and physical acceptance basis

**Hardware owner:** hardware workstream | **Software reference:** frozen draft PR #51 at `bc92d886560dab88c6a53316d8e4cddc5d2fb8ef`. This is a **design contract / pre-schematic engineering release**, not a validated circuit, fabricated PCB, or software/hardware certification. Keep `hardware/DESIGN_SPEC.md` / `NET_PLAN.csv` unchanged as the Rev A single-ESP32 baseline. Proposed requirements in this file must not be interpreted as features implemented in PR #51.

## System boundary: two physical enclosures, four MCU boards

- Each **car** gets **one compact enclosure**, housing **one primary hiBCTR ESP32-S3 N16R8 + one autonomous backup ESP32-S3**, a hardwired supervisor/arbiter, independent input-conditioning banks, two logic regulator domains, 12 independent pixel-data paths per processor (future target), and 10 optional auxiliary switched outputs. **Two cars = two such controllers = four ESP32-S3s.**
- No extra external watchdog or processor boxes. LED power injections, wiring/protective distribution, and any independent fail-safe luminous path may be remote from the enclosure as physically required. Existing factory road lamps remain independent until this project's electrical and regulatory lighting proof.
- **Do not claim design freeze at PCB/component/footprint level.** Pinout of the exact dev board, regulator current, enclosure **internal** volume, actual connectors, wire exits, thermal limits, and critical part selections are unverified. Nominal external case 152 × 64 × 38 mm is a **fit target**, not clearance evidence. A redesigned mechanical stackup may be necessary.

### Functional topology (not an as-built schematic)

```
J_VIN_CTRL 5–24 V DC OPERATING RANGE GOAL
  -> input fuse/TVS/EMI/reverse-battery/overvoltage/undervoltage protection
     +-- independently protected REG_P -> P_5V/P_3V3 -> PRIMARY ESP32-S3
     +-- independently protected REG_B -> B_5V/B_3V3 -> BACKUP ESP32-S3
     +-- independently supplied SUPERVISOR -> watchdog/rail-valid/owner arbiter
J_VEH_TRIGGER[1..8] (vehicle voltage / sensor harness)
     +-- protected isolated/conditioned bank P -> PRIMARY GPIO4..11
     +-- separate protected isolated/conditioned bank B -> BACKUP GPIO4..11
PRIMARY DATA[1..12] -> 3.3-to-5 V gated output buffers P --+
                                                       exclusive 12-lane hardware interlock -> J_LED_DATA[1..12]
BACKUP  DATA[1..12] -> 3.3-to-5 V gated output buffers B --+
PRIMARY I2C (GPIO1/2 provisional) -> MCP23017 -> 3 x TPS4H160-Q1 -> J_AUX[1..10] (default OFF)
J_LED_12V (SEPARATE *QUALIFIED* 12 V supply feed) -> rated fuse/branch distribution/injections -> J_LED_12V / strips
J_LED_RETURN -> rated return distribution; deliberate common LED-data reference / no full LED return in logic PCB
```

### 5–24 V controller **vs** mandatory 12 V pixel supply

1. **5–24 V is the target for the low-current controller logic input, NOT a permit to apply 5 V or 24 V to 12 V WS2815 strips.** Regulator architecture must meet 5 V input with cold-start/headroom and 24 V sustained input: e.g. a correctly specified automotive-protected buck-boost/3.3 V power tree, or validated equivalent. A generic step-down board called “5–30 V in / 5 V out” does not inherently regulate at VIN=5 V, survive crank/load dump, or deliver its advertised current thermally. Keep the on-hand AITRIP and JP_LOGIC_5V_VERIFY **disconnected** until tested independently.
2. **J_LED_12V is a separate supply input** suitable for the actual LED load: regulated/protected 12 V with connector, harness and injection ratings. If only 5 V control input is connected, LED power is **not available** unless an appropriately rated 5→12 V power solution is provided; do not integrate a high-current boost on the thin logic carrier. At 24 V vehicle input, LED power requires an appropriately rated 24→12 V supply. At a 12 V vehicle, battery/alternator supply is not a constant guaranteed 12.0 V; strip voltage/transient envelope must be validated. Any branch that uses raw automotive voltage without adequate protection stays unapproved.
3. Design each MCU regulator as a separately protected failure domain. Shared input fuse, battery, chassis ground, transient suppressor and common connectors **remain common-mode vulnerabilities**. A single dead common battery/feed does not become fail-safe because two ESP32s exist. Supervisor power must remain valid under the specific primary-regulator fault scenarios being claimed.
4. Input protection selection: automotive reverse polarity, jump-start, load-dump/transients, EFT/ESD, overvoltage/undervoltage and EMI; assess ISO 7637-2 and ISO 16750-2 test strategy and the exact vehicle architecture. Choose component ratings using **waveforms and energy**, not just a “24 V max” label. Qualified rail monitor and reset sequencing for both MCUs, buffer output-disable and external supervisor required.
5. Provide **USB vs external-power reverse-current blocking** and no half-powered I/O paths. Test primary/backup regulator OFF while opposite rail, trigger signals, USB, LED 12 V and arbiter supply are ON. 3.3 V GPIOs never see a 12 V trigger.
6. Controller total input current, sleep/off load, regulator efficiency, protected output thermal characteristics and fuse values are **TBD until measurements**. “No drain” is a desired parked-state behavior requiring a true disconnect/wake-power architecture and quiescent-current characterization: ESP32 deep sleep is **not** zero current. Wake on ACC/hazards/fob remains a separate not-yet-validated circuit and shall not suppress essential road-light triggers.

### Independent takeover and exclusivity

- Primary is the full effects/trigger renderer. Backup is independently bootable and maintains the last authenticated and integrity-checked **essential-light-only** zone map: Park, Brake, Left, Right/hazards, Reverse. Full effects, BLE, Wi-Fi, USB and phone are irrelevant to its emergency operation.
- Each processor receives IN1..IN8 through its **own conditioning bank**, even if sharing the protected vehicle-signal harness. A single existing PC817 8-channel module per car is **not** two independent banks. Confirm input polarity, optocoupler behavior across vehicle input voltage, isolation clearance, reverse polarity and LED current, fault isolation, debounce, and no output-side backfeed.
- Independent supervisor/arbiter must have qualified P_OK, B_OK, rail-valid, frame-progress timeout and profile-valid conditions. **A timer-only heartbeat is insufficient:** completed pixel-frame activity and physical output-driver health require separate observed evidence; a jammed data wire can appear healthy to software. Watchdog/timing thresholds and handback hysteresis remain **TBD measured**.
- Exactly one owner can drive each LED data lane: default **both disabled** in reset, brownout and unqualified states; electrically interlocked break-before-make; no direct paralleled ESP32 or AHCT outputs. 12-lane mux/gating must be qualified at 800 kbit/s with cable load, high-Z and unpowered isolation. A combinatorial handshake without tested boot-state pulls and power-off isolation is insufficient.
- Proposed ownership truth table (external hardware enforces, firmware cannot override):

| Supervisor condition | Primary data enable | Backup data enable | Result |
| --- | --- | --- | --- |
| Reset / invalid rails / unknown | 0 | 0 | no output contention; **NOT** safe road illumination |
| Primary qualified healthy | 1 | 0 | full CORE operation |
| Primary failed, backup fully qualified | 0 | 1 | autonomous essential light |
| Primary healthy, backup unavailable | 1 | 0 | degraded redundancy alarm |
| Both failed, profile invalid, arbiter fault | 0 | 0 | independent *luminous* fail-safe needed |
| Transition / handback | 0 | 0 during mandatory dead time | then a complete freshly generated LED frame |

- Check failure modes: frozen primary GPIO HI/LO, frozen frame producer, RMT DMA stall, primary voltage collapse, backup crash, stale/invalid backup profile, supervisor fault, failed mux lane, 5 V buffer rail outage, watchdog false pulse, repeated handback, USB plugging/unplugging, ISR stress, and timing against actual 623-pixel chains. Status diagnostics cannot make an unsafe state acceptable.
- **Independent luminous fallback is still an unsolved release blocker:** a failed first WS2815/pixel supply/harness/ground or shared output mux can darken both MCU paths. Preserve factory road lamps or design a physically independent lamp emitter/feed/control path that conforms to applicable automotive lighting requirements. The WS2815 **BI** connection is for pixel-chain break continuity, not controller failover. Its pinout remains unverified on the exact product, so Rev A BI pads remain disconnected.

### 12 output hardware and 10 high-side switched outputs

- Rev A provisional **primary** 12 data GPIO map: O1=16, O2=17, O3..O12=12,13,14,15,18,21,38,39,40,41. For the **backup**, a matching data mapping is only a routing proposal; verify real hiBCTR board pin exposure, GPIO38 onboard RGB conflict, strapping/glitch behavior, OPI PSRAM GPIO33..37 restrictions, and active PR #51 firmware allocation. USB GPIO19/20 reserved. Do not assume all 12 outputs can be generated in parallel using the current two-output CORE firmware.
- Use **2 × 8-channel 5 V tri-state level shifters per processor** for the 12 signals (four chips per dual-ESP box if SN74AHCT245 is selected), with A→B fixed, six spare gates not treated as extra outputs. AHCT245 needs a 4.5–5.5 V rail; its unpowered behavior, output-OE default pulls, input thresholds, drive and buffers' decoupling are to be proved. Independent P/B buffers plus approved lane mux / interlock; shared 5 V mux may be a new single point of failure and must be fault-analyzed. The existing Adafruit #6066 covers only **two** bench data channels.
- **Do not use GPIO44 as an assumed final enable**: Rev A proposes it but it conflicts with UART0 RX and cannot itself enforce a failsafe takeover. Reserve supervisor handshake nets independent of app controls; GPIO42/43/47 and microphone/UART uses are still provisional. Hardware interface to PR #51 is **NOT yet implemented in that frozen binary** and must be negotiated with the firmware owner as a separate future candidate.
- 12 data lanes are **signal only** on logic carrier. Fused high-current positive/negative LED supply routing belongs in a separate rated distribution harness/bus inside enclosure as mechanically feasible; multiple injection points branch-fused and returns adequately sized. Use low-impedance shared signal reference near a deliberate star/bond location (do not sever necessary LED signal return). Consult exact pixel datasheet for DI/BI/DO/BO and clockless timing.
- 10 auxiliary positive-switched channels remain proposed **3 × TPS4H160-Q1 (4 channels each; two unused)**, driven by MCP23017 from the primary if allowed by PR #51. All OFF on MCU/expander reset. These devices are **not main LED current switches** and any safety-critical lamp connected to an AUX output requires its own independently reviewed fallback. At the nominal 160 mΩ ON resistance, illustrative per-channel conduction at 2.5 A gives ~1 W/ch; simultaneous load/thermal/ambient, current-sense error, hard short, fuse coordination and SOA must be explicitly analyzed. Never imply 2.5 A is a safe continuous application rating without thermal proof.

### GS400 pixel chains and output compatibility

- **One physically daisy-chained serpentine chain per side**; left and right independent data chains. Each: Top=118, Turn/Amber=109, Reverse=42, Bottom=253, **Trunk appended after Bottom=101** → 623 pixels per side; both total **1,246**.
- Zero-based ranges on each side: Top 0–117; Turn 118–226; Reverse 227–268; Bottom 269–521; Trunk 522–622. The sealed physical order must be verified against real soldered DI/DO connections, especially the trunk transition, and correct GRB/BRG behavior verified *physically* per strip.
- **Exact frozen PR #51 capability is two physical outputs (`OT_CHAIN_COUNT=2`) with configurable `CONFIG_OPENTAIL_BUFFER_PIXELS_PER_OUTPUT` defaulting to 2,048; the prior 522-pixel value is a historical bench fixture, NOT the present code limit.** Thus **O1 left 623 / O2 right 623** can pass software geometry validation and should be explicitly configured as one full chain per side, subject to real GPIO/RMT timing, frame rate, power, and current validation on the exact frozen image. Stage first using verified 522-pixel housing zones, then add each 101-pixel trunk as the fifth zone **without changing physical side-chain order**. No 623-pixel physical pass is claimed, and 12-output expansion still requires a new backend/software candidate.
- Raw wire time for WS2815 800 kbit/s, 24 bits/pixel: 522 pixels ≥15.66 ms, 623 pixels ≥18.69 ms (plus reset/latch, computation, bus contention/overhead). **Best-case physical throughput per continuously updated 623-pixel lane is <53.5 frames/s**; therefore the old **60–120 FPS** goal is not achievable on a single 623-pixel 800 kbit/s chain. For 6,000 pixels on 12 equally loaded lanes, 500/lane gives ≥15.0 ms wire time; ideal <66.7 FPS, *only* if 12 lanes can be transmitted concurrently. Current S3 RMT resources and CORE scheduler must be benchmarked, not extrapolated.
- For 144 LEDs/m, the documented product power varies materially by BTF SKU/manual: observed literature **20–36 W/m**, with one 144 LED/m sheet at **36 W/m = 3 A/m**. Preliminary **worst published figure**: 1,246 / 144 × 36 = **311.5 W / 25.96 A at 12 V**; 6,000 / 144 × 36 = **1,500 W / 125 A**. This is a **design envelope**, not confirmed for the installed strip or user brightness. The current fused 20 A bench source cannot safely demonstrate assumed 1,246-pixel full-load current at 36 W/m. `OT_MAX_BRIGHTNESS=32` is **not** a verified current limiter. Measure worst-case color and white patterns at the actual voltage/temp; select conductor AWG, connector, ground return, power injection, branch fuses and feed protection from measured/derated current and voltage drop. No fixed fuse value is approved here.
- 6,000 is a **pixel data-capacity target**, not a promise that 1,500 W/125 A is deliverable through a 152×64×38 mm enclosure. High-current LED power should use qualified, appropriately located external distribution/injection as necessary; if the enclosure size and one-box preference conflict with safety, safety wins and the mechanical design must be revised.

### Acceptance and exact frozen software reconciliation

- Scope validation against **exact PR #51 SHA**, not whatever HEAD is latest. Record source, build, flashed image hash, ESP-IDF tool version, physical board revision, LED SKU, strip operating voltage, pixel counts, observed waveforms, current, heat, faults and independent operator signoff.
- **No hardware test is asserted as passed.** Frozen software is software-only / bench-only; its statement requiring 22 separate physical gates cannot be waived by this hardware document or a passing unit test. Use `REV_B_ACCEPTANCE.csv` for our supplemental hardware gates and link actual evidence to the official release evidence gate later.
- **Do not flash #51 onto a replacement road-lighting system or release/merge based on this document.** Confirm GPIO/backup supervisor handshake and 623/12-channel runtime compatibility with firmware owner as a **new software candidate**, explicitly leaving #51 unchanged.

### Component primary references (verify exact revision/footprint)

- TI SN74AHCT245: https://www.ti.com/product/SN74AHCT245
- TI TPS4H160-Q1: https://www.ti.com/product/TPS4H160-Q1
- TI automotive battery protection reference: https://www.ti.com/tool/TIDA-01167
- Espressif S3 design guide: https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32s3/schematic-checklist.html
- BTF 144 LEDs/m 36 W/m technical sheet: https://exhibitorsearch.messefrankfurt.com/images/original/document_downloads/10000016202601/508802/1769759081255_208731075.pdf
- BTF alternative 144 LEDs/m product/manual (non-identical ratings): https://device.report/m/ba9c17d98dbbae9c7ff9d283656ba616bea4838f269789084b1028cfd01a8fb3
