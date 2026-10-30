#include "palette_profile.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static ot_pal_profile_t profile(unsigned index,unsigned rev){
    ot_pal_profile_t p={0};p.index=(uint8_t)index;
    p.revision=(uint16_t)rev;p.stop_count=4u;
    const uint8_t stops[]={0u,1u,2u,3u,64u,18u,37u,59u,
                           160u,173u,193u,213u,255u,251u,253u,255u};
    memcpy(p.stops,stops,sizeof(stops));return p;
}
static void test_codec(void){
    ot_pal_profile_t p=profile(0u,1u),d={0};
    uint8_t buf[OT_PAL_RECORD_BYTES]={0};
    assert(ot_pal_encode(&p,buf));
    assert(ot_pal_decode(buf,sizeof(buf),&d));
    assert(!memcmp(&p,&d,sizeof(p)));
    assert(!ot_pal_decode(buf,sizeof(buf)-1u,&d));
    for(unsigned i=0;i<OT_PAL_RECORD_BYTES;i++){
        uint8_t b[OT_PAL_RECORD_BYTES];memcpy(b,buf,sizeof(b));
        b[i]^=1u;assert(!ot_pal_decode(b,sizeof(b),&d));
    }
    p.revision=0u;assert(!ot_pal_encode(&p,buf));p.revision=2u;
    p.index=128u;assert(!ot_pal_encode(&p,buf));p.index=0u;
    p.stops[0]=1u;assert(!ot_pal_encode(&p,buf));p.stops[0]=0u;
    p.stops[4]=0u;assert(!ot_pal_encode(&p,buf));p.stops[4]=64u;
    p.stops[8]=64u;assert(!ot_pal_encode(&p,buf));p.stops[8]=160u;
    p.stops[12]=254u;assert(!ot_pal_encode(&p,buf));p.stops[12]=255u;
    p.stop_count=1u;assert(!ot_pal_encode(&p,buf));p.stop_count=19u;
    assert(!ot_pal_encode(&p,buf));p.stop_count=4u;
    p.stops[72u-1u]=1u;assert(!ot_pal_encode(&p,buf));
    p.stops[72u-1u]=0u;
    assert(ot_pal_encode(&p,buf));
    ot_pal_bank_t bank={0};ot_wled_palette16_t pixels={0};
    uint16_t rev=0u;
    assert(!ot_pal_bank_get(&bank,200u,&pixels,&rev));
    assert(ot_pal_bank_put(&bank,&p));
    assert(ot_pal_bank_get(&bank,200u,&pixels,&rev)&&rev==2u);
    assert(pixels.entries[0].r==1u&&pixels.entries[0].g==2u);
    assert(!ot_pal_bank_get(&bank,201u,&pixels,&rev));
    assert(!ot_pal_bank_get(&bank,72u,&pixels,&rev));
    assert(ot_pal_bank_remove(&bank,0u));
    assert(!ot_pal_bank_get(&bank,200u,&pixels,&rev));
    for(unsigned i=0;i<128u;i++){
        p=profile(i,1u);assert(ot_pal_bank_put(&bank,&p));
        assert(ot_pal_bank_get(&bank,(uint8_t)(200u-i),&pixels,&rev));
        assert(rev==1u);
    }
}
static void test_transfer(void){
    ot_pal_transfer_t tx={0};ot_pal_profile_t result={0},p=profile(127u,1u);
    const size_t n=(size_t)p.stop_count*4u;
    const uint16_t crc=ot_pal_crc16(p.stops,n);
    assert(!ot_pal_transfer_begin(&tx,128u,0u,4u,crc,0u));
    assert(!ot_pal_transfer_begin(&tx,127u,0u,19u,crc,0u));
    assert(ot_pal_transfer_begin(&tx,127u,0u,4u,crc,0u));
    assert(!ot_pal_transfer_complete(&tx,1u,&result));
    assert(ot_pal_transfer_append(&tx,127u,0u,p.stops,9u,20u));
    assert(!ot_pal_transfer_append(&tx,127u,0u,p.stops,1u,21u));
    assert(!tx.active);
    assert(ot_pal_transfer_begin(&tx,127u,0u,4u,crc,50u));
    assert(!ot_pal_transfer_append(&tx,127u,0u,p.stops,17u,55u));
    assert(!tx.active);
    assert(ot_pal_transfer_begin(&tx,127u,0u,4u,crc,100u));
    assert(ot_pal_transfer_append(&tx,127u,0u,p.stops,10u,101u));
    assert(!ot_pal_transfer_append(&tx,127u,10u,p.stops+10u,6u,15102u));
    assert(!tx.active);
    assert(ot_pal_transfer_begin(&tx,127u,0u,4u,crc,0xfffffffeu));
    assert(ot_pal_transfer_append(&tx,127u,0u,p.stops,10u,0xffffffffu));
    assert(ot_pal_transfer_append(&tx,127u,10u,p.stops+10u,6u,4u));
    assert(ot_pal_transfer_complete(&tx,5u,&result));
    assert(result.index==127u&&result.stop_count==4u&&result.revision==1u);
    ot_pal_transfer_reset(&tx);
    assert(ot_pal_transfer_begin(&tx,127u,0u,4u,(uint16_t)(crc^0x100u),0u));
    assert(ot_pal_transfer_append(&tx,127u,0u,p.stops,n,1u));
    assert(!ot_pal_transfer_complete(&tx,2u,&result));
    assert(ot_pal_transfer_begin(&tx,127u,65535u,4u,crc,0u));
    assert(ot_pal_transfer_append(&tx,127u,0u,p.stops,n,0u));
    assert(ot_pal_transfer_complete(&tx,1u,&result));
    assert(tx.expected_revision==65535u);
}
int main(void){test_codec();test_transfer();
 puts("Custom palette strict codec: 128 IDs, CRC corruption, malformed stops, transfer offset, chunk size, timeout/wrap, missing entry and reset PASS");return 0;}
