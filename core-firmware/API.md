# CORE Alpha v0.1.22 BLE bench-show protocol (v1)

Existing HTTP endpoints and read-only BLE UUID are unchanged.

## HTTP (CORE WPA2 AP, `192.168.4.1`)

`GET /api/v1/ble/session` returns `{"protocol":1,"token":"32 lowercase hex characters","bench_only":true,"volatile":true}`. Requires local WPA2 AP access; avoid sharing response/logging secrets. The token is random each ESP32 boot and is **not** encrypted on the BLE connection in this prototype.

`GET /api/v1/capabilities` includes `ble_bench_show_v1:true` and `ble_session_requires_wifi:true`. The existing Studio WLED/Wi-Fi capability field `ble_studio_controls:false` remains true: the Web UI does not issue BLE commands.

## GATT

Service UUID: `55a56022-4915-7a92-cf44-b8b7102eaf21`.
- Information READ `55a56022-4915-7a92-cf44-b8b7102eaf22` (unchanged).
- Command WRITE REQUEST `55a56022-4915-7a92-cf44-b8b7102eaf23`.
- Last-result READ `55a56022-4915-7a92-cf44-b8b7102eaf24`.

Wire protocol uses unsigned bytes, v1. AUTH: `[0x01,0x01,<16 raw token bytes>]`. SHOW: `[0x01,0x02,mode,output_mask,zone_mask,R,G,B,brightness,speed,ttl_sec]`, mode 1–5 matching solid/chase/breathe/wipe/rainbow; mask outputs 1,2,3 and zones 1..255; brightness 0..32, speed 0..100, TTL 1..60. STOP: `[0x01,0x03]`. Read result after GATT write completes (`OK AUTH`, `OK SHOW`, `OK STOP`, `ERR AUTH`, `ERR RANGE`, `ERR FORMAT`, `ERR BUSY`).

Re-auth is required after every BLE disconnect; tokens are not saved on Android. Shows are volatile and expire. All physical trigger input arbitration and permanent zone configuration remain on firmware/Wi-Fi without BLE writes.

**NOT FOR ROAD LIGHTING:** WPA2 Wi-Fi bootstrap plus token over unencrypted BLE is not production-grade authentication or transport security.
