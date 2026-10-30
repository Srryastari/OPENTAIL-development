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
const char *ot_show_name(ot_show_mode_t mode){
    switch(mode){case OT_SHOW_SOLID:return "solid";case OT_SHOW_CHASE:return "chase";
      case OT_SHOW_BREATHE:return "breathe";case OT_SHOW_WIPE:return "wipe";
      case OT_SHOW_RAINBOW:return "rainbow";default:return "off";}
}
bool ot_show_valid(const ot_show_t *s,const ot_layout_t *layout){
    return s && ot_layout_valid(layout) && s->mode>=OT_SHOW_OFF && s->mode<=OT_SHOW_RAINBOW &&
      s->brightness<=layout->brightness_cap && s->speed<=100u &&
      s->chain_mask && !(s->chain_mask & ~((1u<<OT_CHAIN_COUNT)-1u)) &&
      s->section_mask && !(s->section_mask & ~((1u<<OT_SECTION_COUNT)-1u));
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
      default:return black();
    }
    return scale(c,s->brightness);
}
