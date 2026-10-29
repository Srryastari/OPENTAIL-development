#!/usr/bin/env python3
"""Source guard for the cleared onboard status LED's first startup frame.

ESP-IDF owns the actual pixel driver; status_logic host tests own timing.
"""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
driver = (root / "main/status_led.c").read_text(encoding="utf-8")
logic = (root / "main/status_logic.c").read_text(encoding="utf-8")
task = driver.split("static void status_task(void *arg){", 1)[1].split(
    "void ot_st_init(void){", 1
)[0]
init = driver.split("void ot_st_init(void){", 1)[1]

assert "led_strip_clear(status_pixel)" in init
assert "bool prior_valid=false;" in task, "The cleared first frame is not cached"
assert "(!prior_valid||c.r!=prior.r||c.g!=prior.g||c.b!=prior.b)" in task
assert task.index("led_strip_refresh(status_pixel)") < task.index("prior_valid=true;")
assert "case OT_ST_STARTUP" in logic
print("PASS: cleared status LED sends the initial white startup frame")
