#include "show_logic.h"
#include <stddef.h>

static ot_rgb_t black(void){return (ot_rgb_t){0,0,0};}
static ot_rgb_t scale(ot_rgb_t c,unsigned brightness){
    return (ot_rgb_t){(uint8_t)((c.r*brightness+127u)/255u),
        (uint8_t)((c.g*brightness+127u)/255u),
        (uint8_t)((c.b*brightness+127u)/255u)};
}
static ot_rgb_t wheel(uint8_t hue){
    /* Six saturated segments, no floating point or pastel channel contamination. */
    unsigned seg=hue/43u, frac=(unsigned)(hue%43u)*255u/43u;
    switch(seg){
      case 0:return (ot_rgb_t){255,(uint8_t)frac,0};
      case 1:return (ot_rgb_t){(uint8_t)(255-frac),255,0};
      case 2:return (ot_rgb_t){0,255,(uint8_t)frac};
      case 3:return (ot_rgb_t){0,(uint8_t)(255-frac),255};
      case 4:return (ot_rgb_t){(uint8_t)frac,0,255};
      default:return (ot_rgb_t){255,0,(uint8_t)(255-frac)};
    }
}
/* Deterministic stateless noise; no heap allocation and no shared random state. */
static uint32_t noise32(uint32_t n){
    n^=n>>16;n*=0x7feb352du;n^=n>>15;n*=0x846ca68bu;n^=n>>16;return n;
}
static unsigned triangle(unsigned x,unsigned period){
    if(period<2u)return 0;
    x%=period;return x<(period/2u)?(x*255u)/(period/2u):((period-x)*255u)/(period-period/2u);
}
const char *ot_show_name(ot_show_mode_t mode){
    switch(mode){case OT_SHOW_SOLID:return "solid";case OT_SHOW_CHASE:return "chase";
      case OT_SHOW_BREATHE:return "breathe";case OT_SHOW_WIPE:return "wipe";
      case OT_SHOW_RAINBOW:return "rainbow";case OT_SHOW_STROBE:return "strobe";
      case OT_SHOW_SCAN:return "scan";case OT_SHOW_PULSE:return "pulse";
      case OT_SHOW_FADE:return "fade";case OT_SHOW_BLINK:return "blink";
      case OT_SHOW_PLASMA_SWEEP:return "plasma_sweep";case OT_SHOW_SPARKLE:return "sparkle";
      case OT_SHOW_METEOR:return "meteor";case OT_SHOW_COLOR_WAVES:return "color_waves";
      case OT_SHOW_TWINKLE:return "twinkle";default:return "off";}
}
bool ot_show_valid(const ot_show_t *s,const ot_layout_t *layout){
    return s && ot_layout_valid(layout) && s->mode>=OT_SHOW_OFF && s->mode<=OT_SHOW_LAST &&
      s->brightness<=layout->brightness_cap && s->speed<=100u &&
      s->chain_mask && !(s->chain_mask & ~3u) &&
      s->section_mask && !(s->section_mask & ~255u);
}
ot_rgb_t ot_show_pixel(const ot_show_t *s,const ot_layout_t *layout,int chain,
                       unsigned pixel,uint32_t elapsed_ms){
    if(!ot_show_valid(s,layout)||chain<0||chain>=OT_CHAIN_COUNT||
       pixel>=layout->chain_pixels||s->mode==OT_SHOW_OFF||
       !(s->chain_mask & (1u<<chain)))return black();
    int zone=ot_section_for_pixel(layout,pixel);
    if(zone<0 || !(s->section_mask & (1u<<zone)))return black();
    unsigned offset=0;
    for(int i=0;i<zone;++i)offset+=layout->sections[i];
    unsigned local=pixel-offset, count=layout->sections[zone];
    unsigned period=3200u-(unsigned)s->speed*26u; /* 3200..600 ms per main cycle */
    unsigned phase=elapsed_ms%period;
    ot_rgb_t c=s->color;
    switch(s->mode){
      case OT_SHOW_SOLID:break;
      case OT_SHOW_CHASE:{
        unsigned step=(elapsed_ms/(period/24u?period/24u:1u));
        if(((local/3u+step)%9u)>2u)return black();
        break;
      }
      case OT_SHOW_BREATHE:{
        unsigned half=period/2u;
        unsigned ramp=phase<half?phase:(period-phase);
        unsigned pct=8u+(ramp*92u)/(half?half:1u);
        return scale(c,(unsigned)s->brightness*pct/100u);
      }
      case OT_SHOW_WIPE:{
        unsigned index=(phase*(2u*count))/period;
        if(index<count){if(local>index)return black();}
        else if(local<=index-count)return black();
        break;
      }
      case OT_SHOW_RAINBOW:{
        uint8_t hue=(uint8_t)((local*256u/(count?count:1u)+elapsed_ms*256u/period)&255u);
        c=wheel(hue);break;
      }
      case OT_SHOW_STROBE:{
        const unsigned beat=80u+(100u-s->speed)*4u;
        if(elapsed_ms%beat >=beat/6u)return black();break;
      }
      case OT_SHOW_SCAN:{
        const unsigned pos=triangle((unsigned)(elapsed_ms/8u),count>1u?2u*(count-1u):2u);
        const unsigned head=(pos*(count>1u?count-1u:0u))/255u;
        const unsigned distance=local>head?local-head:head-local;
        if(distance>2u)return black();
        return scale(c,(unsigned)s->brightness*(3u-distance)/3u);
      }
      case OT_SHOW_PULSE:{
        unsigned pct=triangle(phase,period);
        return scale(c,(unsigned)s->brightness*pct/255u);
      }
      case OT_SHOW_FADE:{
        return scale(c,(unsigned)s->brightness*triangle(phase,period)/255u);
      }
      case OT_SHOW_BLINK:{
        if(phase>=period/2u)return black();break;
      }
      case OT_SHOW_PLASMA_SWEEP:{
        const unsigned a=triangle((unsigned)(local*13u+elapsed_ms/9u),512u);
        const unsigned b=triangle((unsigned)(local*7u+elapsed_ms/17u),384u);
        c=wheel((uint8_t)((a+b+elapsed_ms/30u)&255u));
        const unsigned amplitude=96u+(a+b)/3u;
        return scale(c,(unsigned)s->brightness*(amplitude>255u?255u:amplitude)/255u);
      }
      case OT_SHOW_SPARKLE:{
        uint32_t slice=elapsed_ms/(20u+(100u-s->speed)*2u);
        if(noise32((uint32_t)local*0x9e3779b9u+slice*0x85ebca6bu)%100u>12u)return black();
        break;
      }
      case OT_SHOW_METEOR:{
        const unsigned head=(phase*count)/period;
        const unsigned gap=(head+count-local)%count;
        if(gap>9u)return black();
        return scale(c,(unsigned)s->brightness*(10u-gap)/10u);
      }
      case OT_SHOW_COLOR_WAVES:{
        unsigned hue=(local*256u/(count?count:1u)+elapsed_ms/20u)&255u;
        unsigned amount=32u+triangle((unsigned)(local*11u+elapsed_ms/13u),510u)*223u/255u;
        return scale(wheel((uint8_t)hue),(unsigned)s->brightness*amount/255u);
      }
      case OT_SHOW_TWINKLE:{
        unsigned slice=elapsed_ms/100u;
        uint32_t seed=(uint32_t)local*0x9e3779b9u;
        const unsigned a=noise32(seed+slice*0x85ebca6bu)%256u;
        const unsigned b=noise32(seed+(slice+1u)*0x85ebca6bu)%256u;
        unsigned amp=(a*(100u-elapsed_ms%100u)+b*(elapsed_ms%100u))/100u;
        return scale(c,(unsigned)s->brightness*amp/255u);
      }
      default:return black();
    }
    return scale(c,s->brightness);
}
