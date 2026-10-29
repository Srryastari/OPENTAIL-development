# OpenTail locked release contract — October 28, 2026

This is the acceptance contract for the unified CORE release. It records decisions, not proof of implementation. Never mark a requirement complete solely because code compiles or a catalog name appears.

## Immutable product decisions
- The only firmware delivered or flashed is **OpenTail CORE ESP-IDF**. Upstream WLED source is a pinned code/algorithm reference to port into CORE, not an installed WLED runtime, separate firmware, network API, or branded UI. Retain legally required notices, attribution, and license files for adapted source.
- One universal controller architecture; no GS400-specific startup layout, counts, role assignments or section names. Existing user-saved NVS profiles take precedence; unconfigured outputs remain inert.
- Current bench: ESP32-S3 N16R8, two physical pixel outputs GPIO16/17, PC817 inputs IN1..IN8 on GPIO4..11, configurable zones. Future target 12 pixel outputs and 10 switched 12V outputs is not a present capability.
- Preserve physical input-trigger precedence: Brake over Park; hazards drive both turn roles; Reverse and other assigned triggers override shows only in their assigned zones; shows continue elsewhere and resume when triggers release; global Stop/off wins.
- Local physical triggers must work independently of BLE session authorization/disconnection.
- Keep Studio v0.19.118 approved navigation and styling; no redesign, duplicate navigation or bottom tabs. Advanced developer tools may remain while a simpler consumer UI is built separately.
- Keep stable `web-studio/opentail.htm`, versioned `opentail-v0.19.118.htm`, and Android offline bundle identical.
- Restore approved status LED: startup white flash; Wi-Fi Ready cyan pulse ~1s; connected green double pulse ~2.2s; disconnected red double pulse ~2.2s; Show rainbow ~1.4s; fault red 90ms blink.
- Effects must appear in a single Effects catalog, with original pinned upstream 1D/particle algorithms ported into CORE alongside OpenTail native effects, proper palettes, dual colors, per-zone settings and persistence. Compatibility approximations are NOT pixel-identical source integration. Matrix/audio hardware-dependent modes remain gated until their dependencies are implemented and tested.
- Do not claim backup-controller failover or safe on-road sole-taillight operation before physical failover and electrical validation.

## Verified historical observations — do not silently erase
- Previously observed IN1..IN8 GPIO mapping, masks, combined hazards mask 12 and brake+hazards 14, release 0; physical brake input IN2 retested on v0.1.26 and passed.
- Prior WS2815 GRB color order correction; physical output and trigger override demonstrations; profile persistence on earlier firmware. See `docs/verified-bench-regression-matrix.md` for evidence and distinctions.
- Earlier passes belong to earlier firmware. Avoid repetitive manual tests for unchanged components; changes to critical paths require targeted revalidation.

## Known regressions and open gaps
- v0.1.26 status LED patterns regressed: restore and verify on updated firmware.
- Effects absent or incomplete: compare catalog, rendering semantics, controls and output against locked requirements.
- Studio Layout Advanced card closed on Output 1/2 selection: source-level preservation patch applied on recovery branch; browser interaction acceptance still required.
- Output-specific test/preview control missing in Studio: investigate and implement without global Color Demo being misrepresented as independent output control.
- The WLED-native experimental image is a separate reference target, explicitly NOT the OpenTail product firmware. Complete source port to CORE, its authenticated BLE/GATT/profile integration, output-bus integration, pixel parity and physical validation remain release blockers.

## Release gates (all required before calling the release complete)
1. Compare changes against this contract and `docs/verified-bench-regression-matrix.md`; maintain a traceable requirement-to-test matrix.
2. Pass CORE host tests, Studio UI tests, Android build, ESP-IDF build, source-port tests, and reference-renderer parity checks on the SAME candidate commit. WLED-native experimental build checks may remain reference CI but are not a release firmware target.
3. Confirm Studio stable/versioned/Android copies are byte-identical.
4. Validate output-specific controls, disclosure persistence, saved profile isolation, NVS migration, triggers and status LED with targeted checks. Do not mistake a global color demo for per-output proof.
5. Verify authenticated BLE/profile/scenes and universal outputs drive the original rendering algorithms **inside CORE**, plus real upstream effect equivalence; do not ship incomplete approximations under a full-parity claim.
6. For road-lighting use, verify independent failover and safe power/current behavior on actual hardware. Until then label firmware bench-only.
7. Deliver the established single ZIP artifact with bootloader, partition table, application binary and `flasher_args.json`, plus one PowerShell flash command. No separate v0.1.25 flash and no merged-image substitution.

A green CI run means software checks/build passed, not that all release gates passed.
