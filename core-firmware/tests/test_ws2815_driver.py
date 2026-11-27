#!/usr/bin/env python3
"""Compile the actual pinned led_strip sources against a fake RMT peripheral.
Supply managed_components/espressif__led_strip (CI) or the upstream 3.1.1 tree.
This validates symbols/buffer lifecycle, not RF, GPIO voltage or illumination.
"""
from pathlib import Path
import hashlib, re, subprocess, sys, tempfile
root=Path(__file__).resolve().parents[1]
core=(root/'main/opentail_core.c').read_text()
init=core.split('static bool strip_configure_channel(',1)[1].split('static bool strip_init(',1)[0]
assert '.led_model=LED_MODEL_CUSTOM' in init
assert '.resolution_hz=10000000' in init
assert '.mem_block_symbols=64' in init
assert '.flags={.with_dma=false}' in init
assert 'strip_probe_route_pending[ch]=true' in init
render=core.split('static void led_task(',1)[1]
probe=render.split('if(strip_probe_route_pending[ch]&&probe_nonblack[ch]&&',1)[1].split('esp_err_t e=led_strip_refresh_async(strip[ch]);',1)[0]
assert 'local_diag.mode==OT_PZ_DIAG_PROBE' in probe
assert 'strip_probe_route_pending[ch]=false' in probe
assert 'gpio_dump_io_configuration(stdout,(1ULL << 16) | (1ULL << 17))' in probe
assert 'route.sig_out==strip_gpio_signal[ch]' in probe
assert 'CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=1024' in (root/'sdkconfig.defaults').read_text()
if len(sys.argv)==1:
    print('PASS WS2815 CORE contract: custom timing, GPIO16 non-DMA/64 + GPIO17 non-DMA/64, 10 MHz; one-shot authorized routing after each resize')
    sys.exit(0)
dep=Path(sys.argv[1])
expected={
 'src/led_strip_rmt_dev.c':'42d57fc98147239ca23389a58fbf33cfe83dc932d46ee38d9f6104949b88c32b',
 'src/led_strip_rmt_encoder.c':'7aec4c1946a554cae1b307166ad0185f6335436f5ffd1cafb71e812c484f4050',
 'include/led_strip_types.h':'18fa8b9d558782df0c8a980ddd38b02d5fd1705801d86e1be1902e385d15e26a',
}
assert re.search(r'''^version:\s*["']?3\.1\.1["']?\s*$''', (dep/'idf_component.yml').read_text(), re.M)
for p,sha in expected.items():
    assert hashlib.sha256((dep/p).read_bytes()).hexdigest()==sha,('pinned dependency changed',p)
def clean(p):
    return re.sub(r'^\s*#(?:include|pragma once)[^\n]*','', (dep/p).read_text(), flags=re.M)
