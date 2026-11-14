#!/usr/bin/env python3
"""Verify Android's immutable Firmware #127 Panel/Zone and BLE integration contract."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
PIN = "1dfe28ca50ad54fd92c6a0b5fb1c919cd7687d08"
SOURCE = ROOT / "pinned-firmware127"

def read(name):
    return (SOURCE / "core-firmware/main" / name).read_text()

def git(*args):
    return subprocess.check_output(["git", "-C", str(SOURCE), *args], text=True).strip()

def require(text, tokens, label):
    missing = [token for token in tokens if token not in text]
    assert not missing, f"{label} missing {missing}"

def main():
    assert git("rev-parse", "HEAD") == PIN, "Wrong immutable Firmware #127 source"
    panel_h = read("panel_zone.h")
    panel_c = read("panel_zone.c")
    ble_h = read("ble_fx_command.h")
    ble_c = read("ble_fx_command.c")
    ble_status = read("ble_status.c")
    core_main = read("opentail_core.c")
    effects_c = read("effects_1d.c")
    effects_h = read("effects_1d.h")

    require(panel_h, [
        "#define OT_PZ_PANEL_ID_PREFIX 0x8000u",
        "#define OT_PZ_ZONE_ID_PREFIX 0x4000u",
        "#define OT_PZ_ID_WIRE_MASK 0x00ffu",
        "#define OT_PZ_TX_ACTION_MASK 0x30u",
        "#define OT_PZ_TX_DELETE_ZONE 2u",
        "#define OT_PZ_TX_DELETE_PANEL 3u",
    ], "Panel/Zone wire header")
    require(panel_c, [
        "static int pz_find_panel_wire",
        "static int pz_find_zone_wire",
        "if(found>=0)return -2;",
        "if(panel_index==-2)return false;",
        "if(zone_index==-2)return false;",
        "state->committing",
    ], "Panel/Zone alias and COMMITTING guard")
    require(ble_h, [
        '#define OT_BLE_FX_TERTIARY13_CAPABILITY "fx-tertiary13-v1"',
        "#define OT_BLE_FX_TERTIARY_WIRE_BYTES 20u",
    ], "BLE third-color wire contract")
    require(ble_c, [
        "return ot_wled_fx_tertiary_supported(id);",
        "(candidate.effect_id==38u && candidate.palette_id!=4u)",
    ], "BLE third-color admission and Aurora palette gate")
    require(ble_status, ["OT_BLE_FX_TERTIARY13_CAPABILITY"], "BLE capability advertisement")
    require(panel_h, ["bool ot_pz_live_geometry_matches("],
            "Metadata-compatible full Panel v4 Save geometry matcher")
    require(core_main, [
        "ot_pz_live_geometry_matches(accepted_geometry,next,lane)",
        "if(structural_conflict)",
        "return 5;",
        "const bool live_active=pz_preview[lane].active;",
        "pz_preview[lane].committing=true;",
    ], "Atomic metadata-only Save and unsaved-geometry refusal")
    require(ble_status, [
        "case 5:return 10u;",
        "case 1:return 7u;",
        "case 2:return 2u;",
        "case 3:return 3u;",
        "case 4u:{",
        "case 5u:{",
        "panel_save_status(save_rc)",
    ], "Distinct v4 Save NACK status and journal acknowledgement")
    require(effects_c, [
        "ot_effects_1d_find_prior_zone_state",
        "ot_effects_1d_move_state",
    ], "CORE stable Panel/Zone renderer-state migration")
    require(effects_h, [
        "ot_effects_1d_find_prior_zone_state",
        "ot_effects_1d_move_state",
    ], "CORE stable Panel/Zone renderer-state declarations")
    print("PASS: Firmware #127 metadata-compatible atomic v4 Save/NACK, 16-bit identities, role-independent 13-mode BLE, and stable renderer migration")

if __name__ == "__main__":
    main()
