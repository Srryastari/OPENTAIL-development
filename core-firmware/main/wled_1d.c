#include "wled_1d.h"
#include <string.h>

/* Stable upstream WLED ID/name catalog, 1D non-audio/non-matrix modes only. */
static const ot_wled_1d_meta_t catalog[]={
    {0,"static","Solid",0},
    {1,"blink","Blink",1},
    {2,"breath","Breathe",2},
    {3,"color_wipe","Wipe",3},
    {4,"color_wipe_random","Wipe Random",3},
    {5,"random_color","Random Colors",5},
    {6,"color_sweep","Sweep",3},
    {7,"dynamic","Dynamic",10},
    {8,"rainbow","Colorloop",6},
    {9,"rainbow_cycle","Rainbow",6},
    {10,"scan","Scan",7},
    {11,"dual_scan","Scan Dual",7},
    {12,"fade","Fade",2},
    {13,"theater_chase","Theater",4},
    {14,"theater_chase_rainbow","Theater Rainbow",4},
    {15,"running_lights","Running",5},
    {16,"saw","Saw",5},
    {17,"twinkle","Twinkle",8},
    {18,"dissolve","Dissolve",8},
    {19,"dissolve_random","Dissolve Rnd",8},
    {20,"sparkle","Sparkle",8},
    {21,"flash_sparkle","Sparkle Dark",8},
    {22,"hyper_sparkle","Sparkle+",8},
    {23,"strobe","Strobe",1},
    {24,"strobe_rainbow","Strobe Rainbow",1},
    {25,"multi_strobe","Strobe Mega",1},
    {26,"blink_rainbow","Blink Rainbow",1},
    {27,"android","Android",4},
    {28,"chase_color","Chase",4},
    {29,"chase_random","Chase Random",4},
    {30,"chase_rainbow","Chase Rainbow",4},
    {31,"chase_flash","Chase Flash",4},
    {32,"chase_flash_random","Chase Flash Rnd",4},
    {33,"chase_rainbow_white","Rainbow Runner",4},
    {34,"colorful","Colorful",6},
    {35,"traffic_light","Traffic Light",4},
    {36,"color_sweep_random","Sweep Random",3},
    {37,"running_color","Chase 2",4},
    {38,"aurora","Aurora",5},
    {39,"running_random","Stream",4},
    {40,"larson_scanner","Scanner",7},
    {41,"comet","Lighthouse",7},
    {42,"fireworks","Fireworks",12},
    {43,"rain","Rain",11},
    {44,"tetrix","Tetrix",5},
    {45,"fire_flicker","Fire Flicker",9},
    {46,"gradient","Gradient",6},
    {47,"loading","Loading",3},
    {48,"rollingballs","Rolling Balls",5},
    {49,"fairy","Fairy",8},
    {50,"two_dots","Two Dots",7},
    {51,"fairytwinkle","Fairytwinkle",8},
    {52,"running_dual","Running Dual",5},
    {54,"tricolor_chase","Chase 3",4},
    {55,"tricolor_wipe","Tri Wipe",3},
    {56,"tricolor_fade","Tri Fade",2},
    {57,"lightning","Lightning",1},
    {58,"icu","ICU",7},
    {59,"multi_comet","Multi Comet",7},
    {60,"dual_larson_scanner","Scanner Dual",7},
    {61,"random_chase","Stream 2",4},
    {62,"oscillate","Oscillate",5},
    {63,"pride_2015","Pride 2015",5},
    {64,"juggle","Juggle",5},
    {65,"palette","Palette",6},
    {66,"fire_2012","Fire 2012",9},
    {67,"colorwaves","Colorwaves",5},
    {68,"bpm","Bpm",5},
    {69,"fillnoise8","Fill Noise",10},
    {70,"noise16_1","Noise 1",10},
    {71,"noise16_2","Noise 2",10},
    {72,"noise16_3","Noise 3",10},
    {73,"noise16_4","Noise 4",10},
    {74,"colortwinkle","Colortwinkles",8},
    {75,"lake","Lake",5},
    {76,"meteor","Meteor",11},
    {78,"railway","Railway",5},
    {79,"ripple","Ripple",11},
    {80,"twinklefox","Twinklefox",8},
    {81,"twinklecat","Twinklecat",8},
    {82,"halloween_eyes","Halloween Eyes",5},
    {83,"static_pattern","Solid Pattern",0},
    {84,"tri_static_pattern","Solid Pattern Tri",0},
    {85,"spots","Spots",8},
    {86,"spots_fade","Spots Fade",8},
    {87,"glitter","Glitter",8},
    {88,"candle","Candle",9},
    {89,"starburst","Fireworks Starburst",12},
    {90,"exploding_fireworks","Fireworks 1D",12},
    {91,"bouncingballs","Bouncing Balls",13},
    {92,"sinelon","Sinelon",7},
    {93,"sinelon_dual","Sinelon Dual",7},
    {94,"sinelon_rainbow","Sinelon Rainbow",7},
    {95,"popcorn","Popcorn",11},
    {96,"drip","Drip",11},
    {97,"plasma","Plasma",10},
    {98,"percent","Percent",0},
    {99,"ripple_rainbow","Ripple Rainbow",11},
    {100,"heartbeat","Heartbeat",2},
    {101,"pacifica","Pacifica",5},
    {102,"candle_multi","Candle Multi",9},
    {103,"solid_glitter","Solid Glitter",8},
    {104,"sunrise","Sunrise",2},
    {105,"phased","Phased",5},
    {106,"twinkleup","Twinkleup",8},
    {107,"noisepal","Noise Pal",10},
    {108,"sinewave","Sine",5},
    {109,"phasednoise","Phased Noise",5},
    {110,"flow","Flow",5},
    {111,"chunchun","Chunchun",5},
    {112,"dancing_shadows","Dancing Shadows",5},
    {113,"washing_machine","Washing Machine",3},
    {115,"blends","Blends",5},
    {116,"tv_simulator","TV Simulator",14},
    {117,"dynamic_smooth","Dynamic Smooth",10},
    {179,"flowstripe","Flow Stripe",5},
    {184,"wavesins","Wavesins",5},
    {185,"rocktaves","Rocktaves",5},
    {218,"colorclouds","Color Clouds",5},
    {219,"slow_transition","Slow Transition",2},
};
size_t ot_wled_1d_count(void){return sizeof(catalog)/sizeof(catalog[0]);}
const ot_wled_1d_meta_t *ot_wled_1d_at(size_t i){return i<ot_wled_1d_count()?&catalog[i]:NULL;}
const ot_wled_1d_meta_t *ot_wled_1d_by_key(const char *key){
    if(!key)return NULL;
    for(size_t i=0;i<ot_wled_1d_count();++i)if(strcmp(key,catalog[i].key)==0)return &catalog[i];
    return NULL;
}
const ot_wled_1d_meta_t *ot_wled_1d_by_id(unsigned id){
    for(size_t i=0;i<ot_wled_1d_count();++i)if(id==catalog[i].id)return &catalog[i];
    return NULL;
}
static uint32_t hash32(uint32_t x){x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;x^=x>>16;return x;}
static uint8_t tri(uint32_t n,uint32_t d){
    if(d<2u)return 0;
    n%=d;return (uint8_t)(n<d/2u?n*255u/(d/2u):(d-n)*255u/(d-d/2u));
}
static uint8_t clamp8(unsigned n){return n>255u?255u:(uint8_t)n;}
static ot_rgb_t rgb(unsigned r,unsigned g,unsigned b){return (ot_rgb_t){clamp8(r),clamp8(g),clamp8(b)};}
static ot_rgb_t mix(ot_rgb_t a,ot_rgb_t b,unsigned t){
    if(t>255u)t=255u;
    return rgb((a.r*(255u-t)+b.r*t)/255u,(a.g*(255u-t)+b.g*t)/255u,(a.b*(255u-t)+b.b*t)/255u);
}
static ot_rgb_t wheel(unsigned hue){
    unsigned segment=(hue&255u)/43u,frac=((hue&255u)%43u)*255u/43u;
    switch(segment){
      case 0:return rgb(255,frac,0);case 1:return rgb(255-frac,255,0);
      case 2:return rgb(0,255,frac);case 3:return rgb(0,255-frac,255);
      case 4:return rgb(frac,0,255);default:return rgb(255,0,255-frac);
    }
}
static uint8_t softnoise(uint32_t x,uint32_t t,unsigned seed){
    const uint32_t cell=x>>5u, frac=x&31u, epoch=t>>8u, tfrac=t&255u;
    uint32_t a=hash32(cell*7919u+epoch*104729u+seed)&255u;
    uint32_t b=hash32((cell+1u)*7919u+epoch*104729u+seed)&255u;
    uint32_t c=hash32(cell*7919u+(epoch+1u)*104729u+seed)&255u;
    uint32_t d=hash32((cell+1u)*7919u+(epoch+1u)*104729u+seed)&255u;
    uint32_t ab=(a*(32u-frac)+b*frac)/32u, cd=(c*(32u-frac)+d*frac)/32u;
    return (uint8_t)((ab*(256u-tfrac)+cd*tfrac)/256u);
}
ot_rgb_t ot_wled_1d_pixel(unsigned id,unsigned px,unsigned len,uint32_t ms,
                          uint8_t speed,uint8_t intensity,ot_rgb_t fg,ot_rgb_t bg,ot_rgb_t accent){
    const ot_wled_1d_meta_t *m=ot_wled_1d_by_id(id);
    if(!m||!len||px>=len)return rgb(0,0,0);
    const uint32_t period=3600u-(uint32_t)speed*10u, phase=ms%period;
    const uint32_t tick=ms/(15u+(255u-speed)/6u);
    const uint32_t step=tick%len;
    const uint32_t sig=hash32((uint32_t)id*99991u+(uint32_t)px*2654435761u+(ms/110u)*2246822519u);
    const uint8_t wave=tri((uint32_t)px*16u+ms/(3u+(255u-speed)/64u),512u);
    const uint8_t h=(uint8_t)(px*256u/len+ms/(12u+(255u-speed)/13u));
    const uint32_t width=1u+(uint32_t)(intensity)*len/2048u;
    const uint8_t n=softnoise(px*3u,ms/(2u+(255u-speed)/32u),id);
    switch(m->family){
      case 0: /* solid and section patterns */
        if(id==98)return px*100u/len<=intensity?fg:bg;
        if(id==83||id==84)return (px/(1u+len/12u))%(id==84?3u:2u)==0?fg:(id==84?accent:bg);
        return fg;
      case 1: { /* blink, rainbow blink, strobes, lightning */
        unsigned duty=id==25?((ms/85u)%3u==0u):id==57?((sig&63u)<4u):(phase<period*(64u+intensity)/512u);
        if(id==23||id==24||id==25||id==57)duty=ms%(65u+(255u-speed))<(8u+intensity/18u);
        if(!duty)return bg;
        return (id==24||id==26)?wheel(h):fg;
      }
      case 2: { /* fades, heartbeat, sunrise, tri-color */
        unsigned amount=tri((uint32_t)(ms/(5u+(255u-speed)/48u)),512u);
        if(id==100)amount=(amount*amount)/255u;
        if(id==104){amount=ms>60000u?255u:ms*255u/60000u;}
        return mix(id==56?accent:bg,fg,amount);
      }
      case 3: { /* wipes and sweeps, forward/back, tri-color */
        unsigned sweep=phase*2u*len/period;
        if(id==6||id==36||id==113)sweep=2u*len-1u-sweep;
        unsigned active=sweep<len?(px<=sweep):(px>sweep-len);
        if(id==55)return active?(px<len/3u?fg:px<len*2u/3u?accent:wheel(h)):bg;
        return active?(id==4||id==36?wheel(hash32((ms/period)*7907u+id)&255u):fg):bg;
      }
      case 4: { /* chase, theater, streams, tricolors */
        unsigned stride=(id==13||id==14)?3u:id==54?5u:7u;
        unsigned active=((px/2u+tick/2u)%stride)<(id==25?2u:1u);
        if(id==27)active=(px+tick)%len < (len/6u);
        if(id==35)return (px+tick)%len < len/3u ? rgb(255,0,0):(px+tick)%len<2u*len/3u?rgb(255,255,0):rgb(0,255,0);
        return active?((id==14||id==30||id==33)?wheel(h):id==54?accent:fg):bg;
      }
      case 5: { /* moving waves, gradient lanes, ocean, flow */
        unsigned amp=(wave+(unsigned)softnoise(px*4u,ms/12u,id))/2u;
        if(id==16)amp=(px+tick)%len < len/2u?220u:40u;
        if(id==68)amp=tri(ms/(6u+(255u-speed)/64u),256u);
        ot_rgb_t base=(id==63||id==67||id==75||id==101||id==218)?wheel(h+amp/3u):fg;
        return mix(bg,base,amp);
      }
      case 6: /* rainbow gradients / palettes */
        return id==65?mix(fg,accent,wave):wheel((id==8?ms/24u:px*256u/len+ms/20u)+id*3u);
      case 7: { /* scanners, sinelon, comet */
        unsigned head=tri(ms/(4u+(255u-speed)/80u),len>1u?2u*(len-1u):2u)*(len-1u)/255u;
        unsigned dist=px>head?px-head:head-px;
        if(id==11||id==60||id==93)dist=dist<len/2u?dist:len-dist;
        if(id==41||id==59)dist=(head+len-px)%len;
        if(dist>width*6u)return bg;
        return mix(bg,(id==94?wheel(h):fg),255u-dist*255u/(width*6u+1u));
      }
      case 8: { /* twinkle, sparkle and dissolve variants */
        const uint32_t epoch=ms/(90u+(255u-speed)*2u);
        const uint32_t k=hash32((uint32_t)px*0x9e3779b9u+(uint32_t)id*3331u+epoch*0x85ebca6bu);
        const unsigned density=(id==20||id==21||id==22)?intensity/12u+1u:intensity/2u+20u;
        unsigned bright=(k&255u)<density?255u:(id==103?90u:0u);
        if(id==17||id==51||id==74||id==80||id==81||id==106)bright=bright*(255u-(ms%300u)*255u/300u)/255u;
        return mix(bg,id==74?wheel(k>>8u):fg,bright);
      }
      case 9: { /* fire, candles */
        const unsigned heat=(n+wave)/2u;
        const ot_rgb_t flame=heat>190u?rgb(255,150u+(heat-190u),heat-160u):heat>90u?rgb(255,heat-60u,0):rgb(heat*2u,heat/8u,0);
        return mix(bg,flame,id==88||id==102?120u+intensity/3u:230u);
      }
      case 10: /* structured moving noise, plasma */
        return mix(bg,id==97||id==70||id==71||id==72||id==73?wheel(h+n):fg,n);
      case 11: { /* meteor, ripples, rain, drops */
        unsigned head=(tick*(1u+len/40u))%len;
        unsigned dist=(head+len-px)%len;
        if(id==79||id==99)dist=px>len/2u?px-len/2u:len/2u-px;
        if(dist>width*8u)return bg;
        return mix(bg,id==99?wheel(h):fg,255u-dist*255u/(width*8u+1u));
      }
      case 12: { /* fireworks / explosions */
        const uint32_t life=ms%(1700u+(255u-speed)*4u);
        unsigned center=hash32((ms/(1700u+(255u-speed)*4u))*1337u+id)%len;
        unsigned dist=px>center?px-center:center-px;
        unsigned radius=life*len/(1700u+(255u-speed)*4u);
        return (dist>radius+width||dist+width<radius)?bg:wheel(h+center);
      }
      case 13: { /* bouncing balls / oscillation */
        unsigned head=tri(ms/(4u+(255u-speed)/48u),len>1u?2u*(len-1u):2u)*(len-1u)/255u;
        unsigned dist=px>head?px-head:head-px;
        return dist<width+1u?fg:bg;
      }
      case 14: /* ambient film-like variation */
        return mix(bg,fg,(unsigned)(softnoise(px*13u,ms/18u,id)+softnoise(px,ms/37u,id*7u))/2u);
      default:return fg;
    }
}
