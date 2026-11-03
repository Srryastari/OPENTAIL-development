# OpenTail BLE-first + Wi-Fi-secondary combined controller: implementation receipt

## Locked architecture
- ONE ESP32-S3 N16R8. BLE and WPA2 diagnostic Wi-Fi AP run simultaneously against the SAME live CORE state.
- Native Android six-screen UI layout unchanged; existing Connection/Controller Status text reflects transport state.
- Authenticated BLE is always preferred for lighting/configuration commands. Physical eight GPIO inputs and real RMT outputs do not depend on any radio.
- Wi-Fi is **secondary** and uses the original `192.168.4.1` HTTP diagnostic route when Android is already associated to the CORE AP. The app does not join Wi-Fi automatically or disconnect BLE.
- **No unauthenticated Wi-Fi writes:** read-only HTTP status is the only current app fallback while an owner-authenticated Wi-Fi command protocol is absent. This is an intentional security gate, NOT completed seamless control failover.

## New work
- CORE exposes `controller_id` from its base MAC in both BLE Info (additive `| id=...` token) and existing Wi-Fi `/api/v1/status` JSON. Public ID is not secret or authentication; never authorize writes from identity alone.
- Android remembers this ID only after the existing BLE HMAC-owner authorization. Matching device is mandatory before accepting a Wi-Fi status response. No identity on legacy CORE => refuse Wi-Fi fallback safely.
- Pure Java `ControllerLinkPolicy` prefers BLE, accepts lease-fenced Wi-Fi status only from same controller, expires stale results, clears identity on explicit controller switch, and NEVER grants Wi-Fi writes.
- Wi-Fi probe uses explicit Android Wi-Fi Network routing (not cellular default), short bounded read, no redirects, no state-changing HTTP calls. Scoped cleartext policy allows only local AP host; all other cleartext remains forbidden.
- No Android XML/Compose visual structure changes, no new screens, no brightness/effects/zone or NVS changes, no CORE version bump.

## Tests / non-claims
- `ControllerLinkPolicyTest`: BLE priority, read-only Wi-Fi status, stale expiry, controller-switch lease, mismatched MAC, legacy info fallback.
- Android exact SHA JUnit and debug APK; firmware host and ESP-IDF v5.5.5 build / SHA256 four-image artifact are CI acceptance requirements.
- Bench-only. No claim that Wi-Fi is cryptographically authenticated, that fallback supports command delivery, that the app can join AP without an OS-approved connection, or that physical failover exists.
- DO NOT flash/merge/road-use based on this PR alone. Independent Trigger, physical 22 gates, essential-light failure-path acceptance and real phone/board transport switching remain outstanding.

## Next bounded security integration
Define an owner-authenticated Wi-Fi command interface with anti-replay and exact request/response and active/staged-revision semantics, then migrate BLE command clients to the shared coordinator one operation at a time (STOP, live FX, outputs, trigger map, palettes, OTA). Must re-authorize after transport switch, reject ambiguous writes (never retry mutation blindly), and preserve current Android UI 1:1.
