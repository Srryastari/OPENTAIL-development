#!/usr/bin/env python3
"""Source-contract audit for CORE durable config finalization (software only).

Pure tests cannot simulate an ESP32 interrupt or torn physical flash, so this
pins the real trigger, legacy-output compatibility, and authoritative
Panel/Zone v3 control flow. Host journal recovery tests complement this source
audit; physical brownout/ISR testing remains mandatory.
"""
from pathlib import Path

root=Path(__file__).resolve().parents[1]
core=(root/"main/opentail_core.c").read_text()
journal=(root/"main/config_journal.c").read_text()
header=(root/"main/config_journal.h").read_text()


def between(start,end):
    assert start in core and end in core,(start,end)
    return core.split(start,1)[1].split(end,1)[0]


# The trigger profile and the retained legacy-output compatibility path keep
# their original stage -> active -> erase -> quiet-window -> RAM ordering.
for operation,begin,end,ram in (
    ("trigger","static void tm_promote(uint32_t epoch){",
     "static void om_promote(uint32_t epoch){","ot_tm_apply_if_released("),
    ("legacy output compatibility","static void om_promote(uint32_t epoch){",
     "static void config_activation_task(void *unused){","ot_om_apply_if_released("),
):
    body=between(begin,end)
    assert body.index("config_write_active(h,")<body.index("config_clear_stage(h)"),operation
    assert body.index("config_clear_stage(h)")<body.index(ram),operation
    assert body.rfind("config_activation_idle(epoch,false)")<body.index(ram),operation
    assert body.index("tm_gpio_input_mask()==0u")<body.index(ram),operation
    assert "input_quiet_samples>=OT_DEBOUNCE_TICKS" in body
    assert "__atomic_load_n(&input_edge_epoch,__ATOMIC_ACQUIRE)==epoch" in body
    assert "config_recovery_fault=true" in body
    assert "stage.candidate_revision" in body
    assert "stage.previous_revision" in body
    assert "const bool staged=read==ESP_OK" in body
    assert "OT_CFG_JOURNAL_KEY_ACTIVE" in body


# The legacy v3 combined record is read-only migration input. The authoritative
# v4 runtime joins a lane-only output_cfg journal and map-only zone_map journal.
pz_load=between("static void pz_load(void){",
                "static bool pz_save_controller_locked(")
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
assert "pz_save_controller_locked(&candidate)" in legacy_output_save
assert "nvs_open(" not in legacy_output_save
assert "nvs_set_blob(" not in legacy_output_save
assert "static void om_promote(" not in core
assert "static void pz_sync_from_legacy(" not in core

# The exact same portable v4 store used by CORE implements all eight durable
# write boundaries and is exercised by test_panel_zone_pair_store.c.
store=(root/"main/panel_zone_pair_store.c").read_text()
store_header=(root/"main/panel_zone_pair_store.h").read_text()
for token in ("ot_pz_pair_store_begin","ot_pz_pair_store_finish",
              "ot_pz_pair_store_snapshot","ot_pz_pair_store_recover",
              "ot_pz_pair_store_mark_reset","write_verified","erase_verified",
              "write_pair_verified","OT_PZ_PAIR_STORE_MARKER_KEY","has_marker"):
    assert token in store,token
for token in ("OT_PZ_LANE_KEY_STAGED","OT_PZ_MAP_KEY_STAGED",
              "OT_PZ_PAIR_INTENT_KEY","OT_PZ_LANE_KEY_ACTIVE",
              "OT_PZ_MAP_KEY_ACTIVE"):
    assert token in store_header or token in store,token
assert store.index("OT_PZ_LANE_KEY_STAGED") < store.index(
    "OT_PZ_MAP_KEY_STAGED") < store.index("OT_PZ_PAIR_INTENT_KEY")
assert store.index("OT_PZ_LANE_KEY_ACTIVE") < store.index(
    "OT_PZ_MAP_KEY_ACTIVE")
for phase in range(1,9):
    assert f"phase_context,{phase}u" in store

# Runtime promotion resumes an intent, validates the matching active pair,
# checks the quiet-input epoch, then publishes only the joined candidate.
panel_promotion=between("static void pz_promote(uint32_t epoch){",
                        "static void om_promote(uint32_t epoch){")
for token in ("ot_pz_pair_store_finish","ot_pz_pair_store_snapshot",
              "ot_pz_pair_store_recover","ot_pz_join",
              "config_activation_idle(epoch,true)",
              "input_quiet_samples>=OT_DEBOUNCE_TICKS",
              "live_input_mask==0u","tm_gpio_input_mask()==0u",
              "input_edge_epoch,__ATOMIC_ACQUIRE",
              "pz_outputs=recovered_pending.candidate",
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
