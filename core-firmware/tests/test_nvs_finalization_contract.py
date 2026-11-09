#!/usr/bin/env python3
"""Source-contract audit for CORE durable config finalization (software only).

Pure tests cannot simulate an ESP32 interrupt or torn physical flash. This
pins trigger policy promotion, read-only legacy migration, and the authoritative
v4 lane/map runtime pair store. Physical brownout/ISR testing remains mandatory.
"""
from pathlib import Path

root=Path(__file__).resolve().parents[1]
core=(root/"main/opentail_core.c").read_text()
journal=(root/"main/config_journal.c").read_text()
header=(root/"main/config_journal.h").read_text()
board=(root/"main/board_config.h").read_text()
logic_header=(root/"main/core_logic.h").read_text()
implementation_doc=(root.parent/"docs/ISSUE126-PANEL-ZONE-IMPLEMENTATION.md").read_text()


def between(start,end):
    assert start in core and end in core,(start,end)
    return core.split(start,1)[1].split(end,1)[0]


# Trigger policy retains its independent stage -> active -> erase ->
# quiet-window -> RAM ordering. Legacy Output Manager records are migration
# input only and are never promoted as a combined runtime configuration.
trigger_promotion=between("static void tm_promote(uint32_t epoch){",
                          "static void pz_promote(uint32_t epoch){")
assert trigger_promotion.index("config_write_active(h,")<trigger_promotion.index(
    "config_clear_stage(h)")
assert trigger_promotion.index("config_clear_stage(h)")<trigger_promotion.index(
    "ot_tm_apply_if_released(")
assert trigger_promotion.rfind("config_activation_idle(epoch,false)")<trigger_promotion.index(
    "ot_tm_apply_if_released(")
assert trigger_promotion.index("tm_gpio_input_mask()==0u")<trigger_promotion.index(
    "ot_tm_apply_if_released(")
for token in ("input_quiet_samples>=OT_DEBOUNCE_TICKS",
              "__atomic_load_n(&input_edge_epoch,__ATOMIC_ACQUIRE)==epoch",
              "config_recovery_fault=true","stage.candidate_revision",
              "stage.previous_revision","OT_CFG_JOURNAL_KEY_ACTIVE"):
    assert token in trigger_promotion,token
# Approved physical input/debounce policy is a separate versioned record
# in ot_trig_cfg. Old profiles migrate only the policy defaults; live timers,
# ISR timestamps and debouncer state are never serialized.
policy_load=between("static bool tm_policy_load(",
                    "/* Size/version-aware reader")
for token in ('nvs_open("ot_trig_cfg",NVS_READWRITE',
              "OT_TM_POLICY_KEY","ESP_ERR_NVS_NOT_FOUND",
              "nvs_set_blob(h,OT_TM_POLICY_KEY","nvs_commit(h)",
              "ot_tm_policy_valid(&stored)","memcmp(&stored,&expected",
              "OT_INPUT_ACTIVE_LOW","OT_DEBOUNCE_TICKS","10u",
              "OT_TM_TURN_RELEASE_INPUT_MASK","OT_TURN_RELEASE_HOLD_US"):
    assert token in policy_load,token
assert "if(!tm_policy_load())" in core
assert core.index("if(!tm_policy_load())") < core.index("tm_journal_load();")
assert "#define OT_TM_POLICY_VERSION 1u" in header
assert '#define OT_TM_POLICY_KEY "policy_v1"' in header
assert "ot_tm_policy_build" in journal and "ot_tm_policy_valid" in journal
# The handoff document must describe the actual v4 split and must not
# repeat the stale "not yet complete" combined-journal status.
for token in ("lane_active_v4","map_active_v4","policy_v1",
              "turn_release_input_mask","immediate ISR assertion",
              "cross-namespace hardware/map migration",
              "100-cycle physical power-cut/recovery"):
    assert token in implementation_doc,token
assert "NOT YET COMPLETE" not in implementation_doc
assert "The map journal currently carries a combined runtime snapshot" not in implementation_doc
assert "turn_release_hold_us" in header and "assertion_samples" in header
assert "turn_release_input_mask" in header and "reserved[3]" in header
assert "#define OT_TM_TURN_RELEASE_INPUT_MASK 0x0cu" in header
assert "sizeof(ot_tm_policy_record_t)==24u" in journal
assert "#define OT_INPUT_ACTIVE_LOW 1" in board
assert "#define OT_DEBOUNCE_TICKS 4" in board
assert "#define OT_TURN_RELEASE_HOLD_US UINT64_C(400000)" in logic_header
input_sampler=core.split("static void input_sampler_task(",1)[1].split(
    "static void frame_wake_callback(",1)[0]
assert "pdMS_TO_TICKS(10u)" in input_sampler
assert "ot_signal_debouncer" not in header.split(
    "} ot_tm_policy_record_t",1)[0].split("typedef struct {")[-1]
# The legacy v3 combined record is read-only migration input. The authoritative
# v4 runtime joins a lane-only output_cfg journal and map-only zone_map journal.
pz_load=between("static void pz_load(void){",
                "static bool pz_save_controller_locked(")
