# Offline preview source boundary

`OfflineFxSketch.java` adapts only Solid (0), Blink (1), Wipe (3), Rainbow
(9), and Fade (12), with palette 0, from
`core-firmware/main/wled_fx_compat.c` at Effects #50 commit
`1767b144e193ed6978816899ebb5eba237ee6e2d`.

That implementation adapts WLED `FX.cpp`, `colors.cpp`, `FX_fcn.cpp` and
`fastled_slim.cpp`, pinned upstream commit
`20e7a717664295eefc6ad77972cee1888d2da072`.
Copyright (c) 2016 Harm Aldick and individual WLED contributors.
License: EUPL version 1.2 or later; see [LICENSE](LICENSE).

Android changes: Java integer-width handling, reusable ARGB destination,
monotonic display-clock time, bounded parameters, and no physical output I/O.
Linear brightness rounding and Blink's sparse-frame rule are retained. Local
Blink uses a nominal FRAMETIME of 16 ms; CORE supplies its own runtime interval,
so matching RGB vectors at this interval is not a hardware timing claim.
Differential host tests compare complete RGB frames with the pinned C source.

These are local illustrations of a single 30-pixel logical segment. They do
not model installed geometry, gamma, bus/global brightness, trigger arbitration,
custom options, dynamic/custom palettes, or the other effects. Unsupported
effects/palettes remain unavailable. No 140-effect parity is claimed.
The existing local-preview label is preserved. Actual connected frames still
come only from complete, sequence/offset/CRC-validated CORE captures.

Distribution of this WLED-derived code must preserve its EUPL notices and
applicable source availability obligations; the license is included in source
and in the APK assets (`wled-LICENSE.txt`, `wled-preview-NOTICE.txt`).
