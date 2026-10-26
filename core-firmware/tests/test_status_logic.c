#include <assert.h>
#include <stdio.h>
#include "status_logic.h"
static int lit(ot_st_rgb_t c){return c.r||c.g||c.b;}
int main(void){
    int checks=0;
    #define CHECK(x) do{assert(x);++checks;}while(0)
    CHECK(lit(ot_st_render(OT_ST_STARTUP,0,65,true)));
    CHECK(lit(ot_st_render(OT_ST_STARTUP,299,65,true)));
    CHECK(lit(ot_st_render(OT_ST_STARTUP,500,65,true)));
    CHECK(!lit(ot_st_render(OT_ST_STARTUP,800,65,true)));
    CHECK(!lit(ot_st_render(OT_ST_WIFI,0,65,true)));
    CHECK(lit(ot_st_render(OT_ST_WIFI,250,65,true)));
    ot_st_rgb_t wifi_blue=ot_st_render(OT_ST_WIFI,250,100,true);
    CHECK(wifi_blue.r==255);
    CHECK(wifi_blue.r>wifi_blue.g && wifi_blue.g>wifi_blue.b);
    CHECK(wifi_blue.b==0);
    /* Pure primaries prevent pastel/pink/washed-out status hues. */
    ot_st_rgb_t green=ot_st_render(OT_ST_CONNECTED,0,100,true);
    ot_st_rgb_t red=ot_st_render(OT_ST_DISCONNECTED,0,100,true);
    ot_st_rgb_t fault=ot_st_render(OT_ST_FAULT,0,100,true);
    CHECK(green.r==0 && green.g==255 && green.b==0);
    CHECK(red.r==0 && red.g==0 && red.b==0); /* standby begins dark; amber pulses */
    CHECK(fault.r==255 && fault.g==0 && fault.b==0);
    ot_st_rgb_t rainbow_red=ot_st_render(OT_ST_SHOW,0,100,true);
    ot_st_rgb_t rainbow_cyan=ot_st_render(OT_ST_SHOW,1800,100,true);
    CHECK(rainbow_red.r==255 && rainbow_red.g==0 && rainbow_red.b==0);
    CHECK(rainbow_cyan.r==0 && rainbow_cyan.g==255 && rainbow_cyan.b==255);
    CHECK(!lit(ot_st_render(OT_ST_WIFI,800,65,true)));
    CHECK(lit(ot_st_render(OT_ST_WIFI,2450,65,true)));
    CHECK(!lit(ot_st_render(OT_ST_WIFI,3000,65,true)));
    CHECK(lit(ot_st_render(OT_ST_CONNECTED,0,65,true)));
    CHECK(lit(ot_st_render(OT_ST_CONNECTED,120,65,true)));
    CHECK(lit(ot_st_render(OT_ST_CONNECTED,240,65,true)));
    CHECK(lit(ot_st_render(OT_ST_CONNECTED,360,65,true)));
    CHECK(lit(ot_st_render(OT_ST_DISCONNECTED,240,65,true)));
    CHECK(ot_st_render(OT_ST_DISCONNECTED,240,65,true).r>0);
    CHECK(ot_st_render(OT_ST_DISCONNECTED,240,65,true).b==0);
    CHECK(lit(ot_st_render(OT_ST_FAULT,0,65,true)));
    CHECK(lit(ot_st_render(OT_ST_FAULT,89,65,true)));
    CHECK(!lit(ot_st_render(OT_ST_FAULT,90,65,true)));
    CHECK(!lit(ot_st_render(OT_ST_FAULT,179,65,true)));
    CHECK(lit(ot_st_render(OT_ST_FAULT,180,65,true)));
    CHECK(lit(ot_st_render(OT_ST_FAULT,360,65,true)));
    CHECK(!lit(ot_st_render(OT_ST_FAULT,90,65,false))); /* off half stays off */
    CHECK(lit(ot_st_render(OT_ST_FAULT,0,5,false))); /* fault visible despite mute */
    CHECK(!lit(ot_st_render(OT_ST_CONNECTED,0,65,false)));
    CHECK(!lit(ot_st_render(OT_ST_WIFI,250,65,false)));
    CHECK(lit(ot_st_render(OT_ST_SHOW,0,65,true)));
    CHECK(lit(ot_st_render(OT_ST_SHOW,1800,65,true)));
    ot_st_rgb_t ble = ot_st_render(OT_ST_BLE,0,100,true);
    CHECK(ble.r==140 && ble.g==0 && ble.b==255);
    ot_st_rgb_t both = ot_st_render(OT_ST_BOTH,0,100,true);
    CHECK(both.r==0 && both.g==255 && both.b==145);
    CHECK(ot_st_render(OT_ST_SHOW,0,65,true).r==ot_st_render(OT_ST_SHOW,3600,65,true).r);
    CHECK(ot_st_render(OT_ST_SHOW,0,65,true).g==ot_st_render(OT_ST_SHOW,3600,65,true).g);
    CHECK(ot_st_render(OT_ST_SHOW,0,65,true).b==ot_st_render(OT_ST_SHOW,3600,65,true).b);
    CHECK(lit(ot_st_render(OT_ST_CONNECTED,0,100,true)));
    CHECK(ot_st_render(OT_ST_CONNECTED,0,100,true).g>=ot_st_render(OT_ST_CONNECTED,0,50,true).g);
    /* Self-test duration expires at exactly five seconds, even across clock wrap. */
    CHECK(ot_st_test_window_active(OT_ST_SHOW,1000u,6000u));
    CHECK(ot_st_test_window_active(OT_ST_FAULT,5999u,6000u));
    CHECK(!ot_st_test_window_active(OT_ST_SHOW,6000u,6000u));
    CHECK(!ot_st_test_window_active(OT_ST_FAULT,6001u,6000u));
    CHECK(!ot_st_test_window_active(OT_ST_CONNECTED,1000u,6000u));
    CHECK(ot_st_test_window_active(OT_ST_SHOW,0xFFFFFF00u,0x00001288u));
    CHECK(!ot_st_test_window_active(OT_ST_SHOW,0x00001288u,0x00001288u));
    /* A five-second bench self-test reuses the exact firmware status renderers. */
    for(unsigned t=0;t<5000;t+=100) {
        ot_st_rgb_t c=ot_st_render(OT_ST_SHOW,t,65,true);
        CHECK(lit(c)); /* continuous hue, no prolonged blackout */
    }
    for(unsigned t=0;t<5000;t+=90) {
        ot_st_rgb_t c=ot_st_render(OT_ST_FAULT,t,65,true);
        CHECK(lit(c)==((t/90)%2==0));
    }
    CHECK(ot_st_render(OT_ST_SHOW,0,65,true).r==ot_st_render(OT_ST_SHOW,3600,65,true).r);
    CHECK(lit(ot_st_render(OT_ST_FAULT,5000,65,true))==lit(ot_st_render(OT_ST_FAULT,5000%180,65,true)));
    printf("Passed %d onboard RGB timing assertions.\n",checks);
    return 0;
}