pair_store=(root/"main/panel_zone_pair_store.c").read_text()
assert "const bool v4_authoritative=snapshot->has_marker||snapshot->has_intent||" in core
assert "!s->has_intent&&s->has_map_active&&!s->has_marker" in pair_store
assert "snapshot->has_map_active||snapshot->has_lane_active;" in core
for token in ("ot_pz_pair_store_snapshot", "ot_pz_pair_store_recover",
              "ot_pz_pair_store_begin", "pz_load_legacy_snapshot",
              "om_deferred.pending=false"):
    assert token in pz_load,token
legacy_load=between("static bool pz_load_legacy_snapshot(",
                    "static void pz_load(void){")
assert 'nvs_open("ot_zone_map",NVS_READONLY' in legacy_load
assert "ot_pz_journal_recover(" in legacy_load
assert "pz_reset_tombstone_valid" in legacy_load
assert "OT_PZ_RESET_TOMBSTONE_KEY" in legacy_load
assert "nvs_set_blob(" not in legacy_load and "nvs_commit(" not in legacy_load

# The runtime save delegates to the shared pair-store implementation. No RAM
# candidate/revision is published unless lane stage, map stage and pair intent
# each commit and read back successfully.
pz_save=between("static bool pz_save_controller_locked(",
                "/* Legacy output writes are lossless")
assert "ot_pz_pair_store_begin" in pz_save
assert "previous_revision" in pz_save and "candidate_revision" in pz_save
assert "reset_mask" in pz_save
assert pz_save.index("ot_pz_pair_store_begin") < pz_save.index(
    "pz_deferred.candidate=*candidate;")
assert "pz_pair_commit_pending" in pz_save
assert "nvs_open(" not in pz_save and "nvs_set_blob(" not in pz_save

# Legacy Output v1/HTTP and BLE codecs are projections only. They must route
# accepted edits through the same v4 pair store and may not write the old
# combined profile/journal keys.
legacy_output_save=between("static bool om_save(","#define OT_SHOW_MAX_MS")
assert "pz_save_controller_locked(candidate)" in legacy_output_save
assert "nvs_open(" not in legacy_output_save
assert "nvs_set_blob(" not in legacy_output_save
assert "static void om_promote(" not in core
assert "static void pz_sync_from_legacy(" not in core

# A damaged legacy Output Manager journal cannot veto a valid v3
# Panel/Zone journal or tombstone. It is fatal only if migration must fall
# all the way back to the old Output Manager records.
legacy_om_load=between("static void om_journal_load(void){",
                       "static esp_err_t pz_journal_read(")
assert "config_recovery_fault" not in legacy_om_load
assert "om_legacy_journal_fault=true" in legacy_om_load
pz_load=between("static void pz_load(void){",
                "static bool pz_save_controller_locked(")
assert "snapshot->has_marker||snapshot->has_intent" in pz_load
assert "if(om_legacy_journal_fault)" not in pz_load
assert pz_load.index("if(v4_authoritative)") < pz_load.index(
    "pz_load_legacy_snapshot")
assert "if(om_legacy_journal_fault)return false;" in legacy_load
assert legacy_load.index("if(om_legacy_journal_fault)return false;") > legacy_load.index(
    "if(has_tombstone)")
assert legacy_load.index("if(om_legacy_journal_fault)return false;") < legacy_load.index(
    "ot_pz_migrate_legacy")
# Multi-kilobyte pair/journal records live in one bounded static workspace,
# never as nested local snapshots on the 5,120-byte activation task stack.
store_header=(root/"main/panel_zone_pair_store.h").read_text()
assert "ot_pz_pair_store_workspace_t" in store_header
assert "static ot_pz_pair_store_workspace_t pz_pair_workspace;" in core
assert "sizeof(pz_pair_workspace)<=16u*1024u" in core
assert "ot_pz_pair_store_snapshot_t snapshot" not in core
assert "ot_pz_pair_store_snapshot_t s;" not in pair_store
assert "ot_pz_controller_t previous,candidate;" not in journal.split(
    "bool ot_pz_pair_journal_recover(",1)[1]
assert "ot_cfg_crc32_zeroed" in journal and "ot_cfg_crc32_zeroed" in header
assert "ot_pz_journal_t active={0},stage={0}" not in legacy_load
assert "ot_pz_reset_tombstone_t tombstone={0}" not in legacy_load
# The no-change save path already persists the reset bit. Reset confirmation
# must not issue a second identical map-active NVS write after that save.
reset_confirm=between("int ot_ble_panel_reset_confirm(",
                      "int ot_ble_panel_save(")
assert "ot_pz_pair_store_mark_reset" not in reset_confirm
assert "ot_pz_pair_store_mark_reset" in pz_save
assert "__atomic_store_n(&pz_pair_commit_phase,intent_pending?0u:9u" in pz_load

