# OpenTail development

Universal automotive addressable-lighting controller project using ESP32-S3 firmware, Web Studio, Android and planned iOS clients, and custom hardware.

## Repository layout

- [`core-firmware/`](core-firmware/) — active ESP32-S3 firmware workspace
- [`web-studio/`](web-studio/) — active browser/WLED Studio workspace
- [`android/`](android/) — active Android app workspace
- [`ios/`](ios/) — planned iOS app (not yet developed)
- [`hardware/`](hardware/) — controller electronics and PCB workspace
- [`docs/BASELINE.md`](docs/BASELINE.md) — release selection and verification gates
- [`archive/library-releases/`](archive/library-releases/) — immutable original historical releases and SHA-256 manifest
- [`scripts/`](scripts/) — import tooling

**Status:** historical archives and the extracted v0.1.24 CORE / v0.1.15 Android / v0.19.50 Studio baseline are now on `main`. GitHub Actions compiled the ESP32-S3 firmware and Android APK and ran baseline source checks on PR #8. These are **not** physical controller, BLE phone, power, or automotive safety validations. See [`docs/BASELINE.md`](docs/BASELINE.md).
