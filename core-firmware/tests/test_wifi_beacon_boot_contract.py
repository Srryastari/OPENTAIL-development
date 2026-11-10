#!/usr/bin/env python3
"""9cc47fc1 beacon allocation failure: source contract, NOT physical acceptance."""
from pathlib import Path
import re
r=Path(__file__).resolve().parents[1]
s=(r/"main/opentail_core.c").read_text()
cfg=(r/"sdkconfig.defaults").read_text()
wifi=s.split("static bool wifi_ap_init(void)",1)[1].split("/* ESP-IDF 5.5.x",1)[0]
assert "ieee80211_hostap_attach" in s
assert "wifi_heap_start_preflight()" in wifi
assert wifi.index("esp_wifi_init(&init)")<wifi.index('ot_boot_heap_snapshot("wifi-driver-initialized")')
assert wifi.index("esp_wifi_set_config(")<wifi.index("wifi_heap_start_preflight()")<wifi.index("esp_wifi_start()")
assert "init.mgmt_sbuf_num=8;" in wifi
assert "esp_wifi_deinit()" in wifi
assert "esp_netif_destroy_default_wifi(wifi_netif)" in wifi
assert "esp_netif_attach_wifi_ap(wifi_netif)" in wifi
assert "esp_wifi_set_default_wifi_ap_handlers()" in wifi
assert "esp_netif_create_default_wifi_ap()" not in wifi
assert "esp_netif_new(&ap_cfg)" in wifi
assert wifi.index("wifi_heap_start_preflight()") < wifi.index("esp_netif_init()")
assert "BLE remains primary" in wifi
assert "MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA" in s
for k,v in {"ESP_WIFI_STATIC_RX_BUFFER_NUM":"6","ESP_WIFI_RX_BA_WIN":"6",
"ESP_WIFI_STATIC_TX_BUFFER_NUM":"6","ESP_WIFI_DYNAMIC_RX_BUFFER_NUM":"16",
"ESP_WIFI_IRAM_OPT":"n","ESP_WIFI_RX_IRAM_OPT":"n","LWIP_IRAM_OPTIMIZATION":"n"}.items():
    assert re.search(r"^CONFIG_"+k+"="+v+r"$",cfg,re.M),k
assert "CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL=32768" in cfg
assert "CONFIG_FREERTOS_PLACE_TASK_STACKS_IN_EXT_RAM=n" in cfg
assert "#define OT_RENDER_TASK_STACK_BYTES 8192u" in s
assert "OT_OUT_LEFT_GPIO 16" in (r/"main/board_config.h").read_text()
assert "OT_OUT_RIGHT_GPIO 17" in (r/"main/board_config.h").read_text()
print("PASS P0 Wi-Fi heap preflight source guard; hardware retest pending")