# The exact same portable v4 store used by CORE implements all nine durable
# write boundaries and is exercised by test_panel_zone_pair_store.c.
store=(root/"main/panel_zone_pair_store.c").read_text()
store_header=(root/"main/panel_zone_pair_store.h").read_text()
for token in ("ot_pz_pair_store_begin","ot_pz_pair_store_finish",
              "ot_pz_pair_store_snapshot","ot_pz_pair_store_recover",
              "ot_pz_pair_store_mark_reset","write_verified","erase_verified",
              "OT_PZ_PAIR_STORE_MARKER_KEY","has_marker"):
    assert token in store,token
for token in ("OT_PZ_LANE_KEY_STAGED","OT_PZ_MAP_KEY_STAGED",
              "OT_PZ_PAIR_INTENT_KEY","OT_PZ_LANE_KEY_ACTIVE",
              "OT_PZ_MAP_KEY_ACTIVE"):
    assert token in store_header or token in store,token
assert store.index("OT_PZ_LANE_KEY_STAGED") < store.index(
    "OT_PZ_MAP_KEY_STAGED") < store.index("OT_PZ_PAIR_INTENT_KEY")
assert store.index("OT_PZ_LANE_KEY_ACTIVE") < store.index(
    "OT_PZ_MAP_KEY_ACTIVE")
for phase in range(1,10):
    assert f"phase_context,{phase}u" in store

# The ESP-IDF adapter commits one NVS key per durable phase. There is no
# multi-key "atomic batch" abstraction hiding an untested internal write.
nvs_write=between("static bool pz_pair_nvs_write_commit(",
                  "static bool pz_pair_nvs_erase_commit(")
assert nvs_write.count("nvs_set_blob(")==1
assert "nvs_commit(h)" in nvs_write
assert "pz_pair_nvs_write_batch_commit" not in core
pair_finish=store.split("bool ot_pz_pair_store_finish(",1)[1].split(
    "bool ot_pz_pair_store_mark_reset(",1)[0]
for token in ("OT_PZ_MAP_KEY_ACTIVE","phase_context,5u",
              "OT_PZ_PAIR_STORE_MARKER_KEY","phase_context,6u",
              "OT_PZ_LANE_KEY_STAGED","phase_context,7u",
              "OT_PZ_MAP_KEY_STAGED","phase_context,8u",
              "OT_PZ_PAIR_INTENT_KEY","phase_context,9u"):
    assert token in pair_finish,token

# Runtime promotion resumes an intent, validates the matching active pair,
# checks the quiet-input epoch, then publishes only the joined candidate.
panel_promotion=between("static void pz_promote(uint32_t epoch){",
                        "static void config_activation_task(void *unused){")
for token in ("ot_pz_pair_store_finish","ot_pz_pair_store_snapshot",
              "ot_pz_pair_store_recover","ot_pz_join",
              "config_activation_idle(epoch,true)",
              "input_quiet_samples>=OT_DEBOUNCE_TICKS",
              "live_input_mask==0u","tm_gpio_input_mask()==0u",
              "input_edge_epoch,__ATOMIC_ACQUIRE",
              "pz_outputs=workspace->pending.candidate",
              "config_recovery_fault=true"):
    assert token in panel_promotion,token
assert "nvs_open(" not in panel_promotion

# Hardware phase marker and status endpoint are present; physical cycles are
# still separate and cannot be inferred from these software assertions.
assert "PZ_PAIR_PHASE phase=%u transaction=%08lx" in core
status=between("static esp_err_t status_get(","static esp_err_t caps_get(")
for token in ("config_commit_phase","config_commit_transaction_id",
              "config_commit_pending"):
    assert token in status,token

# Legacy trigger/output records remain explicitly versioned migration input;
# do not reinterpret old blobs or erase unrelated namespaces.
assert "ot_tm_journal_upgrade_v1" in core
assert "ot_om_journal_upgrade_v1" in core
assert "ot_tm_journal_build_revisions" in core
assert "ot_tm_journal_recover" in core
assert "ot_om_journal_recover" in core
assert "#define OT_CFG_JOURNAL_VERSION 2u" in header
assert 'OT_CFG_JOURNAL_KEY_ACTIVE "active_v3"' in header
assert 'OT_CFG_JOURNAL_KEY_STAGED "stage_v3"' in header
assert "new_rev==UINT16_MAX" in core
assert "tm_revision==UINT16_MAX" in core
# Panel/Zone v3 rejects per-lane revision exhaustion in pz_save_controller_locked above.
assert 'expected_config_revision required' in core
assert 'om_number(obj,"expected_config_revision",&expected_rev,1,UINT16_MAX)' in core
assert "if(memcmp(&next_all[index],&next,sizeof(next))==0)" in core
assert "if(memcmp(&current,next,sizeof(current))==0)" in core
assert "tm_deferred.pending=true" in core
assert "pz_deferred.pending=true" in core
assert 'cJSON_AddNumberToObject(root,"active_config_revision",active_config_revision)' in core
assert 'cJSON_AddArrayToObject(obj,"active_config_revisions")' in core
assert "pending->profile=r->candidate" in journal
assert "pending->outputs,r->candidate" in journal
assert "*live=r->previous" in journal
assert "memcpy(live,r->previous" in journal
print("PASS: trigger, legacy migration, authoritative v4 lane/map pair finalization, recovery, revision and readback contracts")
