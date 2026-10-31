#!/usr/bin/env python3
"""Fail closed if backup POLICY is accidentally wired to real outputs."""
from pathlib import Path

root=Path(__file__).resolve().parents[1]
h=(root/"main/ot_failover_essential.h").read_text()
c=(root/"main/ot_failover_essential.c").read_text()
core=(root/"main/opentail_core.c").read_text()
cmake=(root/"main/CMakeLists.txt").read_text()
host=(root/"tests/run-host-tests.sh").read_text()
assert "ot_failover_essential.c" in cmake
assert "tests/test_failover_essential.c" in host
assert "tests/test_failover_runtime_isolation.py" in host
for unsafe in ("gpio_set_level(", "gpio_config(", "led_strip_refresh", "nvs_set_",
               "nvs_commit(", "ble_gap_", "esp_restart(", "esp_timer_"):
    assert unsafe not in c, f"Unexpected physical interface in backup policy: {unsafe}"
assert "return OT_FO_DENY_NO_INTERLOCK" in c
assert "return OT_FO_DENY_CONTENTION" in c
assert "return OT_FO_DENY_HANDOFF_INCOMPLETE" in c
assert "fresh_full_frame_after_handoff" in c
assert "independent_physical_inputs" in c
assert "ot_tm_resolve(&p->trigger,independent_physical_inputs)" in c
assert "ot_om_trigger_color(role,mask,brightness)" in c
assert "ot_om_calibrate" not in c
assert "ot_effects_1d_render" not in c
assert "bool committed_active_snapshot" in h
assert "ot_fo_can_activate_profile" in h
assert "hardware_failover" in core and "Do not install as road lights." in core
assert "No watchdog, fail-safe, independent backup" in core
assert "ot_fo_assess_grant(" in h
print("PASS: failover software is inert by default; no unauthorized GPIO/output, NVS, BLE or UI takeover")
