#include <assert.h>
#include <string.h>
#include "palette_profile.h"
#include "palette_transfer.h"
#include "palette_ble_v3.h"
#include "palette_runtime.h"
/* Host fake for the NVS commit boundary. Inject failure before exposing the
 * new profile, then simulate reboot by restoring the durable slot. */
static ot_palette_entry_t live[OT_PALETTE_SLOTS],saved[OT_PALETTE_SLOTS];
static ot_palette_record_t raw_saved[OT_PALETTE_SLOTS];
static bool fail_commit=false,used=false;
void ot_palette_runtime_load(void){memcpy(live,saved,sizeof(saved));}
bool ot_palette_runtime_get(uint8_t i,ot_palette_entry_t *out){
    if(i>=OT_PALETTE_SLOTS||!out)return false;
    *out=live[i];return true;
}
bool ot_palette_runtime_read_record(uint8_t i,ot_palette_record_t *r){
    ot_wled_palette16_t decoded;
    if(i>=OT_PALETTE_SLOTS||!r||!live[i].present||
       live[i].revision!=raw_saved[i].revision||
       !ot_palette_record_valid(&raw_saved[i],&decoded))return false;
    *r=raw_saved[i];return true;
}
bool ot_palette_runtime_put(const ot_palette_record_t *r,uint32_t old){
    ot_palette_entry_t e={0};
    if(!r||!ot_palette_entry_from_record(&e,r)||r->revision!=old+1u||
       live[r->index].revision!=old||fail_commit)return false;
    saved[r->index]=e;live[r->index]=e;raw_saved[r->index]=*r;return true;
}
bool ot_palette_runtime_delete(uint8_t i,uint32_t old){
    ot_palette_record_t r={0};
    if(i>=OT_PALETTE_SLOTS||!live[i].present||live[i].revision!=old||
       !ot_palette_record_tombstone(&r,i,old+1u)||fail_commit)return false;
    saved[i]=(ot_palette_entry_t){.revision=old+1u};
    live[i]=saved[i];raw_saved[i]=r;return true;
}
void ot_palette_runtime_snapshot(const uint8_t *idx,size_t n,ot_palette_entry_t *out){
    for(size_t i=0;i<n;i++)out[i]=idx[i]<OT_PALETTE_SLOTS?
        live[idx[i]]:(ot_palette_entry_t){0};
}
bool ot_ble_palette_in_use(uint8_t i){(void)i;return used;}
static uint8_t reply[OT_PALETTE_REPLY_BYTES];
static uint8_t result(const uint8_t *p,size_t n,uint32_t now){
    ot_palette_ble_process(p,n,now);ot_palette_ble_reply(reply);return reply[3];
}
static void begin(uint8_t index,uint8_t stops,uint32_t revision,uint16_t crc,uint32_t ms){
    uint8_t p[10]={3,1,index,stops};
    for(unsigned k=0;k<4;k++)p[4+k]=(uint8_t)(revision>>(k*8u));
    p[8]=(uint8_t)crc;p[9]=(uint8_t)(crc>>8);
    assert(result(p,sizeof(p),ms)==0);
}
static void chunk(uint16_t offset,const uint8_t *b,size_t n,uint32_t now){
    uint8_t p[20]={3,2,(uint8_t)offset,(uint8_t)(offset>>8)};
    assert(n<=15);memcpy(p+4,b,n);
    assert(result(p,n+4u,now)==0);
}
int main(void){
    const uint8_t stops[]={0,255,0,0,255,0,0,255};
    const uint16_t crc=ot_palette_crc16(stops,sizeof(stops));
    ot_palette_record_t r={0};ot_wled_palette16_t pal;
    assert(ot_palette_record_build(&r,0,2,1,stops));
    assert(ot_palette_record_valid(&r,&pal));
    ot_palette_record_t bad=r;bad.crc^=1u;
    assert(!ot_palette_record_valid(&bad,&pal));
    bad=r;bad.stops[1]^=1u;assert(!ot_palette_record_valid(&bad,&pal));
    bad=r;bad.stops[71]=1u;assert(!ot_palette_record_valid(&bad,&pal));
    bad=r;bad.index=128;assert(!ot_palette_record_valid(&bad,&pal));
    bad=r;bad.byte_length=7;assert(!ot_palette_record_valid(&bad,&pal));
    uint8_t desc[]={255,255,0,0,0,0,0,255};
    assert(!ot_palette_record_build(&bad,0,2,1,desc));
    uint8_t dup[]={0,255,0,0,0,0,0,255};
    assert(!ot_palette_record_build(&bad,0,2,1,dup));
    assert(!ot_palette_record_build(&bad,0,1,1,stops));
    assert(!ot_palette_record_build(&bad,128,2,1,stops));
    uint8_t idx=255;
    assert(ot_palette_id_to_index(200,&idx)&&idx==0);
    assert(ot_palette_id_to_index(73,&idx)&&idx==127);
    assert(!ot_palette_id_to_index(72,&idx)&&!ot_palette_id_to_index(201,&idx));
    assert(ot_palette_record_tombstone(&bad,0,2));
    assert(ot_palette_record_is_tombstone(&bad));
    assert(!ot_palette_record_valid(&bad,&pal));
    ot_palette_ble_session_reset();
    begin(0,2,0,crc,100);
    chunk(0,stops,5,101);chunk(5,stops+5,3,102);
    const uint8_t commit[]={3,3,0};
    assert(result(commit,sizeof(commit),103)==0);
    assert(live[0].present&&live[0].revision==1&&reply[11]==1);
    const uint8_t first_read[]={3,7,0,0};
    assert(result(first_read,sizeof(first_read),105)==0);
    assert(reply[4]==0&&reply[5]==6&&memcmp(reply+6,stops,6)==0);
    const uint8_t second_read[]={3,7,0,6};
    assert(result(second_read,sizeof(second_read),106)==0);
    assert(reply[4]==6&&reply[5]==2&&memcmp(reply+6,stops+6,2)==0);
    uint8_t stale[]={3,1,0,2,0,0,0,0,(uint8_t)crc,(uint8_t)(crc>>8)};
    assert(result(stale,sizeof(stale),110)==3);
    begin(0,2,1,crc,120);
    const uint8_t wrong[]={3,2,4,0,255};
    assert(result(wrong,sizeof(wrong),121)==1);
    assert(result(commit,sizeof(commit),122)==1);
    assert(live[0].revision==1);
    begin(0,2,1,crc,130);
    chunk(0,stops,8,131);
    assert(result(commit,sizeof(commit),130+OT_PALETTE_TRANSFER_TIMEOUT_MS)==1);
    assert(live[0].revision==1);
    begin(0,2,1,0,200);chunk(0,stops,8,201);
    assert(result(commit,sizeof(commit),202)==1);assert(live[0].revision==1);
    begin(0,2,1,crc,300);chunk(0,stops,8,301);
    fail_commit=true;assert(result(commit,sizeof(commit),302)==4);
    assert(live[0].revision==1&&saved[0].revision==1);
    fail_commit=false;
    begin(0,2,1,crc,400);chunk(0,stops,8,401);
    assert(result(commit,sizeof(commit),402)==0);
    assert(live[0].revision==2);
    const uint8_t deletion[]={3,4,0,2,0,0,0};
    used=true;assert(result(deletion,sizeof(deletion),450)==2);
    used=false;assert(result(deletion,sizeof(deletion),451)==0);
    assert(!live[0].present&&live[0].revision==3);
    assert(result(first_read,sizeof(first_read),452)==1);
    assert(result(deletion,sizeof(deletion),452)==3);
    ot_palette_runtime_load();
    assert(!live[0].present&&live[0].revision==3);
    const uint8_t query[]={3,5,0};
    assert(result(query,sizeof(query),453)==0&&reply[11]==0&&reply[4]==3);
    ot_palette_ble_session_reset();
    begin(0,2,3,crc,500);chunk(0,stops,5,501);
    ot_palette_ble_session_reset();assert(result(commit,sizeof(commit),502)==1);
    uint8_t snapshots[]={0,127,255};ot_palette_entry_t copies[3];
    ot_palette_runtime_snapshot(snapshots,3,copies);
    assert(!copies[0].present&&!copies[1].present&&!copies[2].present);
    return 0;
}
