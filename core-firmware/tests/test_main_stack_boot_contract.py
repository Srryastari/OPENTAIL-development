#!/usr/bin/env python3
"""Guard the exact v0.1.31 ESP32-S3 main-task stack bootloop regression.

A passing host test does NOT establish physical boot. Serial must confirm
app_main advances past SPI status LED initialization and starts services.
"""
from pathlib import Path
import re

core = Path(__file__).resolve().parents[1]
defaults = (core / "sdkconfig.defaults").read_text(encoding="utf-8")
source = (core / "main/opentail_core.c").read_text(encoding="utf-8")
found = re.findall(r"^CONFIG_ESP_MAIN_TASK_STACK_SIZE=(\d+)$", defaults, re.M)
assert len(found) == 1, "main task stack size must be explicit"
assert int(found[0]) >= 8192, "ESP-IDF default main stack caused physical reboot loop"
assert "#if CONFIG_ESP_MAIN_TASK_STACK_SIZE < 8192" in source
boot = source.split("void app_main(void) {", 1)[1]
assert boot.index("om_journal_load()") < boot.index("ot_st_init()")
assert boot.index("tm_journal_load()") < boot.index("ot_st_init()")
assert boot.index("ot_st_init()") < boot.index("uxTaskGetStackHighWaterMark(NULL)")
assert boot.index("uxTaskGetStackHighWaterMark(NULL)") < boot.index("input_init()")
assert boot.index("input_init()") < boot.index("ot_ble_start()")
print("PASS: ESP32-S3 main task startup stack explicit, guarded, and observable")
