from pathlib import Path

core = Path("main/opentail_core.c").read_text()
ble = Path("main/ble_status.c").read_text()
api = Path("main/panel_zone_api.h").read_text()

for token in (
    "ot_pz_build_pixel_map(&local_panel[ch],pixel_zone_map[ch],pixel_logical_map[ch])",
    "pixel_zone_map[ch][px]",
    "pixel_logical_map[ch][px]",
    "ot_pz_fallback_color",
    "ot_pz_diag_render",
    "prior_pixel_zone_map",
    "fx_zone_scratch",
):
    assert token in core, token
assert "prior_fx_offset" not in core
for token in ("ot_ble_panel_preview_begin","ot_ble_panel_preview_update",
              "ot_ble_panel_preview_commit","ot_ble_panel_preview_cancel"):
    assert token in core and token in api, token
assert "OT_PZ_PREVIEW_LEASE_MS" in core
assert "config_activation_idle(epoch,true)" in core
assert "config_activation_idle(epoch,false)" in core
panel_save = core.split("int ot_ble_panel_save(",1)[1].split("bool ot_ble_pixel_probe(",1)[0]
assert "show_window(now)" not in panel_save
assert "raw_active" not in panel_save
for opcode in ("case 0x10u:", "case 0x11u:", "case 0x12u:",
               "case 0x13u:", "case 0x14u:"):
    assert opcode in ble, opcode
assert "case 0xabu: /* proposed Fill opcode intentionally not accepted */" in ble
assert "BLE_GATT_CHR_F_WRITE_NO_RSP" in ble
assert "panel_caps_seen" in ble and "authorized" in ble
print("PASS Issue126 static runtime contract")
