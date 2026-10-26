# OpenTail UI approval baseline — in-chat concept restoration

**Status:** implementation candidate in Studio v0.19.56 / Android v0.1.23. Automated regression checks pass; user phone visual approval is still pending. Neither generated illustrative screenshot is an acceptable reference.

## Visual source of truth

The original four-tab interactive in-chat concept (Triggers / Layout / Auto Events / Show Modes) is the approved reference for component geometry, icon placement, typography, state appearance and click/expand behavior. Generated images are **not** design references. The ChatGPT UI's internal stylesheet is not available as an importable CSS file; our authored CSS tokens are a first approximation that must be refined using app screenshots at equal viewports. Never claim pixel-perfect rendering solely because CSS rules or code tests pass.

Use these locked presentation rules:
- OLED black #000 main background, dark neutral surfaces, subtle gray borders; no global green/blue accent theme, gradients, glowing frames or decorative hero icons.
- Platform UI system font, compact headings and native-looking labels. No extra oversized icon in a page heading; icon belongs in its own concept navigation pill.
- Four pill navigation items with lightning bolt (Triggers), four squares (Layout), sun (Auto Events), and sparkle (Show Modes).
- Main page card with about 20px rounding and ~14px padding; zone rows nested in **one** layout card. Minimize nested framed cards in every panel.
- Neutral selected and pressed states, with accessible focus-visible outlines. The one destructive pill is muted red **Remove Zone** below the LED strip.
- Rounded project/zone name fields, neutral LED-length range, zone-only **Reverse**, one strip of uniformly sized simulation indicators in ascending data-index order. The 16-sample Layout preview must not imply live LED telemetry.
- Add Zone visible in the main Layout card header; expandable zone name rows with chevrons.

## Locked behavior
- Trigger *selection for editing* independent of Trigger *testing*. Tap a card to add/remove it. Park+Brake and Left+Right Turn are mandatory batch selections. Batch changes affect all selected Trigger profiles and assigned zones while untouched fields remain unchanged; never override zone Reverse.
- Offline Brake/Hazards simultaneous testing and existing trigger arbitration/stop/restore must remain.
- Offline Startup/Shutdown animation preview must not require WLED, CORE or Bluetooth, and must never transmit controller requests.
- Physical output mapping and WLED/CORE live commands remain separate from design-time controls; legacy safety assertions must stay intact.
- Show Modes effect paths, native effects and exact indexed live WLED pixel view are retained.

## Release checks
1. Run all source-copy parity, WebView JS parsing, CORE host + ESP-IDF build and Android APK checks.
2. Run `web-studio/tests/approved-chat-ui.test.cjs`, `pixel-direction-and-ui.test.cjs`, `offline-trigger-panel.test.cjs`, `offline-trigger-preview.test.cjs`, `layout-history.test.cjs`, and `exact-live-indexed.test.cjs`.
3. Record actual same-viewport app screenshots for nav and each submenu, including selected multiple Triggers, Layout expanded zone, live/offline LED strips, Auto Events preview and Shows. Compare side-by-side against the original in-chat UI; **visual approval remains manual until approved reference screenshots and a screenshot-diff test are committed**.
4. Run the physical WLED/CORE bench and persistence tests separately. Build success cannot establish hardware or automotive road-lighting safety.
5. PR must stay in draft; merging requires explicit user approval after actual on-device comparison.

Do not remove, hide or re-theme any previously approved component without recording the change and obtaining approval.
