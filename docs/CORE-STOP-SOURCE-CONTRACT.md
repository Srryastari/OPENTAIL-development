# CORE software STOP source contract

Status: **software STOP path unified; no physical STOP GPIO is assigned.**

## Approved source-level behavior

Both supported software STOP entry points dispatch to the same CORE implementation:

- BLE STOP command → `ot_ble_stop_show()` → `core_stop_all_sources()`.
- Bench/developer HTTP `POST /api/v1/stop` → `core_stop_all_sources()`.

The shared implementation stops decorative/show sources:

- Native FX and BLE-owned scenes.
- Demo and simulation.
- Non-committing geometry preview and active diagnostic preview.
- Status indication is restored and an immediate render refresh is requested.

## Protected state and physical safety

- If a geometry preview is already COMMITTING or its commit worker is active, STOP does not cancel it, clear its shadow candidate, or discard its NVS journal. The durable geometry transaction continues to its terminal state.
- STOP does not clear `live_input_mask`, alter optocoupler input state, call GPIO output functions, or change the trigger policy.
- Brake, Parking, Left, Right, Hazards and Reverse remain under the existing CORE physical arbitration. Software STOP never suppresses those inputs.
- Disconnect cleanup remains separate from STOP; it only expires BLE-owned temporary effects.

## Hardware boundary

No physical STOP/system-inhibit input is mapped in this implementation. No GPIO is invented, and none of the eight approved vehicle-trigger inputs is repurposed. A physical source can only be added after the owner approves a separate, electrically verified input and Hardware documents its wiring.

## Verification

`tests/test_http_stop_contract.py` verifies the shared HTTP/BLE path, immediate render wake, COMMITTING preservation, and absence of GPIO or physical-input mutation. Host/source tests do not constitute physical STOP wiring acceptance.
