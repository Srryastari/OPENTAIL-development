# OpenTail Web Studio

## Single source of truth

- Stable deployment: `web-studio/opentail.htm`
- Versioned release: `web-studio/opentail-v0.19.118.htm`
- Android offline bundle: `android/app/src/main/assets/opentail.htm`

These three files must be byte-identical. CI verifies the invariant before accepting a build. UI changes should be made in the stable source and then propagated to the versioned release and Android bundle in the same change set.

The approved main navigation is a single in-flow pill row, in this order: Home → Show Modes → Layout → Triggers → Auto Events → Settings. Do not restore the legacy Dashboard/Connect/Diagnostics navigation or a fixed bottom-tab bar. The Android native shell may supply the same navigation for its own header, but it must not add a competing navigation system.

The source UI is not a physical-device acceptance test. Controller connectivity, save persistence, exact live pixel telemetry, and hardware behavior must be verified separately; offline preview data must never be presented as exact hardware output.
