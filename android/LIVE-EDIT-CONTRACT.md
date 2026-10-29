# OpenTail Android — universal live-edit contract (Oct 29, 2026)

This is the **default behavior** of the six-screen Kotlin/Compose app, not a separate "Live Edit Mode".

## Immediate design feedback

- **Show Modes:** effect selection, foreground/background/accent colors, brightness, speed, and intensity immediately update the 25 Hz *approximate* preview and the local design settings. CORE remains the sole authoritative renderer.
- **Sequences:** step list, order, duration, Shuffle, Repeat, and Shared Palette persist as each control changes. No controller side effects.
- **Layout:** zone name, pixel count, direction and downstream zero-based offsets change in the UI during editing; valid numeric/text changes save to local v4 format. A dragged slider updates the visual immediately and persists the new zone count at gesture completion to avoid per-frame durable writes. Live per-pixel colors are exported in the v4 `selectedPixels` map.
- **Triggers:** multiple selection, zones, effect and valid hex colors update the local design as changed; Start/Stop is a local *simulation only*, never a physical input override.
- **Auto Events:** enabled state, effect, fade and duration persist immediately under the historical v4 keys.
- **Settings:** connection, readback and edited output draft text update immediately; permanent hardware/NVS changes remain an explicitly confirmed Save transaction.

Do **not** put a Save button in the authoring zone/trigger editor. "Done" closes a live editor; it is not the operation that applies its edits.

## Supported physical live preview (preexisting BLE v1 FX opcode)

An owner authorizes BLE; Android reads supported effect IDs on authorization. **No Live Edit Mode toggle exists.** The next FX edit gesture automatically begins a temporary authenticated `showFx` on existing saved output/zone assignments; max one send per 120 ms, max brightness 24/255, five-second TTL, and STOP on background/exit or an explicit Stop CORE show action. Unknown effects cannot be transmitted. Hardware trigger priority remains CORE-owned. This action **never** calls `saveOutput` / NVS persistence.

## Not currently supported by CORE's BLE protocol

The existing configuration command only implements durable output map writes/readback. The app **must not** simulate zero-latency physical geometry changes by committing flash on every slider event. Changing physical zone offsets/lengths/roles/color-order, individually selecting physical pixel addresses, and applying new startup/shutdown policies in hardware therefore remain local until CORE offers a **volatile, authorized and automatically reverting preview transaction**.

Required future firmware API: authenticated session-bound preview begin, bounded volatile layout/pixel-effect delta, revision-checked commit, revoke/expiry on disconnect, zero-time rollback on emergency/STOP, normal trigger precedence, and readback of the active volatile state. Android must negotiate this explicit capability before sending these commands; never invent an opcode. Production persistence remains explicit, validated, atomic with NVS readback, and cannot occur from drag callbacks.

## Release evidence

Source/Compose CI proves only compile and static contracts. Before enabling the Compose launcher, physically test offline v4 import/export and pixel persistence, responsive zone offsets during drag, hardware FX rate and TTL, STOP/background behavior, authorized vs unauthenticated commands, reconnect, independent outputs, theme parity with the locked design, and actual pixels after CORE adds volatile geometry support.
