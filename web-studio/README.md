# OpenTail Studio — independent authoring application

## Product boundaries

**OpenTail Studio creates projects**: effects, animation parameters, sequences, presets,
universal layouts, trigger assignments and offline previews. Its approved six-item
OLED/white-mode authoring UI is intentionally independent of the ESP32-S3 build.

**OpenTail CORE renders and drives LEDs**: controller firmware, BLE auth, stored
configurations, effect execution, trigger arbitration, physical output and status LED.

**OpenTail Dev** runs on the controller at `http://192.168.4.1/dev` and owns all
physical input tests, effect bench previews, output NVS mapping, diagnostics and
status-LED testing. No duplication of the firmware development dashboard in Studio.

Studio's existing project export/import is **local JSON**; importing a file into
Studio does not upload it to a controller. Whole-project/sequence deployment needs
a separately versioned contract and CORE validation before enabling it. Studio may
optionally connect to CORE, but offline authoring works without an ESP32 attached.

## Independent versioning

| Deliverable | File | Current version |
|---|---|---|
| Stable web authoring UI | `web-studio/opentail.htm` | **0.19.119** |
| Pinned web release | `web-studio/opentail-v0.19.119.htm` | **0.19.119** |
| Previous web rollback | `web-studio/opentail-v0.19.118.htm` | 0.19.118 |
| Android bundled authoring UI | `android/app/src/main/assets/opentail.htm` | **0.19.118** |
| Android native wrapper | `android/app/build.gradle` | 0.1.29 |
| CORE firmware | `core-firmware/main/ot_version.h` | 0.1.28-alpha |

**Only the stable web source and the current pinned web release must be byte-identical.**
Android must match its **own** pinned asset/hash and preserve its own approved native
navigation, but does not have to rebuild every time the standalone web designer changes.
Promoting Studio v0.19.119 to Android is a separate, explicit app release.

Web Studio-only changes must not rebuild CORE firmware. Android-only changes must
not rebuild CORE or the web designer. CORE changes must pass CORE host tests and
ESP-IDF compile independently. The `verify-active-sources` workflow selectively
runs each product's tests based on changed paths.

## Approved design baseline

Navigation: **Home → Show Modes → Layout → Triggers → Auto Events → Settings**.
Keep the approved OLED black/white styling, native fonts, capsule sliders/fields,
contextual Layout editor, multi-select Trigger authoring, and local previews.
Do not restore duplicated bottom tabs or the legacy Diagnostics workspace.
Hardware-safe bench verification and road-lighting safety claims remain separate
from editor UI checks.
