/*
 * OpenTail-owned one-shot transitions and continuous Show Mode originals.
 * Pure, deterministic per-zone RGB rendering; no BLE, GPIO, allocations or
 * physical output ownership. The seven authored one-shot endpoints are kept
 * separate from the twelve continuously animated proprietary concepts.
 */
#include "ot_auto_effects.h"

static const uint16_t ids[] = {
    OT_FX_MATERIALIZE_SPARKS, OT_FX_RANDOM_MATERIALIZE,
    OT_FX_SMOOTH_FADE, OT_FX_PLASMA_SWEEP, OT_FX_SMOOTH_FADE_OUT,
    OT_FX_DIMENSIONAL_RIFT, OT_FX_METEOR_STORM, OT_FX_HELIX_DRIVE,
    OT_FX_CRYSTAL_GROWTH, OT_FX_FUSION_CORE, OT_FX_PORTAL_GATE,
    OT_FX_TIME_FRACTURE, OT_FX_AURORA_SURGE, OT_FX_CHROMATIC_SHATTER,
    OT_FX_VORTEX, OT_FX_QUANTUM_COLLAPSE, OT_FX_REALITY_BREAK,
    OT_FX_EVENT_HORIZON, OT_FX_SINGULARITY_SHUTDOWN
};

bool ot_auto_fx_id(uint16_t id) {
    for(size_t i=0;i<sizeof(ids)/sizeof(ids[0]);i++)
        if(ids[i]==id)return true;
    return false;
}

size_t ot_auto_fx_ids(uint16_t *dst,size_t capacity){
    const size_t n=sizeof(ids)/sizeof(ids[0]);
    if(dst)for(size_t i=0;i<n&&i<capacity;i++)dst[i]=ids[i];
    return n;
}
bool ot_auto_fx_metadata(uint16_t id,ot_wled_fx_metadata_t *out){
    if(!out)return false;
    const char *name=NULL;
    switch(id){
      case OT_FX_MATERIALIZE_SPARKS:name="Materialize Sparks";break;
      case OT_FX_RANDOM_MATERIALIZE:name="Random Materialize";break;
      case OT_FX_SMOOTH_FADE:name="Smooth Fade";break;
      case OT_FX_PLASMA_SWEEP:name="Plasma Sweep";break;
      case OT_FX_SMOOTH_FADE_OUT:name="Smooth Fade Out";break;
      case OT_FX_DIMENSIONAL_RIFT:name="Dimensional Rift";break;
      case OT_FX_METEOR_STORM:name="Meteor Storm";break;
      case OT_FX_HELIX_DRIVE:name="Helix Drive";break;
      case OT_FX_CRYSTAL_GROWTH:name="Crystal Growth";break;
      case OT_FX_FUSION_CORE:name="Fusion Core";break;
      case OT_FX_PORTAL_GATE:name="Portal Gate";break;
      case OT_FX_TIME_FRACTURE:name="Time Fracture";break;
      case OT_FX_AURORA_SURGE:name="Aurora Surge";break;
      case OT_FX_CHROMATIC_SHATTER:name="Chromatic Shatter";break;
      case OT_FX_VORTEX:name="Vortex";break;
      case OT_FX_QUANTUM_COLLAPSE:name="Quantum Collapse";break;
      case OT_FX_REALITY_BREAK:name="Reality Break";break;
      case OT_FX_EVENT_HORIZON:name="Event Horizon";break;
      case OT_FX_SINGULARITY_SHUTDOWN:name="Singularity Shutdown";break;
      default:return false;
    }
    *out=(ot_wled_fx_metadata_t){id,name,name,true,true,true,true,false};
    return true;
}

