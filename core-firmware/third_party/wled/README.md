# WLED effect-source attribution and port boundary

Source project: https://github.com/wled/WLED  
Pinned upstream commit: `20e7a717664295eefc6ad77972cee1888d2da072` (2026-10-25)  
Source files: `wled00/FX.cpp` and `wled00/FX.h`.  
WLED contributors; original effect lineage copyright (c) 2016 Harm Aldick.  
License: **EUPL v. 1.2 or later** (full upstream license copied to this folder).

OpenTail's `main/wled_fx_compat.c` is an isolated **adaptation** of WLED's
`mode_static`, `blink`, `mode_blink`, and `mode_strobe` timing logic.
It preserves upstream WLED IDs 0, 1, 23 and the relevant effect metadata
text. Source modifications include a pure-C input/output structure, explicit
frame timing/iteration state, 1D buffer writes, brightness scaling for the
OpenTail demo model, and deterministic host tests.

**Important differences from native WLED:**
- Only direct foreground/background colors are supported (`palette_id=0`);
  palette lookups and global/color-gamma processing are not reproduced.
- Static brightness scaling is intentionally OpenTail-specific.
- No 2D, random, audio-reactive or multi-segment effects are supported.
- This module does **not** run on the LED strip until the CORE output
  integration and precedence checks are implemented and bench-validated.
- This is not a full WLED engine import, nor evidence of parity for WLED
  effect output outside the explicitly tested timing behavior.
- Before distributing firmware or a compiled App/APK using WLED-derived
  code, review the EUPL-1.2-or-later licensing obligations for the combined
  distributed work, including source access, modifications, attribution and
  notices. Do not assume a third-party license file alone settles compliance.

No OpenTail road-lighting arbitration, fallback, or output ownership is
modified by this port. This is a bench-only prototyping milestone.
