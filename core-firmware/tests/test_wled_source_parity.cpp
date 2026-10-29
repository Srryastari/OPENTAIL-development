/*
 * PINNED-UPSTREAM GOLDEN-FRAME HARNESS — BENCH/HOST TEST ONLY.
 * Source: https://github.com/wled/WLED/tree/20e7a717664295eefc6ad77972cee1888d2da072
 * WLED copyright (c) 2016 Harm Aldick and individual contributors.
 * EUPL 1.2 or later (see third_party/wled/LICENSE).
 *
 * The functions marked UPSTREAM BELOW are directly extracted, verbatim,
 * from the pinned FX.cpp, colors.cpp, wled_math.cpp and fastled_slim.cpp.
 * The surrounding adapter supplies only the narrow RGB, palette=0 WLED
 * Segment/Environment types. No WLED firmware is flashed or integrated.
 * Comparing against real upstream function bodies is intentionally stronger
 * than repeating CORE's own formulas in a second expected-value function.
 */
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>
extern "C" {
#include "wled_fx_compat.h"
}
#define WLED_O2_ATTR
#define IRAM_ATTR
#define WHITE 0x00FFFFFFu
#define PALETTE_SOLID_WRAP true
using std::min;
using std::abs;
using byte = uint8_t;

// BEGIN UPSTREAM math/color functions, unchanged source bodies.
void hsv2rgb_rainbow(uint16_t h, uint8_t s, uint8_t v, uint8_t* rgbdata, bool isRGBW) {
  uint8_t hue = h>>8;
  uint8_t sat = s;
  uint32_t val = v;
  uint32_t offset = h & 0x1FFF; // 0..31, shifted
  uint32_t third16 = (offset * 21846); // offset16 = offset * 1/3<<16
  uint8_t third = third16 >> 21; // max = 85
  uint8_t r, g, b; // note: making these 32bit is significantly slower

  if (!(hue & 0x80)) {
    if (!(hue & 0x40)) { // section 0-1
      if (!(hue & 0x20)) {
        r = 255 - third;
        g = third;
        b = 0;
      } else {
        r = 171;
        g = 85 + third;
        b = 0;
      }
    } else { // section 2-3
      if (!(hue & 0x20)) {
        uint8_t twothirds = third16 >> 20; // max=170
        r = 171 - twothirds;
        g = 170 + third;
        b = 0;
      } else {
        r = 0;
        g = 255 - third;
        b = third;
      }
    }
  } else { // section 4-7
    if (!(hue & 0x40)) {
      if (!(hue & 0x20)) {
        r = 0;
        uint8_t twothirds = third16 >> 20; // max=170
        g = 171 - twothirds;
        b = 85 + twothirds;
      } else {
        r = third;
        g = 0;
        b = 255 - third;
      }
    } else {
      if (!(hue & 0x20)) {
        r = 85 + third;
        g = 0;
        b = 171 - third;
      } else {
        r = 170 + third;
        g = 0;
        b = 85 - third;
      }
    }
  }

  // scale down colors if desaturated and add the brightness_floor to r, g, and b.
  if (sat != 255) {
    if (sat == 0) {
      r = 255;
      g = 255;
      b = 255;
    } else {
      //we know sat is < 255 and > 1, lets use that: scale8video is always +1, so drop the conditional
      uint32_t desat = 255 - sat;
      desat = (desat * desat); // scale8_video(desat, desat) but more accurate, dropped the "+1" for speed: visual difference is negligible
      uint8_t brightness_floor = desat >> 8;
      uint32_t satscale = 0xFFFF - desat;
      if (r) r = ((r * satscale) >> 16);
      if (g) g = ((g * satscale) >> 16);
      if (b) b = ((b * satscale) >> 16);

      r += brightness_floor;
      g += brightness_floor;
      b += brightness_floor;
    }
  }

  // scale everything down if value < 255.
  if (val != 255) {
    if (val == 0) {
      r = 0;
      g = 0;
      b = 0;
    } else {
       val = val*val + 512; // = scale8_video(val,val)+2;
      if (r) r = ((r * val) >> 16) + 1;
      if (g) g = ((g * val) >> 16) + 1;
      if (b) b = ((b * val) >> 16) + 1;
    }
  }
  if(isRGBW) {
    rgbdata[0] = b;
    rgbdata[1] = g;
    rgbdata[2] = r;
    //rgbdata[3] = 0; // white
  } else {
    rgbdata[0] = r;
    rgbdata[1] = g;
    rgbdata[2] = b;
  }
}

