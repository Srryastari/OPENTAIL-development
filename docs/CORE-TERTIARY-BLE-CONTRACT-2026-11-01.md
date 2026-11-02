# OpenTail CORE P0 — tertiary color BLE integration, bench-only

## Immutable provenance
- Baseline Android/CORE PR #94: `b970f92c9155ac9e2d2c9ffa54afa2d1ca3502cd`.
- Effects-owned authoritative shared renderer PR #95: `984a12aeec4e348b869ba2504a2f994fc944f7f2`, directly based on #94.
- This Firmware branch starts at the exact #95 source commit and changes only BLE admission, volatile zone-state wiring, CORE renderer parameter transfer, focused tests/docs/workflow. The source renderer files `wled_fx_compat.c/.h` and `effects_1d.c/.h` remain **byte-identical to Effects #95**.
- `OT_FW_VERSION`, Android UI, JNI source, NVS output/trigger/profile/preset formats, OTA and all physical release gates are unchanged. **No owner-authorized firmware version bump**.

## Capability negotiation and precise minimal wire format

The existing GATT service and COMMAND characteristic UUIDs are reused. The unchanged INFO text retains `continuous-fx-v1` and appends **`fx-tertiary-v1`**. Clients must discover this exact token and authenticate with the existing HMAC owner/session mechanism before sending the new command. No version-number inference. A missing token means do not send opcode 11; older clients continue opcode 5 (17-byte TTL) or opcode 10 (17-byte continuous) unchanged.

**New 20-byte ATT-value opcode** — fits an ATT MTU of 23 (20-byte GATT value):

| Byte(s) | Description |
| --- | --- |
| 0 | protocol 1 (same existing command namespace) |
| 1 | new opcode **11** = tertiary continuous Show Mode |
| 2–3 | existing u16LE effect ID |
| 4 | output/chain bitmask, physically configured output IDs only |
| 5 | zone bitmask, validated against configured, nonempty zones |
| 6–8 | foreground RGB, same as old command |
| 9–11 | background RGB, same as old command |
| 12 | absolute bench brightness cap, same as old |
| 13 | Speed |
| 14 | Intensity |
| 15 | permitted palette ID, existing source admission logic |
| 16 | presence flag exactly **1**: third RGB is explicitly set |
| 17–19 | tertiary RGB888, with **0,0,0 meaning true black** |

No TTL is present in opcode 11. Its lifetime is continuous until replaced/stopped, identical to opcode 10; it does **not** revert to a 60-second keepalive, trigger a one-shot, persist on NVS, start on boot, or restart after an output fault. Length, version, unknown flag, unauthorized owner, invalid palette/effect/chain/brightness and targets are rejected without mutation. The older 17-byte opcodes remain byte-for-byte unchanged, including their final TTL/reserved rules. No extra characteristics or GATT UUIDs are needed, and no remote fallback is fabricated.

The firmware initially accepts third RGB only for **11 source-verified consumers** from Effects #95:
- Chase 28, Chase Random 29, Colorful 34, Two Dots 50, Running Dual 52, Chase 3 54, Tri Wipe 55, Tri Fade 56, Solid Pattern Tri 84, Glitter 87, Solid Glitter 103.

The source roles remain authoritative including **Solid Glitter: BG / disabled / GL** (do not compact slot positions). Eight other declared third-role modes **10, 11, 38, 40, 48, 60, 91, 95** remain NOT VERIFIED and are deliberately rejected for explicit third-color transport pending Effects source-specific proof; the legacy two-color effects still work. Existing palette admission, effect-specific metadata and global palettes are not bypassed. No additive brightness floor: third black means zero.

## Volatile CORE application and isolation
- New parsed `ot_ble_fx_tertiary_t` is separate from the **unchanged** `ot_ble_fx_packet_t` structure. Hence existing `fx_preset_record.h` scene/preset records, CRC, schema versions and NVS size remain unchanged.
- `fx_zone_bank` holds the tertiary only in a new in-RAM field in each live zone slot. A new continuous setter accepts optional third color with the same existing all-target validation, mask resolution and generation/phase-preserving update. Existing 17-byte commands reset tertiary to omitted/default without resetting an active same-effect phase. An update to Output 1/Zone 1 cannot alter Output 2/Zone 1 or a different active zone.
- The existing CORE render task copies the slot's tertiary to `ot_wled_fx_config_t`, preserves Effects #95's own exact renderer math, and keeps physical Brake, Turn, Hazards, Reverse overlays **after** decorative effects. STOP, output RMT faults and inactive zone logic remain unchanged. No Android-only approximation.
- Existing debug developer Studio effects, saved scenes and manual preview do **not** gain third-color persistence; an older 17-byte command never claims a third color.

## Acceptance and explicit non-claims

Dedicated exact-head CI must verify the pinned source blobs, all existing CORE host tests plus new 20-byte admission/auth/palette/zero-black/11 IDs/8 excluded IDs/per-zone targeting/continuous-phase/STOP checks, complete Trigger regressions and ESP-IDF **v5.5.5 ESP32-S3** build. Only after all jobs pass upload the exact-SHA bench application, bootloader, partition and flasher manifest. Firmware version remains unchanged, so select binaries by exact source SHA, not the displayed version label.

**Android handoff:** Android Architecture owns adding capability-gated opcode 11 serialization to existing approved editor controls, updating its shared JNI renderer source and frozen file hashes, and re-running exact 140-effect JNI oracle comparisons; Firmware does not change the six approved native screens or Android sources. An Android control appearing on screen is **not** proof that CORE received tertiary. Ask Android to report explicit GATT acknowledged write, selected per-zone readback/frame and on-device LED proof separately. The eight unverified third-role modes remain gated.

**Safety:** Existing physical Brake/Turn/Reverse/aux priorities, STOP, output fault and NVS behavior remain authoritative. All 22 mandatory physical release checks are unverified, automatic dual-MCU hardware failover unimplemented, and vehicle release blocked. This is a software/bench candidate only; **no merging, flashing, release certification or version bump** is implied.
