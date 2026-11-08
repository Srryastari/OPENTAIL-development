#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "panel_zone_diag.h"

static void probe_and_lease(void){
    ot_pz_diag_state_t s={0};const uint16_t lengths[2]={2048u,623u};
    const uint8_t packet[5]={0xaau,0x01u,0x07u,0xffu,0x01u};
    assert(!ot_pz_diag_probe(&s,packet,sizeof(packet),false,true,lengths,100u));
    assert(!ot_pz_diag_probe(&s,packet,sizeof(packet),true,false,lengths,100u));
    assert(!ot_pz_diag_probe(&s,packet,4u,true,true,lengths,100u));
    assert(!ot_pz_diag_probe(&s,packet,6u,true,true,lengths,100u));
    uint8_t bad[5];memcpy(bad,packet,sizeof(bad));bad[0]=0xabu;
    assert(!ot_pz_diag_probe(&s,bad,sizeof(bad),true,true,lengths,100u));
    memcpy(bad,packet,sizeof(bad));bad[1]=3u;
    assert(!ot_pz_diag_probe(&s,bad,sizeof(bad),true,true,lengths,100u));
    memcpy(bad,packet,sizeof(bad));bad[4]=3u;
    assert(!ot_pz_diag_probe(&s,bad,sizeof(bad),true,true,lengths,100u));
    memcpy(bad,packet,sizeof(bad));bad[2]=0x02u;bad[3]=0x70u; /* 624, outside lane 2 */
    bad[1]=2u;
    assert(!ot_pz_diag_probe(&s,bad,sizeof(bad),true,true,lengths,100u));
    assert(ot_pz_diag_probe(&s,packet,sizeof(packet),true,true,lengths,100u));
    assert(s.mode==OT_PZ_DIAG_PROBE&&s.lane==0u&&s.endpoint==2047u);
    assert(s.expires_ms==2100u);
    bool owned=false;ot_rgb_t color={0};
    assert(ot_pz_diag_render(&s,0,2047u,2099u,&owned,&color));
    assert(owned&&color.r==255u&&color.g==255u&&color.b==255u);
    assert(ot_pz_diag_render(&s,0,100u,2099u,&owned,&color)&&!owned);
    assert(!ot_pz_diag_render(&s,0,2047u,2100u,&owned,&color));
    assert(ot_pz_diag_expire(&s,2100u)&&s.mode==OT_PZ_DIAG_NONE);

    /* 100 consecutive Write-Without-Response packets renew the same 2s lease;
     * the parser is stateless per packet and never touches RMT. */
    for(unsigned i=0;i<100;i++){
        uint8_t burst[5]={0xaa,1,0,(uint8_t)i,2};
        assert(ot_pz_diag_probe(&s,burst,sizeof(burst),true,true,lengths,3000u+i));
    }
    assert(s.expires_ms==5099u);
    assert(!ot_pz_diag_expire(&s,5098u));
    assert(ot_pz_diag_expire(&s,5099u));
    /* Off is a valid probe color and owns the exact pixel as black. */
    uint8_t off[5]={0xaa,2,0,10,0};
    assert(ot_pz_diag_probe(&s,off,sizeof(off),true,true,lengths,6000u));
    assert(ot_pz_diag_render(&s,1,10u,6001u,&owned,&color));
    assert(owned&&color.r==0u&&color.g==0u&&color.b==0u);
    ot_pz_diag_clear(&s);assert(s.mode==OT_PZ_DIAG_NONE);
}
static void cumulative_fill(void){
    ot_pz_diag_state_t s={0};bool owned=false;ot_rgb_t color={0};
    assert(!ot_pz_diag_fill(&s,0,74u,0u,100u,10u));
    assert(!ot_pz_diag_fill(&s,2,74u,1u,100u,10u));
    assert(!ot_pz_diag_fill(&s,0,100u,1u,100u,10u));
    assert(ot_pz_diag_fill(&s,0,74u,2u,100u,1000u));
    assert(ot_pz_diag_fill_count(&s)==75u);
    assert(ot_pz_diag_render(&s,0,0u,1001u,&owned,&color));
    assert(owned&&color.r==0u&&color.g==255u&&color.b==255u);
    assert(ot_pz_diag_render(&s,0,74u,1001u,&owned,&color)&&owned);
    assert(ot_pz_diag_render(&s,0,75u,1001u,&owned,&color));
    assert(owned&&color.r==0u&&color.g==0u&&color.b==0u);
    assert(!ot_pz_diag_render(&s,1,0u,1001u,&owned,&color));
    assert(!ot_pz_diag_expire(&s,2999u));
    assert(ot_pz_diag_expire(&s,3000u));
    assert(ot_pz_diag_fill_count(&s)==0u);
    /* Expiry comparison is safe across uint32 millisecond wrap. */
    assert(ot_pz_diag_fill(&s,1,0u,1u,10u,UINT32_MAX-1000u));
    assert(!ot_pz_diag_expire(&s,UINT32_MAX-1u));
    assert(ot_pz_diag_expire(&s,999u));
}
int main(void){
    probe_and_lease();cumulative_fill();
    puts("PASS Issue126 Pixel Probe: auth/capability, exact five bytes, 256-bit index, WWR burst renewal, disconnect clear primitive, 2s lease and inclusive fill");
    return 0;
}
