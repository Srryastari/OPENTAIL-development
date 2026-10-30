#include "palette_transfer.h"
#include <string.h>
void ot_palette_transfer_reset(ot_palette_transfer_t *u){if(u)memset(u,0,sizeof(*u));}
static bool within(const ot_palette_transfer_t *u,uint32_t now){
    return u&&u->active&&
        (uint32_t)(now-u->started_ms)<OT_PALETTE_TRANSFER_TIMEOUT_MS;
}
bool ot_palette_transfer_begin(ot_palette_transfer_t *u,uint8_t index,
    uint8_t count,uint32_t expected_revision,uint16_t expected_crc,uint32_t now){
    if(!u||index>=OT_PALETTE_SLOTS||count<2||count>OT_PALETTE_STOPS_MAX||
       expected_revision==UINT32_MAX)return false;
    /* A new BEGIN always invalidates any partial transaction. */
    ot_palette_transfer_reset(u);
    u->index=index;u->count=count;u->expected_crc=expected_crc;
    u->expected_revision=expected_revision;u->started_ms=now;u->active=true;
    return true;
}
bool ot_palette_transfer_chunk(ot_palette_transfer_t *u,uint16_t offset,
    const uint8_t *data,size_t n,uint32_t now){
    if(!within(u,now)||!data||n==0||n>OT_PALETTE_CHUNK_MAX||
       offset!=u->received||u->received+n>(size_t)u->count*4u){
        ot_palette_transfer_reset(u);return false;
    }
    memcpy(u->bytes+u->received,data,n);u->received+=n;return true;
}
bool ot_palette_transfer_finish(ot_palette_transfer_t *u,uint8_t index,
    uint32_t now,ot_palette_record_t *out){
    if(!within(u,now)||!out||index!=u->index||
       u->received!=(size_t)u->count*4u||
       ot_palette_crc16(u->bytes,u->received)!=u->expected_crc){
        ot_palette_transfer_reset(u);return false;
    }
    bool ok=ot_palette_record_build(out,u->index,u->count,
                                   u->expected_revision+1u,u->bytes);
    ot_palette_transfer_reset(u);
    return ok;
}
