# Active development baseline

| Area | Archived candidate | Status |
|---|---|---|
| CORE | `opentail-core-v0.1.24-ble-boot-enrollment.zip` | Archived; extraction/build pending |
| Studio | `opentail-v0.19.50.htm` | Archived; deployment/functional checks pending |
| Android | `opentail-android-v0.1.15-approved-compact-status-easy-build.zip` | Archived; extraction/build pending |
| iOS | None — planned | Not developed; no build available |
| Hardware | `OpenTail-CORE-Flux-RevA-Design-Package.zip` | Archived; design review pending |

All original files live under `archive/library-releases/original-files/` and are inventoried in `archive/library-releases/SHA256-MANIFEST.csv`.

## Acceptance gates

1. Extract the chosen release into its working directory without modifying the archive.
2. Record toolchain and exact build steps for CORE and Android.
3. Build both software projects, and test Studio against the bench controller.
4. Run regression tests for trigger arbitration, persisted output settings, and backup/failover behavior where hardware permits. Do not infer automotive safety certification from bench tests.
5. Commit tested source and results, then merge the baseline PR.
