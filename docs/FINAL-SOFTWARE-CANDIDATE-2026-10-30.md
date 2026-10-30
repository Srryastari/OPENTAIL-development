# OpenTail final CORE software candidate — 2026-10-30

**Status:** freeze *candidate*, benchmark/host validation only. Not production, no hardware acceptance, no flash, no merge to main/master.

## Exactly pinned independently reviewed inputs
| Owner | Pinned SHA | Integration source |
| --- | --- | --- |
| Firmware CORE #49 | `adcdff0b2f43061bc3e222d1d77de0a34b095c0a` | Runtime foundation; **no changes** to core-firmware/main |
| Triggers #48 | `e44a98fcaa51ecbf5e9c15bf85b8d129adb4e1a9` | One source contract test, exact blob `c2a81a83ef5f7640cfde85248e8721bb95be7e4a` |
| Effects #50 | `1767b144e193ed6978816899ebb5eba237ee6e2d` | Three Effects tests plus test-runner addition; **no** effects/renderer runtime changes |
| Native Android #21 | `73827e55a503670ac21c8554e4d9f14de27749cf` | Entire native Android source subtree, exact Git tree `88ade4b946861c9e74ce2b0a780f9fbe84792159` |

A purpose-built integration branch carries the four inputs as **read-only approved source bytes**, not merges of Triggers, Effects or Android PRs. Firmware runtime remains byte-for-byte identical to #49. Full Android native Kotlin/Compose project is included; no WebView/Studio APK HTML runtime. Studio is a separate, unchanged web authoring product.

## Software contracts
- Trigger: 8 physical optocoupler inputs, safe remapping and universal output zones, simultaneous Brake/Turn/hazards/Reverse/AUX, show resumption and STOP, BLE auth and safety priority; no decorative overlay or gamma on physical trigger override.
- NVS: CRC-protected previous/candidate active+staged journal, safe finalization before RAM publication, GPIO any-edge epoch and quiet release, recovery after reboot with held inputs; monotonic non-wrapping trigger and per-output revisions with distinct active/staged readback. Legacy wire packet sizes unchanged.
- Effects: all 120 source-derived modes and golden tests, shared harmonic palette tick, interpolation/custom palette v3, particles/spark/flare overlays, immutable frame snapshot, gamma table (decorative only), no regression in show/STOP and zone isolation; source parity is bounded by tested profiles.
- Android: native Compose six-section UI with locked OLED/white/pill design; versioned offline authoring; BLE owner authorization, custom palette CRC/revision negotiation, readback-fenced staged saves, exact CORE 120-mode ID/name parity. Native APK is a separate artifact from CORE.
- Studio: standalone browser animation designer, unchanged from the CORE base.

## Required release distinction
The CI workflow `verify-opentail-software-freeze.yml` checks out the exact PR HEAD for provenance, host, ESP-IDF 5.5.5 ESP32-S3 build, Android JUnit/debug APK, and Studio. `scripts/check_final_software_contracts.py` refuses any CORE runtime source edit, mismatched approved owner SHA/tree, unexpected integration file change, or modifications to the **release-evidence-gate**. The legacy Android WebView checks in the active-source workflow are replaced with native tests; physical evidence gate logic remains bit-identical to #49.

Passing all software workflows establishes a *frozen software source candidate only*. The mandatory release gate is intentionally blocked until independent 22 physical acceptance checks, real 12V input/edge ISR latency, NVS flash write interruption and brownout injection, actual pixel electrical/optical RGB + gamma parity, resource/FPS/heap measurements under installed LED load, dual-ESP hardware takeover, true BLE/OTA recovery and old-firmware rollback compatibility have been validated. No GPIO/fault host model substitutes for physical evidence. No merge into main, flash authorization or road-use approval is implied.

Source-review owner sign-offs remain tied to each exact input SHA. Any change to production code, protocol, application, or test bytes requires a *new* combined SHA and full revalidation; do not imply previous independent sign-offs automatically cover a modified runtime.
