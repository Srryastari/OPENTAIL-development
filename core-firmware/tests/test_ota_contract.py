#!/usr/bin/env python3
"""OTA migration, image boundaries and local-only update contract checks.

No GPIO/flash writes occur here. Hardware rollback and NVS preservation are
separate mandatory physical release evidence.
"""
from pathlib import Path
import csv
import io
import re

root=Path(__file__).resolve().parents[1]
partition=(root/"partitions_ota.csv").read_text()
cfg=(root/"sdkconfig.defaults").read_text()
core=(root/"main/opentail_core.c").read_text()
ota=(root/"main/ot_ota_update.c").read_text()
installer=(root.parent/"scripts/flash-from-github.ps1").read_text()
dev=(root/"main/assets/dev_console.htm").read_text()
work=(root.parent/".github/workflows/verify-active-sources.yml").read_text()
assert 'CONFIG_PARTITION_TABLE_CUSTOM=y' in cfg
assert 'CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions_ota.csv"' in cfg
assert 'CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y' in cfg
assert "CONFIG_PARTITION_TABLE_SINGLE_APP_LARGE=y" not in cfg
rows={}
for fields in csv.reader(io.StringIO(partition)):
    if not fields or not fields[0].strip() or fields[0].lstrip().startswith("#"): continue
    name,kind,subtype,offset,size,*_=map(str.strip,fields)
    assert name not in rows
    rows[name]=(kind,subtype,int(offset,0),int(size,0))
assert rows["nvs"]==("data","nvs",0x9000,0x6000)
assert rows["otadata"]==("data","ota",0xf000,0x2000)
assert rows["ota_0"][:2]==("app","ota_0")
assert rows["ota_1"][:2]==("app","ota_1")
assert rows["ota_0"][2]!=rows["ota_1"][2]
for name,(kind,subtype,start,size) in rows.items():
    assert 0<=start<0x1000000 and size>0 and start+size<=0x1000000,name
    if kind=="app": assert start%0x10000==0 and size>=0x300000,name
    for other,(okind,osub,ostart,osize) in rows.items():
        if other!=name: assert start+size<=ostart or ostart+osize<=start,(name,other)
for token in ("esp_ota_begin(", "esp_ota_write(", "esp_ota_end(",
              "esp_ota_set_boot_partition(", "esp_ota_get_next_update_partition(",
              "esp_ota_mark_app_valid_cancel_rollback(",
              "X-OpenTail-Update", "Origin", "192.168.4.1",
              "esp_ota_get_partition_description(", "ot_ota_available()"):
    assert token in ota,token
assert '.uri="/api/v1/ota"' in core
for token in ('id="otaFile"', 'id="otaSend"', 'id="otaStatus"',
              'async function otaCheck()', 'async function otaSend()',
              "'X-OpenTail-Update':'confirmed'", 'fetch('/api/v1/ota'',
              'confirm('):
    assert token in dev, token
assert "ot_ota_confirm_boot(true)" in core
assert "gh run download" in installer and "gh run view" in installer
assert "gh run list" in installer and "-VerifyOnly" not in installer
assert "gh auth status" in installer
assert "if ($VerifyOnly)" in installer
assert "Remove-Item -LiteralPath $work" in installer
assert "github-flash-package-check:" in work
assert "release-evidence-gate:" in work
print("OTA host contract: dual slots, legacy NVS preservation, rollback hooks, GitHub artifact and release gates found (NO HARDWARE TEST).")
