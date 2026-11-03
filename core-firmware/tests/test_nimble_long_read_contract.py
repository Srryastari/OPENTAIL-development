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

# Execute the real callback body: repeated reads reuse one catalog build,
# and an unauthorized connection cannot receive cached bytes.
import subprocess
import tempfile
program = r"""
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#define BLE_GATT_ACCESS_OP_READ_CHR 1
#define BLE_ATT_ERR_READ_NOT_PERMITTED 2
#define BLE_ATT_ERR_INSUFFICIENT_RES 3
struct ble_gatt_access_ctxt {int op; void *om;};
static uint16_t active_conn=7;
static unsigned enumerations,appends;
static uint8_t received[510];
static size_t received_len;
static size_t ot_effects_1d_supported_ids(uint16_t *ids,size_t cap){
    assert(cap>=140);enumerations++;
    for(unsigned i=0;i<140;i++)ids[i]=i<120?i:32768+i-120;
    return 140;
}
static int os_mbuf_append(void *om,const void *p,size_t n){
    (void)om;appends++;memcpy(received,p,n);received_len=n;return 0;
}
static int read_catalog(uint16_t conn,uint16_t handle,struct ble_gatt_access_ctxt *ctx,void *arg)
""" + body("fx_capabilities_read") + r"""
int main(void){
    struct ble_gatt_access_ctxt ctx={1,0};
    assert(read_catalog(8,0,&ctx,0)==2 && enumerations==0);
    assert(read_catalog(7,0,&ctx,0)==0 && received_len==282);
    uint8_t first[282];memcpy(first,received,282);
    for(unsigned i=0;i<20;i++)assert(read_catalog(7,0,&ctx,0)==0);
    assert(enumerations==1 && appends==21 && memcmp(first,received,282)==0);
    assert(read_catalog(8,0,&ctx,0)==2 && appends==21);
    assert(received[0]==1 && received[1]==140);
    return 0;
}
"""
with tempfile.TemporaryDirectory() as tmp:
    c=Path(tmp)/"catalog.c";exe=Path(tmp)/"catalog"
    c.write_text(program)
    subprocess.run(["cc","-std=c11","-Wall","-Wextra","-Werror",str(c),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print("PASS: production catalog callback caches once and preserves connection admission and complete payload")

# Run the actual frame callback against the production snapshot store. Count
# store accesses and verify every transmitted byte for all supported chunk sizes.
frame_program = r'''
#include "ot_frame_snapshot.h"
#include <assert.h>
#include <string.h>
#define BLE_GATT_ACCESS_OP_READ_CHR 1
#define BLE_ATT_ERR_READ_NOT_PERMITTED 2
#define BLE_ATT_ERR_INSUFFICIENT_RES 3
#define BLE_ATT_ERR_UNLIKELY 4
struct ble_gatt_access_ctxt {int op; void *om;};
static uint16_t active_conn=7;
static bool authorized=true;
static uint8_t frame_reply_kind=2,frame_reply_output=1,frame_reply_count;
static uint16_t frame_reply_first=8;
static uint32_t frame_reply_sequence=0x12345678;
static unsigned info_calls,chunk_calls,appends;
static uint8_t received[179];
static size_t received_len;
static bool counted_info(ot_frame_snapshot_info_t *p){
    info_calls++;return ot_frame_snapshot_info(p);
}
static bool counted_chunk(uint8_t o,uint32_t s,uint16_t f,uint8_t n,uint8_t *p){
    chunk_calls++;return ot_frame_snapshot_chunk(o,s,f,n,p);
}
static int os_mbuf_append(void *om,const void *p,size_t n){
    (void)om;appends++;assert(n<=sizeof(received));
    memcpy(received,p,n);received_len=n;return 0;
}
static int read_frame(uint16_t conn,uint16_t handle,struct ble_gatt_access_ctxt *ctx,void *arg)
''' + body("frame_read").replace("ot_frame_snapshot_info(&", "counted_info(&").replace(
    "ot_frame_snapshot_chunk(", "counted_chunk(") + r'''
int main(void){
    struct ble_gatt_access_ctxt ctx={1,0};
    assert(ot_frame_snapshot_request(1));
    assert(ot_frame_snapshot_begin(1,64,frame_reply_sequence));
    for(unsigned i=0;i<64;i++)ot_frame_snapshot_pixel(i,i,i+1,i+2);
    ot_frame_snapshot_complete(true);
    for(unsigned n=1;n<=56;n++){
        frame_reply_count=n;
        assert(read_frame(7,0,&ctx,0)==0);
        assert(info_calls==0 && chunk_calls==n && appends==n);
        assert(received_len==11+3*n);
        const uint8_t header[]={1,2,1,1,0x78,0x56,0x34,0x12,8,0};
        assert(memcmp(received,header,sizeof(header))==0 && received[10]==n);
        for(unsigned i=0;i<n;i++)for(unsigned c=0;c<3;c++)
            assert(received[11+i*3+c]==8+i+c);
    }
    authorized=false;
    assert(read_frame(7,0,&ctx,0)==2 && chunk_calls==56);
    authorized=true;
    assert(read_frame(8,0,&ctx,0)==2 && chunk_calls==56);
    frame_reply_sequence++;
    assert(read_frame(7,0,&ctx,0)==4 && appends==56);
    frame_reply_sequence--;frame_reply_first=63;frame_reply_count=2;
    assert(read_frame(7,0,&ctx,0)==4 && appends==56);
    frame_reply_kind=1;
    assert(read_frame(7,0,&ctx,0)==0 && info_calls==1 && received_len==12);
    ot_frame_snapshot_info_t info;assert(ot_frame_snapshot_info(&info));
    const uint8_t status[]={1,1,1,1,64,0,0x78,0x56,0x34,0x12,
                            (uint8_t)info.crc,(uint8_t)(info.crc>>8)};
    assert(memcmp(received,status,12)==0);
    ot_frame_snapshot_reset();frame_reply_kind=2;frame_reply_first=0;frame_reply_count=1;
    assert(read_frame(7,0,&ctx,0)==4 && appends==57);
    return 0;
}
'''
with tempfile.TemporaryDirectory() as tmp:
    c=Path(tmp)/"frame.c";exe=Path(tmp)/"frame"
    main=Path(__file__).resolve().parents[1]/"main"
    c.write_text(frame_program)
    subprocess.run(["cc","-std=c11","-Wall","-Wextra","-Werror","-I",str(main),
                    str(c),str(main/"ot_frame_snapshot.c"),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print("PASS: frame reads use one store access; all 56 chunk sizes, wire bytes, status, auth, stale sequence, bounds and reset validated")
