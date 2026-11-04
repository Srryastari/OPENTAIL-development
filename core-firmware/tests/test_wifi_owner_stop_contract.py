#!/usr/bin/env python3
"""Scoped source regression: Issue118 first authenticated Wi-Fi STOP slice.
No phone/physical behavior is inferred by this static check."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
core=(root/"main/opentail_core.c").read_text()
owner=(root/"main/ble_status.c").read_text()
codec=(root/"main/ot_wifi_owner_auth.c").read_text()
android=(root.parent/"android/app/src/main/java/com/opentail/android")
ui=(android/"ComposeMainActivity.kt").read_text()
client=(android/"WifiOwnerStopClient.java").read_text()
proof=(android/"WifiOwnerProof.java").read_text()
for token in (
    '{.uri="/api/v1/owner/challenge",.method=HTTP_GET,.handler=wifi_owner_challenge_get}',
    '{.uri="/api/v1/owner/stop",.method=HTTP_POST,.handler=wifi_owner_stop_post}',
    'ot_wifi_owner_verify_stop(&wifi_owner_stop_challenge',
    'return stop_post(r); /* Single authoritative STOP',
    'if(r->content_len!=0)',
    'httpd_req_get_hdr_value_str(r,"X-OpenTail-Nonce"',
    'httpd_req_get_hdr_value_str(r,"X-OpenTail-Proof"',
    'wifi_owner_stop_challenge.pending=false;',
    'if(!ot_ble_owner_wifi_ready())',
): assert token in core,token
assert '"OpenTail-WiFi-v1\\nPOST\\n/api/v1/owner/stop\\n"' in codec
assert 'state->pending=false;' in codec and 'state->issued_ms' in codec
assert 'owner_key' in owner and 'ot_ble_owner_wifi_hmac' in owner
assert 'mbedtls_md_hmac(md,owner_key' in owner
assert 'BleOwnerVault.load(context,previouslyPairedBleAddress)' in client
assert 'WifiOwnerProof.stopProof(ownerKey,nonce)' in client
assert 'id.equals(ControllerLinkPolicy.normalizedId(' in client
assert 'http.setRequestMethod("POST")' in client
assert '!ble.canControl()' not in proof
assert 'wifiStop.stop(id,address)' in ui and 'ble.canControl()' in ui
assert 'livePreview.detach()' in ui
assert '.setInstanceFollowRedirects(false)' in client
assert not any(t in client for t in ('"api/v1/show/start"','setDoOutput(true)',
    'retry(', 'Thread.sleep('))
print("PASS Issue118 Wi-Fi owner proof / one-use STOP and physical priority scope")
