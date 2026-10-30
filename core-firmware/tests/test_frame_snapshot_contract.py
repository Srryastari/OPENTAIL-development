#!/usr/bin/env python3
"""Fail if snapshot capture moves before arbitration/calibration or after strip swizzle.
Static source contract; not a substitute for BLE/optical tests."""
from pathlib import Path
root = Path(__file__).resolve().parents[1]
core = (root/"main/opentail_core.c").read_text()
ble = (root/"main/ble_status.c").read_text()
module = (root/"main/ot_frame_snapshot.c").read_text()
cmake = (root/"main/CMakeLists.txt").read_text()
start = core.index("for(unsigned px=0;px<output->active_pixels;++px)")
end = core.index("led_strip_refresh_async(strip[ch])", start)
per_pixel = core[start:end]
assert per_pixel.index("ot_om_trigger_overlay(") < per_pixel.index("if(decorated)c=ot_om_calibrate(c,output)")
assert per_pixel.index("if(decorated)c=ot_om_calibrate(c,output)") < per_pixel.index("ot_frame_snapshot_pixel(")
assert per_pixel.index("ot_frame_snapshot_pixel(") < per_pixel.index("ot_om_swizzle(")
assert per_pixel.index("ot_om_swizzle(") < per_pixel.index("led_strip_set_pixel(")
assert core.index("led_strip_refresh_async_done(strip[ch])") < core.index("ot_frame_snapshot_complete(true)")
assert "frame_captured[ch]=ot_frame_snapshot_begin(" in core
assert core.count("ot_frame_snapshot_unavailable((uint8_t)ch)") == 2
assert "record.pending && !recording && record.output==output" in module
assert "void ot_frame_snapshot_unavailable(uint8_t output)" in module
assert "ot_frame_snapshot.c" in cmake
assert "portENTER_CRITICAL(&snap_mux)" in module
assert "if(recording){UNLOCK();return false;}" in module
assert "record.ready && !recording" in module
assert "active_generation==generation" in module
# BLE reset may clear shared record.count while the render task is capturing.
# Pixel bounds and CRC length must use the render-owned immutable count.
assert "static uint16_t active_count=0;" in module
assert "active_count=count;" in module
assert "pixel>=active_count" in module
assert "crc16(pixels,(size_t)active_count*3u)" in module
assert "pixel>=record.count" not in module
assert "ot_frame_snapshot_reset()" in ble
assert "conn!=active_conn || !authorized" in ble
for uuid in ("ot_frame_cmd_uuid", "ot_frame_rsp_uuid"):
    assert ble.count(uuid)>=2
assert "OT_FRAME_SNAPSHOT_CHUNK_PIXELS" in ble
assert "ctx->offset" in ble
assert "frame_reply_sequence" in ble
print("PASS: CORE snapshot owner auth, pixel provenance, immutable CRC and RMT completion gate")
