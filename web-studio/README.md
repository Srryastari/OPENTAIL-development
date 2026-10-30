# OpenTail Studio — web authoring

OpenTail's desktop/browser Studio and Android app are two **independently
implemented interfaces** to the same OpenTail project format. Neither
replicates the CORE renderer or requires the ESP32 to design animations.

## Product boundaries

- **CORE firmware:** LED frame generation, triggers/arbitration, output
  configuration, persistence, native effects and physical signal I/O.
- **CORE developer dashboard:** `http://192.168.4.1/dev` owns physical
  lighting tests, NVS output mapping and firmware diagnostics.
- **Web Studio v0.19.120:** an independent HTML authoring workspace with the
  approved six-item OLED/white interface, effects/sequences/presets, triggers,
  user-created zones, offline previews and project import/export.
- **Android Studio v0.2.0-alpha1:** genuine native Android Views, Canvas
  previews and native dialogs. **No WebView, HTML/JS bundle or WebKit library.**
  Android has its own release cycle, theme and device test gates. Native
  optional BLE actions are confined to Settings.

## Project interchange

Web Studio exports OpenTail **v4 JSON**: a project name, universally named and
indexed zones, trigger profiles/assignments, sequences, custom effect presets
and optional startup/shutdown design settings. Native Android imports and
exports this format through the Android document picker. This JSON is
**not** a deployable firmware profile yet; CORE's validated project upload
protocol remains a separate implementation milestone.

Existing Android v0.1.30 **WebView localStorage will not migrate itself**.
Export a project JSON from that old app *before* installing native Android,
then import it from native Settings. Do not clear the previous application's
data before creating a backup.

## Versioning and regression policy

| Component | Current | Artifact |
|---|---|---|
| Web Studio | 0.19.120 | `web-studio/opentail.htm`, `web-studio/opentail-v0.19.120.htm` |
| Native Android | 0.2.0-alpha1 | `android/app/build.gradle`, native Java sources |
| CORE firmware | 0.1.28-alpha dev bench | `core-firmware/` |

The stable web HTML must be byte-identical to the matching versioned **web**
release. Android only needs to preserve the **shared JSON schema**, capability
identifiers and approved appearance; it must not bundle the HTML or be
recompiled when web Studio changes. CORE firmware compiles independently of
both design apps.

Approved navigation: **Home → Show Modes → Layout → Triggers → Auto Events →
Settings**, one top-level navigation surface, no duplicate bottom tabs.
Approve OLED-black/white typography, subtle capsules and native field/slider
geometry separately on Android hardware. No test suite or build makes a
physical failover or road-safety claim.
