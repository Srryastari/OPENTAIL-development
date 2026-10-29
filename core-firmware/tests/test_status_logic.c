#include <assert.h>
#include <stdio.h>
#include "status_logic.h"
static int lit(ot_st_rgb_t c){return c.r||c.g||c.b;}
#define CHECK(x) do{assert(x);++checks;}while(0)
int main(void){
    int checks=0;
    ot_st_rgb_t c;
    /* Approved status patterns: startup white flash, Wi-Fi cyan pulse,
       connected green double pulse, disconnected red double pulse,
       Show Mode rainbow at 1.4 s, and fault red 90/90 ms. */
    c=ot_st_render(OT_ST_STARTUP,0,100,true); CHECK(c.r==255&&c.g==255&&c.b==255);
    CHECK(!lit(ot_st_render(OT_ST_STARTUP,170,100,true)));
    c=ot_st_render(OT_ST_WIFI,250,100,true); CHECK(c.r==0&&c.g==255&&c.b==255);
    CHECK(!lit(ot_st_render(OT_ST_WIFI,0,100,true)));
    CHECK(!lit(ot_st_render(OT_ST_WIFI,999,100,true)));
    c=ot_st_render(OT_ST_CONNECTED,40,100,true); CHECK(c.r==0&&c.g==255&&c.b==0);
    CHECK(!lit(ot_st_render(OT_ST_CONNECTED,250,100,true)));
    c=ot_st_render(OT_ST_CONNECTED,400,100,true); CHECK(c.r==0&&c.g==255&&c.b==0);
    CHECK(!lit(ot_st_render(OT_ST_CONNECTED,600,100,true)));
    c=ot_st_render(OT_ST_DISCONNECTED,40,100,true); CHECK(c.r==255&&c.g==0&&c.b==0);
    CHECK(!lit(ot_st_render(OT_ST_DISCONNECTED,250,100,true)));
    c=ot_st_render(OT_ST_DISCONNECTED,400,100,true); CHECK(c.r==255&&c.g==0&&c.b==0);
    c=ot_st_render(OT_ST_BLE,40,100,true); CHECK(c.g==255&&c.r==0&&c.b==0);
    c=ot_st_render(OT_ST_BOTH,40,100,true); CHECK(c.g==255&&c.r==0&&c.b==0);
    CHECK(ot_st_connection_mode(false,true)==OT_ST_CONNECTED);
    CHECK(ot_st_connection_mode(true,false)==OT_ST_CONNECTED);
    CHECK(ot_st_connection_mode(true,true)==OT_ST_CONNECTED);
    CHECK(ot_st_connection_mode(false,false)==OT_ST_DISCONNECTED);
    c=ot_st_render(OT_ST_SHOW,0,100,true); CHECK(c.r==255&&c.g==0&&c.b==0);
    c=ot_st_render(OT_ST_SHOW,467,100,true); CHECK(c.g==255&&c.r==0&&c.b==0);
    CHECK(ot_st_render(OT_ST_SHOW,0,100,true).r==ot_st_render(OT_ST_SHOW,1400,100,true).r);
    c=ot_st_render(OT_ST_FAULT,0,100,false); CHECK(c.r==255&&c.g==0&&c.b==0);
    CHECK(lit(ot_st_render(OT_ST_FAULT,89,100,true)));
    CHECK(!lit(ot_st_render(OT_ST_FAULT,90,100,true)));
    CHECK(lit(ot_st_render(OT_ST_FAULT,180,100,true)));
    CHECK(!lit(ot_st_render(OT_ST_CONNECTED,0,100,false)));
    CHECK(!lit(ot_st_render(OT_ST_SHOW,0,100,false)));
    CHECK(ot_st_test_window_active(OT_ST_SHOW,1000u,6000u));
    CHECK(ot_st_test_window_active(OT_ST_FAULT,5999u,6000u));
    CHECK(!ot_st_test_window_active(OT_ST_SHOW,6000u,6000u));
    CHECK(!ot_st_test_window_active(OT_ST_CONNECTED,1000u,6000u));
    CHECK(ot_st_test_window_active(OT_ST_SHOW,0xFFFFFF00u,0x00001288u));
    CHECK(!ot_st_test_window_active(OT_ST_SHOW,0x00001288u,0x00001288u));
    printf("Passed %d status LED pattern assertions.\n",checks);
    return 0;
}
