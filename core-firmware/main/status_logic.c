#include "status_logic.h"

const char *ot_st_mode_name(ot_st_mode_t mode) {
    switch(mode) {
        case OT_ST_STARTUP:return "startup";
        case OT_ST_WIFI:return "wifi_ready";
        case OT_ST_CONNECTED:return "connected";
        case OT_ST_DISCONNECTED:return "disconnected";
        case OT_ST_BLE:return "bluetooth";
        case OT_ST_BOTH:return "wifi_bluetooth";
        case OT_ST_SHOW:return "show_mode";
        case OT_ST_FAULT:return "fault";
        default:return "off";
    }
}
static ot_st_rgb_t rgb(uint8_t r,uint8_t g,uint8_t b,uint8_t pct){
    return (ot_st_rgb_t){(uint8_t)(r*pct/100u),(uint8_t)(g*pct/100u),(uint8_t)(b*pct/100u)};
}
static uint8_t portion(uint32_t phase,uint32_t peak,uint32_t end){
    if(phase>=end)return 0;
    if(phase<=peak)return (uint8_t)(phase*100u/(peak?peak:1u));
    return (uint8_t)((end-phase)*100u/(end-peak));
}
ot_st_rgb_t ot_st_render(ot_st_mode_t mode,uint32_t t,uint8_t brightness,bool enabled){
    if(brightness>100)brightness=100;
    /* Fault remains visible even if users disabled ordinary status LEDs. */
    if(!enabled && mode!=OT_ST_FAULT)return (ot_st_rgb_t){0,0,0};
    switch(mode){
        case OT_ST_STARTUP:{
            /* Crisp three-pulse white strobe, finishes at 780 ms. */
            uint32_t q=t%1000u;
            bool on=(q<100u)||(q>=240u&&q<340u)||(q>=480u&&q<580u);
            return t<780u&&on?rgb(255,255,255,brightness):rgb(0,0,0,0);
        }
        case OT_ST_WIFI: case OT_ST_DISCONNECTED:{
            /* Saturated amber standby: never red, never blue ambiguity. */
            uint8_t wave=portion(t%2200u,250u,650u);
            return rgb(255,145,0,(uint8_t)((brightness*wave)/100u));
        }
        case OT_ST_CONNECTED:return rgb(0,255,0,brightness);
        case OT_ST_BLE:return rgb(140,0,255,brightness); /* Slightly bluer saturated purple */
        case OT_ST_BOTH:return rgb(0,255,145,brightness); /* More green/cyan: less sky blue */
        case OT_ST_SHOW:{
            /* Slow, smoothly interpolated six-color saturated HSV wheel. */
            static const uint8_t wheel[7][3]={{255,0,0},{255,255,0},{0,255,0},{0,255,255},{0,0,255},{255,0,255},{255,0,0}};
            const uint32_t cycle=3600u;
            uint32_t phase=t%cycle;
            uint32_t scaled=phase*6u;
            uint32_t idx=scaled/cycle,frac=scaled%cycle;
            uint8_t c[3];
            for(int i=0;i<3;i++)c[i]=(uint8_t)((wheel[idx][i]*(cycle-frac)+wheel[idx+1][i]*frac)/cycle);
            return rgb(c[0],c[1],c[2],brightness);
        }
        case OT_ST_FAULT:
            return (t%180u)<90u?rgb(255,0,0,brightness<30u?30u:brightness):rgb(0,0,0,0);
        default:return (ot_st_rgb_t){0,0,0};
    }
}

bool ot_st_test_window_active(ot_st_mode_t mode,uint32_t now_ms,uint32_t deadline_ms){
    uint32_t remain=deadline_ms-now_ms;
    return (mode==OT_ST_SHOW || mode==OT_ST_FAULT) && remain!=0u && remain<0x80000000u;
}