int16_t sin16_t(uint16_t theta) {
  int scale = 1;
  if (theta > 0x7FFF) {
    theta = 0xFFFF - theta;
    scale = -1; // second half of the sine function is negative (pi - 2*pi)
  }
  uint32_t precal = theta * (0x7FFF - theta);
  uint64_t numerator = (uint64_t)precal * (4 * 0x7FFF); // 64bit required
  int32_t denominator = 1342095361 - precal; // 1342095361 is 5 * 0x7FFF^2 / 4
  int16_t result = numerator / denominator;
  return result * scale;
}

uint8_t ease8InOutCubic(uint8_t i) {
  uint32_t ii = ((uint32_t)i * i);
  uint32_t factor = (3 << 8) - (((uint32_t)i << 1)); // 3 - 2i
  return (ii * factor) >> 16;
}

uint8_t triwave8(uint8_t in) {
  if (in & 0x80) in = 255 - in;
  return in << 1;
}

uint8_t cubicwave8(uint8_t in) {
  return ease8InOutCubic(triwave8(in));
}

uint32_t WLED_O2_ATTR IRAM_ATTR color_blend(uint32_t color1, uint32_t color2, uint8_t blend) {
  // min / max blend checking is omitted: calls with 0 or 255 are rare, checking lowers overall performance
  const uint32_t TWO_CHANNEL_MASK = 0x00FF00FF;     // mask for R and B channels or W and G if negated (poorman's SIMD; https://github.com/wled/WLED/pull/4568#discussion_r1986587221)
  uint32_t rb1 =  color1       & TWO_CHANNEL_MASK;  // extract R & B channels from color1
  uint32_t wg1 = (color1 >> 8) & TWO_CHANNEL_MASK;  // extract W & G channels from color1 (shifted for multiplication later)
  uint32_t rb2 =  color2       & TWO_CHANNEL_MASK;  // extract R & B channels from color2
  uint32_t wg2 = (color2 >> 8) & TWO_CHANNEL_MASK;  // extract W & G channels from color2 (shifted for multiplication later)
  uint32_t rb3 = ((((rb1 << 8) | rb2) + (rb2 * blend) - (rb1 * blend)) >> 8) &  TWO_CHANNEL_MASK; // blend red and blue
  uint32_t wg3 = ((((wg1 << 8) | wg2) + (wg2 * blend) - (wg1 * blend)))      & ~TWO_CHANNEL_MASK; // negated mask for white and green
  return rb3 | wg3;
}

// END UPSTREAM math/color functions.
// Only this minimal adapter code is OpenTail-specific.
struct PinnedSeg {
    uint8_t speed=0, intensity=0, palette=0;
    uint32_t colors[3]={0,0,0};
    std::vector<uint32_t> pixels;
    explicit PinnedSeg(size_t count):pixels(count,0u){}
    uint32_t color_wheel(uint8_t pos) const {
        uint8_t bgrw[4]={0,0,0,0};
        hsv2rgb_rainbow((uint16_t)pos<<8,255,255,bgrw,true);
        return ((uint32_t)bgrw[2]<<16)|((uint32_t)bgrw[1]<<8)|bgrw[0];
    }
    uint32_t color_from_palette(uint16_t,bool,bool,uint8_t slot) const {
        if(palette!=0 || slot>2) {std::fprintf(stderr,"Unsupported reference palette/slot\n");std::abort();}
        return colors[slot];
    }
    void fill(uint32_t color) {std::fill(pixels.begin(),pixels.end(),color);}
    void setPixelColor(size_t i,uint32_t color) {
        if(i<pixels.size()) pixels[i]=color;
    }
};
struct PinnedEnv {
    uint32_t call=0,step=0;
    uint16_t aux0=0,aux1=0;
};
struct PinnedClock {uint32_t now=0;};
static PinnedSeg* current_segment=nullptr;
static PinnedEnv* current_environment=nullptr;
static PinnedClock strip;
#define SEGMENT (*current_segment)
#define SEGENV (*current_environment)
#define SEGLEN (static_cast<unsigned>(current_segment->pixels.size()))
#define SEGCOLOR(i) (current_segment->colors[(i)])