/* 350-3920 ms in a single shot; common speed 180 -> 1400 ms. */
uint32_t ot_auto_fx_duration_ms(uint8_t speed){
    return 350u+(255u-(uint32_t)speed)*14u;
}
static uint32_t hash32(uint32_t x){
    x^=x>>16;x*=0x7feb352du;
    x^=x>>15;x*=0x846ca68bu;
    x^=x>>16;return x;
}
static uint8_t mix8(uint8_t a,uint8_t b,uint8_t amount){
    return (uint8_t)(((uint32_t)a*(255u-amount)+(uint32_t)b*amount+127u)/255u);
}
static ot_wled_rgb_t blend(ot_wled_rgb_t a,ot_wled_rgb_t b,uint8_t amt){
    return (ot_wled_rgb_t){
        mix8(a.r,b.r,amt),mix8(a.g,b.g,amt),mix8(a.b,b.b,amt)
    };
}
static ot_wled_rgb_t scale(ot_wled_rgb_t c,uint8_t bri){
    return (ot_wled_rgb_t){
        (uint8_t)(((uint32_t)c.r*bri+127u)/255u),
        (uint8_t)(((uint32_t)c.g*bri+127u)/255u),
        (uint8_t)(((uint32_t)c.b*bri+127u)/255u)
    };
}
static ot_wled_rgb_t wheel(uint8_t v){
    const unsigned sector=(unsigned)v*6u;
    const uint8_t k=(uint8_t)((sector%256u)*255u/256u);
    switch(sector>>8){
      case 0:return (ot_wled_rgb_t){255,k,0};
      case 1:return (ot_wled_rgb_t){(uint8_t)(255u-k),255,0};
      case 2:return (ot_wled_rgb_t){0,255,k};
      case 3:return (ot_wled_rgb_t){0,(uint8_t)(255u-k),255};
      case 4:return (ot_wled_rgb_t){k,0,255};
      default:return (ot_wled_rgb_t){255,0,(uint8_t)(255u-k)};
    }
}
/* Native original fades use a Q16 interpolation curve. Earlier versions
 * quantized smoothstep to eight bits before applying an eight-bit brightness
 * limit, producing conspicuous stepping in low-brightness shutdown previews.
 * Never apply this easing to the vehicle-trigger or strobe path. */
static uint16_t smoothstep16(uint32_t elapsed,uint32_t duration){
    if(!duration || elapsed>=duration)return 65535u;
    const uint64_t t=((uint64_t)elapsed*65535u)/duration;
    const uint64_t d=(uint64_t)65535u*65535u;
    return (uint16_t)((t*t*(3u*65535u-2u*t)+d/2u)/d);
}
static uint8_t fade_channel(uint8_t from,uint8_t to,uint16_t k,uint8_t brightness){
    const uint64_t n=((uint64_t)from*(65535u-k)+(uint64_t)to*k)*brightness;
    const uint64_t d=(uint64_t)65535u*255u;
    return (uint8_t)((n+d/2u)/d);
}
static ot_wled_rgb_t fade_color(ot_wled_rgb_t from,ot_wled_rgb_t to,
                                uint16_t k,uint8_t brightness){
    return (ot_wled_rgb_t){
        fade_channel(from.r,to.r,k,brightness),
        fade_channel(from.g,to.g,k,brightness),
        fade_channel(from.b,to.b,k,brightness)};
}
/* OpenTail-owned original animations (not WLED compatibility recipes).
 * This is the first shared speed/color/intensity implementation for the
 * approved concepts. Per-effect advanced sliders, visual signoff and golden
 * frames remain release blockers; never claim those are already validated.
 * Frame result is absolute-time derived; no allocation or vehicle geometry. */
