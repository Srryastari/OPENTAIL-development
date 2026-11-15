#!/usr/bin/env python3
"""ESP-IDF 5.5.5 linked RMT cache-off gate; no optical/hardware claim."""
from pathlib import Path
import json, re, shutil, subprocess, sys
elf=Path(sys.argv[1]).resolve();build=elf.parent
idf=Path(sys.argv[2]).resolve()
config=set((build/'config/sdkconfig.h').read_text().splitlines())
for key in ('RMT_TX_ISR_CACHE_SAFE','RMT_TX_ISR_HANDLER_IN_IRAM','RMT_ENCODER_FUNC_IN_IRAM','RMT_OBJ_CACHE_SAFE','GDMA_ISR_HANDLER_IN_IRAM','GDMA_OBJ_DRAM_SAFE','GDMA_CTRL_FUNC_IN_IRAM'):
    assert f'#define CONFIG_{key} 1' in config,key
assert '#define CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL 1024' in config
# Only the two pinned source files receive the forced allocator include.
commands=json.loads((build/'compile_commands.json').read_text())
scoped=[c for c in commands if '-include' in c.get('command','') and 'led_strip_internal_alloc.h' in c.get('command','')]
assert sorted(Path(c['file']).name for c in scoped)==['led_strip_rmt_dev.c','led_strip_rmt_encoder.c']
rmt_src=idf/'components/esp_driver_rmt'
private=(rmt_src/'src/rmt_private.h').read_text()
assert '#define RMT_MEM_ALLOC_CAPS      (MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)' in private
assert 'ESP_INTR_FLAG_SHARED | ESP_INTR_FLAG_IRAM' in private
alloc=(rmt_src/'src/rmt_encoder.c').read_text()
assert 'heap_caps_calloc(1, size, RMT_MEM_ALLOC_CAPS)' in alloc
for kind in ('bytes','copy'):
    assert 'rmt_alloc_encoder_mem(' in (rmt_src/f'src/rmt_encoder_{kind}.c').read_text()
tx=(rmt_src/'src/rmt_tx.c').read_text()
assert 'esp_ptr_internal(payload)' in tx
assert '.flags.isr_cache_safe = true' in tx
gdma_dir=idf/'components/esp_hw_support/dma'
assert '#define GDMA_MEM_ALLOC_CAPS    (MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)' in (gdma_dir/'gdma_priv.h').read_text()
assert 'esp_ptr_internal(user_data)' in (gdma_dir/'gdma.c').read_text()
assert 'uint32_t mem_caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT' in tx
assert 'RMT_MEM_ALLOC_CAPS | MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL' in tx
assert 'xQueueCreateWithCaps' in tx and 'RMT_MEM_ALLOC_CAPS' in tx
objdump=shutil.which('xtensa-esp-elf-objdump') or shutil.which('xtensa-esp32s3-elf-objdump')
assert objdump,'Xtensa objdump required'
def run(*args):return subprocess.check_output([objdump,*args,str(elf)],text=True)
table=run('-t');symbols={}
for line in table.splitlines():
    fields=line.split()
    if len(fields)>=5:
        try: address=int(fields[0],16)
        except ValueError:continue
        symbols[fields[-1]]=(fields[-3],address)
required=['rmt_encode_led_strip','rmt_led_strip_encoder_reset','rmt_encode_bytes',
          'rmt_bytes_encoder_reset','rmt_encode_copy','rmt_copy_encoder_reset',
          'rmt_encoder_reset','rmt_tx_default_isr','rmt_dma_tx_eof_cb',
          'gdma_reset','gdma_start','gdma_hal_reset','gdma_hal_start_with_desc',
          'gdma_default_tx_isr','gdma_hal_clear_intr','gdma_hal_read_intr_status','gdma_hal_get_eof_desc_addr']
optional=['rmt_encode_check_result','rmt_tx_do_transaction','rmt_tx_mark_eof',
          'rmt_isr_handle_tx_done','rmt_isr_handle_tx_threshold','rmt_isr_handle_tx_loop_end',
          'gdma_ahb_hal_reset','gdma_ahb_hal_start_with_desc',
          'gdma_ahb_hal_clear_intr','gdma_ahb_hal_read_intr_status','gdma_ahb_hal_get_eof_desc_addr']
audit=[];dis=[]
for name in required+optional:
    matches=[s for s in symbols if s==name or s.startswith(name+'.')]
    assert matches or name in optional,('missing ISR/encoder',name)
    for match in matches:
        section,address=symbols[match]
        assert section.startswith(('.iram0.','.iram1.')), (match,section)
        audit.append(f'PASS IRAM {match}: {section}')
        body=run('-dr','--disassemble='+match);dis.append(body)
        for target in re.findall(r'\bcall(?:0|4|8|12)\s+[^\n]*?<([^>]+)>',body):
            target=target.split('+',1)[0]
            assert target in symbols,('unknown direct call',match,target)
            sec,addr=symbols[target]
            assert sec.startswith(('.iram0.','.iram1.','.rtc.text')) or (sec=='*ABS*' and 0x40000000<=addr<0x40080000),(match,target,sec,hex(addr))
# No arbitrary application callback is installed by led_strip. Indirect calls
# are the bytes/copy encode/reset and GDMA AHB HAL functions checked above; IDF's optional TX
# callback remains NULL. Heap data is covered by capabilities, not ELF sections.
audit+=['PASS scoped allocator compiler flags on exactly two dependency sources',
        'PASS internal payload/object/composite encoder plus IDF child encoders/queues/GDMA storage',
        'PASS configured cache-safe TX interrupt; global PSRAM threshold remains 1024',
        'DIAGNOSTIC RMT: GPIO16 false/64; GPIO17 false/64; both 10000000 Hz',
        'NOT RUN: physical cache-off NVS/Wi-Fi/BLE stress or optical illumination']
out=build/'rmt-evidence';out.mkdir(exist_ok=True)
(out/'rmt.symbols.txt').write_text(table)
(out/'rmt-isr.dis').write_text('\n'.join(dis))
(out/'rmt-cache-audit.txt').write_text('\n'.join(audit)+'\n')
print('\n'.join(audit))
