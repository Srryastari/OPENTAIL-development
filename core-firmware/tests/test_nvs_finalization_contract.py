#!/usr/bin/env python3
"""Source-contract audit for CORE durable config finalization (software only).

Pure tests cannot simulate an ESP32 interrupt or torn physical flash, so this
pins the *real* firmware control flow and complements config_journal.c host
recovery/CRC/revision tests. Physical validation remains mandatory.
"""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
core=(root/"main/opentail_core.c").read_text()
journal=(root/"main/config_journal.c").read_text()
header=(root/"main/config_journal.h").read_text()
def between(start,end):
    assert start in core and end in core,(start,end)
    return core.split(start,1)[1].split(end,1)[0]
for operation,begin,end,ram in (
    ("trigger","static void tm_promote(uint32_t epoch){",
     "static void om_promote(uint32_t epoch){","ot_tm_apply_if_released("),
    ("output","static void om_promote(uint32_t epoch){",
     "static void config_activation_task(void *unused){","ot_om_apply_if_released("),
):
    body=between(begin,end)
    assert body.index("config_write_active(h,")<body.index("config_clear_stage(h)")
    assert body.index("config_clear_stage(h)")<body.index(ram),operation
    assert body.rfind("config_activation_idle(epoch)")<body.index(ram),operation
    assert body.index("tm_gpio_input_mask()==0u")<body.index(ram),operation
    assert "input_quiet_samples>=OT_DEBOUNCE_TICKS" in body
    assert "__atomic_load_n(&input_edge_epoch,__ATOMIC_ACQUIRE)==epoch" in body
    assert "config_recovery_fault=true" in body
    assert "stage.candidate_revision" in body
    assert "stage.previous_revision" in body
    assert "const bool staged=read==ESP_OK" in body
    assert "OT_CFG_JOURNAL_KEY_ACTIVE" in body
for start,end,rev in (
    ("static void tm_journal_load(void){","static bool tm_save(","tm_revision"),
    ("static void om_journal_load(void){","static bool om_save(","om_revision"),
):
    recovery=between(start,end)
    assert rev in recovery
    assert "previous_revision" in recovery and "candidate_revision" in recovery
    assert "ot_tm_journal_recover" in recovery or "ot_om_journal_recover" in recovery
assert "ot_tm_journal_upgrade_v1" in core
assert "ot_om_journal_upgrade_v1" in core
assert "ot_tm_journal_build_revisions" in core
assert "ot_om_journal_build_revisions" in core
assert "new_rev==UINT16_MAX" in core
assert "tm_revision==UINT16_MAX" in core and "om_revision[index]==UINT16_MAX" in core
assert 'expected_config_revision required' in core
assert 'om_number(obj,"expected_config_revision",&expected_rev,1,UINT16_MAX)' in core
assert "tm_deferred.pending=true" in core and "om_deferred.pending=true" in core
assert 'cJSON_AddNumberToObject(root,"active_config_revision",active_config_revision)' in core
assert 'cJSON_AddArrayToObject(obj,"active_config_revisions")' in core
assert "pending->profile=r->candidate" in journal
assert "pending->outputs,r->candidate" in journal
assert "*live=r->previous" in journal
assert "memcpy(live,r->previous" in journal
assert "#define OT_CFG_JOURNAL_VERSION 2u" in header
assert 'OT_CFG_JOURNAL_KEY_ACTIVE "active_v3"' in header
assert 'OT_CFG_JOURNAL_KEY_STAGED "stage_v3"' in header
print("PASS: firmware-only NVS finalization fence, replay, revision and REST/BLE source contract")