static uint8_t tri8(uint32_t v){
    v&=511u;
    return (uint8_t)(v>255u?511u-v:v);
}
static uint32_t absdiff(uint32_t a,uint32_t b){return a>b?a-b:b-a;}
static bool original_concept(uint16_t id){
    return id>=OT_FX_DIMENSIONAL_RIFT&&id<=OT_FX_SINGULARITY_SHUTDOWN;
}
static bool render_original_concept(const ot_wled_fx_config_t *cfg,uint32_t seed,
                                    uint32_t ms,ot_wled_rgb_t *pixels,size_t count){
    const uint32_t duration=ot_auto_fx_duration_ms(cfg->speed);
    const uint32_t elapsed=ms%duration;
    /* All continuous motifs use integral full-turn phases. In particular,
     * t*3 / t*5 in the old 0..254 progress clock ended mid-wave and snapped
     * to the beginning; cycle-dependent hashes also teleported particles.
     * A 512-unit wrapped turn closes every triangular waveform exactly. */
    const uint32_t phase=((uint64_t)elapsed*512u)/duration;
    const uint32_t pulse=tri8(phase); /* grow -> retract, no reveal reset */
    const uint32_t t=(elapsed*255u)/duration; /* one-shot Event Horizon only */
    const uint32_t whole=(ms>=duration)?duration:ms;
    const uint32_t one_shot_t=(whole*255u)/duration;
    const uint32_t center=(uint32_t)count/2u;
    const uint32_t width=1u+((uint32_t)cfg->intensity*(uint32_t)count)/4096u;
    const ot_wled_rgb_t fg=cfg->foreground,bg=cfg->background;
    const ot_wled_rgb_t white={255u,255u,255u};
    for(size_t px=0;px<count;px++){
        const uint32_t p=(uint32_t)px;
        const uint32_t normalized=(p*255u)/(uint32_t)count;
        const uint32_t distance=absdiff(p,center);
        const uint32_t h=hash32(p*2654435761u+seed);
        const uint32_t drift=(phase*(uint32_t)count)/512u;
        ot_wled_rgb_t c=bg;
        switch(cfg->effect_id){
          case OT_FX_DIMENSIONAL_RIFT:{
            uint32_t tear=(pulse*center)/255u;
            uint32_t d=absdiff(distance,tear);
            c=distance<tear?fg:bg;
            if(d<=width)c=blend(c,white,(uint8_t)(240u-(d*170u)/(width+1u)));
            else if(d<=width*4u)c=blend(c,wheel((uint8_t)(h>>12)),80u);
            break;
          }
          case OT_FX_METEOR_STORM:{
            c=bg;
            for(uint32_t lane=0;lane<4u;lane++){
                uint32_t head=(drift+(hash32(lane*137u+seed)%(uint32_t)count))%(uint32_t)count;
                uint32_t d=(head+(uint32_t)count-p)%(uint32_t)count;
                uint32_t tail=3u+width*7u;
                if(d<tail){
                    uint32_t v=255u-(d*255u)/tail;
                    c=blend(c,lane==0u?white:wheel((uint8_t)(lane*57u+h)),(uint8_t)v);
                }
            }
            break;
          }
          case OT_FX_HELIX_DRIVE:{
            uint8_t a=tri8(normalized*5u+phase*2u);
            uint8_t b=tri8(normalized*5u+phase*2u+255u);
            c=blend(bg,fg,a);
            c=blend(c,white,(uint8_t)((b*(uint32_t)cfg->intensity)/510u));
            break;
          }
          case OT_FX_CRYSTAL_GROWTH:{
            uint32_t reveal=hash32(p*7919u+seed)%256u;
            c=reveal<=pulse?fg:bg;
            if(reveal<=pulse&&pulse-reveal<20u)
                c=blend(c,white,(uint8_t)((20u-(pulse-reveal))*10u));
            else if(reveal<=pulse)
                c=blend(c,wheel((uint8_t)(p*29u)),(uint8_t)(cfg->intensity/6u));
            break;
          }
          case OT_FX_FUSION_CORE:{
            uint32_t radius=1u+(uint32_t)count*(32u+tri8(phase*2u))/1536u;
            c=distance<radius?blend(fg,white,(uint8_t)(220u-distance*150u/(radius+1u))):bg;
            if(distance>=radius&&distance<radius*4u){
                uint32_t aura=128u-(distance-radius)*128u/(radius*3u+1u);
                c=blend(bg,fg,(uint8_t)(aura*cfg->intensity/255u));
            }
            break;
          }
          case OT_FX_PORTAL_GATE:{
            uint32_t portal=(uint32_t)count*(32u+tri8(phase))/1024u;
            uint32_t d=absdiff(distance,portal);
            c=distance<portal?blend(bg,fg,90u):bg;
            if(d<width+3u)c=blend(c,white,(uint8_t)(230u-d*160u/(width+4u)));
            break;
          }
          case OT_FX_TIME_FRACTURE:{
            uint32_t chunk=p/(1u+(uint32_t)count/12u);
            uint32_t offset=hash32(chunk*109u+seed)%128u;
            uint8_t v=tri8(normalized*3u+phase*2u+offset);
            c=blend(bg,(chunk&1u)?fg:wheel((uint8_t)((phase>>1)+chunk*17u)),
                    (uint8_t)((uint32_t)v*cfg->intensity/255u));
            break;
          }
          case OT_FX_AURORA_SURGE:{
            uint32_t v=(uint32_t)tri8(normalized*4u+phase)+tri8(normalized*7u+phase*2u+90u);
            c=blend(bg,fg,(uint8_t)(v/2u));
            c=blend(c,wheel((uint8_t)(normalized+(phase>>1))),(uint8_t)(cfg->intensity/3u));
            break;
          }
          case OT_FX_CHROMATIC_SHATTER:{
            uint32_t shard=h&15u;
            uint32_t d=absdiff((p+shard*3u)%(uint32_t)count,drift);
            /* Wrap the distance as well as the sweep coordinate. Without
             * this, the last shard teleports from one endpoint to another. */
            if((uint32_t)count-d<d)d=(uint32_t)count-d;
            c=d<width*4u?wheel((uint8_t)(h>>8)):bg;
            if(d<width)c=blend(c,white,180u);
            break;
          }
          case OT_FX_VORTEX:{
            uint8_t v=tri8(distance*24u+phase*2u);
            c=blend(bg,wheel((uint8_t)(normalized*3u+(phase>>1))),v);
            if(distance<width)c=blend(c,white,150u);
            break;
          }
          case OT_FX_QUANTUM_COLLAPSE:{
            uint32_t radius=((uint32_t)count*(255u-pulse))/512u+1u;
            c=distance<=radius?blend(fg,white,(uint8_t)(pulse/2u)):bg;
            if(absdiff(distance,radius)<width)c=white;
            break;
          }
          case OT_FX_REALITY_BREAK:{
            uint32_t crack=(hash32(p/(1u+(uint32_t)count/25u)+seed)&255u);
            uint8_t distortion=tri8(normalized*11u+phase*3u+crack);
            c=blend(bg,fg,distortion);
            if(distortion>220u)c=blend(c,wheel((uint8_t)(h>>14)),cfg->intensity);
            break;
          }
          case OT_FX_EVENT_HORIZON:{
            if(ms>=duration){c=fg;break;}
            const uint32_t front=(one_shot_t*((uint32_t)count-1u))/255u;
            uint32_t d=absdiff(p,front);
            c=fg;
            if(d<=width)c=bg; /* traveling black gravitational core */
            else if(d<=width+2u)c=white; /* compressed bright ring */
            else if(p<front && d<=width+12u)
                c=blend(fg,wheel((uint8_t)(t*4u+d*19u)),
                    (uint8_t)(200u-(d-width-2u)*120u/13u));
            break;
          }
          case OT_FX_SINGULARITY_SHUTDOWN:{
            if(ms>=duration){c=(ot_wled_rgb_t){0,0,0};break;}
            const uint32_t radius=(center*(255u-one_shot_t))/255u;
            c=distance<=radius?fg:(ot_wled_rgb_t){0,0,0};
            if(distance<=radius && absdiff(distance,radius)<width+2u)c=white;
            if(radius<width+2u && distance<=width)c=white;
            break;
          }
          default:return false;
        }
        pixels[px]=scale(c,cfg->brightness);
    }
    return true;
}

