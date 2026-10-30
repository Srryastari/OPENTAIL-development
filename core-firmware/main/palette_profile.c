#include "palette_profile.h"
#include <string.h>
uint16_t ot_pal_crc16(const uint8_t *p,size_t n){
    if(!p&&n)return 0;
    uint16_t c=0xffffu;
    for(size_t i=0;i<n;i++){
        c^=(uint16_t)p[i]<<8;
        for(unsigned b=0;b<8u;b++)
            c=(c&0x8000u)?(uint16_t)((c<<1)^0x1021u):(uint16_t)(c<<1);
    }
    return c;
}
bool ot_pal_validate(const ot_pal_profile_t *p){
    if(!p||p->index>=OT_PAL_CUSTOM_SLOTS||p->revision==0u||
       p->stop_count<2u||p->stop_count>OT_PAL_STOP_MAX||
       p->stops[0]!=0u||p->stops[4u*(p->stop_count-1u)]!=255u)return false;
    for(unsigned i=1u;i<p->stop_count;i++)
        if(p->stops[i*4u]<=p->stops[(i-1u)*4u])return false;
    /* No unvalidated padding in persisted CRC or memory. */
    for(unsigned i=4u*p->stop_count;i<OT_PAL_STOP_BYTES;i++)
        if(p->stops[i])return false;
    return true;
}
bool ot_pal_encode(const ot_pal_profile_t *p,uint8_t out[OT_PAL_RECORD_BYTES]){
    if(!out||!ot_pal_validate(p))return false;
    uint8_t next[OT_PAL_RECORD_BYTES]={0};
    next[0]=0x4cu;next[1]=0x50u;next[2]=0x54u;next[3]=0x4fu;
    next[4]=OT_PAL_VERSION;next[5]=p->index;
    next[6]=(uint8_t)p->revision;next[7]=(uint8_t)(p->revision>>8);
    next[8]=p->stop_count;
    memcpy(next+9u,p->stops,OT_PAL_STOP_BYTES);
    const uint16_t crc=ot_pal_crc16(next,OT_PAL_RECORD_BYTES-2u);
    next[81]=(uint8_t)crc;next[82]=(uint8_t)(crc>>8);
    memcpy(out,next,sizeof(next));return true;
}
bool ot_pal_decode(const uint8_t *bytes,size_t n,ot_pal_profile_t *out){
    if(!bytes||!out||n!=OT_PAL_RECORD_BYTES||
       bytes[0]!=0x4cu||bytes[1]!=0x50u||bytes[2]!=0x54u||
       bytes[3]!=0x4fu||bytes[4]!=OT_PAL_VERSION)return false;
    const uint16_t crc=(uint16_t)bytes[81]|((uint16_t)bytes[82]<<8);
    if(ot_pal_crc16(bytes,OT_PAL_RECORD_BYTES-2u)!=crc)return false;
    ot_pal_profile_t next={0};
    next.index=bytes[5];next.revision=(uint16_t)bytes[6]|((uint16_t)bytes[7]<<8);
    next.stop_count=bytes[8];memcpy(next.stops,bytes+9u,OT_PAL_STOP_BYTES);
    if(!ot_pal_validate(&next))return false;
    *out=next;return true;
}
bool ot_pal_bank_get(const ot_pal_bank_t *bank,uint8_t id,
                     ot_wled_palette16_t *out,uint16_t *revision){
    if(!bank||id<73u||id>200u)return false;
    const unsigned idx=200u-id;
    if(!bank->revision[idx])return false;
    if(out)*out=bank->colors[idx];
    if(revision)*revision=bank->revision[idx];
    return true;
}
bool ot_pal_bank_put(ot_pal_bank_t *bank,const ot_pal_profile_t *record){
    if(!bank||!ot_pal_validate(record))return false;
    ot_wled_palette16_t colors;
    if(!ot_wled_palette_from_gradient_stops(record->stops,
                                             record->stop_count,&colors))return false;
    bank->colors[record->index]=colors;
    bank->revision[record->index]=record->revision;return true;
}
bool ot_pal_bank_remove(ot_pal_bank_t *bank,uint8_t index){
    if(!bank||index>=OT_PAL_CUSTOM_SLOTS)return false;
    bank->revision[index]=0u;
    memset(&bank->colors[index],0,sizeof(bank->colors[index]));
    return true;
}
void ot_pal_transfer_reset(ot_pal_transfer_t *t){if(t)memset(t,0,sizeof(*t));}
bool ot_pal_transfer_begin(ot_pal_transfer_t *t,uint8_t index,
                          uint16_t expected_revision,uint8_t count,
                          uint16_t crc,uint32_t now_ms){
    if(!t||index>=OT_PAL_CUSTOM_SLOTS||
       count<2u||count>OT_PAL_STOP_MAX)return false;
    ot_pal_transfer_reset(t);
    t->active=true;t->profile.index=index;
    t->profile.stop_count=count;
    t->expected_revision=expected_revision;
    t->expected_crc=crc;t->last_ms=now_ms;return true;
}
bool ot_pal_transfer_append(ot_pal_transfer_t *t,uint8_t index,
                             uint8_t offset,const uint8_t *data,size_t length,
                             uint32_t now_ms){
    if(!t||!t->active||t->profile.index!=index||
       (uint32_t)(now_ms-t->last_ms)>OT_PAL_UPLOAD_TIMEOUT_MS||
       !data||!length||length>16u||offset!=t->received||
       (size_t)offset+length>4u*t->profile.stop_count){
        ot_pal_transfer_reset(t);return false;
    }
    memcpy(t->profile.stops+offset,data,length);
    t->received=(uint8_t)(t->received+length);t->last_ms=now_ms;
    return true;
}
bool ot_pal_transfer_complete(const ot_pal_transfer_t *t,
                              uint32_t now_ms,ot_pal_profile_t *out){
    if(!t||!out||!t->active||
       (uint32_t)(now_ms-t->last_ms)>OT_PAL_UPLOAD_TIMEOUT_MS||
       t->received!=4u*t->profile.stop_count||
       ot_pal_crc16(t->profile.stops,t->received)!=t->expected_crc)return false;
    ot_pal_profile_t candidate=t->profile;
    candidate.revision=1u;
    if(!ot_pal_validate(&candidate))return false;
    *out=candidate;return true;
}
