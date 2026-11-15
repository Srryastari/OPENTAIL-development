# Issue #102 — eight source-specific third-color verdicts

**Baseline:** Effects #95 reviewed `984a12aeec4e348b869ba2504a2f994fc944f7f2`, stacked on Issue #99. The same 140 IDs, existing animation identity, trigger priority and UI geometry remain locked.

| WLED ID | Mode | What source actually renders | Tertiary policy | Verdict |
|---|---|---|---|---|
| 10 | Scan | Foreground moving head over background; third metadata is not consumed in the supported 1D default (overlay option not implemented) | Ignore third; preserve existing color-role metadata | NOT SUPPORTED |
| 11 | Scan Dual | Symmetric pair of FG moving heads over BG; no independent 3rd paint in supported 1D | Ignore third | NOT SUPPORTED |
| 38 | Aurora | Waves sample authored palette. Explicit palette 4 (Colors 1,2,3) accepts third RGB as palette ingredient, whereas default Aurora palette 50 must remain source-authentic | Use `tertiary` only for explicitly resolved three-color eligible palette, with legacy zero third when unset | PASS, conditional |
| 40 | Scanner | Source renderer fades prior frame and paints FG moving scan | Third editor metadata represents unported source options, not current core RGB slot | NOT SUPPORTED |
| 48 | Rolling Balls | One color of balls FG; base BG; no third channel in supported default 1D | Third ignored; source and seeded dynamics unchanged | NOT SUPPORTED |
| 60 | Scanner Dual | Dual reflected FG stroke with retained fade/BG; no third paint in supported default | Third ignored | NOT SUPPORTED |
| 91 | Bouncing Balls | FG balls with BG, no third slot in accepted source options | Third ignored | NOT SUPPORTED |
| 95 | Popcorn | Source RNG draws 0/1/2 per spawned particle, but prior CORE collapsed every palette0 draw to FG | Preserve RNG; when `tertiary_set=true` use actual FG/BG/tertiary index 0/1/2, including true black; otherwise keep old FG-only source behavior | PASS, explicit |

`ot_wled_fx_tertiary_supported(id)` is a new pure C capabilities query which returns true for 13 authoritatively implemented IDs (11 from #95 + Aurora38 + Popcorn95). It does *not* alter BLE capability responses or opcode11 admission. Firmware and Android teams must independently review exact integrated SHA and maintain the transport whitelist until proven safe. Do not infer support from metadata slots alone.

Tests: `test_issue102_third_roles.c` compares two independent seed/state renderers with only tertiary differing. For six unsupported modes, frames must remain binary-identical; for the two verified ones, frames must differ, while non-enabled legacy modes remain binary-identical. The test asserts all 140 IDs. Run in the full existing CORE host suite with source RGB/golden references, safety and ESP-IDF v5.5.5.

**Status:** Software source-only. Phone, JNI repin and 22 physical tests NOT RUN. Backup failover NOT VERIFIED. Vehicle release BLOCKED. PR draft/unmerged.