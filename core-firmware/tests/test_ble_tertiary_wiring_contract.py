#!/usr/bin/env python3
"""Fail closed if third-color BLE bypasses owner auth or modifies persistence."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
def file(n):
    return (root/"main"/n).read_text()
ble=file("ble_status.c")
core=file("opentail_core.c")
parser=file("ble_fx_command.c")
hdr=file("ble_fx_command.h")
bank=file("fx_zone_bank.c")
bank_h=file("fx_zone_bank.h")
preset=file("fx_preset_record.h")
effect=file("wled_fx_compat.h")
assert 'OT_BLE_FX_TERTIARY_CAPABILITY' in ble
assert 'fx-tertiary-v1' in hdr
assert '#define OT_BLE_FX_TERTIARY_WIRE_BYTES 20u' in hdr
assert '#define OT_BLE_FX_WIRE_BYTES 17u' in hdr
assert 'p[16]!=1u' in parser
assert 'ot_ble_fx_parse(legacy,sizeof(legacy),true,&candidate)' in parser
assert 'if(!authorized)return OT_BLE_FX_UNAUTHORIZED' in parser
assert 'third_color_source_verified' in parser
assert 'ot_wled_fx_tertiary_supported(id)' in parser
assert '(candidate.effect_id==38u && candidate.palette_id!=4u)' in parser
assert 'ot_effects_1d_color_roles' in parser
assert 'ot_ble_fx_parse_tertiary(buf,n,authorized,&fx,&tertiary)' in ble
assert 'ot_ble_apply_continuous_fx_tertiary' in ble
assert 'tertiary,now_ms' in core
assert '.tertiary={slot->tertiary.r,slot->tertiary.g,slot->tertiary.b}' in core
assert '.tertiary_set=slot->tertiary.set' in core
assert 'ot_ble_fx_tertiary_t tertiary' in bank_h
assert 'slot->tertiary=(ot_ble_fx_tertiary_t){0}' in bank
assert 'if(tertiary)bank->zones[o][z].tertiary=*tertiary' in bank
assert 'ot_ble_fx_packet_t effect[' in preset
assert 'ot_ble_fx_packet_t fx;' in preset
assert 'ot_ble_fx_tertiary_t' not in preset
assert 'ot_wled_rgb_t tertiary;' in effect
assert 'bool tertiary_set;' in effect
assert '"hardware_failover"' in core or r'\"hardware_failover\"' in core
assert 'ot_om_trigger_overlay' in core
assert 'ot_fx_bank_stop_all(&fx_bank)' in core
assert "static bool ot_ble_apply_fx_internal" in core
assert 'OT_FW_VERSION' in ble
print("PASS: 20B tertiary capability/auth, 17B compatibility, independent volatile color, NVS/STOP/priority boundaries")
