#include "arbitration_logic.h"
#include <stddef.h>

ot_arbitration_t ot_arbitrate(uint8_t effective_mask,bool demo_requested,bool show_requested){
    uint8_t signals=effective_mask; /* AUX assignments now participate in vehicle priority. */
    ot_arb_mode_t mode=signals?OT_ARB_VEHICLE:demo_requested?OT_ARB_DEMO:show_requested?OT_ARB_SHOW:OT_ARB_IDLE;
    return (ot_arbitration_t){
        .mode=mode,.signals=signals,
        .hazards=(signals&(OT_SIG_LEFT|OT_SIG_RIGHT))==(OT_SIG_LEFT|OT_SIG_RIGHT),
        .brake_over_parking=(signals&(OT_SIG_BRAKE|OT_SIG_PARK))==(OT_SIG_BRAKE|OT_SIG_PARK),
        .show_requested=show_requested,.show_suppressed=show_requested&&mode!=OT_ARB_SHOW,
        .demo_requested=demo_requested,.demo_suppressed=demo_requested&&mode==OT_ARB_VEHICLE
    };
}

ot_zone_mode_t ot_arb_zone(const ot_arbitration_t *state,int chain,int section,
                           uint8_t show_chain_mask,uint16_t show_section_mask){
    if(!state||chain<0||chain>=OT_CHAIN_COUNT||section<0||section>=OT_SECTION_COUNT)return OT_ZONE_OFF;
    if(state->mode==OT_ARB_SHOW)
        return (show_chain_mask&(1u<<chain))&&(show_section_mask&(1u<<section))?OT_ZONE_SHOW:OT_ZONE_OFF;
    if(state->mode==OT_ARB_DEMO)return OT_ZONE_DEMO;
    if(state->mode!=OT_ARB_VEHICLE)return OT_ZONE_OFF;
    uint8_t m=state->signals;
    if(section==OT_TOP||section==OT_BOTTOM||section==OT_TRUNK){
        if(m&OT_SIG_BRAKE)return OT_ZONE_BRAKE;
        if(m&OT_SIG_PARK)return OT_ZONE_PARK;
    }
    if(section==OT_TURN){
        if(chain==0&&(m&OT_SIG_LEFT))return OT_ZONE_LEFT_TURN;
        if(chain==1&&(m&OT_SIG_RIGHT))return OT_ZONE_RIGHT_TURN;
    }
    if(section==OT_REVERSE&&(m&OT_SIG_REVERSE))return OT_ZONE_REVERSE;
    return OT_ZONE_OFF;
}
const char *ot_arb_mode_name(ot_arb_mode_t mode){
    switch(mode){case OT_ARB_VEHICLE:return "vehicle";case OT_ARB_DEMO:return "demo";
        case OT_ARB_SHOW:return "native_show";default:return "idle";}
}
const char *ot_zone_mode_name(ot_zone_mode_t mode){
    switch(mode){case OT_ZONE_PARK:return "parking";case OT_ZONE_BRAKE:return "brake";
        case OT_ZONE_LEFT_TURN:return "left_turn";case OT_ZONE_RIGHT_TURN:return "right_turn";
        case OT_ZONE_REVERSE:return "reverse";case OT_ZONE_DEMO:return "demo";
        case OT_ZONE_SHOW:return "native_show";default:return "off";}
}
