# Active development baseline — 2026-10-26

| Area | Tracked baseline | Verification status |
|---|---|---|
| CORE | `core-firmware/` extracted from `opentail-core-v0.1.24-ble-boot-enrollment.zip` | ESP-IDF 5.5.5 ESP32-S3 compile and host tests succeeded in PR #8 CI. Flash and safety/failover not tested here. |
| Studio | `web-studio/opentail-v0.19.50.htm` and `web-studio/opentail.htm` | File consistency check succeeded in PR #8. Browser/device regression still required. |
| Android | `android/` extracted from `opentail-android-v0.1.15-approved-compact-status-easy-build.zip` | Debug APK compiled in PR #8 CI. Phone/BLE regression still required. |
| iOS | None | Not developed. |
| Hardware | `OpenTail-CORE-Flux-RevA-Design-Package.zip` in archive | Design and electrical safety review still required. |

Original historical files are retained in `archive/library-releases/original-files/` with the SHA-256 manifest. PR #8 merged the active baseline into `main` after all four *dedicated* verification jobs passed. Generic source-detection CI checks are not substitutes for target builds.

## Remaining release gates

1. Verify Android on a physical phone, including authorized BLE connection, Output 1/2 readback and persistence after power cycle.
2. Test Studio real WLED communication, live indexed pixels, restore after stopping effects, and simultaneous trigger arbitration with actual input hardware.
3. Measure controller memory/CPU/LED refresh rates and validate any particle system or effects on physical WS2815 chains.
4. Independently verify hardware fallback, watchdog and road-lighting behavior before considering vehicle deployment.
5. Progress stacked PRs #4/#5/#6 through code and manual validation; the newer UI branch is not part of `main` yet.

Passing automated checks does **not** establish hardware roadworthiness, secure BLE onboarding, failover or full visual parity.
