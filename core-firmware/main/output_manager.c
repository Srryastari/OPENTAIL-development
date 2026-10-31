#include "output_manager.h"
#include "board_config.h"
#include <string.h>
#include <stdio.h>

bool ot_om_saved_apply(ot_om_output_t active[OT_OM_OUTPUTS],
                        ot_om_pending_t *deferred,
                        const ot_om_output_t saved[OT_OM_OUTPUTS],
                        uint8_t instantaneous,uint8_t debounced){
    if(!active||!deferred||!saved)return false;
    for(unsigned i=0;i<OT_OM_OUTPUTS;i++)if(!ot_om_valid(&saved[i]))return false;
    if(instantaneous||debounced){
        memcpy(deferred->outputs,saved,sizeof(deferred->outputs));
        deferred->pending=true;
        return false;
    }
    memcpy(active,saved,sizeof(deferred->outputs));
    deferred->pending=false;
    return true;
}
bool ot_om_apply_if_released(ot_om_output_t active[OT_OM_OUTPUTS],
                             ot_om_pending_t *deferred,
                             uint8_t instantaneous,uint8_t debounced){
    if(!active||!deferred||!deferred->pending||
       instantaneous||debounced)return false;
    memcpy(active,deferred->outputs,sizeof(deferred->outputs));
    deferred->pending=false;
    return true;
}

static const char *const role_names[]={"off","park_brake","parking","brake","left_turn","right_turn","reverse","aux1","aux2","aux3","decorative"};
static const char *const order_names[]={"RGB","RBG","GRB","GBR","BRG","BGR"};
const char *ot_om_role_name(uint8_t v){return v<=OT_OM_DECORATIVE?role_names[v]:"invalid";}
const char *ot_om_order_name(uint8_t v){return v<=OT_OM_BGR?order_names[v]:"invalid";}
int ot_om_role_from_name(const char *str){if(!str)return -1;for(int i=0;i<=OT_OM_DECORATIVE;i++)if(!strcmp(str,role_names[i]))return i;return -1;}
int ot_om_order_from_name(const char *str){if(!str)return -1;for(int i=0;i<=OT_OM_BGR;i++)if(!strcmp(str,order_names[i]))return i;return -1;}
static bool name_ok(const char *s){
    if(!s)return false;
    for(unsigned i=0;i<OT_OM_NAME_LEN;i++){
        unsigned char c=(unsigned char)s[i];
        if(c==0)return i>0;
        if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c==' '||c=='-'||c=='_'||c=='.'||c=='/'||c=='('||c==')'))return false;
    }
    return false;
}
/* First boot is truly unconfigured: zero active pixels, zero zones, and
 * no RMT output. Stored v1/v2 profiles are restored unchanged by om_load().
 * No vehicle layout, lighting function or LED count may be inferred here.
 */
void ot_om_defaults(ot_om_output_t outputs[OT_OM_OUTPUTS]){
    if(!outputs)return;
    memset(outputs,0,sizeof(ot_om_output_t)*OT_OM_OUTPUTS);
    for(unsigned o=0;o<OT_OM_OUTPUTS;o++){
        ot_om_output_t *c=&outputs[o];
        snprintf(c->name,OT_OM_NAME_LEN,"Pixel OUT %u",o+1u);
        c->color_order=OT_OM_GRB;
        c->rgb_gain[0]=c->rgb_gain[1]=c->rgb_gain[2]=255;
        c->white_gain[0]=c->white_gain[1]=c->white_gain[2]=255;
    }
}

