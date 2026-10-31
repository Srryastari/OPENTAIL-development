#!/usr/bin/env python3
"""Regression for ESP-IDF NimBLE's ATT Read Blob callback contract.

NimBLE ble_gatts_val_access() calls the characteristic access callback for
every Read Blob, then applies the requested offset to the *complete* value.
The application MUST NOT pre-slice by ctx->offset or ATT MTU. Pre-slicing
caused physical Android GATT status 7 while reading 140 FX IDs.
This host test is a source-contract + protocol-model check; the actual ESP
stack implementation and radio are still verified by ESP-IDF CI and bench.
"""
from pathlib import Path
import re

source = (Path(__file__).resolve().parent.parent / "main" / "ble_status.c").read_text()

def body(name):
    match = re.search(r"static int " + re.escape(name)
                      + r"\([^;]*?\)\s*\{", source, re.S)
    assert match, f"Missing {name} characteristic callback"
    opening = match.end() - 1
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[opening:index + 1]
    raise AssertionError(f"Unterminated {name} callback")

for name, expected in (
    ("fx_capabilities_read", "os_mbuf_append(ctx->om,payload,payload_len)"),
    ("frame_read", "os_mbuf_append(ctx->om,p,size)"),
    ("palette_read", "os_mbuf_append(ctx->om,reply,sizeof(reply))"),
    ("palette_caps", "os_mbuf_append(ctx->om,caps,sizeof(caps))"),
):
    callback = body(name)
    # Strip comments before asserting: explanatory text can name ctx->offset.
    callback = re.sub(r"/\*.*?\*/|//[^\n]*", "", callback, flags=re.S)
    assert expected in callback, f"{name} did not return the complete value"
    assert "ctx->offset" not in callback, f"{name} double-applies NimBLE's offset"
    assert "ble_att_mtu" not in callback, f"{name} chunks before NimBLE"
    assert "BLE_ATT_ERR_INVALID_OFFSET" not in callback, (
        f"{name} must delegate invalid offsets to NimBLE's attribute logic")

# Model Android's ATT Read Blob offsets using the documented NimBLE host
# behavior. It is not a mock claiming actual radio success.
ids = list(range(120)) + [32768 + i for i in range(20)]
payload = bytes([1, len(ids)]) + b"".join(x.to_bytes(2, "little") for x in ids)
assert len(payload) == 282

for mtu in (23, 64, 185, 247, 517):
    output = bytearray()
    offset = 0
    while offset < len(payload):
        # The app supplies the complete payload, host slices at requested
        # offset and caps Read Blob responses to MTU-1 bytes.
        response = payload[offset:offset + mtu - 1]
        assert response
        output.extend(response)
        offset += len(response)
    assert bytes(output) == payload, f"Mismatch after MTU {mtu} long read"
    assert output[1] == 140
    decoded = [int.from_bytes(output[2 + i * 2:4 + i * 2], "little")
               for i in range(output[1])]
    assert decoded == ids

# Demonstrate why the *old* pre-sliced callback was wrong for a nonzero offset:
# the host subtracts that offset again, leaving a negative length.
offset = 40
prior_callback_bytes = payload[offset:offset + 22]
assert len(prior_callback_bytes) - offset < 0
print("PASS: FX 140-ID NimBLE Read Blob offsets across MTUs, no double-slicing in 4 callbacks")
