# Offline preview source and attribution

The native Android preview compiles byte-preserved pure-C render sources from
Effects Team #50, `1767b144e193ed6978816899ebb5eba237ee6e2d`:
`android/app/src/main/cpp/core/SOURCE.json` records each source file's SHA256.
The source registry supplies 120 WLED-derived modes, Embers and 19 original
OpenTail modes. Firmware, hardware drivers, BLE, NVS and trigger arbitration
are not linked into this local renderer.

WLED source pin: `20e7a717664295eefc6ad77972cee1888d2da072`.
Copyright (c) 2016 Harm Aldick and individual WLED contributors.
WLED-derived code is EUPL version 1.2 or later; see LICENSE. Original OpenTail
modules preserve their existing notices; no new public license is asserted.

Android changes are confined to the native bridge/lifecycle, display clock,
frame-stable parameter/palette input, measured Android microphone input for
Rocktaves, and JNI ARGB delivery. The authoritative C files are unmodified.
The five-mode Java approximation has been removed.

Full coverage means the pinned CORE's 140-effect algorithms and supported
inputs, not every upstream WLED configuration. Source-default/custom options
not represented in CORE's config remain at source defaults. Local RNG is seeded
for independent reproducibility; its random trace is not the physical device's.
Verified custom stops are required for custom palettes; no fallback palette is
invented. Rocktaves requires microphone permission and real audio. Microphone
PCM stays local/in memory, is never persisted or sent over BLE, and capture stops
when its local preview pauses/disposes/backgrounds. Missing inputs fail closed.

The preview is one logical 30-pixel segment. It does not claim physical geometry,
trigger/OEM overrides, optical gamma, bus calibration or controller telemetry.
Actual connected snapshots retain their independent validated BLE path.

Distribution must preserve applicable EUPL notices/source access obligations.
The license and this notice are included in APK assets as wled-LICENSE.txt and
wled-preview-NOTICE.txt. Full source is in the Android PR/repository.
