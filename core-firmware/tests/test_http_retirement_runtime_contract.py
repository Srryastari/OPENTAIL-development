#!/usr/bin/env python3
"""Source guard for owner-retired HTTP runtime on physically working non-DMA CORE.

The legacy HTTP source is intentionally kept for rollback comparison but no
server task or route may be started. Does not prove radio/optical hardware.
"""
from pathlib import Path

root=Path(__file__).resolve().parents[1]
core=(root/"main/opentail_core.c").read_text()
config=(root/"sdkconfig.defaults").read_text()
boot=core.split("void app_main(void) {",1)[1]
render=core.split("static void led_task(void *p) {",1)[1].split("void app_main(void) {",1)[0]
rmt=core.split("static bool strip_configure_channel(",1)[1].split("static bool strip_init(",1)[0]

assert 'http_start();' not in boot, "HTTP listener must not start"
assert 'http_startup_ready' not in core, "Do not gate OTA on obsolete HTTP"
assert 'HTTP diagnostic server DISABLED (obsolete)' in boot
assert "ot_ota_confirm_boot(true)" in render
assert "full_frame && ot_sw_idf_stable()" in render
assert "om_write_mutex && ble_startup_ready &&" in render
assert "ot_ble_radio_ever_ready() && !boot_frame_verified" in render
assert "const bool wifi_started=wifi_ap_init();" in boot
assert "if(!wifi_started)ot_st_set_fault(" in boot
assert "if(!http_startup_ready)" not in core
assert "total<24576u || largest<4096u || dma_total<12288u || dma_largest<2048u" in core
assert "ESP_LOGI(TAG,\"Secondary Wi-Fi AP %s started; no HTTP listener/control transport\"" in core
assert "init.mgmt_sbuf_num=6;" in core
assert "CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM=4" in config
assert "CONFIG_ESP_WIFI_RX_BA_WIN=4" in config
assert "CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL=32768" in config
assert ".mem_block_symbols=64" in rmt and ".flags={.with_dma=false}" in rmt
assert "t0h_ns=300,.t0l_ns=900,.t1h_ns=900,.t1l_ns=300,.reset_us=320" in rmt
assert "#define OT_ST_FIXED_BRIGHTNESS 65u" in (root/"main/status_led.h").read_text()
assert "#define CONFIG_OPENTAIL_MAX_BRIGHTNESS_LIMIT 32" in (root/"main/board_config.h").read_text()
assert 'ot_st_set_fault("Wi-Fi diagnostic AP startup failed")' in core
print("PASS: no HTTP listener; OTA RMT/BLE guard retained; BLE-first Wi-Fi memory preflight unchanged; Output1 non-DMA/64 unchanged (HOST ONLY)")