stubs=r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include "output_manager.h"
typedef int esp_err_t; typedef int gpio_num_t; typedef int rmt_clock_source_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NO_MEM 1
#define ESP_ERR_INVALID_ARG 2
#define ESP_ERR_INVALID_STATE 3
#define ESP_ERR_NOT_SUPPORTED 4
#define ESP_IDF_VERSION_VAL(a,b,c) ((a)*10000+(b)*100+(c))
#define ESP_IDF_VERSION ESP_IDF_VERSION_VAL(5,5,5)
#define RMT_ENCODER_FUNC_ATTR
#define BIT(n) (1u<<(n))
#define __containerof(p,t,m) ((t*)((char*)(p)-offsetof(t,m)))
#define ESP_RETURN_ON_FALSE(c,e,...) do{if(!(c))return(e);}while(0)
#define ESP_RETURN_ON_ERROR(x,...) do{int rc=(x);if(rc)return rc;}while(0)
#define ESP_GOTO_ON_FALSE(c,e,label,...) do{if(!(c)){ret=(e);goto label;}}while(0)
#define ESP_GOTO_ON_ERROR(x,label,...) do{ret=(x);if(ret)goto label;}while(0)
#define ESP_LOGE(...) ((void)0)
#define RMT_CLK_SRC_DEFAULT 1
#define RMT_ENCODING_COMPLETE 1
#define RMT_ENCODING_MEM_FULL 2
typedef unsigned rmt_encode_state_t;
typedef struct { unsigned duration0:15,level0:1,duration1:15,level1:1; } rmt_symbol_word_t;
typedef struct rmt_channel { int gpio; bool dma; unsigned symbols; } *rmt_channel_handle_t;
typedef struct rmt_encoder rmt_encoder_t;
struct rmt_encoder {
 size_t (*encode)(rmt_encoder_t*,rmt_channel_handle_t,const void*,size_t,rmt_encode_state_t*);
 esp_err_t (*reset)(rmt_encoder_t*); esp_err_t (*del)(rmt_encoder_t*);
};
typedef rmt_encoder_t *rmt_encoder_handle_t;
typedef struct {rmt_symbol_word_t bit0,bit1;struct {bool msb_first;} flags;} rmt_bytes_encoder_config_t;
typedef struct {int unused;} rmt_copy_encoder_config_t;
typedef struct {int clk_src,gpio_num;unsigned mem_block_symbols,resolution_hz,trans_queue_depth;struct {bool with_dma,invert_out;} flags;} rmt_tx_channel_config_t;
typedef struct {int loop_count;} rmt_transmit_config_t;
static rmt_bytes_encoder_config_t encoded_bits;
static rmt_symbol_word_t encoded_reset;
static const void *submitted;static size_t submitted_bytes;static int wait_error;
static size_t fake_bytes(rmt_encoder_t *e,rmt_channel_handle_t ch,const void *p,size_t n,rmt_encode_state_t *s){(void)e;(void)ch;(void)p;*s=RMT_ENCODING_COMPLETE;return n*8;}
static size_t fake_copy(rmt_encoder_t *e,rmt_channel_handle_t ch,const void *p,size_t n,rmt_encode_state_t *s){(void)e;(void)ch;assert(n==sizeof(encoded_reset));memcpy(&encoded_reset,p,n);*s=RMT_ENCODING_COMPLETE;return 1;}
static int fake_reset(rmt_encoder_t *e){(void)e;return 0;}
static int rmt_new_bytes_encoder(const rmt_bytes_encoder_config_t *c,rmt_encoder_handle_t *e){encoded_bits=*c;*e=calloc(1,sizeof(**e));(*e)->encode=fake_bytes;return 0;}
static int rmt_new_copy_encoder(const rmt_copy_encoder_config_t *c,rmt_encoder_handle_t *e){(void)c;*e=calloc(1,sizeof(**e));(*e)->encode=fake_copy;return 0;}
static int rmt_del_encoder(rmt_encoder_handle_t e){if(e->del)return e->del(e);free(e);return 0;}
static int rmt_encoder_reset(rmt_encoder_handle_t e){return e->reset?e->reset(e):fake_reset(e);}
static int rmt_new_tx_channel(const rmt_tx_channel_config_t *c,rmt_channel_handle_t *ch){assert(c->resolution_hz==10000000);*ch=calloc(1,sizeof(**ch));(*ch)->gpio=c->gpio_num;(*ch)->dma=c->flags.with_dma;(*ch)->symbols=c->mem_block_symbols;return 0;}
static int rmt_del_channel(rmt_channel_handle_t ch){free(ch);return 0;}
static int rmt_enable(rmt_channel_handle_t ch){assert(ch);return 0;}
static int rmt_disable(rmt_channel_handle_t ch){assert(ch);return 0;}
static int rmt_transmit(rmt_channel_handle_t ch,rmt_encoder_handle_t e,const void *p,size_t n,const rmt_transmit_config_t *c){(void)c;assert(ch);submitted=p;submitted_bytes=n;rmt_encode_state_t s=0;assert(e->encode(e,ch,p,n,&s)==n*8+1);assert(s==RMT_ENCODING_COMPLETE);return 0;}
static int rmt_tx_wait_all_done(rmt_channel_handle_t ch,int timeout){assert(ch&&timeout==-1);return wait_error;}
static int rmt_tx_switch_gpio(rmt_channel_handle_t ch,int gpio,bool invert){(void)invert;ch->gpio=gpio;return 0;}
'''
cfg=re.search(r'const led_strip_config_t cfg=\{.*?\n    \};',init,re.S).group()
rmt=re.search(r'const led_strip_rmt_config_t rmt=\{.*?\n    \};',init,re.S).group()
source=stubs+clean('include/led_strip_types.h')+clean('interface/led_strip_interface.h')+clean('include/led_strip_rmt.h')+clean('src/led_strip_rmt_encoder.h')+clean('src/led_strip_common.h')
# Stage B compiles the exact allocation shim used by the two dependency TUs.
shim=root/'main/led_strip_internal_alloc.h'
if shim.exists():
    source+=r"""
#define MALLOC_CAP_INTERNAL 1u
#define MALLOC_CAP_8BIT 2u
#define ESP_LOGI(...) ((void)0)
static unsigned internal_allocations;
static bool fail_internal;
static void *heap_caps_calloc(size_t n,size_t size,unsigned caps){
 assert(caps==(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));internal_allocations++;
 return fail_internal?NULL:calloc(n,size);
}
static bool esp_ptr_internal(const void *p){return p!=NULL;}
"""
    source+=re.sub(r'^\s*#(?:include|pragma once)[^\n]*','',shim.read_text(),flags=re.M)
source+=clean('src/led_strip_rmt_encoder.c').replace('TAG','ENC_TAG')+clean('src/led_strip_rmt_dev.c')
source+='\nstatic const int out_pin[2]={16,17};\nstatic led_strip_config_t core_cfg(unsigned ch,unsigned pixels){'+cfg+'return cfg;}\n'
source+='static led_strip_rmt_config_t core_rmt(unsigned ch){'+rmt+'return rmt;}\n'
if shim.exists():
    source+=r"""
static void allocation_failure_test(void){
 led_strip_handle_t handle=NULL;led_strip_config_t cfg=core_cfg(0,2048);
 led_strip_rmt_config_t rmt=core_rmt(0);fail_internal=true;
 assert(led_strip_new_rmt_device(&cfg,&rmt,&handle)==ESP_ERR_NO_MEM&&handle==NULL);
 fail_internal=false;
}
"""
source+=r'''
int main(void){
 led_strip_handle_t handles[2]={0};
 for(unsigned ch=0;ch<2;ch++){
  led_strip_config_t cfg=core_cfg(ch,2048);led_strip_rmt_config_t rmt=core_rmt(ch);
  assert(led_strip_new_rmt_device(&cfg,&rmt,&handles[ch])==0);
  led_strip_rmt_obj *obj=(void*)handles[ch];
  assert(obj->rmt_chan->gpio==out_pin[ch]);assert(obj->rmt_chan->dma==false);
  assert(obj->rmt_chan->symbols==64);
  assert(encoded_bits.bit0.duration0==3&&encoded_bits.bit0.duration1==9);
  assert(encoded_bits.bit1.duration0==9&&encoded_bits.bit1.duration1==3);
  assert(encoded_bits.bit0.level0==1&&encoded_bits.bit0.level1==0&&encoded_bits.flags.msb_first);
  assert(encoded_bits.bit1.level0==1&&encoded_bits.bit1.level1==0);
  ot_rgb_t white=ot_om_limit_brightness((ot_rgb_t){255,255,255},32);
  for(unsigned order=0;order<6;order++){
   ot_rgb_t wire=ot_om_swizzle(white,order);
   assert(wire.r==32&&wire.g==32&&wire.b==32);
   assert(handles[ch]->set_pixel(handles[ch],2047,wire.r,wire.g,wire.b)==0);
  }
  assert(obj->pixel_buf[2047*3]==32&&obj->pixel_buf[2047*3+1]==32&&obj->pixel_buf[2047*3+2]==32);
  assert(handles[ch]->refresh_async(handles[ch])==0);
  assert(submitted==obj->pixel_buf&&submitted_bytes==6144);
  assert(encoded_reset.level0==0&&encoded_reset.level1==0);
  assert(encoded_reset.duration0==1600&&encoded_reset.duration1==1600);
  assert(handles[ch]->set_pixel(handles[ch],0,1,2,3)==ESP_ERR_INVALID_STATE);
  assert(handles[ch]->del(handles[ch])==ESP_ERR_INVALID_STATE);
  wait_error=ESP_FAIL;assert(handles[ch]->refresh_async_done(handles[ch])==ESP_FAIL);
  wait_error=0;assert(handles[ch]->refresh_async_done(handles[ch])==0);
  assert(handles[ch]->set_pixel(handles[ch],0,11,22,33)==0);
  assert(obj->pixel_buf[0]==33&&obj->pixel_buf[1]==11&&obj->pixel_buf[2]==22); /* Existing BRG driver + CORE permutation retained. */
 }
 assert(((led_strip_rmt_obj*)handles[0])->pixel_buf!=((led_strip_rmt_obj*)handles[1])->pixel_buf);
 for(unsigned ch=0;ch<2;ch++){
  assert(handles[ch]->clear(handles[ch])==0);
  assert(handles[ch]->del(handles[ch])==0);handles[ch]=NULL;
  led_strip_config_t cfg=core_cfg(ch,7);led_strip_rmt_config_t rmt=core_rmt(ch);
  assert(led_strip_new_rmt_device(&cfg,&rmt,&handles[ch])==0);
  assert(handles[ch]->set_pixel(handles[ch],6,32,32,32)==0);
  assert(handles[ch]->set_pixel(handles[ch],7,32,32,32)==ESP_ERR_INVALID_ARG);
  assert(handles[ch]->refresh_async(handles[ch])==0);
  assert(submitted==((led_strip_rmt_obj*)handles[ch])->pixel_buf&&submitted_bytes==21);
  assert(handles[ch]->refresh_async_done(handles[ch])==0);
  assert(handles[ch]->del(handles[ch])==0);
 }
 puts("PASS actual led_strip 3.1.1: bit0=3/9 bit1=9/3 reset=1600+1600 low; separate 6144-byte buffers, exact transmit pointer/length, nonzero white, busy/error paths and resize; GPIO16 non-DMA/64 + GPIO17 non-DMA/64 at 10MHz (host only)");
}
'''
if shim.exists():
    source=source.replace('int main(void){','int main(void){allocation_failure_test();')
    source=source.replace(' puts("PASS actual', ' assert(internal_allocations==9); puts("PASS internal-SRAM-only allocations and no-memory failure");\n puts("PASS actual')
with tempfile.TemporaryDirectory(prefix='ot-ws2815-') as d:
    c=Path(d)/'driver.c';exe=Path(d)/'driver';c.write_text(source)
    subprocess.run(['cc','-std=gnu11','-Wall','-Wextra','-Wno-unused-variable','-I',str(root/'main'),str(c),str(root/'main/output_manager.c'),str(root/'main/core_logic.c'),str(root/'main/arbitration_logic.c'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
