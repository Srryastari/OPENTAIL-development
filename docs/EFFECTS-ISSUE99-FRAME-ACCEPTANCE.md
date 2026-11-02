# Issue #99 — source renderer acceptance (draft, host only)

Parent source: Effects #95 `984a12aeec4e348b869ba2504a2f994fc944f7f2`; no Android UI, BLE transport, OEM arbitration, NVS, or firmware version changes.

| ID | Reported symptom | Root-cause change | Software host evidence | Owner phone |
|---|---|---|---|---|
| 1 Blink, 23 Strobe, 24 Strobe Rainbow, 26 Blink Rainbow | maximum-speed appears stuck | WLED 255 uses frame-locked inclusive on-time + cycle-catch-up; explicit 255 uses strict half-cycle | `test_issue99_frame_quality.c`: 8/16/33ms 16 frames, 8 ON/8 OFF per mode; other speeds preserve source parity | NOT RUN |
| 68 BPM | brightness flickers / snaps on phase wrap | 8-bit modulo subtraction flashes negative levels bright and sawtooth resets at 5.12s; use reflected ramp + saturated luminance | frame identity/delta over wrap, deterministic 8/16/33ms | NOT RUN |
| 28 Chase | unwanted background light | existing CORE already honors RGB0 background and no brightness floor; third moving band remains authored | black gap and moving FG test | NOT RUN |
| 29 Chase Random, 30 Chase Rainbow | implicit colored background despite BG=#000000 | source wheel floor now yields exact black if explicitly selected BG black, while nonblack BG retains wheel | black-gap tests and nonzero authored BG upstream parity | NOT RUN |
| 116 TV Simulator | near-static/frozen scenes | original scene duration truncated to uint16 and grew with Speed, sometimes minutes-long. Use full 32-bit timer and faster high-Speed scene durations while retaining seeded per-zone TV flickers | two independent seeded instances match, multi-scene frames change, guards unchanged | NOT RUN |
| 32778 Fusion Core | resembles Solid Glitter | restore coherent concentric white energy boundary, retain spatial aura and seamless phase | reseeded 12-proprietary pinned golden (Fusion updated), 60+ frame motion trace | NOT RUN |
| 32786 Event Horizon | not visible | strengthen leading white ring around moving black gravitational core, preserve terminal FG | multi-frame moving-front trace, one-shot endpoint; no looping claim | NOT RUN |
| 55 Tri Wipe, 56 Tri Fade, 103 Solid Glitter | tri colors fail or leak | prior #95 supplied explicit third-color contract; #99 adds region/frame extreme-speed regression | independent slots / true black and speed cases | NOT RUN |
| 2 Breathe, 12 Fade | perceptual stair-stepping | authored source 8-bit waveform is retained; no proven CORE monotonic time error, do not invent unapproved interpolation | bounded consecutive high-speed 8ms host frame deltas | **OPEN** |
| 90 Fireworks 1D | stutter | source-driven retained per-frame particle physics retained; no independent proof host frame scheduling is source of phone stutter | byte-identical two-zone, same-seed 23ms physics traces | **OPEN** |
| Second-output hardware | suspected stutter | physically untested; no pin or bus changes | host no output bleed / trigger regression | **NOT RUN** |

**Important parity accounting:** Issue99 intentionally changes WLED behavior in *only* documented speed=255 Blink/Strobe, BG-black Chase 29/30, BPM ID68, and TV ID116 cases. `test_wled_source_parity.cpp` continues to execute the upstream and CORE reference loops but excludes only these authorized deviations from strict RGB equality, covering changed behavior with independent temporal tests. Remaining pinned WLED profiles retain original byte equality; do not label this as complete 120-mode unqualified WLED source parity for the deviations. Proprietary Fusion Core has an explicitly updated golden while other 11 original hashes remain frozen.

**Regression gate:** `core-firmware/tests/run-host-tests.sh` includes Issue99 tests alongside 140-mode matrix (5 speeds × 3 intensities × 0/255 brightness), proprietary golden/one-shot, palettes, NVS, OEM priority, and universal zones. Dedicated exact-HEAD ESP-IDF 5.5.5 and Android *metadata only* CI required. **No physical, optical, owner-phone, JNI, backup hardware or vehicle release certification.**

**Integration:** Firmware and Android should repin shared source only after exact-head review. The approved native Android six screens remain 1:1 unchanged. Issue #102 separately covers eight third-color metadata cases and must not preempt firmware's authenticated BLE opcode11 whitelist.
