# OpenTail CORE — Independent hardware failover contract (Rev B proposal)

**Status:** architecture proposal only; no schematic, PCB, assembled redundant hardware, runtime takeover, fault-injection evidence, or road-use qualification exists. This document does not supersede the single-MCU Rev A bench design in `DESIGN_SPEC.md`.

## Scope and non-negotiable boundary

The custom addressable LEDs may be the car's *only* rear lamps. Therefore an ESP task watchdog, MCU reset, replicated software, or the WS2815 BI wire alone is **not** road-lighting failover. No production or on-road readiness claim is permitted until an **independent road-lighting safety path** is designed and electrically proven, including power, pixel-chain and harness failures. Keep existing road lamps electrically independent during development.

Rev B intent is **one compact enclosure per car**, containing Primary and Backup processors and the hardware arbitration/protection circuitry. Two complete enclosures are needed for the two-car prototype plan. Do not place required takeover hardware in a second external box. Remote LED power-injection wiring is not itself a redundant control path.

The existing Rev A proposal has **one** ESP32-S3, one trigger input interface, a single logic regulator candidate, 12 proposed signals (only two driven by today's CORE firmware), and one set of proposed AHCT245 buffers. **None of that is already independent failover hardware.** Retain the existing GPIO16/GPIO17 bench proof and user-saved universal zones; no hardcoded GS400 geometry.

## Fault containment and responsibility

| Component | Primary path | Backup path | Independent/common failure implication |
|---|---|---|---|
| Control | Primary ESP32-S3, full CORE, effects, BLE and profiles | Backup processor, deterministic essential-light-only state machine | Each MCU must boot, sample inputs, and generate correctly timed LED frames without help from the other |
| Trigger acquisition | Protected/isolated IN1–IN8 to primary | Separately qualified IN1–IN8 acquisition to backup | Two MCUs connected to one failing opto board are **not** trigger-independent; avoid backfeeding either MCU |
| Output generation | Independently buffered physical signal bank | Independently buffered signal bank | Do not connect two GPIOs/buffer outputs directly; provide interlocked exclusive selection for **every** populated data lane |
| Arbitration | Primary sends qualified frame-complete/health indication | Backup sends qualified health indication | **Independent hardware** controls output ownership; primary firmware never holds an irrevocable veto |
| Power | Verified protected logic regulator + power monitoring | Independently protected regulator/rail + monitoring | Shared 12V battery entry, LED feed, connector or ground can still be a single point of failure unless separately addressed |
| Config | Authoritative validated universal zone + trigger profile | Independently readable, versioned, integrity-checked last-known-good essential map | No emergency dependency on BLE/Wi-Fi, primary RAM, an in-flight write, or a partially synchronized profile |
| Light source | Fused WS2815 output branches + injection | Same chains in MCU-takeover-only phase | A broken LED supply, failed first IC, damaged strip or shared connector can defeat **both** MCUs; independent luminous fallback is a separate required design |

## Hardware arbitration — required electrical behavior

1. All primary and backup pixel drivers default **high impedance/disabled** while power, clock, reset, watchdog, and output-valid conditions are unresolved. Use external pulls and supervisory logic, not firmware GPIO reset defaults, to enforce this.
2. Each active physical output is fed from **exactly one** owner (Primary or Backup) through a reviewed digital mux or truly isolated tri-state driver banks. Select/enable logic must be externally interlocked with measured break-before-make; test power-off backfeed, high-Z behavior, mixed 3.3V/5V rails, and transient overlap.
3. An independent supervisor monitors primary/backup readiness and qualified *completed-output-frame* progress. Merely toggling a timer-driven “alive” pin does not prove the render/output task is making progress. A stuck-high line, repeated stale frame, stalled task, reboot loop, and output-disable fault all require detection strategies.
4. Normal owner = Primary **only after** a valid boot, profile validation and a stable healthy observation window. If Primary is unhealthy and Backup + essential profile + input sampler are healthy, the supervisor grants **exclusive** ownership to Backup.
5. Backup takeover must be autonomous: no primary software request, BLE connection, Wi-Fi transport, or primary NVS read is needed. Backup renders only assigned Park, Brake, Left, Right/hazards, and Reverse zones from physical inputs. Show/particle/overlay state is not required.
6. Backup health failure while primary remains healthy cannot steal the bus. If neither owner is trustworthy, drivers disable rather than contend; this **does not** produce safe road lighting. Independent safety lamps/power must handle that case. Never describe a dark output as a successful fail-safe.
7. Return to Primary uses a longer verified recovery window and an explicit supervisor handback condition to prevent oscillation. Preserve trigger state through transfer; re-send a full fresh LED frame before declaring the new owner visible. Boot/reset/handoff transient behavior must be characterized on a scope and physical strips.
8. Prevent ambiguous LED data after mux transitions: test a low output/valid latch-reset interval per the **specific WS2815 product datasheet**, then send a full replacement frame. “BI” is a pixel-chain redundancy pin, **not** a second-processor selector; leave Rev A BI breakout disconnected until wiring is verified.

## Software contract — firmware teams may implement only after hardware interface review

- Use a shared versioned essential-light profile format with a per-output color order, validated pixel count, independent user-defined contiguous zones, trigger mappings, brightness limit and integrity check; atomically install a complete profile to both owners. Reject mismatches/corruption; never assume fixed section indices or a vehicle model.
- Preserve authoritative trigger precedence: Brake over Park on shared brake/park zones; simultaneous Left+Right drives both turns/hazards; Reverse overrides shows only on reverse-assigned zones; on ordinary Primary operation shows continue on unaffected zones and resume when triggers release.
- Backup reads **real isolated vehicle triggers**, not a mirrored mask from Primary. Mirror link may aid diagnostics but is never the only source of Brake/Turn/Reverse truth.
- Establish a distinct `PRIMARY_ACTIVE`, `BACKUP_ACTIVE`, `TRANSFER_PENDING`, `BOTH_UNAVAILABLE`, `PROFILE_INVALID` diagnostic model with latched fault cause, switchover count and timestamp. Do not present primary heartbeat detection as proven physical takeover.
- Store/recover the last committed complete essential profile transactionally; profile change must not disarm a working backup before the replacement validates, and must be prohibited during unsafe active-trigger conditions if that is necessary for a coherent handover.
- Keep all network/authorization and effect logic outside the safety takeover loop. An app must not be able to force simultaneous output enable or suppress local triggers. No new default GPIO assignments are approved by this document.

## Required prototype evidence / release gates

| Fault injection | Required measured observation |
|---|---|
| Primary power cut, reset held, boot loop | Backup takes output ownership without bus contention; essential physical commands visible on all configured zones |
| Primary app hang, effect task stall, RMT/output stall | Supervisor recognizes actual missing frame progress; measured blackout/switchover time recorded |
| Primary pixel-data line stuck HIGH and stuck LOW | Defective owner electrically isolated; replacement full frame reaches real strip without contention |
| Backup reset or failure with healthy Primary | Primary remains sole driver; status identifies degraded redundancy |
| Both processors unhealthy, arbiter failure | No false fail-safe claim; independent light path remains functional |
| One logic regulator failure / undervoltage / brownout | Healthy path persists where topology promises independence; no backfeed or latch-up |
| Individual input conditioning failure | Independent path retains correct physical brake, hazards, reverse where promised |
| Stored profile corruption, stale backup profile, update interruption | Only fully validated essential map can arm backup; diagnostics show explicit disarmed/degraded state |
| Rapid repeated fault/recovery | Handback hysteresis prevents output ownership flapping |
| Output enable during boot/OTA/USB plug/unplug | No unpredictable illuminated pixels, driver overlap, 5V backfeed, or dangerous 12V feed |
| LED 12V branch disconnected, damaged strip/first pixel, common ground loss | Record actual outcome; MCU redundancy alone must **not** be used to pass road-light fallback gate |
| 2-output bench then expanded 12-output design | Record exact pin/driver ownership, logic traces, worst-case frame latency, rail measurements, thermal behavior, input combinations and independent Stop/reset behavior |

**Timing and current numbers are deliberately TBD.** Do not invent millisecond switchover limits or accept frame-rate claims until measured against the intended real strip lengths, output topology, minimum legal conspicuity/lighting behavior, and automotive electrical environment.

## Design order

1. Decide how **independent luminous fallback** survives failure of a shared LED strip, its first pixel, feed, ground, or common connector. It can live inside the same custom taillight housing but must not depend on the failed chain/controller. A second processor alone is insufficient for this requirement.
2. Schematic the independent protected trigger/power branches and supervisor-controlled mutually exclusive data route for the current two outputs. Select and review exact components (power sequencing, voltage, isolation, fault outputs, thermal and EMC). Do **not** reuse GPIO44 or the Rev A buffers blindly.
3. Add a minimal backup essential-light application and diagnostic protocol on a firmware branch; do not change primary effects, layouts, Studio or Android to accomplish takeover.
4. Bench with dummy loads, logic analyzer/oscilloscope and short strips at capped brightness; inject every fault above and record evidence. Only then repeat at the proposed 12-output/6,000-pixel scale.
5. Perform a vehicle electrical/harness and jurisdictional lighting review before any on-road deployment. **Rev A and Rev B are not road-ready until proven.**

### Relationship to the existing design

Keep `hardware/DESIGN_SPEC.md`, `NET_PLAN.csv` and `GPIO_ALLOCATION.csv` as the **single-MCU Rev A baseline** until a real dual-processor schematic, protected power tree, and pin-mux design exist. This document is a change proposal / acceptance contract, not an automatically corrected physical netlist.