// BEGIN UPSTREAM FX.cpp functions, unchanged source bodies.
void mode_breath(void) {
  unsigned var = 0;
  unsigned counter = (strip.now * ((SEGMENT.speed >> 3) +10)) & 0xFFFFU;
  counter = (counter >> 2) + (counter >> 4); //0-16384 + 0-2048
  if (counter < 16384) {
    if (counter > 8192) counter = 8192 - (counter - 8192);
    var = sin16_t(counter) / 103; //close to parabolic in range 0-8192, max val. 23170
  }

  uint8_t lum = 30 + var;
  for (unsigned i = 0; i < SEGLEN; i++) {
    SEGMENT.setPixelColor(i, color_blend(SEGCOLOR(1), SEGMENT.color_from_palette(i, true, PALETTE_SOLID_WRAP, 0), lum));
  }

}

void mode_rainbow(void) {
  unsigned counter = (strip.now * ((SEGMENT.speed >> 2) +2)) & 0xFFFF;
  counter = counter >> 8;

  if (SEGMENT.intensity < 128){
    SEGMENT.fill(color_blend(SEGMENT.color_wheel(counter),WHITE,uint8_t(128-SEGMENT.intensity)));
  } else {
    SEGMENT.fill(SEGMENT.color_wheel(counter));
  }
}

void mode_rainbow_cycle(void) {
  unsigned counter = (strip.now * ((SEGMENT.speed >> 2) +2)) & 0xFFFF;
  counter = counter >> 8;

  for (unsigned i = 0; i < SEGLEN; i++) {
    //intensity/29 = 0 (1/16) 1 (1/8) 2 (1/4) 3 (1/2) 4 (1) 5 (2) 6 (4) 7 (8) 8 (16)
    uint8_t index = (i * (16 << (SEGMENT.intensity /29)) / SEGLEN) + counter;
    SEGMENT.setPixelColor(i, SEGMENT.color_wheel(index));
  }
}

void mode_colorful(void) {
  unsigned numColors = 4; //3, 4, or 5
  uint32_t cols[9]{0x00FF0000,0x00EEBB00,0x0000EE00,0x000077CC};
  if (SEGMENT.intensity > 160 || SEGMENT.palette) { //palette or color
    if (!SEGMENT.palette) {
      numColors = 3;
      for (size_t i = 0; i < 3; i++) cols[i] = SEGCOLOR(i);
    } else {
      unsigned fac = 80;
      if (SEGMENT.palette == 52) {numColors = 5; fac = 61;} //C9 2 has 5 colors
      for (size_t i = 0; i < numColors; i++) {
        cols[i] = SEGMENT.color_from_palette(i*fac, false, true, 255);
      }
    }
  } else if (SEGMENT.intensity < 80) //pastel (easter) colors
  {
    cols[0] = 0x00FF8040;
    cols[1] = 0x00E5D241;
    cols[2] = 0x0077FF77;
    cols[3] = 0x0077F0F0;
  }
  for (size_t i = numColors; i < numColors*2 -1U; i++) cols[i] = cols[i-numColors];

  uint32_t cycleTime = 50 + (8 * (uint32_t)(255 - SEGMENT.speed));
  uint32_t it = strip.now / cycleTime;
  if (it != SEGENV.step)
  {
    if (SEGMENT.speed > 0) SEGENV.aux0++;
    if (SEGENV.aux0 >= numColors) SEGENV.aux0 = 0;
    SEGENV.step = it;
  }

  for (unsigned i = 0; i < SEGLEN; i+= numColors)
  {
    for (unsigned j = 0; j < numColors; j++) SEGMENT.setPixelColor(i + j, cols[SEGENV.aux0 + j]);
  }
}

void gradient_base(bool loading) {
  if (SEGLEN <= 1) FX_FALLBACK_STATIC;
  uint16_t counter = strip.now * ((SEGMENT.speed >> 2) + 1);
  uint16_t pp = (counter * SEGLEN) >> 16;
  if (SEGENV.call == 0) pp = 0;
  int val; //0 = sec 1 = pri
  int brd = 1 + loading ? SEGMENT.intensity/2 : SEGMENT.intensity/4;
  //if (brd < 1) brd = 1;
  int p1 = pp-SEGLEN;
  int p2 = pp+SEGLEN;

  for (int i = 0; i < (int)SEGLEN; i++) {
    if (loading) {
      val = abs(((i>pp) ? p2:pp) - i);
    } else {
      val = min(abs(pp-i),min(abs(p1-i),abs(p2-i)));
    }
    val = (brd > val) ? (val * 255) / brd : 255;
    SEGMENT.setPixelColor(i, color_blend(SEGCOLOR(0), SEGMENT.color_from_palette(i, true, PALETTE_SOLID_WRAP, 1), uint8_t(val)));
  }
}

