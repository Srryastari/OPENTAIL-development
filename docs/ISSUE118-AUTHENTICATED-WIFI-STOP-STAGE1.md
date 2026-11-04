# Issue #118 — authenticated Wi-Fi secondary STOP, stage 1

## Connection policy (owner-locked)
BLE and Wi-Fi coexist against ONE ESP32-S3 CORE. BLE owner-HMAC stays preferred for commands. Android uses Wi-Fi only after prior owner BLE authorization, a matching physical controller ID, and currently joined local CORE Wi-Fi. No extra UI, screen, firmware version change, MCU, radio power switch, ignition mode, or effect code.

## New narrowly scoped command
- GET `/api/v1/owner/challenge` on CORE AP. Requires an existing enrolled BLE owner key; generates unpredictable 16-byte nonce held ONLY in RAM. TTL 8 seconds. Single latest challenge, invalidates earlier challenge.
- POST `/api/v1/owner/stop` with empty body and `X-OpenTail-Nonce` (32 lowercase hex) and `X-OpenTail-Proof` (64 hex). 
- Proof is HMAC-SHA256 with the existing **16-byte BLE owner key** over `ASCII("OpenTail-WiFi-v1\\nPOST\\n/api/v1/owner/stop\\n") || nonce[16]`. POST method and exact path are bound. Never transmit the key, log it, or use public `controller_id` as proof.
- CORE consumes nonce on any attempted request, rejects wrong nonce, proof, stale challenge, bad headers, body or missing enrollment. On success it calls the same `stop_post` arbiter as old bench REST STOP. Physical Brake/Turn/Hazards/Reverse remain untouched.
- Android stores key only in existing encrypted BLE owner vault and signs one STOP in memory. It does not automatically retry after uncertain network timeout, silently associate to Wi-Fi, issue stored FX writes, or reuse another controller's replies.

## Security limitations (must stay visible)
**THIS DOES NOT YET SECURE LEGACY BENCH REST APIs.** Original developer endpoints (`/api/v1/show/start`, `/api/v1/demo`, `/api/v1/stop`, `/api/v1/ota` etc.) are still historical password-protected AP bench routes, not authenticated by owner HMAC. Do not claim end-to-end secured Wi-Fi control or road readiness until every legacy mutable route is migrated or disabled and a production transport threat review passes.

Full Issue #118 remains open for effects, palette, NVS-revisioned configuration, authenticated Wi-Fi OTA, cross-transport transaction ownership, legacy REST hardening, Wi-Fi joining/roaming and actual reconnection tests. No NVS writes on link polling.

## Tests
- Pure-C challenge and HMAC admission tests: legitimate STOP, wrong nonce, bad HMAC, reuse, expiry, absent owner; JNI and physical outputs not simulated.
- Android JUnit HMAC-SHA256 known vector and negative cases; existing Android UI contracts, BLE v3 and 140 offline effect regressions.
- ESP-IDF v5.5.5 host and ESP32-S3 firmware compile; four-image (including actual 0xF000 OTA data) package checksum verification.
- Mandatory physical phone/bench STOP delivery, BLE→Wi-Fi→BLE handoff, actual LED/trigger priority tests **NOT RUN**. No merge/flash without review. Original 22 physical release gates intact.
