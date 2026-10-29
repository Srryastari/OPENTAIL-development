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
#define BLACK 0u
#define PALETTE_SOLID_WRAP true
#define FRAMETIME 23u
static long map(long x,long a,long b,long c,long d) {return (x-a)*(d-c)/(b-a)+c;}
// Tests use lengths >=2; upstream's 1-pixel mode_static fallback is excluded.
#define FX_FALLBACK_STATIC { return; }
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

uint8_t sin8_t(uint8_t theta) {
  int32_t sin16 = sin16_t((uint16_t)theta * 257); // 255 * 257 = 0xFFFF
  sin16 += 0x7FFF + 128; //shift result to range 0-0xFFFF, +128 for rounding
  return min(sin16, int32_t(0xFFFF)) >> 8; // min performs saturation, and prevents overflow
}
static uint8_t sin_gap(uint16_t in) {
  if (in & 0x100) return 0;
  return sin8_t(in + 192); // correct phase shift of sine so that it starts and stops at 0
}

uint16_t triwave16(uint16_t in) {
  if (in < 0x8000) return in *2;
  return 0xFFFF - (in - 0x8000)*2;
}

/* Pinned upstream color_fade() declares a white-channel scratch byte that
 * is unused in the RGB-only reference; keep its source body unmodified. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
uint32_t IRAM_ATTR color_fade(uint32_t c1, uint8_t amount, bool video) {
  if (c1 == BLACK) return 0;           // fast path early exit
  if (amount == 255) return c1;        // no change
  else if (amount == 0) return BLACK;  // full fade (rarely used)
  const uint32_t TWO_CHANNEL_MASK = 0x00FF00FF;
  uint32_t rb = c1 & TWO_CHANNEL_MASK; // extract R and B channels
  uint32_t wg = (c1 >> 8) & TWO_CHANNEL_MASK; // extract W and G channels (shifted for multiplication)

  // video scaling: colors do not dim to zero if they started non-zero unless they distort the hue
  if (video) {
    uint8_t r = byte(rb>>16), g = byte(wg), b = byte(rb), w = byte(wg>>16); // extract r, g, b, w channels from original color (wg is shifted)
    uint8_t maxc = (r > g) ? r : g;           // determine dominant channel for hue preservation
    maxc = (maxc > b) ? maxc : b;
    uint32_t thr = (uint32_t)((maxc>>1) + 1); // 50%, +1 to avoid wash-out of dark colors
    thr |= (thr<<16); // threshold for both color lanes
    // check threshold: set the MSB of each channel and if the subtraction is "negative" (channel < threshold) the MSB gets cleared
    uint32_t domRB = (((rb | 0x80008000) - thr) & 0x80008000) >> 15; // after shifting, the LSB of each 16bit lane is set if the channel is dominant
    uint32_t domWG = (((wg | 0x80008000) - thr) & 0x80008000) >> 7;  // wg use the upper bytes so dont shift all the way down
    // scale and add dominant channel bit for color-biasing and rounding
    uint32_t rb_scaled = (((rb * amount) >> 8) & TWO_CHANNEL_MASK) + domRB;
    uint32_t wg_scaled = ((wg * amount) & ~TWO_CHANNEL_MASK) + domWG;
    return (rb_scaled | wg_scaled);
  }
  uint32_t rb_scaled = ((rb * (amount + 1)) >> 8) & TWO_CHANNEL_MASK; // scale red and blue
  uint32_t wg_scaled = ((wg * (amount + 1)) & ~TWO_CHANNEL_MASK);     // scale white and green
  return (rb_scaled | wg_scaled);
}

#pragma GCC diagnostic pop

// END UPSTREAM math/color functions.
// Only this minimal adapter code is OpenTail-specific.
// Small RGB structs replicate upstream class conversion (gamma disabled).
struct CHSV {uint8_t h,s,v;CHSV(uint8_t a,uint8_t b,uint8_t c):h(a),s(b),v(c){}};
struct CRGBW {uint32_t color32=0u;};
struct CRGB : CRGBW {
    explicit CRGB(CHSV c) {
        uint8_t rgb[4]={0u,0u,0u,0u};
        hsv2rgb_rainbow((uint16_t)c.h<<8,c.s,c.v,rgb,false);
        color32=((uint32_t)rgb[0]<<16)|((uint32_t)rgb[1]<<8)|rgb[2];
    }
};
static uint32_t gamma32inv(uint32_t color) { return color; }

struct PinnedSeg {
    uint8_t speed=0, intensity=0, palette=0;
    bool check2=false;
    uint32_t colors[3]={0,0,0};
    std::vector<uint32_t> pixels;
    explicit PinnedSeg(size_t count):pixels(count,0u){}
    uint32_t color_wheel(uint8_t pos) const {
        uint8_t bgrw[4]={0,0,0,0};
        hsv2rgb_rainbow((uint16_t)pos<<8,255,255,bgrw,true);
        return ((uint32_t)bgrw[2]<<16)|((uint32_t)bgrw[1]<<8)|bgrw[0];
    }
    uint32_t color_from_palette(uint16_t,bool,bool,uint8_t slot,uint8_t pbri=255u) const {
        if(palette!=0 || slot>2) {std::fprintf(stderr,"Unsupported reference palette/slot\n");std::abort();}
        return color_fade(colors[slot],pbri,true);
    }
    void blendPixelColor(size_t i,CRGBW color,uint8_t b) {
        if(i<pixels.size()) pixels[i]=color_blend(pixels[i],color.color32,b);
    }
    void blendPixelColor(size_t i,uint32_t color,uint8_t b) {
        if(i<pixels.size()) pixels[i]=color_blend(pixels[i],color,b);
    }
    void fill(uint32_t color) {std::fill(pixels.begin(),pixels.end(),color);}
    void setPixelColor(size_t i,uint32_t color) {
        if(i<pixels.size()) pixels[i]=color;
    }
};
struct PinnedEnv {
    uint32_t call=0,step=0;
    uint16_t aux0=0,aux1=0;
    uint32_t data_counter=0;       // upstream mode_android allocation
    void* data=&data_counter;
    bool allocateData(size_t bytes) {return bytes<=sizeof(data_counter);}
};
struct PinnedClock {uint32_t now=0;};
static PinnedSeg* current_segment=nullptr;
static PinnedEnv* current_environment=nullptr;
static PinnedClock strip;
#define millis() strip.now
static inline uint16_t scale16(uint16_t i,uint16_t scale) {
    return (uint16_t)(((uint32_t)i*(1u+(uint32_t)scale))>>16);
}
uint16_t beat88(uint16_t,uint32_t=0u);
uint16_t beatsin88_t(uint16_t,uint16_t,uint16_t,uint32_t=0u,uint16_t=0u);
// BEGIN PINNED upstream util.cpp beat timing. No synthesized golden values.
uint16_t beat88(uint16_t beats_per_minute_88, uint32_t timebase) {
  return ((millis() - timebase) * beats_per_minute_88 * 280) >> 16;
}

uint16_t beatsin88_t(uint16_t beats_per_minute_88, uint16_t lowest, uint16_t highest, uint32_t timebase, uint16_t phase_offset) {
  uint16_t beat = beat88(beats_per_minute_88, timebase);
  uint16_t beatsin = (sin16_t( beat + phase_offset) + 32768);
  uint16_t rangewidth = highest - lowest;
  uint16_t scaledbeat = scale16(beatsin, rangewidth);
  uint16_t result = lowest + scaledbeat;
  return result;
}
// END PINNED beat timing.
#undef millis

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

void blink(uint32_t color1, uint32_t color2, bool strobe, bool do_palette) {
  uint32_t cycleTime = (255 - SEGMENT.speed)*20;
  uint32_t onTime = FRAMETIME;
  if (!strobe) onTime += ((cycleTime * SEGMENT.intensity) >> 8);
  cycleTime += FRAMETIME*2;
  uint32_t it = strip.now / cycleTime;
  uint32_t rem = strip.now % cycleTime;

  bool on = false;
  if (it != SEGENV.step //new iteration, force on state for one frame, even if set time is too brief
      || rem <= onTime) {
    on = true;
  }

  SEGENV.step = it; //save previous iteration

  uint32_t color = on ? color1 : color2;
  if (color == color1 && do_palette)
  {
    for (unsigned i = 0; i < SEGLEN; i++) {
      SEGMENT.setPixelColor(i, SEGMENT.color_from_palette(i, true, PALETTE_SOLID_WRAP, 0));
    }
  } else SEGMENT.fill(color);
}

void mode_blink(void) {
  blink(SEGCOLOR(0), SEGCOLOR(1), false, true);
}

void mode_blink_rainbow(void) {
  blink(SEGMENT.color_wheel(SEGENV.call & 0xFF), SEGCOLOR(1), false, false);
}

void mode_strobe(void) {
  return blink(SEGCOLOR(0), SEGCOLOR(1), true, true);
}

void mode_strobe_rainbow(void) {
  return blink(SEGMENT.color_wheel(SEGENV.call & 0xFF), SEGCOLOR(1), true, false);
}

static void running(uint32_t color1, uint32_t color2, bool theatre = false) {
  int width = (theatre ? 3 : 1) + (SEGMENT.intensity >> 4);  // window
  uint32_t cycleTime = 50 + (255 - SEGMENT.speed);
  uint32_t it = strip.now / cycleTime;
  bool usePalette = color1 == SEGCOLOR(0);

  for (unsigned i = 0; i < SEGLEN; i++) {
    uint32_t col = color2;
    if (usePalette) color1 = SEGMENT.color_from_palette(i, true, PALETTE_SOLID_WRAP, 0);
    if (theatre) {
      if ((i % width) == SEGENV.aux0) col = color1;
    } else {
      int pos = (i % (width<<1));
      if ((pos < SEGENV.aux0-width) || ((pos >= SEGENV.aux0) && (pos < SEGENV.aux0+width))) col = color1;
    }
    SEGMENT.setPixelColor(i,col);
  }

  if (it != SEGENV.step) {
    SEGENV.aux0 = (SEGENV.aux0 +1) % (theatre ? width : (width<<1));
    SEGENV.step = it;
  }
}

void mode_theater_chase(void) {
  running(SEGCOLOR(0), SEGCOLOR(1), true);
}

void mode_theater_chase_rainbow(void) {
  running(SEGMENT.color_wheel(SEGENV.step), SEGCOLOR(1), true);
}

static void running_base(bool saw, bool dual=false) {
  unsigned x_scale = SEGMENT.intensity >> 2;
  uint32_t counter = (strip.now * SEGMENT.speed) >> 9;

  for (unsigned i = 0; i < SEGLEN; i++) {
    unsigned a = i*x_scale - counter;
    if (saw) {
      a &= 0xFF;
      if (a < 16)
      {
        a = 192 + a*8;
      } else {
        a = map(a,16,255,64,192);
      }
      a = 255 - a;
    }
    uint8_t s = dual ? sin_gap(a) : sin8_t(a);
    uint32_t ca = color_blend(SEGCOLOR(1), SEGMENT.color_from_palette(i, true, PALETTE_SOLID_WRAP, 0), s);
    if (dual) {
      unsigned b = (SEGLEN-1-i)*x_scale - counter;
      uint8_t t = sin_gap(b);
      uint32_t cb = color_blend(SEGCOLOR(1), SEGMENT.color_from_palette(i, true, PALETTE_SOLID_WRAP, 2), t);
      ca = color_blend(ca, cb, uint8_t(127));
    }
    SEGMENT.setPixelColor(i, ca);
  }
}

void mode_running_dual(void) {
  running_base(false, true);
}

void mode_running_lights(void) {
  running_base(false);
}

void mode_saw(void) {
  running_base(true);
}

void mode_running_color(void) {
  running(SEGCOLOR(0), SEGCOLOR(1));
}

void mode_static_pattern()
{
  unsigned lit = 1 + SEGMENT.speed;
  unsigned unlit = 1 + SEGMENT.intensity;
  bool drawingLit = true;
  unsigned cnt = 0;

  for (unsigned i = 0; i < SEGLEN; i++) {
    SEGMENT.setPixelColor(i, (drawingLit) ? SEGMENT.color_from_palette(i, true, PALETTE_SOLID_WRAP, 0) : SEGCOLOR(1));
    cnt++;
    if (cnt >= ((drawingLit) ? lit : unlit)) {
      cnt = 0;
      drawingLit = !drawingLit;
    }
  }
}

void scan(bool dual) {
  if (SEGLEN <= 1) FX_FALLBACK_STATIC;
  uint32_t cycleTime = 750 + (255 - SEGMENT.speed)*150;
  uint32_t perc = strip.now % cycleTime;
  int prog = (perc * 65535) / cycleTime;
  int size = 1 + ((SEGMENT.intensity * SEGLEN) >> 9);
  int ledIndex = (prog * ((SEGLEN *2) - size *2)) >> 16;

  if (!SEGMENT.check2) SEGMENT.fill(SEGCOLOR(1));

  int led_offset = ledIndex - (SEGLEN - size);
  led_offset = abs(led_offset);

  if (dual) {
    for (int j = led_offset; j < led_offset + size; j++) {
      unsigned i2 = SEGLEN -1 -j;
      SEGMENT.setPixelColor(i2, SEGMENT.color_from_palette(i2, true, PALETTE_SOLID_WRAP, (SEGCOLOR(2))? 2:0));
    }
  }

  for (int j = led_offset; j < led_offset + size; j++) {
    SEGMENT.setPixelColor(j, SEGMENT.color_from_palette(j, true, PALETTE_SOLID_WRAP, 0));
  }
}

void mode_scan(void) {
  scan(false);
}

void mode_dual_scan(void) {
  scan(true);
}

static void spots_base(uint16_t threshold)
{
  if (SEGLEN <= 1) FX_FALLBACK_STATIC;
  if (!SEGMENT.check2) SEGMENT.fill(SEGCOLOR(1));

  unsigned maxZones = SEGLEN >> 2;
  unsigned zones = 1 + ((SEGMENT.intensity * maxZones) >> 8);
  unsigned zoneLen = SEGLEN / zones;
  unsigned offset = (SEGLEN - zones * zoneLen) >> 1;

  for (unsigned z = 0; z < zones; z++)
  {
    unsigned pos = offset + z * zoneLen;
    for (unsigned i = 0; i < zoneLen; i++)
    {
      unsigned wave = triwave16((i * 0xFFFF) / zoneLen);
      if (wave > threshold) {
        unsigned index = 0 + pos + i;
        unsigned s = (wave - threshold)*255 / (0xFFFF - threshold);
        SEGMENT.setPixelColor(index, color_blend(SEGMENT.color_from_palette(index, true, PALETTE_SOLID_WRAP, 0), SEGCOLOR(1), uint8_t(255-s)));
      }
    }
  }
}

void mode_spots()
{
  spots_base((255 - SEGMENT.speed) << 8);
}

void mode_spots_fade()
{
  unsigned counter = strip.now * ((SEGMENT.speed >> 2) +8);
  unsigned t = triwave16(counter);
  unsigned tr = (t >> 1) + (t >> 2);
  spots_base(tr);
}

void mode_heartbeat(void) {
  unsigned bpm = 40 + (SEGMENT.speed >> 3);
  uint32_t msPerBeat = (60000L / bpm);
  uint32_t secondBeat = (msPerBeat / 3);
  uint32_t bri_lower = SEGENV.aux1;
  unsigned long beatTimer = strip.now - SEGENV.step;

  bri_lower = bri_lower * 2042 / (2048 + SEGMENT.intensity);
  SEGENV.aux1 = bri_lower;

  if ((beatTimer > secondBeat) && !SEGENV.aux0) { // time for the second beat?
    SEGENV.aux1 = UINT16_MAX; //3/4 bri
    SEGENV.aux0 = 1;
  }
  if (beatTimer > msPerBeat) { // time to reset the beat timer?
    SEGENV.aux1 = UINT16_MAX; //full bri
    SEGENV.aux0 = 0;
    SEGENV.step = strip.now;
  }

  for (unsigned i = 0; i < SEGLEN; i++) {
    SEGMENT.setPixelColor(i, color_blend(SEGMENT.color_from_palette(i, true, PALETTE_SOLID_WRAP, 0), SEGCOLOR(1), uint8_t(255 - (SEGENV.aux1 >> 8))));
  }
}

void mode_android(void) {
  if (!SEGENV.allocateData(sizeof(uint32_t))) FX_FALLBACK_STATIC;
  uint32_t* counter = reinterpret_cast<uint32_t*>(SEGENV.data);
  unsigned size = SEGENV.aux1 >> 1; // upper 15 bit
  unsigned shrinking = SEGENV.aux1 & 0x01; // lowest bit
  if(strip.now >= SEGENV.step) {
    SEGENV.step = strip.now + 3 + ((8 * (uint32_t)(255 - SEGMENT.speed)) / SEGLEN);
    if (size > (SEGMENT.intensity * SEGLEN) / 255)
      shrinking = 1;
    else if (size < 2)
      shrinking = 0;
    if (!shrinking) { // growing
      if ((*counter % 3) == 1)
        SEGENV.aux0++; // advance start position
      else
        size++;
    } else { // shrinking
      SEGENV.aux0++;
      if ((*counter % 3) != 1)
        size--;
    }
    SEGENV.aux1 = size << 1 | shrinking; // save back
    (*counter)++;
    if (SEGENV.aux0 >= SEGLEN) SEGENV.aux0 = 0;
  }
  uint32_t start = SEGENV.aux0;
  uint32_t end = (SEGENV.aux0 + size) % SEGLEN;
  for (unsigned i = 0; i < SEGLEN; i++) {
    if ((start < end && i >= start && i < end) || (start >= end && (i >= start || i < end)))
      SEGMENT.setPixelColor(i, SEGCOLOR(0));
    else
      SEGMENT.setPixelColor(i, SEGMENT.color_from_palette(i, true, PALETTE_SOLID_WRAP, 1));
  }
}

// BEGIN UPSTREAM 1D Perlin source, util.cpp, pinned commit.
// No alternate or approximate Perlin implementation is used.
#define PERLIN_SHIFT 1
int32_t hashToGradient(uint32_t h) {
  // using more steps yields more "detailed" perlin noise but looks less like the original fastled version (adjust PERLIN_SHIFT to compensate, also changes range and needs proper adustment)
  // return (h & 0xFF) - 128; // use PERLIN_SHIFT 7
  // return (h & 0x0F) - 8; // use PERLIN_SHIFT 3
  // return (h & 0x07) - 4; // use PERLIN_SHIFT 2
  return (h & 0x03) - 2; // use PERLIN_SHIFT 1 -> closest to original fastled version
}

int32_t gradient1D(uint32_t x0, int32_t dx) {
  uint32_t h = x0 * 0x27D4EB2D;
  h ^= h >> 15;
  h *= 0x92C3412B;
  h ^= h >> 13;
  h ^= h >> 7;
  return (hashToGradient(h) * dx) >> PERLIN_SHIFT;
}

uint32_t smoothstep(const uint32_t t) {
  uint32_t t_squared = (t * t) >> 16;
  uint32_t factor = (3 << 16) - ((t << 1));
  return (t_squared * factor) >> 18; // scale to avoid overflows and give best resolution
}

int32_t lerpPerlin(int32_t a, int32_t b, int32_t t) {
    return a + (((b - a) * t) >> 14); // match scaling with smoothstep to yield 16.16bit values
}

int32_t perlin1D_raw(uint32_t x, bool is16bit) {
  // integer and fractional part coordinates
  int32_t x0 = x >> 16;
  int32_t x1 = x0 + 1;
  if(is16bit) x1 = x1 & 0xFF; // wrap back to zero at 0xFF instead of 0xFFFF

  int32_t dx0 = x & 0xFFFF;
  int32_t dx1 = dx0 - 0x10000;
  // gradient values for the two corners
  int32_t g0 = gradient1D(x0, dx0);
  int32_t g1 = gradient1D(x1, dx1);
  // interpolate and smooth function
  int32_t tx = smoothstep(dx0);
  int32_t noise = lerpPerlin(g0, g1, tx);
  return noise;
}

uint8_t perlin8(uint16_t x) {
  return (((perlin1D_raw((uint32_t)x << 8, true) * 1353) >> 10) + 32769) >> 8; //scale to 16 bit, offset, then scale to 8bit
}
#undef PERLIN_SHIFT
// END UPSTREAM 1D Perlin source.
static void phased_base(uint8_t moder) {                  // We're making sine waves here. By Andrew Tuline.

  unsigned allfreq = 16;                                          // Base frequency.
  float *phase = reinterpret_cast<float*>(&SEGENV.step);         // Phase change value gets calculated (float fits into unsigned long).
  unsigned cutOff = (255-SEGMENT.intensity);                      // You can change the number of pixels.  AKA INTENSITY (was 192).
  unsigned modVal = 5;//SEGMENT.fft1/8+1;                         // You can change the modulus. AKA FFT1 (was 5).

  unsigned index = strip.now/64;                                  // Set color rotation speed
  *phase += SEGMENT.speed/32.0;                                  // You can change the speed of the wave. AKA SPEED (was .4)

  for (unsigned i = 0; i < SEGLEN; i++) {
    if (moder == 1) modVal = (perlin8(i*10 + i*10) /16);         // Let's randomize our mod length with some Perlin noise.
    unsigned val = (i+1) * allfreq;                              // This sets the frequency of the waves. The +1 makes sure that led 0 is used.
    if (modVal == 0) modVal = 1;
    val += *phase * (i % modVal +1) /2;                          // This sets the varying phase change of the waves. By Andrew Tuline.
    unsigned b = cubicwave8(val);                                 // Now we make an 8 bit sinewave.
    b = (b > cutOff) ? (b - cutOff) : 0;                         // A ternary operator to cutoff the light.
    SEGMENT.setPixelColor(i, color_blend(SEGCOLOR(1), SEGMENT.color_from_palette(index, false, false, 0), uint8_t(b)));
    index += 256 / SEGLEN;
    if (SEGLEN > 256) index ++;                                  // Correction for segments longer than 256 LEDs
  }
}

void mode_phased(void) {
  phased_base(0);
}

void mode_phased_noise(void) {
  phased_base(1);
}

void mode_colorwaves_pride_base(bool isPride2015) {
  unsigned duration = 10 + SEGMENT.speed;
  unsigned sPseudotime = SEGENV.step;
  unsigned sHue16 = SEGENV.aux0;

  uint8_t sat8 = isPride2015 ? beatsin88_t(87, 220, 250) : 255;
  unsigned brightdepth = beatsin88_t(341, 96, 224);
  unsigned brightnessthetainc16 = beatsin88_t(203, (25 * 256), (40 * 256));
  unsigned msmultiplier = beatsin88_t(147, 23, 60);

  unsigned hue16 = sHue16;
  unsigned hueinc16 = isPride2015 ? beatsin88_t(113, 1, 3000) : 
                                     beatsin88_t(113, 60, 300) * SEGMENT.intensity * 10 / 255;

  sPseudotime += duration * msmultiplier;
  sHue16 += duration * beatsin88_t(400, 5, 9);
  unsigned brightnesstheta16 = sPseudotime;

  for (unsigned i = 0; i < SEGLEN; i++) {
    hue16 += hueinc16;
    uint8_t hue8;

    if (isPride2015) {
      hue8 = hue16 >> 8;
    } else {
      unsigned h16_128 = hue16 >> 7;
      hue8 = (h16_128 & 0x100) ? (255 - (h16_128 >> 1)) : (h16_128 >> 1);
    }

    brightnesstheta16 += brightnessthetainc16;
    unsigned b16 = sin16_t(brightnesstheta16) + 32768;
    unsigned bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
    uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
    bri8 += (255 - brightdepth);

    if (isPride2015) {
      CRGBW newcolor = CRGB(CHSV(hue8, sat8, bri8));
      newcolor.color32 = gamma32inv(newcolor.color32);
      SEGMENT.blendPixelColor(i, newcolor, 64);
    } else {
      SEGMENT.blendPixelColor(i, SEGMENT.color_from_palette(hue8, false, PALETTE_SOLID_WRAP, 0, bri8), 128);
    }
  }

  SEGENV.step = sPseudotime;
  SEGENV.aux0 = sHue16;
}

void mode_pride_2015(void) {
  mode_colorwaves_pride_base(true);
}

void mode_colorwaves() {
  mode_colorwaves_pride_base(false);
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
    {108,mode_sinewave,"Sine"},
    {1,mode_blink,"Blink"},
    {23,mode_strobe,"Strobe"},
    {26,mode_blink_rainbow,"Blink Rainbow"},
    {24,mode_strobe_rainbow,"Strobe Rainbow"},
    {13,mode_theater_chase,"Theater"},
    {14,mode_theater_chase_rainbow,"Theater Rainbow"},
    {37,mode_running_color,"Chase 2"},
    {15,mode_running_lights,"Running"},
    {16,mode_saw,"Saw"},
    {52,mode_running_dual,"Running Dual"},
    {83,mode_static_pattern,"Solid Pattern"},
    {10,mode_scan,"Scan"},
    {11,mode_dual_scan,"Scan Dual"},
    {85,mode_spots,"Spots"},
    {86,mode_spots_fade,"Spots Fade"},
    {100,mode_heartbeat,"Heartbeat"},
    {27,mode_android,"Android"},
    {105,mode_phased,"Phased"},
    {109,mode_phased_noise,"Phased Noise"},
    {63,mode_pride_2015,"Pride 2015"},
    {67,mode_colorwaves,"Colorwaves"}
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
