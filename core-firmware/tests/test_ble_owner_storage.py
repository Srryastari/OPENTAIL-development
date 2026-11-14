#!/usr/bin/env python3
"""Execute actual production enrollment/storage callbacks with simulated NVS/GAP.
No RF claim: OS SMP negotiation and bonding persistence still need a board.
"""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
source=(root/'main/ble_status.c').read_text()
production=source[source.index('typedef struct {\n    uint32_t version;'):source.index('static int owner_challenge_read(')]
stubs=r'''
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "ble_owner_policy.h"
#include "config_journal.h"
typedef int esp_err_t; typedef int nvs_handle_t;
#define ESP_OK 0
#define ESP_ERR_NVS_NOT_FOUND 1
#define NVS_READONLY 0
#define NVS_READWRITE 1
#define BLE_HS_CONN_HANDLE_NONE 65535
#define BLE_HS_IO_DISPLAY_ONLY 0
#define BLE_HS_IO_NO_INPUT_OUTPUT 3
#define BLE_SM_PAIR_KEY_DIST_ENC 1
#define BLE_SM_PAIR_KEY_DIST_ID 2
#define BLE_ATT_ERR_INSUFFICIENT_ENC 15
#define BLE_ATT_ERR_WRITE_NOT_PERMITTED 3
#define BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN 13
#define BLE_ATT_ERR_UNLIKELY 14
#define BLE_ATT_ERR_READ_NOT_PERMITTED 2
#define BLE_ATT_ERR_INSUFFICIENT_RES 17
#define BLE_GATT_ACCESS_OP_WRITE_CHR 2
#define BLE_GATT_ACCESS_OP_READ_CHR 1
struct {int sm_bonding,sm_sc,sm_mitm,sm_io_cap,sm_our_key_dist,sm_their_key_dist;} ble_hs_cfg;
struct ble_gap_conn_desc {struct {bool encrypted,bonded,authenticated;} sec_state; struct {uint8_t type,val[6];} peer_id_addr;};
struct ble_gap_conn_desc link;
struct os_mbuf {uint8_t bytes[32];uint16_t len;};
struct ble_gatt_access_ctxt {int op;struct os_mbuf *om;};
static uint16_t active_conn=1;
static bool authorized,owner_valid;
static uint8_t owner_key[16];
static uint8_t durable[128],pending[128],legacy[16];
static size_t durable_len,pending_len;
static bool legacy_exists;
static int fault;
static int nvs_open(const char *s,int mode,int *h){(void)s;(void)mode;*h=1;return fault==1?9:0;}
static int nvs_set_blob(int h,const char *k,const void *p,size_t n){(void)h;(void)k;if(fault==2)return 9;memcpy(pending,p,n);pending_len=n;return 0;}
static int nvs_commit(int h){(void)h;if(fault==3)return 9;memcpy(durable,pending,pending_len);durable_len=pending_len;return 0;}
static int nvs_get_blob(int h,const char *k,void *p,size_t *n){(void)h;
 if(fault==4)return 9;
 if(!strcmp(k,"owner")){if(!legacy_exists)return 1;memcpy(p,legacy,16);*n=16;return 0;}
 if(!durable_len)return 1;
 if(*n<durable_len)return 9;
 memcpy(p,durable,durable_len);*n=durable_len;return 0;}
static void nvs_close(int h){(void)h;}
static int ble_gap_conn_find(uint16_t h,struct ble_gap_conn_desc *d){(void)h;*d=link;return 0;}
static void esp_fill_random(void *p,size_t n){memset(p,0x53,n);}
static int os_mbuf_append(struct os_mbuf *b,const void *p,size_t n){memcpy(b->bytes,p,n);b->len=n;return 0;}
static int ble_hs_mbuf_to_flat(struct os_mbuf *b,void *p,size_t cap,uint16_t *n){if(b->len>cap)return 9;memcpy(p,b->bytes,b->len);*n=b->len;return 0;}
'''
tests=r'''
static struct os_mbuf buf;
static struct ble_gatt_access_ctxt ctx={BLE_GATT_ACCESS_OP_READ_CHR,&buf};
static void reboot(void){memset(&owner_record,0,sizeof(owner_record));owner_storage_fault=false;owner_peer_valid=false;owner_valid=false;authorized=false;owner_valid=owner_load();}
static int enroll(void){ctx.op=BLE_GATT_ACCESS_OP_READ_CHR;buf.len=0;return owner_enroll_read(1,0,&ctx,0);}
int main(void){
 link.sec_state.encrypted=true;link.sec_state.bonded=true;link.peer_id_addr.val[0]=1;
 reboot();assert(!owner_valid);
 /* A legacy Wi-Fi token session cannot create a predictable zero owner key. */
 authorized=true;ctx.op=BLE_GATT_ACCESS_OP_WRITE_CHR;
 buf=(struct os_mbuf){{3,1,0x40,0xe2,1,0},6};
 assert(owner_enroll_read(1,0,&ctx,0)!=0&&!owner_valid);authorized=false;
 assert(enroll()==0&&buf.len==16&&owner_valid);
 uint8_t first[16];memcpy(first,buf.bytes,16);
 reboot();assert(owner_valid&&enroll()==0&&!memcmp(first,buf.bytes,16));
 link.peer_id_addr.val[0]=2;assert(enroll()!=0&&buf.len==0);
 link.peer_id_addr.val[0]=1;link.sec_state.encrypted=false;assert(enroll()!=0);link.sec_state.encrypted=true;
 /* Skip changed no PIN state. Owner can set a six-digit PIN later. */
 ctx.op=BLE_GATT_ACCESS_OP_WRITE_CHR;buf=(struct os_mbuf){{3,1,0x40,0xe2,1,0},6};
 assert(owner_enroll_read(1,0,&ctx,0)!=0);authorized=true;
 assert(owner_enroll_read(1,0,&ctx,0)==0&&owner_record.pin==123456);
 reboot();owner_security_config();assert(ble_hs_cfg.sm_mitm&&owner_record.pin_enabled);
 link.peer_id_addr.val[0]=2;assert(enroll()!=0);link.sec_state.authenticated=true;
 assert(enroll()==0&&!memcmp(first,buf.bytes,16));
 /* Corrupt persisted ownership is not a fresh controller. */
 durable[0]^=1;reboot();assert(owner_storage_fault&&!owner_valid&&enroll()!=0);durable[0]^=1;
 /* Every storage failure before key disclosure rejects admission. */
 for(int f=1;f<=4;f++){
  durable_len=0;fault=0;reboot();fault=f;assert(enroll()!=0&&buf.len==0);fault=0;
 }
 /* A legacy saved key is retained and cannot be claimed by a new phone. */
 durable_len=0;legacy_exists=true;memset(legacy,0x24,16);reboot();
 assert(owner_valid&&!owner_peer_valid&&!memcmp(owner_key,legacy,16)&&enroll()!=0);
 puts("PASS production owner callbacks: first claim, persisted reconnect, stranger denial, PIN-after-Skip, authenticated second phone, corrupt NVS, four storage failures, legacy owner preservation");
}
'''
with tempfile.TemporaryDirectory(prefix='ot-owner-') as d:
    c=Path(d)/'owner.c'; exe=Path(d)/'owner';c.write_text(stubs+production+tests)
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Wno-unused-variable','-ffunction-sections','-fdata-sections','-I',str(root/'main'),str(c),str(root/'main/config_journal.c'),'-Wl,--gc-sections','-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
