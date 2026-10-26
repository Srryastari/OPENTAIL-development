# Development instructions — internal codename OpenTail

OpenTail is an INTERNAL DEVELOPMENT CODENAME only. Do not assume the final product name, trademark, branding, or open-source licensing.

## Project areas
- `firmware/`: ESP32-S3 CORE firmware, BLE and Wi-Fi control, automotive triggers and output management.
- `android/`: Android application.
- `studio/`: web-based Studio.
- `hardware/`: schematics, PCB drafts, wiring, BOM, electrical safety planning.
- `docs/`: protocols, requirements, changelogs, testing evidence.
- `archive/`: immutable historical source releases and design artifacts.

## Non-negotiable working rules
1. Preserve all existing working source and features. Inspect current code before making changes.
2. Treat the newest *verified* release as the starting point. Never assume version number alone establishes functionality.
3. Keep historical snapshots separate from active source; never overwrite or silently discard earlier work.
4. Do not claim bench, vehicle, LED, BLE, or safety tests passed unless evidence was supplied.
5. Record tests actually run, environments, failures, and limitations.
6. Do not treat a prototype as automotive-road-safe or certified. Hardware fallback and watchdog behavior need independent validation.
7. Keep product naming provisional; do not rebrand source unnecessarily.
8. Do not commit credentials, tokens, Wi-Fi passwords, keys, private user data, or unlicensed third-party assets.
9. Prefer incremental changes with clear commits, regression checks, and rollback paths.

## Known project requirements (subject to source verification)
Universal automotive addressable-lighting controller; dual ESP32-S3 architecture with independent backup/watchdog intent; 8 configurable trigger inputs, target 12 pixel outputs, 10 switched outputs, ~6,000 pixels initial target; Wi-Fi and BLE; audio reactivity; CAN optional/future. Early prototype supports two pixel outputs. Hardware goals are not claims of completed implementation.

Android: retain existing OLED-black/purple interface and live BLE state. Compact status badges must reflect real state: Connected, Connecting, Authorized, Saving, Syncing, Synced, Bluetooth Off, Disconnected. Wi-Fi and BLE status are independent.

Studio: maintain WLED hosting compatibility, stable `/opentail.htm` deployment path, versioned release copies, user-configurable sections/zones and trigger behavior.

## Import plan
Inventory all prior ChatGPT Library and user-held files. Preserve original filenames and checksums in `archive/`, identify duplicates, then unpack and reconcile the newest verified source into active directories. Do not confuse imported snapshots with hardware-validated releases.