void mode_gradient(void) {
  gradient_base(false);
}

void mode_loading(void) {
  gradient_base(true);
}

void mode_tricolor_wipe(void) {
  uint32_t cycleTime = 1000 + (255 - SEGMENT.speed)*200;
  uint32_t perc = strip.now % cycleTime;
  unsigned prog = (perc * 65535) / cycleTime;
  unsigned ledIndex = (prog * SEGLEN * 3) >> 16;
  unsigned ledOffset = ledIndex;

  for (unsigned i = 0; i < SEGLEN; i++)
  {
    SEGMENT.setPixelColor(i, SEGMENT.color_from_palette(i, true, PALETTE_SOLID_WRAP, 2));
  }

  if(ledIndex < SEGLEN) { //wipe from 0 to 1
    for (unsigned i = 0; i < SEGLEN; i++)
    {
      SEGMENT.setPixelColor(i, (i > ledOffset)? SEGCOLOR(0) : SEGCOLOR(1));
    }
  } else if (ledIndex < SEGLEN*2) { //wipe from 1 to 2
    ledOffset = ledIndex - SEGLEN;
    for (unsigned i = ledOffset +1; i < SEGLEN; i++)
    {
      SEGMENT.setPixelColor(i, SEGCOLOR(1));
    }
  } else //wipe from 2 to 0
  {
    ledOffset = ledIndex - SEGLEN*2;
    for (unsigned i = 0; i <= ledOffset; i++)
    {
      SEGMENT.setPixelColor(i, SEGCOLOR(0));
    }
  }
}

void mode_tricolor_fade(void) {
  uint16_t counter = strip.now * ((SEGMENT.speed >> 3) +1);
  uint32_t prog = (counter * 768) >> 16;

  uint32_t color1 = 0, color2 = 0;
  unsigned stage = 0;

  if(prog < 256) {
    color1 = SEGCOLOR(0);
    color2 = SEGCOLOR(1);
    stage = 0;
  } else if(prog < 512) {
    color1 = SEGCOLOR(1);
    color2 = SEGCOLOR(2);
    stage = 1;
  } else {
    color1 = SEGCOLOR(2);
    color2 = SEGCOLOR(0);
    stage = 2;
  }

  byte stp = prog; // % 256
  for (unsigned i = 0; i < SEGLEN; i++) {
    uint32_t color;
    if (stage == 2) {
      color = color_blend(SEGMENT.color_from_palette(i, true, PALETTE_SOLID_WRAP, 2), color2, stp);
    } else if (stage == 1) {
      color = color_blend(color1, SEGMENT.color_from_palette(i, true, PALETTE_SOLID_WRAP, 2), stp);
    } else {
      color = color_blend(color1, color2, stp);
    }
    SEGMENT.setPixelColor(i, color);
  }
}

void mode_tri_static_pattern()
{
  unsigned segSize = (SEGMENT.intensity >> 5) +1;
  unsigned currSeg = 0;
  unsigned currSegCount = 0;

  for (unsigned i = 0; i < SEGLEN; i++) {
    if ( currSeg % 3 == 0 ) {
      SEGMENT.setPixelColor(i, SEGCOLOR(0));
    } else if( currSeg % 3 == 1) {
      SEGMENT.setPixelColor(i, SEGCOLOR(1));
    } else {
      SEGMENT.setPixelColor(i, SEGCOLOR(2));
    }
    currSegCount += 1;
    if (currSegCount >= segSize) {
      currSeg +=1;
      currSegCount = 0;
    }
  }
}

