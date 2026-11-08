#include <assert.h>
#include <stdio.h>
#include "board_config.h"
#include "output_manager.h"

static uint8_t peak(ot_rgb_t c){
    uint8_t p=c.r>c.g?c.r:c.g;
    return p>c.b?p:c.b;
}
int main(void){
    const uint8_t requested[]={0u,1u,32u,64u,128u,200u,255u};
    const uint8_t expected[]={0u,1u,32u,32u,32u,32u,32u};
    for(unsigned i=0;i<sizeof(requested);i++)
        assert(ot_brightness_limit_request(requested[i])==expected[i]);
    const ot_rgb_t full[]={{255u,0u,0u},{0u,255u,0u},
                           {0u,0u,255u},{255u,255u,255u}};
    for(unsigned i=0;i<sizeof(full)/sizeof(full[0]);i++){
        const ot_rgb_t limited=ot_om_limit_brightness(full[i],OT_MAX_BRIGHTNESS);
        assert(peak(limited)==OT_MAX_BRIGHTNESS);
    }
    assert(peak(ot_om_limit_brightness((ot_rgb_t){1u,0u,0u},32u))==1u);
    assert(peak(ot_om_limit_brightness((ot_rgb_t){200u,100u,50u},32u))==32u);
    assert(peak(ot_om_limit_brightness((ot_rgb_t){200u,100u,50u},0u))==0u);
    puts("PASS brightness contract: 0/1/32/64/128/200/255 requests, RGB primary/white, ratio-preserving final ceiling");
    return 0;
}