bool ot_om_valid(const ot_om_output_t *o){
    if(!o||!name_ok(o->name)||o->active_pixels>OT_OM_MAX_PIXELS_PER_OUTPUT||
       o->zone_count>OT_OM_ZONES||o->color_order>OT_OM_BGR)return false;
    /* Only 0/0 is an inert configuration. Reject 0/N and N/0 so a malformed
     * profile cannot silently become a live lighting output. */
    if(o->active_pixels==0||o->zone_count==0)
        return o->active_pixels==0 && o->zone_count==0;
    unsigned sum=0;
    for(int z=0;z<o->zone_count;z++){
        const ot_om_zone_t *a=&o->zones[z];
        if(!name_ok(a->name)||a->role>OT_OM_DECORATIVE)return false;
        sum+=a->pixels;
    }
    return sum==o->active_pixels;
}
int ot_om_section(const ot_om_output_t *o,unsigned pixel){
    if(!ot_om_valid(o)||pixel>=o->active_pixels)return -1;
    unsigned boundary=0;
    for(int i=0;i<o->zone_count;i++){boundary+=o->zones[i].pixels;if(pixel<boundary)return i;}
    return -1;
}
bool ot_om_layout(const ot_om_output_t *o,ot_layout_t *layout){
    if(!ot_om_valid(o)||!layout||o->active_pixels==0)return false;
    *layout=(ot_layout_t){.chain_pixels=o->active_pixels,.brightness_cap=OT_MAX_BRIGHTNESS};
    for(int i=0;i<o->zone_count;i++)layout->sections[i]=o->zones[i].pixels;
    return ot_layout_valid(layout);
}
static ot_rgb_t scaled(unsigned r,unsigned g,unsigned b,unsigned v){
    return (ot_rgb_t){(uint8_t)((r*v+127)/255),(uint8_t)((g*v+127)/255),(uint8_t)((b*v+127)/255)};
}
ot_rgb_t ot_om_trigger_color(uint8_t role,uint8_t mask,uint8_t brightness){
    if(brightness>OT_MAX_BRIGHTNESS)brightness=OT_MAX_BRIGHTNESS;
    switch(role){
        case OT_OM_PARK_BRAKE:
            if(mask&OT_SIG_BRAKE)return scaled(255,0,0,brightness);
            if(mask&OT_SIG_PARK)return scaled(55,0,0,brightness);
            break;
        case OT_OM_PARK:if(mask&OT_SIG_PARK)return scaled(55,0,0,brightness);break;
        case OT_OM_BRAKE:if(mask&OT_SIG_BRAKE)return scaled(255,0,0,brightness);break;
        case OT_OM_LEFT:if(mask&OT_SIG_LEFT)return scaled(255,150,0,brightness);break;
        case OT_OM_RIGHT:if(mask&OT_SIG_RIGHT)return scaled(255,150,0,brightness);break;
        case OT_OM_REVERSE:if(mask&OT_SIG_REVERSE)return scaled(255,255,255,brightness);break;
        case OT_OM_AUX1:if(mask&(1u<<5))return scaled(128,0,255,brightness);break;
        case OT_OM_AUX2:if(mask&(1u<<6))return scaled(128,0,255,brightness);break;
        case OT_OM_AUX3:if(mask&(1u<<7))return scaled(128,0,255,brightness);break;
        default:break;
    }
    return (ot_rgb_t){0,0,0};
}
/* Zone-owned overlays. BRAKE outranks PARK in the combined park_brake role.
 * Nonmatching inputs must not blank or interrupt the independent FX bank. */
bool ot_om_trigger_overlay(uint8_t role,uint8_t mask,bool show_here,
                           uint8_t brightness,ot_rgb_t *color){
    uint8_t active=show_here?(uint8_t)(mask&~OT_SIG_PARK):mask;
    bool hit=false;
    switch(role){
        case OT_OM_PARK_BRAKE:hit=(active&(OT_SIG_PARK|OT_SIG_BRAKE))!=0;break;
        case OT_OM_PARK:hit=(active&OT_SIG_PARK)!=0;break;
        case OT_OM_BRAKE:hit=(active&OT_SIG_BRAKE)!=0;break;
        case OT_OM_LEFT:hit=(active&OT_SIG_LEFT)!=0;break;
        case OT_OM_RIGHT:hit=(active&OT_SIG_RIGHT)!=0;break;
        case OT_OM_REVERSE:hit=(active&OT_SIG_REVERSE)!=0;break;
        case OT_OM_AUX1:hit=(active&(1u<<5))!=0;break;
        case OT_OM_AUX2:hit=(active&(1u<<6))!=0;break;
        case OT_OM_AUX3:hit=(active&(1u<<7))!=0;break;
        default:break;
    }
    if(hit&&color)*color=ot_om_trigger_color(role,active,brightness);
    return hit;
}
/* led_strip driver configured for physical BRG data ordering.
 * Permute logical RGB arguments to produce the requested wire byte sequence.
 * e.g. requesting GRB -> send (r,g,b) as (r,b,g) to a BRG driver.
 */
ot_rgb_t ot_om_swizzle(ot_rgb_t rgb,uint8_t order){
    const uint8_t v[3]={rgb.r,rgb.g,rgb.b};
    static const uint8_t orders[6][3]={{0,1,2},{0,2,1},{1,0,2},{1,2,0},{2,0,1},{2,1,0}};
    if(order>OT_OM_BGR)order=OT_OM_BRG;
    return (ot_rgb_t){v[orders[order][1]],v[orders[order][2]],v[orders[order][0]]};
}

/* Calibration applies only to decorative shows/demos, never OEM road trigger colors. */
ot_rgb_t ot_om_calibrate(ot_rgb_t c,const ot_om_output_t *cfg){
    if(!cfg)return c;
    const bool neutral=(c.r==c.g && c.g==c.b);
    uint8_t v[3]={c.r,c.g,c.b};
    for(unsigned i=0;i<3;i++){
        unsigned n=(v[i]*cfg->rgb_gain[i]+127u)/255u;
        if(neutral)n=(n*cfg->white_gain[i]+127u)/255u;
        if(cfg->gamma_enabled){ /* mild gamma approximation, bounded and monotonic */
            n=(n*n+OT_MAX_BRIGHTNESS/2u)/OT_MAX_BRIGHTNESS;
            if(n>OT_MAX_BRIGHTNESS)n=OT_MAX_BRIGHTNESS;
        }
        v[i]=(uint8_t)n;
    }
    return (ot_rgb_t){v[0],v[1],v[2]};
}