void mode_sinewave(void) {             // Adjustable sinewave. By Andrew Tuline
  //#define qsuba(x, b)  ((x>b)?x-b:0)               // Analog Unsigned subtraction macro. if result <0, then => 0

  unsigned colorIndex = strip.now /32;//(256 - SEGMENT.fft1);  // Amount of colour change.

  SEGENV.step += SEGMENT.speed/16;                   // Speed of animation.
  unsigned freq = SEGMENT.intensity/4;//SEGMENT.fft2/8;                       // Frequency of the signal.

  for (unsigned i = 0; i < SEGLEN; i++) {                 // For each of the LED's in the strand, set a brightness based on a wave as follows:
    uint8_t pixBri = cubicwave8((i*freq)+SEGENV.step);//qsuba(cubicwave8((i*freq)+SEGENV.step), (255-SEGMENT.intensity)); // qsub sets a minimum value called thiscutoff. If < thiscutoff, then bright = 0. Otherwise, bright = 128 (as defined in qsub)..
    //setPixCol(i, i*colorIndex/255, pixBri);
    SEGMENT.setPixelColor(i, color_blend(SEGCOLOR(1), SEGMENT.color_from_palette(i*colorIndex/255, false, PALETTE_SOLID_WRAP, 0), pixBri));
  }
}

// END UPSTREAM FX.cpp functions.
#undef SEGMENT
#undef SEGENV
#undef SEGLEN
#undef SEGCOLOR

struct RefCase {uint16_t id;void (*effect)();const char* name;};
static const RefCase refs[]={
    {2,mode_breath,"Breathe"},
    {8,mode_rainbow,"Colorloop"},
    {9,mode_rainbow_cycle,"Rainbow"},
    {34,mode_colorful,"Colorful"},
    {46,mode_gradient,"Gradient"},
    {47,mode_loading,"Loading"},
    {55,mode_tricolor_wipe,"Tri Wipe"},
    {56,mode_tricolor_fade,"Tri Fade"},
    {84,mode_tri_static_pattern,"Solid Pattern Tri"},
    {108,mode_sinewave,"Sine"}
};
static uint32_t pack(ot_wled_rgb_t x) {
    return ((uint32_t)x.r<<16)|((uint32_t)x.g<<8)|x.b;
}
int main() {
    unsigned checked=0;
    for(const RefCase& fx:refs) {
        for(unsigned count: {2u,3u,7u,16u,31u}) {
            for(unsigned speed:{0u,16u,127u,255u}) {
                for(unsigned intensity:{0u,64u,128u,200u,255u}) {
                    PinnedSeg seg(count);
                    PinnedEnv env;
                    seg.speed=(uint8_t)speed;seg.intensity=(uint8_t)intensity;
                    seg.colors[0]=0x007B3809u; // configured primary 123,56,9
                    seg.colors[1]=0x000358AEu; // secondary 3,88,174
                    seg.colors[2]=0u;         // missing CORE tertiary stays black
                    current_segment=&seg;current_environment=&env;
                    ot_wled_fx_config_t cfg{};
                    cfg.effect_id=fx.id;cfg.speed=seg.speed;
                    cfg.intensity=seg.intensity;cfg.brightness=255;
                    cfg.frame_time_ms=23;cfg.palette_id=0;
                    cfg.foreground={123,56,9};cfg.background={3,88,174};
                    ot_wled_fx_state_t state{};
                    std::vector<ot_wled_rgb_t> actual(count);
                    for(uint32_t t:{0u,17u,171u,389u,1500u,65530u}) {
                        strip.now=t;
                        fx.effect();
                        env.call++;  // upstream effect engine increments after frame
                        if(!ot_wled_fx_render(&cfg,&state,t,actual.data(),count)) {
                            std::fprintf(stderr,"CORE rejected pinned FX %u (%s)\n",fx.id,fx.name);
                            return 1;
                        }
                        for(size_t i=0;i<count;i++) {
                            if(pack(actual[i])!=(seg.pixels[i]&0xffffffu)) {
                                std::fprintf(stderr,
                                  "WLED 1:1 mismatch id=%u %s len=%u speed=%u intensity=%u ms=%u px=%zu upstream=%06X CORE=%06X\n",
                                  fx.id,fx.name,count,speed,intensity,t,i,
                                  (unsigned)(seg.pixels[i]&0xffffffu),(unsigned)pack(actual[i]));
                                return 1;
                            }
                            checked++;
                        }
                    }
                }
            }
        }
    }
    std::printf("WLED pinned-source golden-frame comparison passed: %u RGB samples, %zu distinct modes (palette=0, RGB, no gamma)\n",
                checked,sizeof(refs)/sizeof(refs[0]));
    return 0;
}
