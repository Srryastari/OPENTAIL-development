#!/usr/bin/env python3
"""Source contract for BLE v3 custom palettes and CORE frame ownership.
Pure source checks complement host codec tests, not physical LED evidence."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]/"main"
core=(root/"opentail_core.c").read_text()
ble=(root/"ble_status.c").read_text()
runtime=(root/"palette_runtime.c").read_text()
profile=(root/"palette_profile.c").read_text()
wire=(root/"palette_ble_v3.c").read_text()
cmake=(root/"CMakeLists.txt").read_text()
fx=(root/"palette_1d.c").read_text()
assert all(f'"{name}.c"' in cmake for name in
    ("palette_runtime","palette_transfer","palette_profile","palette_ble_v3",
     "fx_overlay_engine"))
assert 'ot_palette_runtime_load();' in core.split("void app_main(void) {",1)[1]
assert 'ot_palette_runtime_snapshot(palette_ids,OT_OM_OUTPUTS*OT_OM_ZONES,' in core
assert core.index("ot_palette_runtime_snapshot(") < core.index(
    "ot_wled_palette_global_tick(") < core.index("ot_effects_1d_render(")
assert "if(!palette_resolved(p->palette_id) ||" in core
assert "!palette_resolved(req->palette_id)" in core
assert "bool ot_ble_palette_in_use(uint8_t index)" in core
assert "ot_palette_id_to_index" in profile
assert "ot_palette_record_is_tombstone" in profile
# A valid CRC alone does not authorize a record from a DIFFERENT NVS slot:
# wrong-index tombstones must not advance/rewrite another palette revision.
load=runtime.split("void ot_palette_runtime_load(void){",1)[1].split(
    "bool ot_palette_runtime_get(",1)[0]
assert "if(record.index!=i){" in load
assert load.index("if(record.index!=i){") < load.index("if(ot_palette_record_is_tombstone(&record))")
assert "candidate.crc=record_crc(&candidate)" in profile
assert 'nvs_open("ot_palettes",NVS_READWRITE' in runtime
assert runtime.index("nvs_commit_record(record)") < runtime.index("palette_bank[record->index]=next;")
assert runtime.index("nvs_commit_record(&dead)") < runtime.index("palette_bank[index]=")
assert "ot_palette_runtime_snapshot(" in runtime and "portENTER_CRITICAL(&palette_lock)" in runtime
assert "if(conn!=active_conn||!authorized)" in ble or "conn!=active_conn||!authorized" in ble
assert "ot_palette_ble_session_reset();" in ble
assert "ot_palette_cmd_uuid" in ble and "ot_palette_rsp_uuid" in ble and "ot_palette_caps_uuid" in ble
assert "ot_palette_transfer_finish(&upload,index,now,&rec)" in wire
assert "ot_palette_runtime_put(&rec,rec.revision-1u)" in wire
# Before a palette NVS write, share CORE's GPIO/trigger idle veto and
# configuration-save mutex; never hold the state spinlock over flash I/O.
assert "bool ot_ble_palette_mutation_begin(void)" in core
assert "void ot_ble_palette_mutation_end(bool changed)" in core
start=core.split("bool ot_ble_palette_mutation_begin(void){",1)[1].split(
    "void ot_ble_palette_mutation_end(",1)[0]
assert "if(!input_sampler_ready||!om_write_mutex||" in start
assert "xSemaphoreTake(om_write_mutex,0)" in start
assert "raw_active=tm_gpio_input_mask()!=0" in start
assert "om_deferred.pending||tm_deferred.pending||raw_active" in start
assert "live_input_mask!=0" in start
assert "sim_mask!=0" in start
assert "if(busy)xSemaphoreGive(om_write_mutex)" in start
end=core.split("void ot_ble_palette_mutation_end(bool changed){",1)[1].split(
    "bool ot_ble_palette_in_use(",1)[0]
assert "xSemaphoreGive(om_write_mutex);" in end
assert "if(changed)request_render_refresh();" in end
assert wire.count("ot_ble_palette_mutation_begin()")==2
assert wire.count("ot_ble_palette_mutation_end(")==2
assert "ot_ble_palette_in_use(index)" in wire
assert "ot_palette_runtime_delete(index,e.revision)" in wire
assert "ot_palette_runtime_read_record(index,&record)" in wire
assert "bool ot_palette_runtime_read_record(" in runtime
assert "return palette<=72u||(palette>=73u&&palette<=200u);" in fx
assert '"ot_output_cfg"' not in runtime and '"ot_trig_cfg"' not in runtime
print("PASS: CORE palette v3 transport/persistence/frame integration contracts")
