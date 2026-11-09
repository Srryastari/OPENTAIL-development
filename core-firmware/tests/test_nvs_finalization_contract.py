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


# Authoritative Panel/Zone v3 load/migration uses ot_zone_map and the v3
# previous/candidate journal. A corrupt v3 record must fail closed rather
# than falling back to stale embedded legacy zone definitions.
pz_load=between("static void pz_load(void){",
                "static bool pz_save_controller_locked(")
assert 'nvs_open("ot_zone_map",NVS_READONLY' in pz_load
assert "ot_pz_journal_recover(" in pz_load
assert "pz_stage_legacy_migration()" in pz_load
assert "Corrupt v3 data never falls back to legacy embedded zones" in pz_load
assert "pz_reset_tombstone_valid" in pz_load
assert "OT_PZ_RESET_TOMBSTONE_KEY" in pz_load
assert "pz_reset_pending_mask=tombstone.reset_mask" in pz_load

legacy_migration=between("static bool pz_stage_legacy_migration(void){",
                         "static void pz_load(void){")
assert "ot_pz_migrate_legacy(om_outputs,om_persistent,&previous)" in legacy_migration
assert "ot_pz_journal_build(&migration,&previous,&candidate," in legacy_migration
assert 'nvs_open("ot_zone_map",NVS_READWRITE' in legacy_migration
assert "nvs_set_blob(h,OT_PZ_JOURNAL_KEY_STAGED,&migration,sizeof(migration))" in legacy_migration
assert "nvs_commit(h)" in legacy_migration
assert 'nvs_open("ot_output_cfg",NVS_READWRITE' not in legacy_migration

# Saving a new Panel/Zone map writes and verifies the staged journal before
# publishing any candidate or revision to RAM.
pz_save=between("static bool pz_save_controller_locked(",
                "/* Legacy output writes are lossless")
assert "ot_pz_journal_build(&staged,&previous,candidate," in pz_save
assert 'nvs_open("ot_zone_map",NVS_READWRITE' in pz_save
stage_write=pz_save.index("nvs_set_blob(h,OT_PZ_JOURNAL_KEY_STAGED")
stage_commit=pz_save.index("if(e==ESP_OK)e=nvs_commit(h)",stage_write)
stage_readback=pz_save.index("nvs_get_blob(h,OT_PZ_JOURNAL_KEY_STAGED",stage_commit)
ram_publish=pz_save.index("pz_deferred.candidate=*candidate;")
assert stage_write<stage_commit<stage_readback<ram_publish
assert "ot_pz_journal_valid(&readback)" in pz_save
assert "memcmp(&readback,&staged,sizeof(staged))!=0" in pz_save
assert "candidate_revision[lane]==UINT16_MAX" in pz_save
assert "pz_revision,candidate_revision" in pz_save
assert "No RAM mutation before verified durable stage" in pz_save

# Panel/Zone promotion is distinct from the legacy output promoter. The
# candidate cannot become active until active_v3 readback, reset tombstone
# durability (when applicable), stage erasure, and a fresh quiet GPIO/epoch
# check have all succeeded.
panel_promotion=between("static void pz_promote(uint32_t epoch){",
                        "static void om_promote(uint32_t epoch){")
for token in (
    'nvs_open("ot_zone_map",NVS_READWRITE',
    "pz_journal_read(h,OT_PZ_JOURNAL_KEY_STAGED,&stage)",
    "pz_deferred.pending",
    "pz_revision,stage.candidate_revision",
    "pz_active_revision,stage.previous_revision",
    "memcmp(&pz_outputs,&stage.previous,sizeof(pz_outputs))",
    "memcmp(&pz_deferred.candidate,&stage.candidate,sizeof(pz_deferred.candidate))",
    "config_activation_idle(epoch,true)",
    "input_quiet_samples>=OT_DEBOUNCE_TICKS",
    "live_input_mask==0u",
    "tm_gpio_input_mask()==0u",
    "__atomic_load_n(&input_edge_epoch,__ATOMIC_ACQUIRE)==epoch",
    "config_recovery_fault=true",
):
    assert token in panel_promotion,token
active_write=panel_promotion.index("config_write_panel_active(h,&stage)")
active_readback=panel_promotion.index(
    "pz_journal_read(h,OT_PZ_JOURNAL_KEY_ACTIVE,&active_readback)")
tombstone=panel_promotion.index("pz_reset_tombstone_write_handle(")
stage_erase=panel_promotion.index("config_clear_panel_stage(h)")
fresh_quiet=panel_promotion.index("if(!config_activation_idle(epoch,true))",stage_erase)
ram_publish=panel_promotion.index("pz_outputs=stage.candidate")
assert active_write<active_readback<tombstone<stage_erase<fresh_quiet<ram_publish
assert "pz_refresh_legacy_projection_locked();" in panel_promotion
assert "pz_active_revision,stage.candidate_revision" in panel_promotion
assert "pz_deferred.pending=false" in panel_promotion

# The portable journal validates CRCs and monotonic per-lane revisions and
# restores the previous image when either the staged or active recovery copy
# is the last durable record.
assert "ot_pz_journal_build" in journal
assert "ot_pz_journal_valid" in journal
assert "ot_pz_journal_recover" in journal
pz_recover=journal.split("bool ot_pz_journal_recover(",1)[1]
assert "ot_pz_journal_valid(active)" in pz_recover
assert "ot_pz_journal_valid(staged)" in pz_recover
assert "promotion_copy" in pz_recover and "next_transaction" in pz_recover
assert "*live=record->previous;" in pz_recover
assert "pending->candidate=record->candidate;" in pz_recover
assert "pending->pending=staged!=NULL||" in pz_recover
assert "candidate_revision[lane]<=previous_revision[lane]" in journal

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
print("PASS: trigger, legacy compatibility, authoritative Panel/Zone v3 NVS finalization, recovery, revision and readback contracts")
