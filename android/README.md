# OpenTail Android v0.2.0-alpha1 — native UI

This is a **native Android app**, not a WebView wrapper. The launcher `MainActivity.java`
uses Android `View`, `LinearLayout`, `ScrollView`, `AlertDialog`, `SeekBar`,
`CheckBox`, `EditText`, and a custom Canvas `NativeLedPreview`. There are
no bundled HTML/JS assets, WebKit dependencies, or general Internet permission.

The approved authoring structure is Home → Show Modes → Layout → Triggers →
Auto Events → Settings. Styling uses OLED black/white theme, monochrome cards,
capsule fields/sliders/buttons and single scrollable **top** navigation.

## Project compatibility

`NativeProject.java` imports/exports the web Studio's **OpenTail project v4 JSON**
through Android's system document picker. Names, arbitrary zones, zero-based
offsets, sequence steps, trigger assignments, automatic event settings, and
saved Studio design presets persist locally. No zone or vehicle geometry is
hardcoded. The native effect catalog is the same 120 CORE 1D mode IDs plus six
OpenTail originals as the independent web designer.

**Important before upgrading from the v0.1.30 WebView app:** Export your
existing project to JSON **using the older app before installing** this build,
then import the JSON from Settings in the native app. WebView `localStorage`
cannot automatically migrate into Android `SharedPreferences`. Do not
uninstall or clear the old app's data until you have a backup. If you already
installed over it and have no export, reinstalling a previous build may allow
recovery as long as its app data has not been erased (Android versionCode
downgrades usually require special installation steps).

## BLE and physical controller separation

The pre-existing native `BleDiscovery`, owner-key vault, HMAC authorization
and native GATT protocol are preserved. All controller operations are explicitly
in Settings: scan, physical BOOT enrollment, authorize, read firmware-supported
FX IDs, and a user-confirmed 15-second bench effect test. The test is blocked
without BLE authorization, a matching CORE-reported effect ID, and the
firmware's brightness cap. **No authoring action directly drives LEDs.**

CORE's independent `http://192.168.4.1/dev` owns physical trigger tests,
output/zone NVS mapping and hardware diagnostics. The Android designer's
Canvas illustration is NOT a pixel-identical rendering of the firmware.

## Current migration limitations

Android's previous WebView-specific BLE Output Manager/NVS editor, Wi-Fi
Studio bridge and injected hardware dashboards were intentionally removed
from this designer. Their protocols remain in `BleDiscovery` but are not yet
all exposed as native controls. Use CORE `/dev` for those developer functions.

The phone appearance, project file picker, BLE enrollment, effect playback
and actual LED behavior still require **on-device acceptance testing**. CI
compile and static checks cannot certify physical lighting or failover.

See `PHONE-TESTS.md` and the native-build GitHub Actions artifact.
