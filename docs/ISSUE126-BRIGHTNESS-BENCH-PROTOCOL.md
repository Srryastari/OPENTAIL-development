# Issue #126 — Brightness parity bench protocol

## Current firmware contract

- User/effect/solid brightness requests are 8-bit values in the range 0–255.
- The CORE renderer applies one ratio-preserving effective ceiling at the final RGB pixel boundary.
- The default ceiling remains `CONFIG_OPENTAIL_MAX_BRIGHTNESS_LIMIT=32` (12.5% of channel maximum). It is a conservative software ceiling, **not** a measured current limiter.
- Trigger, unassigned-pixel fallback, diagnostics, Show Mode, Startup/Shutdown, and effects are rendered as 8-bit RGB before that final ceiling.
- The effective ceiling is reported in CORE status and Panel capability readback. No Android UI or BLE UI contract is changed here.
- Raising the build-time ceiling is not authorized by this software change. The existing 32 ceiling remains until electrical and thermal bench evidence is reviewed.

## Hardware compatibility correction

The current OpenTail bench pixel hardware is 12 V WS2815. A 5 V rail measurement or 5 V LED test procedure is not applicable to that strip. Do not connect a 5 V supply to the 12 V WS2815 strip. Measure the 12 V LED rail at the controller and at the farthest powered injection point. Use the actual connected test-segment count; do not assume a software buffer budget is a physical pixel count.

## Graduated test sequence — NOT RUN

1. Verify strip type, power-supply rating, fuse, wire gauge, connector ratings, injection topology, and all ground/data connections before enabling output. Confirm the strip's manufacturer voltage range.
2. Begin at the existing 32/255 ceiling with a representative white-fill pattern. Record supply voltage at the PSU, controller input, and farthest injection point; total current; connector and wire temperatures; controller PCB and power-stage temperatures.
3. Repeat at 64/255 and 128/255 only if the preceding step remains within every component's rated operating range. Increase current-limit/supply capability only after checking the full power path. Stop immediately on abnormal heating, voltage outside the strip/supply specification, connector discoloration, unstable data, or fuse/supply overload.
4. Test 255/255 only if the preceding measurements and the supply, fuse, wiring, connectors, PCB, and LED injection path demonstrate adequate margin. A 20 A fuse is protection, not a current regulator or proof of safe full-white load.
5. If 255/255 is electrically safe, run the 15-minute continuous white soak and record temperatures at regular intervals. Do not perform the soak before the graduated checks establish a safe load.
6. Compare OpenTail and BlueGhozt on the same physical strip sections, supply, voltage at the strip, pixel count, pattern, optical geometry, ambient conditions, and fixed camera exposure or a lux meter. Record current and voltage with each controller.
7. Capture final RGB bytes immediately before RMT transmission for the same requested brightness values. Confirm the requested scale is applied once and the reported effective ceiling matches the actual byte maximum.

## Required evidence

| Measurement | Required record |
|---|---|
| Requested brightness | 32, 64, 128, 255 |
| Effective CORE ceiling | Exact build config and reported status value |
| 12 V rail | PSU, controller, farthest injection point |
| Current | Total and per supply branch where measurable |
| Thermal | PCB, power stage, wire, connectors, LED strip |
| Optical A/B | Same strip/pattern/geometry; fixed exposure or lux |
| RGB/RMT | Requested value and final pre-RMT RGB sample |
| Firmware | Exact commit SHA, ESP-IDF 5.5.5 build receipt |

Physical status: **NOT_RUN**. Do not claim BlueGhozt parity, safe full-brightness operation, or physical certification until the measurements and independent review are complete.
