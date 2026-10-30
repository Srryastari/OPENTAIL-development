# OpenTail universal layout ownership contract — hardware ↔ CORE ↔ Android

**Decision:** **No GS400-specific layout hardcoded in the controller hardware or shipped default profiles.** The installer configures output lengths, logical zones, vehicle trigger roles, color order, and calibration **later in the native Android app**. Standalone Studio can author profiles but is not an independent safety authority. The GS400 623+623 map in `GS400_PIXEL_MAP.csv` is an optional **bench validation fixture / importable example only**, not a default output, a PCB routing requirement, a permanent layout, or an enforced zone partition.

## Fixed hardware capabilities vs changeable runtime configuration

| Physically fixed by PCB/wiring | User-configurable later in app |
| --- | --- |
| Available connector positions / 12 candidate independent pixel-data lanes | Which lanes are enabled and their independent pixel counts, up to empirically validated driver + RAM + electrical limits |
| Protected voltage domain, signal level shifting, independent output-driver ownership | Output display names, GRB/BRG/etc color order, channel calibration and brightness caps within proven safe limits |
| 8 independently isolated and supervisor-visible trigger **terminals** (plus optional hardwired emergency circuits) | IN1–IN8 function mapping (Park/Brake/Left/Right/Reverse/AUX), with fail-closed validation and essential input preservation |
| 10 candidate protected high-side switched-output **connectors** | Per-output non-safety configuration subject to current, duty cycle, fault and safe-off constraints |
| Independent primary and backup processors, supervised bus arbitration, failover circuit | Arbitrary **contiguous** zones per enabled pixel output: names, start/end by automatic cumulative offset, lengths, functional roles, effects and mapping |
| Actual physically soldered LED strip chain order and power-injection topology | Zone boundary/role/color/effect adjustments only within the fixed physical order; cannot physically rewire LEDs in software |
| Fuse and conductor/connector ratings, fixed safety envelopes | A configuration cannot override or increase hardware electrical ratings |

Do not design “Top,” “Amber,” “Reverse,” “Bottom,” “Trunk,” GS400 dimensions or 623 pixels into traces, connectors, GPIO definitions, backup firmware defaults or NVS factory values. A newly unconfigured output must be **inert / role off**, with no assumption about vehicle shape or fixture pixel count. Defaults must not illuminate vehicle lamps without a validated installation profile. Hardware connectors should be labeled `PIX1`...`PIX12` rather than model-specific lamp labels.

## Safety-critical profile activation and failover

1. Android submits the **desired** layout using the authenticated CORE config transport, not direct unsupervised pin writes. CORE performs validation and is authoritative for **active vs staged/pending** revisions, especially if a trigger is asserted. App must not announce an active save before authoritative readback.
2. A validated profile covers output counts/chain length, per-output color order and contiguous zone partitions, trigger function mapping, safety-essential roles and permitted brightness/power limits. A full revision must be integrity-checked before activation and before backup receives it.
3. Backup maintains a **separately stored last-good essential-light profile** based on the same installer-selected zones and trigger mapping, never an embedded GS400 assumption. Prior installed safety mapping remains armed until **both** processors have acknowledged a complete new valid revision and a safe activation boundary. If the backup cannot store/verify the new map, block unsafe activation and report degraded redundancy.
4. Physical signal sampling is independent on Primary/Backup. Neither BLE disconnects, Android offline operation nor decorative effects can suppress Brake/Turn/Reverse. Backup still drives the *configured* essential safety zones if Primary dies.
5. Invalid/empty/mismatched zone map must **not** assign arbitrary outputs or use an unsafe canned vehicle profile. Require installer acknowledgement and external independent road-light fallback until a valid installed map is certified.
6. All zone output drivers, even unused lanes, must default high-Z/disabled; changing app assignments must not bypass interlocks, output current protections, or electrical brake/indicator priority.
7. Retain ability to test one pixel, extend zone end with immediate physical bench preview, adjust next-zone start cumulatively, and recheck live output. These are **requested app/firmware capabilities**, not asserted fully implemented in the frozen Software PR #51. Physical hazard/Brake safety overrides always take precedence; staged active safety maps still need explicit activation.
8. Current Software PR #51 `bc92d886560dab88c6a53316d8e4cddc5d2fb8ef` defines **2 physical pixel outputs** and a configurable buffer budget (default 2048 per output). It does not thereby implement 12 synchronized physical outputs or the backup arbitration pins. Any extension belongs in a separate software revision and must be retested, not silently changed in the frozen candidate.

## Required hardware acceptance implications

- Test arbitrary layouts unrelated to GS400: e.g. a 40-pixel single-zone output, 2 outputs of different sizes, reordered safety roles and an unused lane; ensure safe idle.
- Test each pin/output independently; no electrical dependency on a named taillight section. Validate 12-lane capacity only after a corresponding firmware build exists.
- Physically inject Primary failure with each valid installer-defined essential map; verify Backup has the exact same active safety mapping rather than an old fixture map.
- Test profile corruption, simulated app disconnect, saved-but-pending activation, power loss during updates, and vehicle input asserted during configuration. Observe actual pixels and supervisor signals.
- Always preserve the independently powered / separately controlled road-lighting fallback requirement. Software-configurable sections alone do not establish vehicle regulatory conformance.

## Out-of-scope and cautions

This contract does **not** assert Android has full live hardware editing or that a 12-output board already exists. The electrical design must expose neutral, universal connectors; CORE/Android owners are responsible for implementing and certifying dynamic profiles and app interactions against the hardware interface. Existing GS400 fixture is still useful for the two-output bench test without becoming a global default.
