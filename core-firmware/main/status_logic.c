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
    return (ot_st_rgb_t){(uint8_t)((uint16_t)r*pct/100u),
                         (uint8_t)((uint16_t)g*pct/100u),
                         (uint8_t)((uint16_t)b*pct/100u)};
}
static bool double_pulse(uint32_t t){
    const uint32_t p=t%2200u;
    return p<180u || (p>=320u && p<500u);
}
static uint8_t triangle(uint32_t t,uint32_t period){
    uint32_t p=t%period,half=period/2u;
    return (uint8_t)(p<half?(p*100u/half):((period-p)*100u/(period-half)));
}
ot_st_rgb_t ot_st_render(ot_st_mode_t mode,uint32_t t,uint8_t brightness,bool enabled){
    if(brightness>100u)brightness=100u;
    /* Fault is always visible. Ordinary indications respect the enabled setting. */
    if(!enabled && mode!=OT_ST_FAULT)return (ot_st_rgb_t){0,0,0};
    switch(mode){
        case OT_ST_STARTUP:
            /* Approved startup white flash; the startup state ends at 780 ms. */
            return (t<160u)?rgb(255,255,255,brightness):(ot_st_rgb_t){0,0,0};
        case OT_ST_WIFI: {
            /* Wi-Fi ready: one cyan breathing pulse per second. */
            uint8_t level=triangle(t,1000u);
            return rgb(0,255,255,(uint8_t)((uint16_t)brightness*level/100u));
        }
        case OT_ST_CONNECTED:
            /* Connected: two green pulses every ~2.2 seconds. */
            return double_pulse(t)?rgb(0,255,0,brightness):(ot_st_rgb_t){0,0,0};
        case OT_ST_DISCONNECTED:
            /* Disconnected: two red pulses every ~2.2 seconds. */
            return double_pulse(t)?rgb(255,0,0,brightness):(ot_st_rgb_t){0,0,0};
        case OT_ST_BLE:
            /* BLE-only connection remains visually distinct from Wi-Fi. */
            return double_pulse(t)?rgb(150,0,255,brightness):(ot_st_rgb_t){0,0,0};
        case OT_ST_BOTH:
            /* Wi-Fi and BLE connected: cyan double pulse. */
            return double_pulse(t)?rgb(0,255,255,brightness):(ot_st_rgb_t){0,0,0};
        case OT_ST_SHOW: {
            /* Smooth rainbow loops every 1.4 s as approved for Show Mode. */
            static const uint8_t wheel[7][3]={{255,0,0},{255,255,0},{0,255,0},
                {0,255,255},{0,0,255},{255,0,255},{255,0,0}};
            const uint32_t cycle=1400u;
            uint32_t phase=t%cycle,scaled=phase*6u,idx=scaled/cycle,frac=scaled%cycle;
            uint8_t c[3];
            for(unsigned i=0;i<3;i++)
                c[i]=(uint8_t)(((uint32_t)wheel[idx][i]*(cycle-frac)+
                                (uint32_t)wheel[idx+1u][i]*frac)/cycle);
            return rgb(c[0],c[1],c[2],brightness);
        }
        case OT_ST_FAULT:
            /* Continuous red fault blink: 90 ms on / 90 ms off. */
            return (t%180u)<90u?rgb(255,0,0,brightness):(ot_st_rgb_t){0,0,0};
        default:return (ot_st_rgb_t){0,0,0};
    }
}
bool ot_st_test_window_active(ot_st_mode_t mode,uint32_t now_ms,uint32_t deadline_ms){
    uint32_t remain=deadline_ms-now_ms;
    return (mode==OT_ST_SHOW || mode==OT_ST_FAULT) &&
           remain!=0u && remain<0x80000000u;
}