bool ot_auto_fx_render(const ot_wled_fx_config_t *cfg,uint32_t seed,
                       uint32_t ms,ot_wled_rgb_t *pixels,size_t count){
    if(!cfg||!pixels||!ot_auto_fx_id(cfg->effect_id)||count==0||
       count>65535u||cfg->frame_time_ms==0u||cfg->palette_id!=0u||
       cfg->spark_style>3u||cfg->spark_peak>32u)
        return false;
    const uint32_t duration=ot_auto_fx_duration_ms(cfg->speed);
    const uint32_t elapsed=ms>=duration?duration:ms;
    const ot_wled_rgb_t fg=cfg->foreground,bg=cfg->background;
    if(original_concept(cfg->effect_id))
        return render_original_concept(cfg,seed,ms,pixels,count);
    if(cfg->effect_id==OT_FX_SMOOTH_FADE ||
       cfg->effect_id==OT_FX_SMOOTH_FADE_OUT){
        const uint16_t k=smoothstep16(elapsed,duration);
        const ot_wled_rgb_t from=cfg->effect_id==OT_FX_SMOOTH_FADE?bg:fg;
        const ot_wled_rgb_t to=cfg->effect_id==OT_FX_SMOOTH_FADE?fg:bg;
        const ot_wled_rgb_t out=fade_color(from,to,k,cfg->brightness);
        for(size_t i=0;i<count;i++)pixels[i]=out;
        return true;
    }
    if(ms>=duration){
        const ot_wled_rgb_t out=scale(fg,cfg->brightness);
        for(size_t i=0;i<count;i++)pixels[i]=out;
        return true;
    }
    for(size_t i=0;i<count;i++){
        const uint32_t h=hash32((uint32_t)i ^ (seed?seed:1u) ^ 0x9e3779b9u);
        ot_wled_rgb_t color=bg;
        if(cfg->effect_id==OT_FX_PLASMA_SWEEP){
            const uint32_t front=(elapsed*(uint32_t)count)/duration;
            if(i<=front){
                const uint8_t gap=(uint8_t)((front-i)*21u+(elapsed/12u));
                const uint8_t wave=(gap&128u)?(uint8_t)(255u-(uint8_t)(gap<<1)):
                                             (uint8_t)(gap<<1);
                color=blend(bg,fg,(uint8_t)(128u+(wave>>1)));
            }
        } else {
            /* Hash-order progressive reveal: when completed every pixel is
             * foreground. Spark lives 120ms, with a minimum useful 40ms
             * to preserve a visible glow at high speed. */
            const uint32_t reveal=(h%duration);
            if(elapsed>=reveal){
                const uint32_t age=elapsed-reveal;
                const uint32_t sparkle_ms=120u;
                color=fg;
                if(age<sparkle_ms && cfg->intensity){
                    /* The body remains at the user-selected brightness, while
                     * the short-lived spark may use spare BENCH headroom up to
                     * 32/255. At low brightness a same-hue spark would otherwise
                     * be invisible because it is blended with an already
                     * saturated foreground color. Never light a zero-brightness
                     * request, and never exceed either 32 or a higher explicit
                     * renderer cap. This does not alter vehicle trigger colors. */
                    /* Bench peak is independently adjustable, never above
                     * 32/255; legacy callers passing zero keep auto=32.
                     * Base brightness 0 always stays off. */
                    const uint8_t requested_peak=cfg->spark_peak?cfg->spark_peak:32u;
                    const uint32_t peak_cap=cfg->brightness==0u?0u:requested_peak;
                    const uint32_t peak=peak_cap;
                    const uint8_t fade=(uint8_t)(((sparkle_ms-age)*cfg->intensity)/sparkle_ms);
                    ot_wled_rgb_t spark=fg;
                    const ot_wled_rgb_t white={255u,255u,255u};
                    switch(cfg->spark_style){
                      case 1: /* randomized bright shades of the selected color */
                        spark=blend(fg,white,(uint8_t)(70u+((h>>16)&127u)));
                        break;
                      case 2: /* rainbow particles plus occasional white accents */
                        spark=(h>>24)>239u?white:wheel((uint8_t)(h>>8));
                        break;
                      case 3: /* independently selected spark color */
                        spark=cfg->spark_color;
                        break;
                      default: /* keep existing original animation behavior */
                        if(cfg->effect_id==OT_FX_RANDOM_MATERIALIZE)
                            spark=(h>>24)>239u?white:wheel((uint8_t)(h>>8));
                        break;
                    }
                    const ot_wled_rgb_t base=scale(fg,cfg->brightness);
                    const ot_wled_rgb_t highlight=scale(spark,(uint8_t)peak);
                    pixels[i]=blend(base,highlight,fade);
                    continue;
                }
            }
        }
        pixels[i]=scale(color,cfg->brightness);
    }
    return true;
}
