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
#include <cmath>
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
using std::max;
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

// BEGIN PINNED colors.cpp color_add (verbatim body, host default parameter)
uint32_t WLED_O2_ATTR color_add(uint32_t c1, uint32_t c2, bool preserveCR=false)
{
  if (c1 == BLACK) return c2;
  if (c2 == BLACK) return c1;
  const uint32_t TWO_CHANNEL_MASK = 0x00FF00FF; // mask for R and B channels or W and G if negated
  uint32_t rb = ( c1     & TWO_CHANNEL_MASK) + ( c2     & TWO_CHANNEL_MASK); // mask and add two colors at once
  uint32_t wg = ((c1>>8) & TWO_CHANNEL_MASK) + ((c2>>8) & TWO_CHANNEL_MASK);

  if (preserveCR) { // preserve color ratios
    uint32_t overflow = (rb | wg) & 0x01000100; // detect overflow by checking 9th bit
    if (overflow) {
      uint32_t r = rb >> 16; // extract single color values
      uint32_t b = rb & 0xFFFF;
      uint32_t w = wg >> 16;
      uint32_t g = wg & 0xFFFF;
      uint32_t maxval = (r > g) ? ((r > b) ? r : b) : ((g > b) ? g : b); // find max value. note: faster than using max() function or on par
      maxval = (w > maxval) ? w : maxval; // check white channel as well to avoid division by zero in pure white input
      const uint32_t scale = (uint32_t(255)<<8) / maxval; // division of two 8bit (shifted) values does not work -> use bit shifts and multiplaction instead
      rb = ((rb * scale) >> 8) &  TWO_CHANNEL_MASK;
      wg =  (wg * scale)       & ~TWO_CHANNEL_MASK;
    } else wg <<= 8; //shift white and green back to correct position
  } else {
    // branchless per-channel saturation to 255 (extract 9th bit, subtract 1 if it is set, mask with 0xFF, input is 0xFF+0xFF=0x1EF max)
    // example with overflow: input: 0x01EF01EF -> (0x0100100 - 0x00010001) = 0x00FF00FF -> input|0x00FF00FF = 0x00FF00FF (saturate)
    // example without overflow: input: 0x007F007F -> (0x00000000 - 0x00000000) = 0x00000000 -> input|0x00000000 = input  (no change)
    rb |= ((rb & 0x01000100) - ((rb >> 8) & 0x00010001)) & 0x00FF00FF;
    wg |= ((wg & 0x01000100) - ((wg >> 8) & 0x00010001)) & 0x00FF00FF;
    wg <<= 8; // restore WG position
  }
  return rb | wg;
}


// END PINNED color_add

// END UPSTREAM math/color functions.
// Only this minimal adapter code is OpenTail-specific.
// Small RGB structs replicate upstream class conversion (gamma disabled).
struct CHSV {uint8_t h,s,v;CHSV(uint8_t a,uint8_t b,uint8_t c):h(a),s(b),v(c){}};
struct CHSV32 {uint8_t h,s,v;CHSV32(uint8_t a,uint8_t b,uint8_t c):h(a),s(b),v(c){}};
struct CRGBW {
    union {
        uint32_t color32;
        struct {uint8_t b,g,r,w;};
    };
    CRGBW():color32(0u){}
    CRGBW(uint8_t red,uint8_t green,uint8_t blue):color32(((uint32_t)red<<16)|((uint32_t)green<<8)|blue){}
    CRGBW(uint32_t col):color32(col){}
    unsigned getRGBaverage() const {return ((unsigned)r+(unsigned)g+(unsigned)b)/3u;}
    explicit CRGBW(CHSV32 c) {
        uint8_t rgb[4]={0,0,0,0};
        hsv2rgb_rainbow((uint16_t)c.h<<8,c.s,c.v,rgb,false);
        color32=((uint32_t)rgb[0]<<16)|((uint32_t)rgb[1]<<8)|rgb[2];
    }
    operator uint32_t() const {return color32;}
};
struct CRGB : CRGBW {
    CRGB()=default;
    CRGB(uint8_t red,uint8_t green,uint8_t blue) {r=red;g=green;b=blue;w=0u;}
    explicit CRGB(uint32_t color) { color32=color; }
    explicit CRGB(CHSV c) {
        uint8_t rgb[4]={0u,0u,0u,0u};
        hsv2rgb_rainbow((uint16_t)c.h<<8,c.s,c.v,rgb,false);
        color32=((uint32_t)rgb[0]<<16)|((uint32_t)rgb[1]<<8)|rgb[2];
    }
};
static uint32_t gamma32inv(uint32_t color) { return color; }


struct CRGBPalette16 {
  CRGB entries[16];
  CRGB& operator[](unsigned i){return entries[i];}
  const CRGB& operator[](unsigned i)const{return entries[i];}
  bool operator!=(const CRGBPalette16& other) const {
    for(unsigned i=0;i<16u;i++)if(entries[i].color32!=other.entries[i].color32)return true;
    return false;
  }
};
enum TBlendType { NOBLEND=0,LINEARBLEND=1,LINEARBLEND_NOWRAP=2 };
static uint32_t RGBW32(uint8_t r,uint8_t g,uint8_t b,uint8_t w);
// Original WLED/fastled_slim palette mathematics, verbatim.
void fill_gradient_RGB(CRGB* colors, uint32_t startpos, CRGB startcolor, uint32_t endpos, CRGB endcolor) {
  if(endpos < startpos) { // if the points are in the wrong order, flip them
    unsigned t = endpos;
    CRGB tc = endcolor;
    endcolor = startcolor;
    endpos = startpos;
    startpos = t;
    startcolor = tc;
  }
  int rdistance = endcolor.r - startcolor.r;
  int gdistance = endcolor.g - startcolor.g;
  int bdistance = endcolor.b - startcolor.b;

  int divisor = endpos - startpos;
  divisor = divisor == 0 ? 1 : divisor; // prevent division by zero

  int rdelta = (rdistance << 16) / divisor;
  int gdelta = (gdistance << 16) / divisor;
  int bdelta = (bdistance << 16) / divisor;

  int rshifted = startcolor.r << 16;
  int gshifted = startcolor.g << 16;
  int bshifted = startcolor.b << 16;

  for (unsigned i = startpos; i <= endpos; i++) {
    colors[i] = CRGB(rshifted >> 16, gshifted >> 16, bshifted >> 16);
    rshifted += rdelta;
    gshifted += gdelta;
    bshifted += bdelta;
  }
}

void fill_gradient_RGB(CRGB* colors, uint32_t num, const CRGB& c1, const CRGB& c2) {
  uint32_t last = num - 1;
  fill_gradient_RGB(colors, 0, c1, last, c2);
}

void fill_gradient_RGB(CRGB* colors, uint32_t num, const CRGB& c1, const CRGB& c2, const CRGB& c3) {
  uint32_t half = (num / 2);
  uint32_t last = num - 1;
  fill_gradient_RGB(colors,    0, c1, half, c2);
  fill_gradient_RGB(colors, half, c2, last, c3);
}

void fill_gradient_RGB(CRGB* colors, uint32_t num, const CRGB& c1, const CRGB& c2, const CRGB& c3, const CRGB& c4) {
  uint32_t onethird = (num / 3);
  uint32_t twothirds = ((num * 2) / 3);
  uint32_t last = num - 1;
  fill_gradient_RGB(colors,         0, c1,  onethird, c2);
  fill_gradient_RGB(colors,  onethird, c2, twothirds, c3);
  fill_gradient_RGB(colors, twothirds, c3,      last, c4);
}

uint32_t ColorFromPalette(const CRGBPalette16& pal, unsigned index, uint8_t brightness, TBlendType blendType) {
  if (blendType == LINEARBLEND_NOWRAP) {
    index = (index * 0xF0) >> 8; // Blend range is affected by lo4 blend of values, remap to avoid wrapping
  }
  unsigned hi4 = byte(index) >> 4;
  unsigned lo4 = (index & 0x0F);
  const CRGB* entry = (CRGB*)&(pal[0]) + hi4;
  unsigned red1   = entry->r;
  unsigned green1 = entry->g;
  unsigned blue1  = entry->b;
  if (lo4 && blendType != NOBLEND) {
    if (hi4 == 15) entry = &(pal[0]);
    else ++entry;
    unsigned f2 = (lo4 << 4);
    unsigned f1 = 256 - f2;
    red1   = (red1   * f1 + (unsigned)entry->r * f2) >> 8; // note: using color_blend() is slower
    green1 = (green1 * f1 + (unsigned)entry->g * f2) >> 8;
    blue1  = (blue1  * f1 + (unsigned)entry->b * f2) >> 8;
  }
  if (brightness < 255) { // note: zero checking could be done to return black but that is hardly ever used so it is omitted
    // actually same as color_fade(), using color_fade() is slower
    uint32_t scale = brightness + 1; // adjust for rounding (bitshift)
    red1   = (red1   * scale) >> 8;
    green1 = (green1 * scale) >> 8;
    blue1  = (blue1  * scale) >> 8;
  }
  return RGBW32(red1,green1,blue1,0);
}



/* Independent pinned test loader: WLED fastled_slim.h
 * CRGBPalette16::loadDynamicGradientPalette() algorithm. Original gradient
 * bytes follow gGradientPalettes[] order from pinned palettes.cpp. */
static const uint8_t upstream_gradient_bytes[]={
  0,181,0,0,22,218,85,0,51,255,170,0,85,211,85,77,135,167,0,169,198,73,0,188,
  255,0,0,207,0,24,69,44,101,73,105,70,165,129,140,97,242,200,204,166,255,200,204,166,
  0,16,48,51,89,27,166,175,153,197,233,255,255,0,145,152,0,41,14,99,31,128,24,74,
  63,227,34,50,95,132,31,76,127,47,29,102,159,109,47,101,191,176,66,100,223,129,57,104,
  255,84,48,108,0,222,191,8,255,117,52,1,0,38,0,255,63,86,0,255,127,139,0,255,
  191,196,0,117,255,255,0,0,0,186,63,255,127,227,9,85,175,234,205,213,221,205,38,176,
  255,205,38,176,0,61,135,184,36,129,188,169,87,203,241,155,100,228,237,141,107,255,232,127,
  115,251,202,130,120,248,172,133,128,251,202,130,180,255,232,127,223,255,242,120,255,255,252,113,
  0,175,121,62,29,128,103,60,68,84,84,58,68,248,184,55,97,239,204,93,124,230,225,133,
  178,102,125,129,255,0,26,125,0,255,254,236,12,255,254,236,22,255,254,236,26,223,224,178,
  28,192,195,124,28,176,255,231,50,123,251,236,71,74,246,241,93,33,225,228,120,0,204,215,
  133,4,168,178,136,10,132,143,136,51,189,212,208,23,159,201,255,0,129,190,0,41,18,24,
  51,73,0,22,76,165,170,38,101,255,189,80,127,139,56,40,153,73,0,22,229,41,18,24,
  255,41,18,24,0,53,34,0,42,86,51,0,63,147,108,49,84,212,166,108,106,235,212,180,
  116,255,255,255,138,191,255,193,148,84,255,88,170,0,255,0,191,0,192,0,212,0,128,0,
  255,0,128,0,0,0,0,0,37,31,89,19,76,72,178,43,127,150,235,5,128,186,234,119,
  130,222,233,252,153,197,219,231,204,132,179,253,255,28,107,225,0,12,45,0,19,101,86,2,
  38,207,128,4,63,243,197,18,66,109,196,146,255,5,39,7,0,255,102,41,43,255,140,90,
  86,255,51,90,127,255,153,169,170,255,255,249,209,113,255,85,255,157,255,137,0,251,216,252,
  48,255,192,255,89,239,95,241,160,51,153,217,216,24,184,174,255,24,184,174,0,24,184,174,
  66,8,162,150,104,124,137,7,130,178,186,22,150,124,137,7,201,6,156,144,239,0,128,117,
  255,0,128,117,0,119,97,33,42,235,199,88,84,169,238,124,127,37,238,232,170,7,120,236,
  212,27,1,175,255,4,51,101,0,226,6,12,94,26,96,78,132,130,189,94,255,177,3,9,
  0,29,8,3,53,76,1,0,104,142,96,28,153,211,191,61,255,117,129,42,0,177,160,199,
  72,205,158,149,89,233,155,101,107,255,95,63,141,192,98,109,255,132,101,159,0,0,25,255,
  63,38,140,117,127,86,255,0,191,167,140,19,255,255,25,41,0,0,0,0,46,77,0,0,
  96,177,0,0,108,196,38,9,119,215,76,19,146,235,115,29,174,255,153,41,188,255,178,41,
  202,255,204,41,218,255,230,41,234,255,255,41,244,255,255,143,255,255,255,255,0,0,0,0,
  59,0,51,117,119,0,102,255,149,38,153,255,180,86,204,255,217,167,230,255,255,255,255,255,
  0,61,155,44,25,95,174,77,60,132,193,113,93,154,166,125,106,175,138,136,109,183,121,137,
  113,194,104,138,116,225,179,165,124,255,255,192,168,167,218,203,255,84,182,215,0,79,32,109,
  25,90,40,117,51,102,48,124,76,141,135,185,102,180,222,248,109,208,236,252,114,237,250,255,
  122,206,200,239,149,177,149,222,183,187,130,203,255,198,111,184,0,90,14,5,51,139,41,13,
  84,180,70,17,104,192,202,125,112,177,137,3,122,190,200,131,124,192,202,124,135,177,137,3,
  142,194,203,118,163,177,68,17,204,128,35,12,249,74,5,2,255,74,5,2,0,0,0,0,
  42,0,0,117,84,0,0,255,127,113,0,255,170,255,0,255,212,255,128,255,255,255,255,255,
  0,0,0,0,63,113,0,117,127,255,0,255,191,255,0,117,255,255,0,0,0,0,0,0,
  42,113,0,0,84,255,0,0,127,255,0,117,170,255,0,255,212,255,128,117,255,255,255,0,
  0,0,0,255,63,0,128,255,127,0,255,255,191,113,255,117,255,255,255,0,0,0,150,92,
  55,0,150,92,200,255,72,0,255,255,72,0,0,1,2,14,33,2,5,35,100,13,135,92,
  120,43,255,193,140,247,7,249,160,193,17,208,180,39,255,154,200,4,213,236,220,39,252,135,
  240,193,213,253,255,255,249,255,0,1,5,45,10,1,5,45,25,5,169,175,40,1,5,45,
  61,1,5,45,76,45,175,31,91,1,5,45,112,1,5,45,127,249,150,5,143,1,5,45,
  162,1,5,45,178,255,92,0,193,1,5,45,214,1,5,45,229,223,45,72,244,1,5,45,
  255,1,5,45,0,255,95,23,30,255,82,0,60,223,13,8,90,144,44,2,120,255,110,17,
  150,255,69,0,180,158,13,11,210,241,82,17,255,213,37,4,0,184,4,0,60,184,4,0,
  65,144,44,2,125,144,44,2,130,4,96,2,190,4,96,2,195,7,7,88,255,7,7,88,
  0,196,19,10,65,255,69,45,130,223,45,72,195,255,82,103,255,223,13,17,0,1,5,45,
  64,0,200,23,128,0,255,0,170,0,243,45,200,0,135,7,255,1,5,45,0,0,28,112,
  50,32,96,255,100,0,243,45,150,12,95,82,200,25,190,95,255,40,170,80,0,6,126,2,
  45,6,126,2,46,4,30,114,90,4,30,114,91,255,5,0,135,255,5,0,136,196,57,2,
  180,196,57,2,181,137,85,2,255,137,85,2,0,255,5,0,60,255,5,0,61,196,57,2,
  120,196,57,2,121,6,126,2,180,6,126,2,181,4,30,114,255,4,30,114,0,20,92,171,
  14,15,111,186,28,6,142,211,42,2,161,227,56,16,181,239,70,38,188,201,84,86,204,200,
  99,139,219,176,113,182,229,125,127,196,230,63,141,241,240,22,155,254,222,30,170,251,199,4,
  184,247,157,9,198,243,114,15,226,213,30,29,240,151,38,35,255,151,38,35,0,17,177,13,
  64,121,242,5,128,25,173,121,192,250,77,127,255,171,101,221,0,242,168,38,117,226,78,80,
  255,161,54,225,0,243,242,23,15,242,168,38,142,111,21,151,198,74,22,150,255,0,0,117,
  0,2,239,126,255,145,35,217,0,220,19,187,160,12,225,219,219,203,242,223,255,255,255,255,
  0,0,0,0,12,24,4,38,53,55,8,84,80,43,48,159,119,31,89,237,145,50,59,166,
  186,71,30,98,233,31,15,45,255,0,0,0,0,255,255,255,45,50,64,255,112,242,16,186,
  140,255,255,255,155,242,16,186,196,116,13,166,255,255,255,255,0,36,68,114,104,149,195,248,
  188,255,0,0,255,94,14,9,0,0,0,0,66,130,242,245,96,255,255,53,124,255,255,255,
  153,255,255,53,188,130,242,245,255,0,0,0,0,43,30,57,58,73,0,119,122,87,0,74,
  158,197,57,22,183,218,117,27,219,239,177,32,255,246,247,27,0,0,0,0,9,20,21,22,
  40,46,43,49,66,46,43,49,101,61,16,65,255,0,0,0,0,0,0,0,99,242,12,8,
  130,253,228,163,155,242,12,8,255,0,0,0,0,4,7,4,43,40,25,62,76,61,15,36,
  109,207,39,96,127,255,156,184,165,185,73,207,204,105,66,240,255,77,29,78,0,98,22,93,
  45,103,22,73,99,192,45,56,132,235,187,59,175,228,85,26,201,228,56,48,255,2,0,2,
  0,251,46,0,28,255,139,25,43,246,158,63,58,246,216,123,84,243,94,10,114,177,65,11,
  140,255,241,115,168,177,65,11,196,250,233,158,216,255,94,6,255,126,8,4,0,109,102,102,
  25,42,49,71,48,121,96,84,73,241,214,26,89,216,104,44,130,42,49,71,163,255,177,47,
  186,241,214,26,211,109,102,102,255,20,19,13,0,0,0,0,85,0,255,0,170,255,255,0,
  255,255,0,0,0,255,40,0,47,255,80,0,48,255,100,0,79,255,160,0,80,255,200,0,
  95,255,255,0,96,120,255,0,127,0,255,80,128,0,120,255,159,0,80,255,160,120,0,255,
  191,180,40,255,192,255,255,255,255,255,255,180
};
static const uint16_t upstream_gradient_offsets[61]={0,28,48,64,100,108,128,148,192,224,284,316,364,400,424,452,476,508,536,552,572,596,616,668,696,740,784,836,864,884,912,932,948,992,1060,1096,1128,1148,1172,1196,1236,1268,1340,1360,1372,1392,1400,1416,1452,1480,1496,1524,1552,1576,1596,1628,1656,1700,1740,1756,1812};
static CRGBPalette16 upstream_gradient_palette(uint8_t id) {
  CRGBPalette16 result{};
  const unsigned base=(unsigned)id-13u;
  const size_t start=upstream_gradient_offsets[base];
  const size_t length=(size_t)upstream_gradient_offsets[base+1u]-start;
  const uint8_t *data=&upstream_gradient_bytes[start];
  const int count=(int)(length/4u);
  int lastSlotUsed=-1, indexstart=0;
  CRGB rgbstart(data[1],data[2],data[3]);
  for(int point=1;point<count;point++) {
    const uint8_t *entry=&data[(unsigned)point*4u];
    const int indexend=entry[0];
    CRGB rgbend(entry[1],entry[2],entry[3]);
    int istart8=indexstart/16;
    int iend8=indexend/16;
    if(count<16) {
      if(istart8<=lastSlotUsed&&lastSlotUsed<15) {
        istart8=lastSlotUsed+1;
        if(iend8<istart8)iend8=istart8;
      }
      lastSlotUsed=iend8;
    }
    fill_gradient_RGB(result.entries,(unsigned)istart8,rgbstart,
                      (unsigned)iend8,rgbend);
    indexstart=indexend;rgbstart=rgbend;
  }
  return result;
}

static CRGBPalette16 ref_palette(uint8_t id,uint32_t primary,uint32_t secondary,uint32_t tertiary){
 CRGBPalette16 pal{};const CRGB a(primary),b(secondary),c(tertiary);
 if(id>=13u&&id<=72u)return upstream_gradient_palette(id);
 if(id==2){for(auto& v:pal.entries)v=a;}
 else if(id==3){fill_gradient_RGB(pal.entries,0u,a,5u,a);fill_gradient_RGB(pal.entries,5u,a,10u,b);fill_gradient_RGB(pal.entries,10u,b,15u,b);}
 else if(id==4){fill_gradient_RGB(pal.entries,0u,c,8u,b);fill_gradient_RGB(pal.entries,8u,b,15u,a);}
 else if(id==5){for(unsigned i=0;i<16;i++)pal[i]=tertiary?(i<5?a:i<10?b:i<15?c:a):(i<8?a:b);}
 else if(id==0||id==6){
 const uint32_t colors[16]={0x9B00D5u,0xBD00B8u,0xDA0092u,0xF3005Cu,0xF45500u,0xDC8F00u,0xD5B400u,0xD5D500u,0xD59B00u,0xEF6600u,0xF90044u,0xE10086u,0xC400B0u,0xA300CFu,0x7600E8u,0x0032FCu};
 for(unsigned i=0;i<16;i++)pal[i]=CRGB(colors[i]);
 } else if(id>=7u && id<=10u) {
 const uint32_t colors[4][16]={
 {0x0000FFu,0x00008Bu,0x00008Bu,0x00008Bu,0x00008Bu,0x00008Bu,0x00008Bu,0x00008Bu,0x0000FFu,0x00008Bu,0x87CEEBu,0x87CEEBu,0xADD8E6u,0xFFFFFFu,0xADD8E6u,0x87CEEBu},
 {0x000000u,0x800000u,0x000000u,0x800000u,0x8B0000u,0x8B0000u,0x800000u,0x8B0000u,0x8B0000u,0x8B0000u,0xFF0000u,0xFFA500u,0xFFFFFFu,0xFFA500u,0xFF0000u,0x8B0000u},
 {0x191970u,0x00008Bu,0x191970u,0x000080u,0x00008Bu,0x0000CDu,0x2E8B57u,0x008080u,0x5F9EA0u,0x0000FFu,0x008B8Bu,0x6495EDu,0x7FFFD4u,0x2E8B57u,0x00FFFFu,0x87CEFAu},
 {0x006400u,0x006400u,0x556B2Fu,0x006400u,0x008000u,0x228B22u,0x6B8E23u,0x008000u,0x2E8B57u,0x66CDAAu,0x32CD32u,0x9ACD32u,0x90EE90u,0x7CFC00u,0x66CDAAu,0x228B22u}
 };
 for(unsigned i=0;i<16u;i++)pal[i]=CRGB(colors[id-7u][i]);
 }
 else if(id==11u||id==12u) {
 const uint32_t rainbow[16]={
 0xFF0000u,0xEB7000u,0xD59B00u,0xD5BA00u,0xD5D500u,0x9CEB00u,0x00FF00u,0x00EB70u,
 0x00D59Bu,0x009CD4u,0x0000FFu,0x7000EBu,0x9B00D5u,0xBA00BBu,0xD5009Bu,0xEB0072u};
 const uint32_t bands[16]={
 0xFF0000u,0x000000u,0xD59B00u,0x000000u,0xD5D500u,0x000000u,0x00FF00u,0x000000u,
 0x00D59Bu,0x000000u,0x0000FFu,0x000000u,0x9B00D5u,0x000000u,0xD5009Bu,0x000000u};
 for(unsigned i=0;i<16;i++)pal[i]=CRGB(id==11u?rainbow[i]:bands[i]);
 }
 else std::abort();
 return pal;
}

struct PinnedSeg {
    uint8_t speed=0, intensity=0, palette=0;
    uint32_t step=0u,call=0u;
    uint16_t aux0=0u,aux1=0u;
    uint8_t cct=0u;
    std::vector<uint8_t> segment_data;
    uint8_t *data=nullptr;
    bool allocateData(size_t n) {
      if(segment_data.size()<n)segment_data.resize(n,0u);
      data=segment_data.data();return data!=nullptr;
    }
    uint8_t custom1=48u,custom2=64u,custom3=12u;
    bool check1=false,check2=false,check3=false;
    void fade_out(uint8_t) { std::abort(); } // unsupported controls fail closed
    uint16_t offset=0u;
    unsigned nrOfVStrips() const {return 1u;}
    void setPixelColorXY(unsigned x,unsigned y,uint32_t color) { (void)y;setPixelColor(x,color); }
    uint32_t colors[3]={0,0,0};
    std::vector<uint32_t> pixels;
    explicit PinnedSeg(size_t count):pixels(count,0u){}
    uint32_t getPixelColor(unsigned i) const {return pixels.at(i);}
    uint32_t color_wheel(uint8_t pos) const {
        if(palette!=0u)return ColorFromPalette(ref_palette(palette,colors[0],colors[1],colors[2]),pos,255u,LINEARBLEND);
        uint8_t bgrw[4]={0,0,0,0};
        hsv2rgb_rainbow((uint16_t)pos<<8,255,255,bgrw,true);
        return ((uint32_t)bgrw[2]<<16)|((uint32_t)bgrw[1]<<8)|bgrw[0];
    }
    uint32_t color_from_palette(uint16_t i,bool mapping,bool moving,uint8_t slot,uint8_t pbri=255u) const {
        (void)mapping;
        if(palette!=0){const auto p=ref_palette(palette,colors[0],colors[1],colors[2]);return ColorFromPalette(p,i,pbri,moving?LINEARBLEND:LINEARBLEND_NOWRAP);}
        if(slot>2) {
          if(slot>=3u)return ColorFromPalette(ref_palette(0u,colors[0],colors[1],colors[2]),i,pbri,moving?LINEARBLEND:LINEARBLEND_NOWRAP);
          std::abort();
        }
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
    void setPixelColor(size_t i,uint8_t r,uint8_t g,uint8_t b) {
        setPixelColor(i,((uint32_t)r<<16)|((uint32_t)g<<8)|b);
    }
};
struct PinnedEnv {
    uint32_t call=0,step=0;
    uint16_t aux0=0,aux1=0;
    uint32_t data_counter=0;       // upstream mode_android allocation
    std::vector<uint8_t> allocation;
    uint8_t* data=reinterpret_cast<uint8_t*>(&data_counter);
    bool allocateData(size_t bytes) {
        if(bytes<=sizeof(data_counter) && allocation.empty()) return true;
        if(allocation.size()<bytes) allocation.resize(bytes,0u);
        data=allocation.data();
        return true;
    }
};
struct PinnedClock {
    uint32_t now=0;
    unsigned getActiveSegmentsNum() const {return 1u;}
    unsigned getMaxSegments() const {return 64u;}
    unsigned getBrightness() const {return 255u;}
    bool isMatrix=false;
    uint16_t getCurrSegmentId() const {return 0u;}
};
static PinnedSeg* current_segment=nullptr;
static PinnedEnv* current_environment=nullptr;
static PinnedClock strip;
#define millis() strip.now
static inline uint8_t scale8(uint8_t i,uint8_t scale) {
    return (uint8_t)(((uint16_t)i*(1u+(uint16_t)scale))>>8);
}
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
#define SEG_W SEGLEN
#define SEG_H 1u
#define SEGCOLOR(i) (current_segment->colors[(i)])
#define NUM_COLORS 3u
#define MAX(a,b) ((a)>(b)?(a):(b))
#define indexToVStrip(i,strip) ((void)(strip),(i)) /* 1D keeps strip input, never encodes it */


/* Fixed host RNG injection, identical for CORE and pinned WLED source.
 * This validates stochastic branch/state ordering, not device RNG sequence
 * parity. The 1D source still uses hw_random8 with its original arguments. */
static uint32_t ref_rng_state=0x73a11c5du;
static uint32_t ref_random32() {
    uint32_t x=ref_rng_state;
    if(x==0u)x=0x73a11c5du;
    x^=x<<13;x^=x>>17;x^=x<<5;
    ref_rng_state=x;return x;
}
static uint8_t hw_random8(uint32_t max=256u) {
    return max?(uint8_t)(ref_random32()%max):0u;
}
static uint8_t hw_random8(uint32_t min,uint32_t max) {
    return max>min?(uint8_t)(min+(ref_random32()%(max-min))):(uint8_t)min;
}
static uint16_t hw_random16(uint32_t max=65536u) {
    return max?(uint16_t)(ref_random32()%max):0u;
}
static uint16_t hw_random16(uint32_t min,uint32_t max) {
    return max>min?(uint16_t)(min+(ref_random32()%(max-min))):(uint16_t)min;
}

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

// Host adapter restores util.h's omitted default is16bit=false argument.
int32_t perlin2D_raw(uint32_t x,uint32_t y,bool is16bit=false);
// BEGIN UPSTREAM util.cpp 2D Perlin source (verbatim).
#define PERLIN_SHIFT 1
static inline __attribute__((always_inline)) int32_t gradient2D(uint32_t x0, int32_t dx, uint32_t y0, int32_t dy) {
  uint32_t h = (x0 * 0x27D4EB2D) ^ (y0 * 0xB5297A4D);
  h ^= h >> 15;
  h *= 0x92C3412B;
  h ^= h >> 13;
  return (hashToGradient(h) * dx + hashToGradient(h>>PERLIN_SHIFT) * dy) >> (1 + PERLIN_SHIFT);
}


int32_t perlin2D_raw(uint32_t x, uint32_t y, bool is16bit) {
  int32_t x0 = x >> 16;
  int32_t y0 = y >> 16;
  int32_t x1 = x0 + 1;
  int32_t y1 = y0 + 1;

  if(is16bit) {
    x1 = x1 & 0xFF; // wrap back to zero at 0xFF instead of 0xFFFF
    y1 = y1 & 0xFF;
  }

  int32_t dx0 = x & 0xFFFF;
  int32_t dy0 = y & 0xFFFF;
  int32_t dx1 = dx0 - 0x10000;
  int32_t dy1 = dy0 - 0x10000;

  int32_t g00 = gradient2D(x0, dx0, y0, dy0);
  int32_t g10 = gradient2D(x1, dx1, y0, dy0);
  int32_t g01 = gradient2D(x0, dx0, y1, dy1);
  int32_t g11 = gradient2D(x1, dx1, y1, dy1);

  uint32_t tx = smoothstep(dx0);
  uint32_t ty = smoothstep(dy0);

  int32_t nx0 = lerpPerlin(g00, g10, tx);
  int32_t nx1 = lerpPerlin(g01, g11, tx);

  int32_t noise = lerpPerlin(nx0, nx1, ty);
  return noise;
}


uint16_t perlin16(uint32_t x, uint32_t y) {
 return ((perlin2D_raw(x, y) * 1537) >> 10) + 32725; //scale to 16bit and offset (fastled range: about 1748 to 63697)
}


#undef PERLIN_SHIFT
// END UPSTREAM util.cpp 2D Perlin source.

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

//4 bytes
typedef struct Flasher {
  uint16_t stateStart;
  uint8_t stateDur;
  bool stateOn;
} flasher;

#define FLASHERS_PER_ZONE 6
#define MAX_SHIMMER 92


void mode_fairy() {
  //set every pixel to a 'random' color from palette (using seed so it doesn't change between frames)
  uint16_t PRNG16 = 5100 + strip.getCurrSegmentId();
  for (unsigned i = 0; i < SEGLEN; i++) {
    PRNG16 = (uint16_t)(PRNG16 * 2053) + 1384; //next 'random' number
    SEGMENT.setPixelColor(i, SEGMENT.color_from_palette(PRNG16 >> 8, false, false, 0));
  }

  //amount of flasher pixels depending on intensity (0: none, 255: every LED)
  if (SEGMENT.intensity == 0) return;
  unsigned flasherDistance = ((255 - SEGMENT.intensity) / 28) +1; //1-10
  unsigned numFlashers = (SEGLEN / flasherDistance) +1;

  unsigned dataSize = sizeof(flasher) * numFlashers;
  if (!SEGENV.allocateData(dataSize)) return; //allocation failed
  Flasher* flashers = reinterpret_cast<Flasher*>(SEGENV.data);
  unsigned now16 = strip.now & 0xFFFF;

  //Up to 11 flashers in one brightness zone, afterwards a new zone for every 6 flashers
  unsigned zones = numFlashers/FLASHERS_PER_ZONE;
  if (!zones) zones = 1;
  unsigned flashersInZone = numFlashers/zones;
  uint8_t flasherBri[FLASHERS_PER_ZONE*2 -1];

  for (unsigned z = 0; z < zones; z++) {
    unsigned flasherBriSum = 0;
    unsigned firstFlasher = z*flashersInZone;
    if (z == zones-1) flashersInZone = numFlashers-(flashersInZone*(zones-1));

    for (unsigned f = firstFlasher; f < firstFlasher + flashersInZone; f++) {
      unsigned stateTime = uint16_t(now16 - flashers[f].stateStart);
      //random on/off time reached, switch state
      if (stateTime > flashers[f].stateDur * 10) {
        flashers[f].stateOn = !flashers[f].stateOn;
        if (flashers[f].stateOn) {
          flashers[f].stateDur = 12 + hw_random8(12 + ((255 - SEGMENT.speed) >> 2)); //*10, 250ms to 1250ms
        } else {
          flashers[f].stateDur = 20 + hw_random8(6 + ((255 - SEGMENT.speed) >> 2)); //*10, 250ms to 1250ms
        }
        //flashers[f].stateDur = 51 + hw_random8(2 + ((255 - SEGMENT.speed) >> 1));
        flashers[f].stateStart = now16;
        if (stateTime < 255) {
          flashers[f].stateStart -= 255 -stateTime; //start early to get correct bri
          flashers[f].stateDur += 26 - stateTime/10;
          stateTime = 255 - stateTime;
        } else {
          stateTime = 0;
        }
      }
      if (stateTime > 255) stateTime = 255; //for flasher brightness calculation, fades in first 255 ms of state
      //flasherBri[f - firstFlasher] = (flashers[f].stateOn) ? 255-SEGMENT.gamma8((510 - stateTime) >> 1) : SEGMENT.gamma8((510 - stateTime) >> 1);
      flasherBri[f - firstFlasher] = (flashers[f].stateOn) ? stateTime : 255 - (stateTime >> 0);
      flasherBriSum += flasherBri[f - firstFlasher];
    }
    //dim factor, to create "shimmer" as other pixels get less voltage if a lot of flashers are on
    unsigned avgFlasherBri = flasherBriSum / flashersInZone;
    unsigned globalPeakBri = 255 - ((avgFlasherBri * MAX_SHIMMER) >> 8); //183-255, suitable for 1/5th of LEDs flashers

    for (unsigned f = firstFlasher; f < firstFlasher + flashersInZone; f++) {
      uint8_t bri = (flasherBri[f - firstFlasher] * globalPeakBri) / 255;
      PRNG16 = (uint16_t)(PRNG16 * 2053) + 1384; //next 'random' number
      unsigned flasherPos = f*flasherDistance;
      SEGMENT.setPixelColor(flasherPos, color_blend(SEGCOLOR(1), SEGMENT.color_from_palette(PRNG16 >> 8, false, false, 0), bri));
      for (unsigned i = flasherPos+1; i < flasherPos+flasherDistance && i < SEGLEN; i++) {
        PRNG16 = (uint16_t)(PRNG16 * 2053) + 1384; //next 'random' number
        SEGMENT.setPixelColor(i, SEGMENT.color_from_palette(PRNG16 >> 8, false, false, 0, globalPeakBri));
      }
    }
  }
}

/* gamma8() is the WLED effect-domain lookup even when physical bus gamma
 * correction is disabled. Reconstruct the pinned default gamma=2.2 table
 * independently from the original source's powf-based initializer. */
static uint8_t ref_gamma8(uint8_t input) {
    static bool ready=false;
    static uint8_t gamma_table[256]={0};
    if(!ready) {
        for(size_t i=1;i<256u;i++)
            gamma_table[i]=(uint8_t)(std::pow((float)i/255.0f,2.2f)*255.0f+0.5f);
        ready=true;
    }
    return gamma_table[input];
}
#define gamma8(x) ref_gamma8((x))
void mode_fairytwinkle() {
  unsigned dataSize = sizeof(flasher) * SEGLEN;
  if (!SEGENV.allocateData(dataSize)) FX_FALLBACK_STATIC; //allocation failed
  Flasher* flashers = reinterpret_cast<Flasher*>(SEGENV.data);
  unsigned now16 = strip.now & 0xFFFF;
  uint16_t PRNG16 = 5100 + strip.getCurrSegmentId();

  unsigned riseFallTime = 400 + (255-SEGMENT.speed)*3;
  unsigned maxDur = riseFallTime/100 + ((255 - SEGMENT.intensity) >> 2) + 13 + ((255 - SEGMENT.intensity) >> 1);

  for (unsigned f = 0; f < SEGLEN; f++) {
    uint16_t stateTime = now16 - flashers[f].stateStart;
    //random on/off time reached, switch state
    if (stateTime > flashers[f].stateDur * 100) {
      flashers[f].stateOn = !flashers[f].stateOn;
      bool init = !flashers[f].stateDur;
      if (flashers[f].stateOn) {
        flashers[f].stateDur = riseFallTime/100 + ((255 - SEGMENT.intensity) >> 2) + hw_random8(12 + ((255 - SEGMENT.intensity) >> 1)) +1;
      } else {
        flashers[f].stateDur = riseFallTime/100 + hw_random8(3 + ((255 - SEGMENT.speed) >> 6)) +1;
      }
      flashers[f].stateStart = now16;
      stateTime = 0;
      if (init) {
        flashers[f].stateStart -= riseFallTime; //start lit
        flashers[f].stateDur = riseFallTime/100 + hw_random8(12 + ((255 - SEGMENT.intensity) >> 1)) +5; //fire up a little quicker
        stateTime = riseFallTime;
      }
    }
    if (flashers[f].stateOn && flashers[f].stateDur > maxDur) flashers[f].stateDur = maxDur; //react more quickly on intensity change
    if (stateTime > riseFallTime) stateTime = riseFallTime; //for flasher brightness calculation, fades in first 255 ms of state
    unsigned fadeprog = 255 - ((stateTime * 255) / riseFallTime);
    uint8_t flasherBri = (flashers[f].stateOn) ? 255-gamma8(fadeprog) : gamma8(fadeprog);
    unsigned lastR = PRNG16;
    unsigned diff = 0;
    while (diff < 0x4000) { //make sure colors of two adjacent LEDs differ enough
      PRNG16 = (uint16_t)(PRNG16 * 2053) + 1384; //next 'random' number
      diff = (PRNG16 > lastR) ? PRNG16 - lastR : lastR - PRNG16;
    }
    SEGMENT.setPixelColor(f, color_blend(SEGCOLOR(1), SEGMENT.color_from_palette(PRNG16 >> 8, false, false, 0), flasherBri));
  }
}
#undef gamma8

typedef struct Ball {
  unsigned long lastBounceTime;
  float impactVelocity;
  float height;
} ball;

/*
*  Bouncing Balls Effect
*/

void mode_bouncing_balls(void) {
  if (SEGLEN <= 1) FX_FALLBACK_STATIC;
  //allocate segment data
  const unsigned strips = SEGMENT.nrOfVStrips(); // adapt for 2D
  const size_t maxNumBalls = 16;
  unsigned dataSize = sizeof(ball) * maxNumBalls;
  if (!SEGENV.allocateData(dataSize * strips)) FX_FALLBACK_STATIC; //allocation failed

  Ball* balls = reinterpret_cast<Ball*>(SEGENV.data);

  if (!SEGMENT.check2) SEGMENT.fill(SEGCOLOR(2) ? BLACK : SEGCOLOR(1));

  // virtualStrip idea by @ewowi (Ewoud Wijma)
  // requires virtual strip # to be embedded into upper 16 bits of index in setPixelColor()
  // the following functions will not work on virtual strips: fill(), fade_out(), fadeToBlack(), blur()
  struct virtualStrip {
    static void runStrip(size_t stripNr, Ball* balls) {
      // number of balls based on intensity setting to max of 7 (cycles colors)
      // non-chosen color is a random color
      unsigned numBalls = (SEGMENT.intensity * (maxNumBalls - 1)) / 255 + 1; // minimum 1 ball
      const float gravity = -9.81f; // standard value of gravity
      const bool hasCol2 = SEGCOLOR(2);
      const unsigned long time = strip.now;

      if (SEGENV.call == 0) {
        for (size_t i = 0; i < maxNumBalls; i++) balls[i].lastBounceTime = time;
      }

      for (size_t i = 0; i < numBalls; i++) {
        float timeSinceLastBounce = (time - balls[i].lastBounceTime)/((255-SEGMENT.speed)/64 +1);
        float timeSec = timeSinceLastBounce/1000.0f;
        balls[i].height = (0.5f * gravity * timeSec + balls[i].impactVelocity) * timeSec; // avoid use pow(x, 2) - its extremely slow !

        if (balls[i].height <= 0.0f) {
          balls[i].height = 0.0f;
          //damping for better effect using multiple balls
          float dampening = 0.9f - float(i)/float(numBalls * numBalls); // avoid use pow(x, 2) - its extremely slow !
          balls[i].impactVelocity = dampening * balls[i].impactVelocity;
          balls[i].lastBounceTime = time;

          if (balls[i].impactVelocity < 0.015f) {
            float impactVelocityStart = sqrtf(-2.0f * gravity) * hw_random8(5,11)/10.0f; // randomize impact velocity
            balls[i].impactVelocity = impactVelocityStart;
          }
        } else if (balls[i].height > 1.0f) {
          continue; // do not draw OOB ball
        }

        uint32_t color = SEGCOLOR(0);
        if (SEGMENT.palette) {
          color = SEGMENT.color_wheel(i*(256/MAX(numBalls, 8)));
        } else if (hasCol2) {
          color = SEGCOLOR(i % NUM_COLORS);
        }

        int pos = roundf(balls[i].height * (SEGLEN - 1));
        #ifdef WLED_USE_AA_PIXELS
        if (SEGLEN<32) SEGMENT.setPixelColor(indexToVStrip(pos, stripNr), color); // encode virtual strip into index
        else           SEGMENT.setPixelColor(balls[i].height + (stripNr+1)*10.0f, color);
        #else
        SEGMENT.setPixelColor(indexToVStrip(pos, stripNr), color); // encode virtual strip into index
        #endif
      }
    }
  };

  for (unsigned stripNr=0; stripNr<strips; stripNr++)
    virtualStrip::runStrip(stripNr, &balls[stripNr * maxNumBalls]);
}

void mode_halloween_eyes()
{
  enum eyeState : uint8_t {
    initializeOn = 0,
    on,
    blink,
    initializeOff,
    off,

    count
  };
  struct EyeData {
    eyeState state;
    uint8_t color;
    uint16_t startPos;
    // duration + endTime could theoretically be replaced by a single endTime, however we would lose
    // the ability to end the animation early when the user reduces the animation time.
    uint16_t duration;
    uint32_t startTime;
    uint32_t blinkEndTime;
  };

  if (SEGLEN <= 1) FX_FALLBACK_STATIC;
  const unsigned maxWidth = strip.isMatrix ? SEG_W : SEGLEN;
  const unsigned HALLOWEEN_EYE_SPACE = MAX(2, strip.isMatrix ? SEG_W>>4: SEGLEN>>5);
  const unsigned HALLOWEEN_EYE_WIDTH = HALLOWEEN_EYE_SPACE/2;
  unsigned eyeLength = (2*HALLOWEEN_EYE_WIDTH) + HALLOWEEN_EYE_SPACE;
  if (eyeLength >= maxWidth) FX_FALLBACK_STATIC; //bail if segment too short

  if (!SEGENV.allocateData(sizeof(EyeData))) FX_FALLBACK_STATIC; //allocation failed
  EyeData& data = *reinterpret_cast<EyeData*>(SEGENV.data);

  if (!SEGMENT.check2) SEGMENT.fill(SEGCOLOR(1)); //fill background

  data.state = static_cast<eyeState>(data.state % eyeState::count);
  unsigned duration = max(uint16_t{1u}, data.duration);
  const uint32_t elapsedTime = strip.now - data.startTime;

  switch (data.state) {
    case eyeState::initializeOn: {
      // initialize the eyes-on state:
      // - select eye position and color
      // - select a duration
      // - immediately switch to eyes on state.

      data.startPos = hw_random16(0, maxWidth - eyeLength - 1);
      data.color = hw_random8();
      if (strip.isMatrix) SEGMENT.offset = hw_random16(SEG_H-1); // a hack: reuse offset since it is not used in matrices
      duration = 128u + hw_random16(SEGMENT.intensity*64u);
      data.duration = duration;
      data.state = eyeState::on;
      [[fallthrough]];
    }
    case eyeState::on: {
      // eyes-on steate:
      // - fade eyes in for some time
      // - keep eyes on until the pre-selected duration is over
      // - randomly switch to the blink (sub-)state, and initialize it with a blink duration (more precisely, a blink end time stamp)
      // - never switch to the blink state if the animation just started or is about to end

      unsigned start2ndEye = data.startPos + HALLOWEEN_EYE_WIDTH + HALLOWEEN_EYE_SPACE;
      // If the user reduces the input while in this state, limit the duration.
      duration = min(duration, (128u + (SEGMENT.intensity * 64u)));

      constexpr uint32_t minimumOnTimeBegin = 1024u;
      constexpr uint32_t minimumOnTimeEnd = 1024u;
      const uint32_t fadeInAnimationState = elapsedTime * uint32_t{256u * 8u} / duration;
      const uint32_t backgroundColor = SEGCOLOR(1);
      const uint32_t eyeColor = SEGMENT.color_from_palette(data.color, false, false, 0);
      uint32_t c = eyeColor;
      if (fadeInAnimationState < 256u) {
        c = color_blend(backgroundColor, eyeColor, uint8_t(fadeInAnimationState));
      } else if (elapsedTime > minimumOnTimeBegin) {
        const uint32_t remainingTime = (elapsedTime >= duration) ? 0u : (duration - elapsedTime);
        if (remainingTime > minimumOnTimeEnd) {
          if (hw_random8() < 4u)
          {
            c = backgroundColor;
            data.state = eyeState::blink;
            data.blinkEndTime = strip.now + hw_random8(8, 128);
          }
        }
      }

      if (c != backgroundColor) {
        // render eyes
        for (unsigned i = 0; i < HALLOWEEN_EYE_WIDTH; i++) {
          if (strip.isMatrix) {
            SEGMENT.setPixelColorXY(data.startPos + i, (unsigned)SEGMENT.offset, c);
            SEGMENT.setPixelColorXY(start2ndEye   + i, (unsigned)SEGMENT.offset, c);
          } else {
            SEGMENT.setPixelColor(data.startPos + i, c);
            SEGMENT.setPixelColor(start2ndEye   + i, c);
          }
        }
      }
      break;
    }
    case eyeState::blink: {
      // eyes-on but currently blinking state:
      // - wait until the blink time is over, then switch back to eyes-on

      if (strip.now >= data.blinkEndTime) {
        data.state = eyeState::on;
      }
      break;
    }
    case eyeState::initializeOff: {
      // initialize eyes-off state:
      // - select a duration
      // - immediately switch to eyes-off state

      const unsigned eyeOffTimeBase = SEGMENT.speed*128u;
      duration = eyeOffTimeBase + hw_random16(eyeOffTimeBase);
      data.duration = duration;
      data.state = eyeState::off;
      [[fallthrough]];
    }
    case eyeState::off: {
      // eyes-off state:
      // - not much to do here

      // If the user reduces the input while in this state, limit the duration.
      const unsigned eyeOffTimeBase = SEGMENT.speed*128u;
      duration = min(duration, (2u * eyeOffTimeBase));
      break;
    }
    case eyeState::count: {
      // Can't happen, not an actual state.
      data.state = eyeState::initializeOn;
      break;
    }
  }

  if (elapsedTime > duration) {
    // The current state duration is over, switch to the next state.
    switch (data.state) {
      case eyeState::initializeOn:
      case eyeState::on:
      case eyeState::blink:
        data.state = eyeState::initializeOff;
        break;
      case eyeState::initializeOff:
      case eyeState::off:
      case eyeState::count:
      default:
        data.state = eyeState::initializeOn;
        break;
    }
    data.startTime = strip.now;
  }
}

// BEGIN UPSTREAM FX.cpp Rolling Balls, unchanged source body.
typedef struct RollingBall {
  unsigned long lastBounceUpdate;
  float mass; // could fix this to be = 1. if memory is an issue
  float velocity;
  float height;
} rball_t;

static void mode_rolling_balls(void) {
  //allocate segment data
  const unsigned maxNumBalls = 16; // 255/16 + 1
  unsigned dataSize = sizeof(rball_t) * maxNumBalls;
  if (!SEGENV.allocateData(dataSize)) FX_FALLBACK_STATIC; //allocation failed

  rball_t *balls = reinterpret_cast<rball_t *>(SEGENV.data);

  // number of balls based on intensity setting to max of 16 (cycles colors)
  // non-chosen color is a random color
  unsigned numBalls = SEGMENT.intensity/16 + 1;
  bool hasCol2 = SEGCOLOR(2);

  if (SEGENV.call == 0) {
    SEGMENT.fill(hasCol2 ? BLACK : SEGCOLOR(1));                    // start clean
    for (unsigned i = 0; i < maxNumBalls; i++) {
      balls[i].lastBounceUpdate = strip.now;
      balls[i].velocity = 20.0f * float(hw_random16(1000, 10000))/10000.0f;  // number from 1 to 10
      if (hw_random8()<128) balls[i].velocity = -balls[i].velocity;    // 50% chance of reverse direction
      balls[i].height = (float(hw_random16(0, 10000)) / 10000.0f);     // from 0. to 1.
      balls[i].mass   = (float(hw_random16(1000, 10000)) / 10000.0f);  // from .1 to 1.
    }
  }

  float cfac = float(scale8(8, 255-SEGMENT.speed) +1)*20000.0f; // this uses the Aircoookie conversion factor for scaling time using speed slider

  if (SEGMENT.check3) SEGMENT.fade_out(250); // 2-8 pixel trails (optional)
  else {
  	if (!SEGMENT.check2) SEGMENT.fill(hasCol2 ? BLACK : SEGCOLOR(1)); // don't fill with background color if user wants to see trails
  }

  for (unsigned i = 0; i < numBalls; i++) {
    float timeSinceLastUpdate = float((strip.now - balls[i].lastBounceUpdate))/cfac;
    float thisHeight = balls[i].height + balls[i].velocity * timeSinceLastUpdate; // this method keeps higher resolution
    // test if intensity level was increased and some balls are way off the track then put them back
    if (thisHeight < -0.5f || thisHeight > 1.5f) {
      thisHeight = balls[i].height = (float(hw_random16(0, 10000)) / 10000.0f); // from 0. to 1.
      balls[i].lastBounceUpdate = strip.now;
    }
    // check if reached ends of the strip
    if ((thisHeight <= 0.0f && balls[i].velocity < 0.0f) || (thisHeight >= 1.0f && balls[i].velocity > 0.0f)) {
      balls[i].velocity = -balls[i].velocity; // reverse velocity
      balls[i].lastBounceUpdate = strip.now;
      balls[i].height = thisHeight;
    }
    // check for collisions
    if (SEGMENT.check1) {
      for (unsigned j = i+1; j < numBalls; j++) {
        if (balls[j].velocity != balls[i].velocity) {
          //  tcollided + balls[j].lastBounceUpdate is acutal time of collision (this keeps precision with long to float conversions)
          float tcollided = (cfac*(balls[i].height - balls[j].height) +
                balls[i].velocity*float(balls[j].lastBounceUpdate - balls[i].lastBounceUpdate))/(balls[j].velocity - balls[i].velocity);

          if ((tcollided > 2.0f) && (tcollided < float(strip.now - balls[j].lastBounceUpdate))) { // 2ms minimum to avoid duplicate bounces
            balls[i].height = balls[i].height + balls[i].velocity*(tcollided + float(balls[j].lastBounceUpdate - balls[i].lastBounceUpdate))/cfac;
            balls[j].height = balls[i].height;
            balls[i].lastBounceUpdate = (unsigned long)(tcollided + 0.5f) + balls[j].lastBounceUpdate;
            balls[j].lastBounceUpdate = balls[i].lastBounceUpdate;
            float vtmp = balls[i].velocity;
            balls[i].velocity = ((balls[i].mass - balls[j].mass)*vtmp              + 2.0f*balls[j].mass*balls[j].velocity)/(balls[i].mass + balls[j].mass);
            balls[j].velocity = ((balls[j].mass - balls[i].mass)*balls[j].velocity + 2.0f*balls[i].mass*vtmp)             /(balls[i].mass + balls[j].mass);
            thisHeight = balls[i].height + balls[i].velocity*(strip.now - balls[i].lastBounceUpdate)/cfac;
          }
        }
      }
    }

    uint32_t color = SEGCOLOR(0);
    if (SEGMENT.palette) {
      //color = SEGMENT.color_wheel(i*(256/MAX(numBalls, 8)));
      color = SEGMENT.color_from_palette(i*255/numBalls, false, PALETTE_SOLID_WRAP, 0);
    } else if (hasCol2) {
      color = SEGCOLOR(i % NUM_COLORS);
    }

    if (thisHeight < 0.0f) thisHeight = 0.0f;
    if (thisHeight > 1.0f) thisHeight = 1.0f;
    unsigned pos = round(thisHeight * (SEGLEN - 1));
    SEGMENT.setPixelColor(pos, color);
    balls[i].lastBounceUpdate = strip.now;
    balls[i].height = thisHeight;
  }
}

// END UPSTREAM FX.cpp Rolling Balls.

// BEGIN UPSTREAM FX.cpp Spark and Popcorn, unchanged source body.
#define FAIR_DATA_PER_SEG 1024u // FX.h ESP32-S3 PSRAM: 64KiB / 64 segments
typedef struct Spark {
  float pos, posX;
  float vel, velX;
  uint16_t col;
  uint8_t colIndex;
} spark;

#define maxNumPopcorn 21 // max 21 on 16 segment ESP8266
/*
*  POPCORN
*  modified from https://github.com/kitesurfer1404/WS2812FX/blob/master/src/custom/Popcorn.h
*/
void mode_popcorn(void) {
  if (SEGLEN <= 1) FX_FALLBACK_STATIC;
  //allocate segment data
  unsigned strips = SEGMENT.nrOfVStrips();
  unsigned usablePopcorns = maxNumPopcorn;
  if (usablePopcorns * strips * sizeof(spark) > FAIR_DATA_PER_SEG) usablePopcorns = FAIR_DATA_PER_SEG / (strips * sizeof(spark)) + 1; // at least 1 popcorn per vstrip
  unsigned dataSize = sizeof(spark) * usablePopcorns; // on a matrix 64x64 this could consume a little less than 27kB when Bar expansion is used
  if (!SEGENV.allocateData(dataSize * strips)) FX_FALLBACK_STATIC; //allocation failed

  Spark* popcorn = reinterpret_cast<Spark*>(SEGENV.data);

  bool hasCol2 = SEGCOLOR(2);
  if (!SEGMENT.check2) SEGMENT.fill(hasCol2 ? BLACK : SEGCOLOR(1));

  struct virtualStrip {
    static void runStrip(uint16_t stripNr, Spark* popcorn, unsigned usablePopcorns) {
      float gravity = -0.0001f - (SEGMENT.speed/200000.0f); // m/s/s
      gravity *= SEGLEN;

      unsigned numPopcorn = SEGMENT.intensity * usablePopcorns / 255;
      if (numPopcorn == 0) numPopcorn = 1;

      for (unsigned i = 0; i < numPopcorn; i++) {
        if (popcorn[i].pos >= 0.0f) { // if kernel is active, update its position
          popcorn[i].pos += popcorn[i].vel;
          popcorn[i].vel += gravity;
        } else { // if kernel is inactive, randomly pop it
          if (hw_random8() < 2) { // POP!!!
            popcorn[i].pos = 0.01f;

            unsigned peakHeight = 128 + hw_random8(128); //0-255
            peakHeight = (peakHeight * (SEGLEN -1)) >> 8;
            popcorn[i].vel = sqrtf(-2.0f * gravity * peakHeight);

            if (SEGMENT.palette)
            {
              popcorn[i].colIndex = hw_random8();
            } else {
              byte col = hw_random8(0, NUM_COLORS);
              if (!SEGCOLOR(2) || !SEGCOLOR(col)) col = 0;
              popcorn[i].colIndex = col;
            }
          }
        }
        if (popcorn[i].pos >= 0.0f) { // draw now active popcorn (either active before or just popped)
          uint32_t col = SEGMENT.color_wheel(popcorn[i].colIndex);
          if (!SEGMENT.palette && popcorn[i].colIndex < NUM_COLORS) col = SEGCOLOR(popcorn[i].colIndex);
          unsigned ledIndex = popcorn[i].pos;
          if (ledIndex < SEGLEN) SEGMENT.setPixelColor(indexToVStrip(ledIndex, stripNr), col);
        }
      }
    }
  };

  for (unsigned stripNr=0; stripNr<strips; stripNr++)
    virtualStrip::runStrip(stripNr, &popcorn[stripNr * usablePopcorns], usablePopcorns);
}

// END UPSTREAM FX.cpp Spark and Popcorn.

// Host-only stand-in for Arduino constrain() with 1D unsigned indices.
template<typename T,typename L,typename H>
static unsigned constrain(T value,L lo,H hi) {
    /* Preserve source signed clamp: negative Perlin brightness -> zero. */
    const int64_t x=static_cast<int64_t>(value);
    const int64_t low=static_cast<int64_t>(lo),high=static_cast<int64_t>(hi);
    return (unsigned)(x<low?low:(x>high?high:x));
}
// BEGIN UPSTREAM FX.cpp Drip, unchanged source body.
void mode_drip(void)
{
  if (SEGLEN <= 1) FX_FALLBACK_STATIC;
  //allocate segment data
  unsigned strips = SEGMENT.nrOfVStrips();
  const int maxNumDrops = 4;
  unsigned dataSize = sizeof(spark) * maxNumDrops;
  if (!SEGENV.allocateData(dataSize * strips)) FX_FALLBACK_STATIC; //allocation failed
  Spark* drops = reinterpret_cast<Spark*>(SEGENV.data);

  if (!SEGMENT.check2) SEGMENT.fill(SEGCOLOR(1));

  struct virtualStrip {
    static void runStrip(uint16_t stripNr, Spark* drops) {

      unsigned numDrops = 1 + (SEGMENT.intensity >> 6); // 255>>6 = 3

      float gravity = -0.0005f - (SEGMENT.speed/50000.0f);
      gravity *= max(1, (int)SEGLEN-1);
      int sourcedrop = 12;

      for (unsigned j=0;j<numDrops;j++) {
        if (drops[j].colIndex == 0) { //init
          drops[j].pos = SEGLEN-1;    // start at end
          drops[j].vel = 0;           // speed
          drops[j].col = sourcedrop;  // brightness
          drops[j].colIndex = 1;      // drop state (0 init, 1 forming, 2 falling, 5 bouncing)
        }

        SEGMENT.setPixelColor(indexToVStrip(SEGLEN-1, stripNr), color_blend(BLACK,SEGCOLOR(0), uint8_t(sourcedrop)));// water source
        if (drops[j].colIndex==1) {
          if (drops[j].col>255) drops[j].col=255;
          SEGMENT.setPixelColor(indexToVStrip(uint16_t(drops[j].pos), stripNr), color_blend(BLACK,SEGCOLOR(0),uint8_t(drops[j].col)));

          drops[j].col += map(SEGMENT.speed, 0, 255, 1, 6); // swelling

          if (hw_random8() < drops[j].col/10) {               // random drop
            drops[j].colIndex=2;               //fall
            drops[j].col=255;
          }
        }
        if (drops[j].colIndex > 1) {           // falling
          if (drops[j].pos > 0) {              // fall until end of segment
            drops[j].pos += drops[j].vel;
            if (drops[j].pos < 0) drops[j].pos = 0;
            drops[j].vel += gravity;           // gravity is negative

            for (int i=1;i<7-drops[j].colIndex;i++) { // some minor math so we don't expand bouncing droplets
              unsigned pos = constrain(unsigned(drops[j].pos) +i, 0, SEGLEN-1); //this is BAD, returns a pos >= SEGLEN occasionally
              SEGMENT.setPixelColor(indexToVStrip(pos, stripNr), color_blend(BLACK,SEGCOLOR(0),uint8_t(drops[j].col/i))); //spread pixel with fade while falling
            }

            if (drops[j].colIndex > 2) {       // during bounce, some water is on the floor
              SEGMENT.setPixelColor(indexToVStrip(0, stripNr), color_blend(SEGCOLOR(0),BLACK,uint8_t(drops[j].col)));
            }
          } else {                             // we hit bottom
            if (drops[j].colIndex > 2) {       // already hit once, so back to forming
              drops[j].colIndex = 0;
              drops[j].col = sourcedrop;

            } else {

              if (drops[j].colIndex==2) {      // init bounce
                drops[j].vel = -drops[j].vel/4;// reverse velocity with damping
                drops[j].pos += drops[j].vel;
              }
              drops[j].col = sourcedrop*2;
              drops[j].colIndex = 5;           // bouncing
            }
          }
        }
      }
    }
  };

  for (unsigned stripNr=0; stripNr<strips; stripNr++)
    virtualStrip::runStrip(stripNr, &drops[stripNr*maxNumDrops]);
}

// END UPSTREAM FX.cpp Drip.

// BEGIN UPSTREAM FX.cpp TV Simulator (verbatim), source profile RGB.
typedef struct TvSim {
  uint32_t totalTime = 0;
  uint32_t fadeTime  = 0;
  uint32_t startTime = 0;
  uint32_t elapsed   = 0;
  uint32_t pixelNum  = 0;
  uint16_t sliderValues = 0;
  uint32_t sceeneStart    = 0;
  uint32_t sceeneDuration = 0;
  uint16_t sceeneColorHue = 0;
  uint8_t  sceeneColorSat = 0;
  uint8_t  sceeneColorBri = 0;
  uint8_t  actualColorR = 0;
  uint8_t  actualColorG = 0;
  uint8_t  actualColorB = 0;
  uint16_t pr = 0; // Prev R, G, B
  uint16_t pg = 0;
  uint16_t pb = 0;
} tvSim;

void mode_tv_simulator(void) {
  int nr, ng, nb, r, g, b, i, hue;
  uint8_t  sat, bri, j;

  if (!SEGENV.allocateData(sizeof(tvSim))) FX_FALLBACK_STATIC; //allocation failed
  TvSim* tvSimulator = reinterpret_cast<TvSim*>(SEGENV.data);

  uint8_t colorSpeed     = map(SEGMENT.speed,     0, UINT8_MAX,  1, 20);
  uint8_t colorIntensity = map(SEGMENT.intensity, 0, UINT8_MAX, 10, 30);

  i = SEGMENT.speed << 8 | SEGMENT.intensity;
  if (i != tvSimulator->sliderValues) {
    tvSimulator->sliderValues = i;
    SEGENV.aux1 = 0;
  }

    // create a new sceene
    if (((strip.now - tvSimulator->sceeneStart) >= tvSimulator->sceeneDuration) || SEGENV.aux1 == 0) {
      tvSimulator->sceeneStart    = strip.now;                                               // remember the start of the new sceene
      tvSimulator->sceeneDuration = hw_random16(60* 250* colorSpeed, 60* 750 * colorSpeed);    // duration of a "movie sceene" which has similar colors (5 to 15 minutes with max speed slider)
      tvSimulator->sceeneColorHue = hw_random16(   0, 768);                                    // random start color-tone for the sceene
      tvSimulator->sceeneColorSat = hw_random8 ( 100, 130 + colorIntensity);                   // random start color-saturation for the sceene
      tvSimulator->sceeneColorBri = hw_random8 ( 200, 240);                                    // random start color-brightness for the sceene
      SEGENV.aux1 = 1;
      SEGENV.aux0 = 0;
    }

    // slightly change the color-tone in this sceene
    if (SEGENV.aux0 == 0) {
      // hue change in both directions
      j = hw_random8(4 * colorIntensity);
      hue = (hw_random8() < 128) ? ((j < tvSimulator->sceeneColorHue)       ? tvSimulator->sceeneColorHue - j : 767 - tvSimulator->sceeneColorHue - j) :  // negative
                                ((j + tvSimulator->sceeneColorHue) < 767 ? tvSimulator->sceeneColorHue + j : tvSimulator->sceeneColorHue + j - 767) ;  // positive

      // saturation
      j = hw_random8(2 * colorIntensity);
      sat = (tvSimulator->sceeneColorSat - j) < 0 ? 0 : tvSimulator->sceeneColorSat - j;

      // brightness
      j = hw_random8(100);
      bri = (tvSimulator->sceeneColorBri - j) < 0 ? 0 : tvSimulator->sceeneColorBri - j;

      // calculate R,G,B from HSV
      // Source: https://blog.adafruit.com/2012/03/14/constant-brightness-hsb-to-rgb-algorithm/
      { // just to create a local scope for  the variables
        uint8_t temp[5], n = (hue >> 8) % 3;
        uint8_t x = ((((hue & 255) * sat) >> 8) * bri) >> 8;
        uint8_t s = (  (256 - sat) * bri) >> 8;
        temp[0] = temp[3] =       s;
        temp[1] = temp[4] =   x + s;
        temp[2] =           bri - x;
        tvSimulator->actualColorR = temp[n + 2];
        tvSimulator->actualColorG = temp[n + 1];
        tvSimulator->actualColorB = temp[n    ];
      }
    }
    // expand to 16 bit
    nr = (uint8_t)(tvSimulator->actualColorR) * 257; // New R/G/B
    ng = (uint8_t)(tvSimulator->actualColorG) * 257;
    nb = (uint8_t)(tvSimulator->actualColorB) * 257;

  if (SEGENV.aux0 == 0) {  // initialize next iteration
    SEGENV.aux0 = 1;

    // randomize total duration and fade duration for the actual color
    tvSimulator->totalTime = hw_random16(250, 2500);                   // Semi-random pixel-to-pixel time
    tvSimulator->fadeTime  = hw_random16(0, tvSimulator->totalTime);   // Pixel-to-pixel transition time
    if (hw_random8(10) < 3) tvSimulator->fadeTime = 0;                 // Force scene cut 30% of time

    tvSimulator->startTime = strip.now;
  } // end of initialization

  // how much time is elapsed ?
  tvSimulator->elapsed = strip.now - tvSimulator->startTime;

  // fade from prev color to next color
  if (tvSimulator->elapsed < tvSimulator->fadeTime) {
    r = map(tvSimulator->elapsed, 0, tvSimulator->fadeTime, tvSimulator->pr, nr);
    g = map(tvSimulator->elapsed, 0, tvSimulator->fadeTime, tvSimulator->pg, ng);
    b = map(tvSimulator->elapsed, 0, tvSimulator->fadeTime, tvSimulator->pb, nb);
  } else { // Avoid divide-by-zero in map()
    r = nr;
    g = ng;
    b = nb;
  }

  // set strip color
  for (i = 0; i < (int)SEGLEN; i++) {
    SEGMENT.setPixelColor(i, r >> 8, g >> 8, b >> 8);  // Quantize to 8-bit
  }

  // if total duration has passed, remember last color and restart the loop
  if ( tvSimulator->elapsed >= tvSimulator->totalTime) {
    tvSimulator->pr = nr; // Prev RGB = new RGB
    tvSimulator->pg = ng;
    tvSimulator->pb = nb;
    SEGENV.aux0 = 0;
  }
}

// END UPSTREAM TV Simulator.


// Host declaration for the pinned math cos8_t helper.
static uint8_t cos8_t(uint8_t phase) {return sin8_t((uint8_t)(phase+64u));}
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare" // unchanged upstream loops int < unsigned SEGLEN
// BEGIN UPSTREAM FX.cpp Color Clouds (verbatim), RGB/palette 0 profile.
#define R(c) (uint8_t)(((uint32_t)(c)>>16)&255u)
#define G(c) (uint8_t)(((uint32_t)(c)>>8)&255u)
#define B(c) (uint8_t)((uint32_t)(c)&255u)
void mode_ColorClouds()
{
  // Set random start points for clouds and color.
  if (SEGENV.call == 0) {
    SEGENV.aux0 = hw_random16();
    SEGENV.aux1 = hw_random16();
  }
  const uint32_t volX0 = SEGENV.aux0;
  const uint32_t hueX0 = SEGENV.aux1;
  const uint8_t hueOffset0 = volX0 + hueX0; // derive a 3rd random number

  // Makes a very soft wraparound of the color palette by putting more emphasis on the begin & end
  // of the palette (or on the red'ish colors in case of a rainbow spectrum).
  // This gives the effect oftentimes an even more calm perception.
  const bool cozy = SEGMENT.check3;

  // Higher values make the clouds move faster.
  const uint32_t volSpeed = 1 + SEGMENT.speed;
  
  // Higher values make the color change faster.
  const uint32_t hueSpeed = 1 + SEGMENT.intensity;
  
  // Higher values make more clouds (but smaller ones).
  const uint32_t volSqueeze = 8 + SEGMENT.custom1;
  
  // Higher values make the clouds more colorful.
  const uint32_t hueSqueeze = SEGMENT.custom2;

  // Higher values make larger gaps between the clouds.
  const int32_t volCutoff   = 12500 + SEGMENT.custom3 * 900;
  const int32_t volSaturate = 52000;
  // Note: When adjusting these calculations, ensure that volCutoff is always smaller than volSaturate.

  const uint32_t now = strip.now;
  const uint32_t volT = now * volSpeed / 8;
  const uint32_t hueT = now * hueSpeed / 8;
  const uint8_t hueOffset = beat88(64) >> 8;

  for (int i = 0; i < SEGLEN; i++) {
    const uint32_t volX = i * volSqueeze * 64;
    int32_t vol = perlin16(volX0 + volX, volT);
    vol = map(vol, volCutoff, volSaturate, 0, 255);
    vol = constrain(vol, 0, 255);

    const uint32_t hueX = i * hueSqueeze * 8;
    uint8_t hue = perlin16(hueX0 + hueX, hueT) >> 7;
    hue += hueOffset0;
    hue += hueOffset;
    if (cozy) {
      hue = cos8_t(128 + hue / 2);
    }

    uint32_t pixel;
    if (SEGMENT.palette) { pixel = SEGMENT.color_from_palette(hue, false, true, 0, vol); }
    else { pixel = CRGBW(CHSV32(hue, 255, vol)); }

    // Suppress extremely dark pixels to avoid flickering of plain r/g/b.
    if (int(R(pixel)) + G(pixel) + B(pixel) <= 2) {
      pixel = 0;
    }

    SEGMENT.setPixelColor(i, pixel);
  }
}

#undef R
#undef G
#undef B
// END UPSTREAM FX.cpp Color Clouds.
#pragma GCC diagnostic pop


/* Reference adapter: ESP32 budget and source RGBW constructor, no edits to
 * the upstream starburst function body or particle struct. */
static uint32_t RGBW32(uint8_t r,uint8_t g,uint8_t b,uint8_t w) {
    (void)w;
    return ((uint32_t)r<<16)|((uint32_t)g<<8)|b;
}
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtype-limits" // upstream unsigned start < 0
// BEGIN UPSTREAM FX.cpp starburst (verbatim).
#ifdef ESP8266
  #define STARBURST_MAX_FRAG   8 //52 bytes / star
#else
  #define STARBURST_MAX_FRAG  10 //60 bytes / star
#endif
//each needs 20+STARBURST_MAX_FRAG*4 bytes
typedef struct particle {
  CRGB     color;
  uint32_t birth  =0;
  uint32_t last   =0;
  float    vel    =0;
  uint16_t pos    =-1;
  float    fragment[STARBURST_MAX_FRAG];
} star;

void mode_starburst(void) {
  if (SEGLEN <= 1) FX_FALLBACK_STATIC;
  unsigned maxData = FAIR_DATA_PER_SEG; //ESP8266: 256 ESP32: 640
  unsigned segs = strip.getActiveSegmentsNum();
  if (segs <= (strip.getMaxSegments() /2)) maxData *= 2; //ESP8266: 512 if <= 8 segs ESP32: 1280 if <= 16 segs
  if (segs <= (strip.getMaxSegments() /4)) maxData *= 2; //ESP8266: 1024 if <= 4 segs ESP32: 2560 if <= 8 segs
  unsigned maxStars = maxData / sizeof(star); //ESP8266: max. 4/9/19 stars/seg, ESP32: max. 10/21/42 stars/seg

  unsigned numStars = 1 + (SEGLEN >> 3);
  if (numStars > maxStars) numStars = maxStars;
  unsigned dataSize = sizeof(star) * numStars;

  if (!SEGENV.allocateData(dataSize)) FX_FALLBACK_STATIC; //allocation failed

  uint32_t it = strip.now;

  star* stars = reinterpret_cast<star*>(SEGENV.data);

  float          maxSpeed                = 375.0f;  // Max velocity
  float          particleIgnition        = 250.0f;  // How long to "flash"
  float          particleFadeTime        = 1500.0f; // Fade out time

  for (unsigned j = 0; j < numStars; j++)
  {
    // speed to adjust chance of a burst, max is nearly always.
    if (hw_random8((144-(SEGMENT.speed >> 1))) == 0 && stars[j].birth == 0)
    {
      // Pick a random color and location.
      unsigned startPos = hw_random16(SEGLEN-1);
      float multiplier = (float)(hw_random8())/255.0f * 1.0f;

      stars[j].color = CRGB(SEGMENT.color_wheel(hw_random8()));
      stars[j].pos = startPos;
      stars[j].vel = maxSpeed * (float)(hw_random8())/255.0f * multiplier;
      stars[j].birth = it;
      stars[j].last = it;
      // more fragments means larger burst effect
      int num = hw_random8(3,6 + (SEGMENT.intensity >> 5));

      for (int i=0; i < STARBURST_MAX_FRAG; i++) {
        if (i < num) stars[j].fragment[i] = startPos;
        else stars[j].fragment[i] = -1;
      }
    }
  }

  if (!SEGMENT.check2) SEGMENT.fill(SEGCOLOR(1));

  for (unsigned j=0; j<numStars; j++)
  {
    if (stars[j].birth != 0) {
      float dt = (it-stars[j].last)/1000.0;

      for (int i=0; i < STARBURST_MAX_FRAG; i++) {
        int var = i >> 1;

        if (stars[j].fragment[i] > 0) {
          //all fragments travel right, will be mirrored on other side
          stars[j].fragment[i] += stars[j].vel * dt * (float)var/3.0;
        }
      }
      stars[j].last = it;
      stars[j].vel -= 3*stars[j].vel*dt;
    }

    CRGB c = stars[j].color;

    // If the star is brand new, it flashes white briefly.
    // Otherwise it just fades over time.
    float fade = 0.0f;
    float age = it-stars[j].birth;

    if (age < particleIgnition) {
      c = CRGB(color_blend(WHITE, RGBW32(c.r,c.g,c.b,0), uint8_t(254.5f*((age / particleIgnition)))));
    } else {
      // Figure out how much to fade and shrink the star based on
      // its age relative to its lifetime
      if (age > particleIgnition + particleFadeTime) {
        fade = 1.0f;                  // Black hole, all faded out
        stars[j].birth = 0;
        c = CRGB(SEGCOLOR(1));
      } else {
        age -= particleIgnition;
        fade = (age / particleFadeTime);  // Fading star
        c = CRGB(color_blend(RGBW32(c.r,c.g,c.b,0), SEGCOLOR(1), uint8_t(254.5f*fade)));
      }
    }

    float particleSize = (1.0f - fade) * 2.0f;

    for (size_t index=0; index < STARBURST_MAX_FRAG*2; index++) {
      bool mirrored = index & 0x1;
      unsigned i = index >> 1;
      if (stars[j].fragment[i] > 0) {
        float loc = stars[j].fragment[i];
        if (mirrored) loc -= (loc-stars[j].pos)*2;
        unsigned start = loc - particleSize;
        unsigned end = loc + particleSize;
        if (start < 0) start = 0;
        if (start == end) end++;
        if (end > SEGLEN) end = SEGLEN;
        for (unsigned p = start; p < end; p++) {
          SEGMENT.setPixelColor(p, c);
        }
      }
    }
  }
}
#undef STARBURST_MAX_FRAG

// END UPSTREAM FX.cpp starburst.
static_assert(sizeof(star)==60u,"Pinned ESP32 10-fragment star must be 60 bytes");
#pragma GCC diagnostic pop

// BEGIN UPSTREAM FX.cpp Tetrix (verbatim).
typedef struct Tetris {
  float    pos;
  float    speed;
  uint8_t  col;   // color index
  uint16_t brick; // brick size in pixels
  uint16_t stack; // stack size in pixels
  uint32_t step;  // 2D-fication of SEGENV.step (state)
} tetris;

void mode_tetrix(void) {
  if (SEGLEN <= 1) FX_FALLBACK_STATIC;
  unsigned strips = SEGMENT.nrOfVStrips(); // allow running on virtual strips (columns in 2D segment)
  unsigned dataSize = sizeof(tetris);
  if (!SEGENV.allocateData(dataSize * strips)) FX_FALLBACK_STATIC; //allocation failed
  Tetris* drops = reinterpret_cast<Tetris*>(SEGENV.data);

  //if (SEGENV.call == 0) SEGMENT.fill(SEGCOLOR(1));  // will fill entire segment (1D or 2D), then use drop->step = 0 below

  // virtualStrip idea by @ewowi (Ewoud Wijma)
  // requires virtual strip # to be embedded into upper 16 bits of index in setPixelcolor()
  // the following functions will not work on virtual strips: fill(), fade_out(), fadeToBlack(), blur()
  struct virtualStrip {
    static void runStrip(size_t stripNr, Tetris *drop) {
      // initialize dropping on first call or segment full
      if (SEGENV.call == 0) {
        drop->stack = 0;                  // reset brick stack size
        drop->step = strip.now + 2000;     // start by fading out strip
        if (SEGMENT.check1) drop->col = 0;// use only one color from palette
      }

      if (drop->step == 0) {              // init brick
        // speed calculation: a single brick should reach bottom of strip in X seconds
        // if the speed is set to 1 this should take 5s and at 255 it should take 0.25s
        // as this is dependant on SEGLEN it should be taken into account and the fact that effect runs every FRAMETIME s
        int speed = SEGMENT.speed ? SEGMENT.speed : hw_random8(1,255);
        speed = map(speed, 1, 255, 5000, 250); // time taken for full (SEGLEN) drop
        drop->speed = float(SEGLEN * FRAMETIME) / float(speed); // set speed
        drop->pos   = SEGLEN;             // start at end of segment (no need to subtract 1)
        if (!SEGMENT.check1) drop->col = hw_random8(0,15)<<4;   // limit color choices so there is enough HUE gap
        drop->step  = 1;                  // drop state (0 init, 1 forming, 2 falling)
        drop->brick = (SEGMENT.intensity ? (SEGMENT.intensity>>5)+1 : hw_random8(1,5)) * (1+(SEGLEN>>6));  // size of brick
      }

      if (drop->step == 1) {              // forming
        if (hw_random8()>>6) {               // random drop
          drop->step = 2;                 // fall
        }
      }

      if (drop->step == 2) {              // falling
        if (drop->pos > drop->stack) {    // fall until top of stack
          drop->pos -= drop->speed;       // may add gravity as: speed += gravity
          if (int(drop->pos) < int(drop->stack)) drop->pos = drop->stack;
          for (unsigned i = unsigned(drop->pos); i < SEGLEN; i++) {
            uint32_t col = i < unsigned(drop->pos)+drop->brick ? SEGMENT.color_from_palette(drop->col, false, false, 0) : SEGCOLOR(1);
            SEGMENT.setPixelColor(indexToVStrip(i, stripNr), col);
          }
        } else {                          // we hit bottom
          drop->step = 0;                 // proceed with next brick, go back to init
          drop->stack += drop->brick;     // increase the stack size
          if (drop->stack >= SEGLEN) drop->step = strip.now + 2000; // fade out stack
        }
      }

      if (drop->step > 2) {               // fade strip
        drop->brick = 0;                  // reset brick size (no more growing)
        if (drop->step > strip.now) {
          // allow fading of virtual strip
          for (unsigned i = 0; i < SEGLEN; i++) SEGMENT.blendPixelColor(indexToVStrip(i, stripNr), SEGCOLOR(1), 25); // 10% blend
        } else {
          drop->stack = 0;                // reset brick stack size
          drop->step = 0;                 // proceed with next brick
          if (SEGMENT.check1) drop->col += 8;   // gradually increase palette index
        }
      }
    }
  };

  for (unsigned stripNr=0; stripNr<strips; stripNr++)
    virtualStrip::runStrip(stripNr, &drops[stripNr]);
}

static_assert(sizeof(tetris)==20u,"Pinned Tetris C++ data must be 20 bytes");
// END UPSTREAM FX.cpp Tetrix.

// BEGIN UPSTREAM FX.cpp Flow Stripe, original source.
void mode_FlowStripe(void) {
  if (SEGLEN <= 1) FX_FALLBACK_STATIC;
  const int hl = SEGLEN * 10 / 13;
  uint8_t hue = strip.now / (SEGMENT.speed+1);
  uint32_t t = strip.now / (SEGMENT.intensity/8+1);

  for (unsigned i = 0; i < SEGLEN; i++) {
    int c = ((abs((int)i - hl) * 127) / hl);
    c = sin8_t(c);
    c = sin8_t(c / 2 + t);
    byte b = sin8_t(c + t/8);
    SEGMENT.setPixelColor(i, SEGMENT.color_from_palette(b + hue, false, true, 3));
  }
} // mode_FlowStripe()

// END UPSTREAM FX.cpp Flow Stripe.

// BEGIN UPSTREAM FX.cpp Glitter (verbatim), default paletteBlend=0.
#define ULTRAWHITE 0x00FFFFFFu
static uint8_t paletteBlend=0u;
void glitter_base(uint8_t intensity, uint32_t col = ULTRAWHITE) {
  if (intensity > hw_random8()) SEGMENT.setPixelColor(hw_random16(SEGLEN), col);
}

//Glitter with palette background, inspired by https://gist.github.com/kriegsman/062e10f7f07ba8518af6
void mode_glitter()
{
  if (!SEGMENT.check2) { // use "* Color 1" palette for solid background (replacing "Solid glitter")
    unsigned counter = 0;
    if (SEGMENT.speed != 0) {
      counter = (strip.now * ((SEGMENT.speed >> 3) +1)) & 0xFFFF;
      counter = counter >> 8;
    }

    bool noWrap = (paletteBlend == 2 || (paletteBlend == 0 && SEGMENT.speed == 0));
    for (unsigned i = 0; i < SEGLEN; i++) {
      unsigned colorIndex = (i * 255 / SEGLEN) - counter;
      if (noWrap) colorIndex = map(colorIndex, 0, 255, 0, 240); //cut off blend at palette "end"
      SEGMENT.setPixelColor(i, SEGMENT.color_from_palette(colorIndex, false, true, 255));
    }
  }
  glitter_base(SEGMENT.intensity, SEGCOLOR(2) ? SEGCOLOR(2) : ULTRAWHITE);
}

#undef ULTRAWHITE
// END UPSTREAM FX.cpp Glitter.

// BEGIN original pinned fastled_slim.cpp 8-bit quadwave math.
uint8_t ease8InOutQuad(uint8_t i)
{
  uint32_t j = i;
  if (j & 0x80) j = 255 - j; // mirror if > 127
  uint32_t jj = (j * j) >> 7;
  return (i & 0x80) ? (255 - jj) : jj;
}

// triangular wave generator
// quadratic waveform generator. Spends just a little more time at the limits than "sine" does.
uint8_t quadwave8(uint8_t in) {
  return ease8InOutQuad(triwave8(in));
}


// BEGIN original pinned FX.cpp mode_blends.
void mode_blends(void) {
  unsigned pixelLen = SEGLEN > UINT8_MAX ? UINT8_MAX : SEGLEN;
  unsigned dataSize = sizeof(uint32_t) * (pixelLen + 1);  // max segment length of 56 pixels on 16 segment ESP8266
  if (!SEGENV.allocateData(dataSize)) FX_FALLBACK_STATIC; //allocation failed
  uint32_t* pixels = reinterpret_cast<uint32_t*>(SEGENV.data);
  uint8_t blendSpeed = map(SEGMENT.intensity, 0, UINT8_MAX, 10, 128);
  unsigned shift = (strip.now * ((SEGMENT.speed >> 3) +1)) >> 8;

  for (unsigned i = 0; i < pixelLen; i++) {
    pixels[i] = color_blend(pixels[i], SEGMENT.color_from_palette(shift + quadwave8((i + 1) * 16), false, PALETTE_SOLID_WRAP, 255), blendSpeed);
    shift += 3;
  }

  unsigned offset = 0;
  for (unsigned i = 0; i < SEGLEN; i++) {
    SEGMENT.setPixelColor(i, pixels[offset++]);
    if (offset >= pixelLen) offset = 0;
  }
}

// END original pinned Blends.


/* Reference pin: original ESP32 branch uses float sin_t/cos_t. */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define M_TWOPI 6.28318530717958647692
static float sin_t(float x){return std::sin(x);}
static float cos_t(float x){return std::cos(x);}
// BEGIN PINNED FX.cpp mode_palette(), unchanged source body.
void mode_palette() {
  // Set up some compile time constants so that we can handle integer and float based modes using the same code base.
#ifdef ESP8266
  using mathType = int32_t;
  using wideMathType = int64_t;
  using angleType = unsigned;
  constexpr mathType sInt16Scale             = 0x7FFF;
  constexpr mathType maxAngle                = 0x8000;
  constexpr mathType staticRotationScale     = 256;
  constexpr mathType animatedRotationScale   = 1;
  constexpr int16_t (*sinFunction)(uint16_t) = &sin16_t;
  constexpr int16_t (*cosFunction)(uint16_t) = &cos16_t;
#else
  using mathType = float;
  using wideMathType = float;
  using angleType = float;
  constexpr mathType sInt16Scale           = 1.0f;
  constexpr mathType maxAngle              = M_PI / 256.0;
  constexpr mathType staticRotationScale   = 1.0f;
  constexpr mathType animatedRotationScale = M_TWOPI / double(0xFFFF);
  constexpr float (*sinFunction)(float)    = &sin_t;
  constexpr float (*cosFunction)(float)    = &cos_t;
#endif
  const bool isMatrix = strip.isMatrix;
  const int cols = SEG_W;
  const int rows = isMatrix ? SEG_H : strip.getActiveSegmentsNum();

  const int  inputShift           = SEGMENT.speed;
  const int  inputSize            = SEGMENT.intensity;
  const int  inputRotation        = SEGMENT.custom1;
  const bool inputAnimateShift    = SEGMENT.check1;
  const bool inputAnimateRotation = SEGMENT.check2;
  const bool inputAssumeSquare    = SEGMENT.check3;

  const angleType theta = (!inputAnimateRotation) ? ((inputRotation + 128) * maxAngle / staticRotationScale) : (((strip.now * ((inputRotation >> 4) +1)) & 0xFFFF) * animatedRotationScale);
  const mathType sinTheta = sinFunction(theta);
  const mathType cosTheta = cosFunction(theta);

  const mathType maxX    = std::max(1, cols-1);
  const mathType maxY    = std::max(1, rows-1);
  // Set up some parameters according to inputAssumeSquare, so that we can handle anamorphic mode using the same code base.
  const mathType maxXIn  =  inputAssumeSquare ? maxX : mathType(1);
  const mathType maxYIn  =  inputAssumeSquare ? maxY : mathType(1);
  const mathType maxXOut = !inputAssumeSquare ? maxX : mathType(1);
  const mathType maxYOut = !inputAssumeSquare ? maxY : mathType(1);
  const mathType centerX = sInt16Scale * maxXOut / mathType(2);
  const mathType centerY = sInt16Scale * maxYOut / mathType(2);
  // The basic idea for this effect is to rotate a rectangle that is filled with the palette along one axis, then map our
  // display to it, to find what color a pixel should have.
  // However, we want a) no areas of solid color (in front of or behind the palette), and b) we want to make use of the full palette.
  // So the rectangle needs to have exactly the right size. That size depends on the rotation.
  // This scale computation here only considers one dimension. You can think of it like the rectangle is always scaled so that
  // the left and right most points always match the left and right side of the display.
  const mathType scale = std::abs(sinTheta) + (std::abs(cosTheta) * maxYOut / maxXOut);
  // 2D simulation:
  // If we are dealing with a 1D setup, we assume that each segment represents one line on a 2-dimensional display.
  // The function is called once per segments, so we need to handle one line at a time.
  const int yFrom = isMatrix ? 0 : strip.getCurrSegmentId();
  const int yTo   = isMatrix ? maxY : yFrom;
  for (int y = yFrom; y <= yTo; ++y) {
    // translate, scale, rotate
    const mathType ytCosTheta = mathType((wideMathType(cosTheta) * wideMathType(y * sInt16Scale - centerY * maxYIn))/wideMathType(maxYIn * scale));
    for (int x = 0; x < cols; ++x) {
      // translate, scale, rotate
      const mathType xtSinTheta = mathType((wideMathType(sinTheta) * wideMathType(x * sInt16Scale - centerX * maxXIn))/wideMathType(maxXIn * scale));
      // Map the pixel coordinate to an imaginary-rectangle-coordinate.
      // The y coordinate doesn't actually matter, as our imaginary rectangle is filled with the palette from left to right,
      // so all points at a given x-coordinate have the same color.
      const mathType sourceX = xtSinTheta + ytCosTheta + centerX;
      // The computation was scaled just right so that the result should always be in range [0, maxXOut], but enforce this anyway
      // to account for imprecision. Then scale it so that the range is [0, 255], which we can use with the palette.
      int colorIndex = (std::min(std::max(sourceX, mathType(0)), maxXOut * sInt16Scale) * wideMathType(255)) / (sInt16Scale * maxXOut);
      // inputSize determines by how much we want to scale the palette:
      // values < 128 display a fraction of a palette,
      // values > 128 display multiple palettes.
      if (inputSize <= 128) {
        colorIndex = (colorIndex * inputSize) / 128;
      } else {
        // Linear function that maps colorIndex 128=>1, 256=>9.
        // With this function every full palette repetition is exactly 16 configuration steps wide.
        // That allows displaying exactly 2 repetitions for example.
        colorIndex = ((inputSize - 112) * colorIndex) / 16;
      }
      // Finally, shift the palette a bit.
      const int paletteOffset = (!inputAnimateShift) ? (inputShift) : (((strip.now * ((inputShift >> 3) +1)) & 0xFFFF) >> 8);
      colorIndex -= paletteOffset;
      const uint32_t color = SEGMENT.color_wheel((uint8_t)colorIndex);
      if (isMatrix) {
        SEGMENT.setPixelColorXY(x, y, color);
      } else {
        SEGMENT.setPixelColor(x, color);
      }
    }
  }
}

// END PINNED Palette source.

/* Host shim for pinned GammaWLEDMethod::calcGammaTable(2.2).
 * WLED RGB inverse gamma is applied before its later output gamma stage. */
static uint8_t ref_gamma8inv(uint8_t value) {
    if(value==0u)return 0u;
    const float inverse=1.0f/2.2f;
    return (uint8_t)(powf(((float)value-0.5f)/255.0f,inverse)*255.0f+0.5f);
}
static uint8_t qsub8(uint8_t i,uint8_t j){return i>j?(uint8_t)(i-j):0u;}
#define gamma8inv(x) ref_gamma8inv((uint8_t)(x))
#define SEGPALETTE ref_palette(SEGMENT.palette,SEGMENT.colors[0],SEGMENT.colors[1],SEGMENT.colors[2])
// BEGIN PINNED FX.cpp Twinklefox/Twinklecat original source bodies.
CRGBW twinklefox_one_twinkle(uint32_t ms, uint8_t salt, bool cat)
{
  // Overall twinkle speed (changed)
  unsigned ticks = ms / SEGENV.aux0;
  unsigned fastcycle8 = uint8_t(ticks);
  uint16_t slowcycle16 = (ticks >> 8) + salt;
  slowcycle16 += sin8_t(slowcycle16);
  slowcycle16 = (slowcycle16 * 2053) + 1384;
  uint8_t slowcycle8 = (slowcycle16 & 0xFF) + (slowcycle16 >> 8);

  // Overall twinkle density.
  // 0 (NONE lit) to 8 (ALL lit at once).
  // Default is 5.
  unsigned twinkleDensity = (SEGMENT.intensity >> 5) +1;

  unsigned bright = 0;
  if (((slowcycle8 & 0x0E)/2) < twinkleDensity) {
    unsigned ph = fastcycle8;
    // This is like 'triwave8', which produces a
    // symmetrical up-and-down triangle sawtooth waveform, except that this
    // function produces a triangle wave with a faster attack and a slower decay
    if (cat) { //twinklecat, variant where the leds instantly turn on and fade off
      bright = 255 - ph;
      if (SEGMENT.check2) { //reverse checkbox, reverses the leds to fade on and instantly turn off
        bright = ph;
      }
    } else { //vanilla twinklefox
      if (ph < 86) {
      bright = ph * 3;
      } else {
        ph -= 86;
        bright = 255 - (ph + (ph/2));
      }
    }
  }

  unsigned hue = slowcycle8 - salt;
  CRGBW c;
  if (bright > 0) {
    c = ColorFromPalette(SEGPALETTE, hue, gamma8inv(bright), NOBLEND); // note on gamma8inv: inverting results in non-linear brightness fade as originally designed
    if (!SEGMENT.check1) {
      // This code takes a pixel, and if its in the 'fading down'
      // part of the cycle, it adjusts the color a little bit like the
      // way that incandescent bulbs fade toward 'red' as they dim.
      if (fastcycle8 >= 128)
      {
        unsigned cooling = (fastcycle8 - 128) >> 4;
        c.g = qsub8(c.g, cooling);
        c.b = qsub8(c.b, cooling * 2);
      }
    }
  } else {
    c = 0; // black
  }
  return c;
}

//  This function loops over each pixel, calculates the
//  adjusted 'clock' that this pixel should use, and calls
//  "CalculateOneTwinkle" on each pixel.  It then displays
//  either the twinkle color of the background color,
//  whichever is brighter.
static void twinklefox_base(bool cat)
{
  // "PRNG16" is the pseudorandom number generator
  // It MUST be reset to the same starting value each time
  // this function is called, so that the sequence of 'random'
  // numbers that it generates is (paradoxically) stable.
  uint16_t PRNG16 = 11337;

  // Calculate speed
  if (SEGMENT.speed > 100) SEGENV.aux0 = 3 + ((255 - SEGMENT.speed) >> 3);
  else SEGENV.aux0 = 22 + ((100 - SEGMENT.speed) >> 1);

  // Set up the background color, "bg". Note: using gamma invert for brightness as the FX was written without any gamma correction, it will dim down too much now
  CRGBW bg = SEGCOLOR(1);
  unsigned bglight = bg.getRGBaverage();
  if (bglight > 64) {
    bg = color_fade(bg, gamma8inv(16), true); // very bright, so scale to 1/16th
  } else if (bglight > 16) {
    bg = color_fade(bg, gamma8inv(64), true); // not that bright, so scale to 1/4
  } else {
    bg = color_fade(bg, gamma8inv(86), true); // dim, scale to 1/3rd
  }

  bglight = bg.getRGBaverage(); // update after scaling

  for (unsigned i = 0; i < SEGLEN; i++) {

    PRNG16 = (uint16_t)(PRNG16 * 2053) + 1384; // next 'random' number
    unsigned myclockoffset16= PRNG16; // use that number as clock offset
    PRNG16 = (uint16_t)(PRNG16 * 2053) + 1384; // next 'random' number
    // use that number as clock speed adjustment factor (in 8ths, from 8/8ths to 23/8ths)
    unsigned myspeedmultiplierQ5_3 =  ((((PRNG16 & 0xFF)>>4) + (PRNG16 & 0x0F)) & 0x0F) + 0x08;
    uint32_t myclock30 = (uint32_t)((strip.now * myspeedmultiplierQ5_3) >> 3) + myclockoffset16;
    unsigned  myunique8 = PRNG16 >> 8; // get 'salt' value for this pixel

    // We now have the adjusted 'clock' for this pixel, now we call
    // the function that computes what color the pixel should be based
    // on the "brightness = f( time )" idea.
    CRGBW c = twinklefox_one_twinkle(myclock30, myunique8, cat);

    unsigned cbright = c.getRGBaverage();
    int deltabright = cbright - bglight;
    if (deltabright >= 32 || (bg==0)) {
      // If the new pixel is significantly brighter than the background color,
      // use the new color.
      SEGMENT.setPixelColor(i, c);
    } else if (deltabright > 0) {
      // If the new pixel is just slightly brighter than the background color,
      // mix a blend of the new color and the background color
      SEGMENT.setPixelColor(i, color_blend(bg, c, uint8_t(deltabright * 8)));
    } else {
      // if the new pixel is not at all brighter than the background color,
      // just use the background color.
      SEGMENT.setPixelColor(i, bg);
    }
  }
}

void mode_twinklefox()
{
  twinklefox_base(false);
}


void mode_twinklecat()
{
  twinklefox_base(true);
}

// END PINNED Twinklefox/Twinklecat sources.
#undef SEGPALETTE
#undef gamma8inv

// BEGIN pinned WLED FX.cpp AuroraWave and mode_aurora source.
#define gamma8inv(x) ref_gamma8inv((uint8_t)(x))
#ifdef ESP8266
  #define W_MAX_COUNT  9          //Number of simultaneous waves
#else
  #define W_MAX_COUNT 20          //Number of simultaneous waves
#endif
#define W_MAX_SPEED 6             //Higher number, higher speed
#define W_WIDTH_FACTOR 6          //Higher number, smaller waves

// fixed-point math scaling
#define AW_SHIFT 16
#define AW_SCALE (1 << AW_SHIFT)  // 65536 representing 1.0

// 32 bytes
class AuroraWave {
  private:
    int32_t center;               // scaled by AW_SCALE
    uint32_t ageFactor_cached;    // cached age factor scaled by AW_SCALE
    uint16_t ttl;
    uint16_t age;
    uint16_t width;
    uint16_t basealpha;           // scaled by AW_SCALE
    uint16_t speed_factor;        // scaled by AW_SCALE
    int16_t  wave_start;          // wave start LED index
    int16_t  wave_end;            // wave end LED index
    bool goingleft;
    bool alive = true;
    CRGBW basecolor;

  public:
    void init(uint32_t segment_length, CRGBW color) {
      ttl = hw_random16(500, 1501);
      basecolor = color;
      basealpha = hw_random8(60, 100) * AW_SCALE / 100; // 0-99% note: if using 100% there is risk of integer overflow
      age = 0;
      width = hw_random16(segment_length / 20, segment_length / W_WIDTH_FACTOR) + 1;
      center = (((uint32_t)hw_random8(101) << AW_SHIFT) / 100) * segment_length; // 0-100%
      goingleft = hw_random8() & 0x01; // 50/50 chance
      speed_factor = (((uint32_t)hw_random8(10, 31) * W_MAX_SPEED) << AW_SHIFT) / (100 * 255);
      alive = true;
    }

    void updateCachedValues() {
      uint32_t half_ttl = ttl >> 1;
      if (age < half_ttl) {
        ageFactor_cached = ((uint32_t)age << AW_SHIFT) / half_ttl;
      } else {
        ageFactor_cached = ((uint32_t)(ttl - age) << AW_SHIFT) / half_ttl;
      }
      if (ageFactor_cached >= AW_SCALE) ageFactor_cached = AW_SCALE - 1; // prevent overflow

      uint32_t center_led = center >> AW_SHIFT;
      wave_start = (int16_t)center_led - (int16_t)width;
      wave_end = (int16_t)center_led + (int16_t)width;
    }

    CRGBW getColorForLED(int ledIndex) {
      // linear brightness falloff from center to edge of wave
      if (ledIndex < wave_start || ledIndex > wave_end) return 0;
      int32_t ledIndex_scaled = (int32_t)ledIndex << AW_SHIFT;
      int32_t offset = ledIndex_scaled - center;
      if (offset < 0) offset = -offset;
      uint32_t offsetFactor = offset / width;  // scaled by AW_SCALE
      if (offsetFactor > AW_SCALE) return 0;   // outside of wave
      uint32_t brightness_factor = (AW_SCALE - offsetFactor);
      brightness_factor = (brightness_factor * ageFactor_cached) >> AW_SHIFT;
      brightness_factor = (brightness_factor * basealpha) >> AW_SHIFT;

      CRGBW rgb;
      rgb.r = (basecolor.r * brightness_factor) >> AW_SHIFT;
      rgb.g = (basecolor.g * brightness_factor) >> AW_SHIFT;
      rgb.b = (basecolor.b * brightness_factor) >> AW_SHIFT;
      rgb.w = (basecolor.w * brightness_factor) >> AW_SHIFT;

      return rgb;
    };

    //Change position and age of wave
    //Determine if its still "alive"
    void update(uint32_t segment_length, uint32_t speed) {
      int32_t step = speed_factor * speed;
      center += goingleft ? -step : step;
      age++;

      if (age > ttl) {
        alive = false;
      } else {
        uint32_t width_scaled = (uint32_t)width << AW_SHIFT;
        uint32_t segment_length_scaled = segment_length << AW_SHIFT;

         if (goingleft) {
           if (center < - (int32_t)width_scaled) {
             alive = false;
           }
         } else {
           if (center > (int32_t)segment_length_scaled + (int32_t)width_scaled) {
             alive = false;
           }
         }
      }
    };

    bool stillAlive() { return alive; }
};

void mode_aurora(void) {
  AuroraWave* waves;
  SEGENV.aux1 = map(SEGMENT.intensity, 0, 255, 2, W_MAX_COUNT); // aux1 = Wavecount
  if (!SEGENV.allocateData(sizeof(AuroraWave) * SEGENV.aux1)) {
    FX_FALLBACK_STATIC;
  }
  waves = reinterpret_cast<AuroraWave*>(SEGENV.data);

  // note: on first call, SEGENV.data is zero -> all waves are dead and will be initialized
  for (int i = 0; i < SEGENV.aux1; i++) {
    waves[i].update(SEGLEN, SEGMENT.speed);
    if (!(waves[i].stillAlive())) {
      waves[i].init(SEGLEN, SEGMENT.color_from_palette(hw_random8(), false, false, hw_random8(0, 3)));
    }
    waves[i].updateCachedValues();
  }

  uint8_t backlight = 0; // note: original code used 1, with inverse gamma applied background would never be black
  if (SEGCOLOR(0)) backlight++;
  if (SEGCOLOR(1)) backlight++;
  if (SEGCOLOR(2)) backlight++;
  backlight = gamma8inv(backlight); // preserve backlight when using gamma correction

  for (unsigned i = 0; i < SEGLEN; i++) {
    CRGBW mixedRgb = CRGBW(backlight, backlight, backlight);

    for (int j = 0; j < SEGENV.aux1; j++) {
      CRGBW rgb = waves[j].getColorForLED(i);
      mixedRgb = color_add(mixedRgb, rgb); // sum all waves influencing this pixel
    }

    SEGMENT.setPixelColor(i, mixedRgb);
  }
}

#undef gamma8inv
// END pinned Aurora source.

/* Bit ownership and fixed frame time are host adapters; the function
 * below is the exact pinned FX.cpp body. */
#define FRAMETIME_FIXED 23u
#define bitRead(x,n) (((x)>>(n))&1u)
#define bitWrite(x,n,v) do { if(v)(x)|=(uint8_t)(1u<<(n)); else (x)&=(uint8_t)~(1u<<(n)); } while(0)
#define gamma8inv(x) ref_gamma8inv((uint8_t)(x))
#define SEGPALETTE ref_palette(SEGMENT.palette,SEGMENT.colors[0],SEGMENT.colors[1],SEGMENT.colors[2])
// BEGIN PINNED mode_colortwinkle original body.
void mode_colortwinkle() {
  unsigned dataSize = (SEGLEN+7) >> 3; //1 bit per LED
  if (!SEGENV.allocateData(dataSize)) FX_FALLBACK_STATIC; //allocation failed

  //limit update rate
  if (strip.now - SEGENV.step < FRAMETIME_FIXED) return;
  SEGENV.step = strip.now;

  CRGBW col, prev;
  uint8_t fadeUpAmount = strip.getBrightness()>28 ? 8 + (SEGMENT.speed>>2) : 68-strip.getBrightness();
  uint8_t fadeDownAmount = strip.getBrightness()>28 ? 8 + (SEGMENT.speed>>3) : 68-strip.getBrightness();
  for (unsigned i = 0; i < SEGLEN; i++) {
    CRGBW cur = SEGMENT.getPixelColor(i);
    prev = cur;
    unsigned index = i >> 3;
    unsigned  bitNum = i & 0x07;
    bool fadeUp = bitRead(SEGENV.data[index], bitNum);

    if (fadeUp) {
      CRGBW incrementalColor = color_fade(cur, fadeUpAmount, true);
      col = color_add(cur, incrementalColor);

      if (col.r == 255 || col.g == 255 || col.b == 255) {
        bitWrite(SEGENV.data[index], bitNum, false);
      }

      if (col == cur) {  // color_add did nothing, fix "stuck" pixels by adding the color to itself
        col = color_add(col, col);
      }
      SEGMENT.setPixelColor(i, col);
    }
    else {
      col = color_fade(cur, 255 - fadeDownAmount, false);
      SEGMENT.setPixelColor(i, col);
    }
  }

  for (unsigned j = 0; j <= SEGLEN / 50; j++) {
    if (hw_random8() <= SEGMENT.intensity) {
      for (unsigned times = 0; times < 5; times++) { //attempt to spawn a new pixel 5 times
        int i = hw_random16(SEGLEN);
        if (SEGMENT.getPixelColor(i) == 0) {
          unsigned index = i >> 3;
          unsigned  bitNum = i & 0x07;
          bitWrite(SEGENV.data[index], bitNum, true);
          SEGMENT.setPixelColor(i, ColorFromPalette(SEGPALETTE, hw_random8(), gamma8inv(64), NOBLEND));  // note on gamma8inv: inverting results in non-linear brightness fade as originally designed
          break; //only spawn 1 new pixel per frame per 50 LEDs
        }
      }
    }
  }
}

// END PINNED Colortwinkles.
#undef FRAMETIME_FIXED
#undef bitRead
#undef bitWrite
#undef gamma8inv
#undef SEGPALETTE

#define SEGPALETTE ref_palette(SEGMENT.palette,SEGMENT.colors[0],SEGMENT.colors[1],SEGMENT.colors[2])
#define W(color) (uint8_t)((uint32_t)(color)>>24)
#define millis() strip.now
// BEGIN PINNED FX.cpp Slow Transition original body.
typedef struct SlowTransitionData {
  CRGBPalette16 startPalette;        // initial palette
  CRGBPalette16 currentPalette;      // blended palette for current frame, need permanent storage so we can start from this if target changes mid transition
  CRGBPalette16 endPalette;          // target palette
  uint8_t startWhite;
  uint8_t currentWhite;
  uint8_t endWhite;
  uint8_t startCCT;
  uint8_t currentCCT;
  uint8_t endCCT;
} slow_transition_data;

void mode_slow_transition(void) {
  // aliases
  uint32_t* startTime = &SEGMENT.step;  // use step to store start time of transition
  uint16_t* stepsDone = &SEGMENT.aux0;
  uint16_t* startSpeed = &SEGMENT.aux1; // speed setting at the start of the transition, used to detect changes

  size_t dataSize = sizeof(slow_transition_data);
  if (!SEGMENT.allocateData(dataSize)) FX_FALLBACK_STATIC;
  slow_transition_data* data = reinterpret_cast<slow_transition_data*>(SEGMENT.data);
  // Note: compare currentCCT (not endCCT). SEGMENT.cct is set to currentCCT at the end of each call, if they differ, it was changed externally
  bool changed = (data->endPalette != SEGPALETTE || *startSpeed != SEGMENT.speed || data->endWhite != W(SEGCOLOR(0)) || data->currentCCT != SEGMENT.cct); // detect changes in target color or speed setting

  // (re) init
  if (changed || SEGMENT.call == 0) {
    if (SEGMENT.call == 0) {
      data->startPalette = SEGPALETTE;
      data->currentPalette = SEGPALETTE;
      data->endPalette = SEGPALETTE;
      data->startWhite = data->currentWhite = data->endWhite = W(SEGCOLOR(0));
      data->startCCT = data->currentCCT = data->endCCT = SEGMENT.cct;
      *stepsDone = 0xFFFF; // set to max, fading will start once a change is detected
    }
    else {
      data->startPalette = data->currentPalette;
      data->endPalette = SEGPALETTE;
      data->startWhite = data->currentWhite;
      data->endWhite = W(SEGCOLOR(0));
      data->startCCT = data->currentCCT;
      data->endCCT = SEGMENT.cct;
      *stepsDone = 0; // reset counter
    }
    *startSpeed = SEGMENT.speed;
    *startTime = millis(); // set start time note: intentionally not using strip.now as this is based on real time
  }

  uint32_t totalSteps = SEGMENT.check2 ? 16 * 255 : 255;
  uint32_t duration = (SEGMENT.speed == 0) ? 10000 : (uint32_t)SEGMENT.speed * 60000; // 10s if zero (good for testing), otherwise map 1-255 to 1-255 minutes
  uint32_t elapsed = millis() - *startTime; // note: will overflow after ~50 days if just left alone (edge case unhandled)
  uint32_t expectedSteps = (uint64_t)elapsed * totalSteps / duration;
  expectedSteps = min(expectedSteps, totalSteps); // limit to total steps

  if (*stepsDone > expectedSteps)
    *stepsDone = expectedSteps;// in case sweep was disabled mid transition

  if (*stepsDone < expectedSteps) {
    *stepsDone = expectedSteps; // jump to expected steps to make sure timing is correct (need up to 4080 frames, at 20fps that is ~200 seconds)
    uint8_t blendAmount;
    if (SEGMENT.check2) {
      // sweep: one palette entry at a time
      uint8_t i = *stepsDone % 16;
      blendAmount  = *stepsDone / 16;
      data->currentPalette[i] = CRGB(color_blend(CRGBW(data->startPalette[i]), CRGBW(data->endPalette[i]), blendAmount));
    } else {
      // full palette at once
      blendAmount = (uint8_t)*stepsDone;
      for (uint8_t i = 0; i < 16; i++) {
        data->currentPalette[i] = CRGB(color_blend(CRGBW(data->startPalette[i]), CRGBW(data->endPalette[i]), blendAmount));
      }
    }
    data->currentWhite = (data->startWhite * (255 - blendAmount) + data->endWhite * blendAmount) / 255;
    data->currentCCT = (data->startCCT * (255 - blendAmount) + data->endCCT * blendAmount) / 255;
    if (*stepsDone >= totalSteps) {
      // transition complete, apply end palette
      data->currentPalette = data->endPalette; // set to end palette (sweep may not have set all entries)
      data->currentWhite = data->endWhite;
      data->currentCCT = data->endCCT;
    }
  }
  // display current palette (plus white) over segment
  for (unsigned i = 0; i < SEGLEN; i++) {
    uint8_t paletteIndex = (i * 255) / SEGLEN;
    CRGBW palcol = ColorFromPalette(data->currentPalette, paletteIndex, 255, LINEARBLEND_NOWRAP);
    palcol.w = data->currentWhite; // TODO: currently "sweep mode" does not support white sweep
    SEGMENT.setPixelColor(i, palcol.color32);
  }
  SEGMENT.cct = data->currentCCT;
}

// END PINNED Slow Transition.
#undef SEGPALETTE
#undef W
#undef millis

#define SPOT_TYPE_SOLID 0
#define SPOT_TYPE_GRADIENT 1
#define SPOT_TYPE_2X_GRADIENT 2
#define SPOT_TYPE_2X_DOT 3
#define SPOT_TYPE_3X_DOT 4
#define SPOT_TYPE_4X_DOT 5
#define SPOT_TYPES_COUNT 6
#define SPOT_MAX_COUNT 49
// BEGIN PINNED FX.cpp spotlight struct and Dancing Shadows function.
typedef struct Spotlight {
  float speed;
  uint8_t colorIdx;
  int16_t position;
  unsigned long lastUpdateTime;
  uint8_t width;
  uint8_t type;
} spotlight;

/*
 * Spotlights moving back and forth that cast dancing shadows.
 * Shine this through tree branches/leaves or other close-up objects that cast
 * interesting shadows onto a ceiling or tarp.
 *
 * By Steve Pomeroy @xxv
 */
void mode_dancing_shadows(void)
{
  if (SEGLEN <= 1) FX_FALLBACK_STATIC;
  unsigned numSpotlights = map(SEGMENT.intensity, 0, 255, 2, SPOT_MAX_COUNT);  // 49 on 32 segment ESP32, 17 on 16 segment ESP8266
  bool initialize = SEGENV.aux0 != numSpotlights;
  SEGENV.aux0 = numSpotlights;

  unsigned dataSize = sizeof(spotlight) * numSpotlights;
  if (!SEGENV.allocateData(dataSize)) FX_FALLBACK_STATIC; //allocation failed
  Spotlight* spotlights = reinterpret_cast<Spotlight*>(SEGENV.data);

  SEGMENT.fill(BLACK);

  unsigned long time = strip.now;
  bool respawn = false;

  for (size_t i = 0; i < numSpotlights; i++) {
    if (!initialize) {
      // advance the position of the spotlight
      int delta = (float)(time - spotlights[i].lastUpdateTime) *
                  (spotlights[i].speed * ((1.0 + SEGMENT.speed)/100.0));

      if (abs(delta) >= 1) {
        spotlights[i].position += delta;
        spotlights[i].lastUpdateTime = time;
      }

      respawn = (spotlights[i].speed > 0.0 && spotlights[i].position > (int)(SEGLEN + 2))
             || (spotlights[i].speed < 0.0 && spotlights[i].position < -(spotlights[i].width + 2));
    }

    if (initialize || respawn) {
      spotlights[i].colorIdx = hw_random8();
      spotlights[i].width = hw_random8(1, 10);

      spotlights[i].speed = 1.0/hw_random8(4, 50);

      if (initialize) {
        spotlights[i].position = hw_random16(SEGLEN);
        spotlights[i].speed *= hw_random8(2) ? 1.0 : -1.0;
      } else {
        if (hw_random8(2)) {
          spotlights[i].position = SEGLEN + spotlights[i].width;
          spotlights[i].speed *= -1.0;
        }else {
          spotlights[i].position = -spotlights[i].width;
        }
      }

      spotlights[i].lastUpdateTime = time;
      spotlights[i].type = hw_random8(SPOT_TYPES_COUNT);
    }

    uint32_t color = SEGMENT.color_from_palette(spotlights[i].colorIdx, false, false, 255);
    int start = spotlights[i].position;

    if (spotlights[i].width <= 1) {
      if (start >= 0 && start < (int)SEGLEN) {
        SEGMENT.blendPixelColor(start, color, 128);
      }
    } else {
      switch (spotlights[i].type) {
        case SPOT_TYPE_SOLID:
          for (size_t j = 0; j < spotlights[i].width; j++) {
            if ((start + j) >= 0 && (start + j) < SEGLEN) {
              SEGMENT.blendPixelColor(start + j, color, 128);
            }
          }
        break;

        case SPOT_TYPE_GRADIENT:
          for (size_t j = 0; j < spotlights[i].width; j++) {
            if ((start + j) >= 0 && (start + j) < SEGLEN) {
              SEGMENT.blendPixelColor(start + j, color, cubicwave8(map(j, 0, spotlights[i].width - 1, 0, 255)));
            }
          }
        break;

        case SPOT_TYPE_2X_GRADIENT:
          for (size_t j = 0; j < spotlights[i].width; j++) {
            if ((start + j) >= 0 && (start + j) < SEGLEN) {
              SEGMENT.blendPixelColor(start + j, color, cubicwave8(2 * map(j, 0, spotlights[i].width - 1, 0, 255)));
            }
          }
        break;

        case SPOT_TYPE_2X_DOT:
          for (size_t j = 0; j < spotlights[i].width; j += 2) {
            if ((start + j) >= 0 && (start + j) < SEGLEN) {
              SEGMENT.blendPixelColor(start + j, color, 128);
            }
          }
        break;

        case SPOT_TYPE_3X_DOT:
          for (size_t j = 0; j < spotlights[i].width; j += 3) {
            if ((start + j) >= 0 && (start + j) < SEGLEN) {
              SEGMENT.blendPixelColor(start + j, color, 128);
            }
          }
        break;

        case SPOT_TYPE_4X_DOT:
          for (size_t j = 0; j < spotlights[i].width; j += 4) {
            if ((start + j) >= 0 && (start + j) < SEGLEN) {
              SEGMENT.blendPixelColor(start + j, color, 128);
            }
          }
        break;
      }
    }
  }
}

// END PINNED Dancing Shadows.

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
    {67,mode_colorwaves,"Colorwaves"},
    {49,mode_fairy,"Fairy"},
    {51,mode_fairytwinkle,"Fairytwinkle"},
    {91,mode_bouncing_balls,"Bouncing Balls"},
    {82,mode_halloween_eyes,"Halloween Eyes"},
    {48,mode_rolling_balls,"Rolling Balls"},
    {95,mode_popcorn,"Popcorn"},
    {96,mode_drip,"Drip"},
    {116,mode_tv_simulator,"TV Simulator"},
    {218,mode_ColorClouds,"Color Clouds"},
    {89,mode_starburst,"Fireworks Starburst"},
    {44,mode_tetrix,"Tetrix"},
    {179,mode_FlowStripe,"Flow Stripe"},
    {87,mode_glitter,"Glitter"},
    {115,mode_blends,"Blends"},
    {65,mode_palette,"Palette"},
    {80,mode_twinklefox,"Twinklefox"},
    {81,mode_twinklecat,"Twinklecat"},
    {38,mode_aurora,"Aurora"},
    {74,mode_colortwinkle,"Colortwinkles"},
    {219,mode_slow_transition,"Slow Transition"},
    {112,mode_dancing_shadows,"Dancing Shadows"}
};
static uint32_t pack(ot_wled_rgb_t x) {
    return ((uint32_t)x.r<<16)|((uint32_t)x.g<<8)|x.b;
}
int main() {

    /* Exhaustively compare all 60 pinned gradient tables against source
     * loadDynamicGradientPalette and verbatim ColorFromPalette sampling. */
    unsigned gradient_samples=0u;
    for(unsigned id=13u;id<=72u;id++) {
      const CRGBPalette16 original=upstream_gradient_palette((uint8_t)id);
      ot_wled_palette16_t built{};
      if(!ot_wled_palette_build((uint8_t)id,{123u,56u,9u},
                                  {3u,88u,174u},{0u,0u,0u},&built))
          std::abort();
      for(unsigned ix=0u;ix<256u;ix++) {
        for(unsigned bt:{0u,1u,32u,128u,255u}) {
          for(unsigned blend=0u;blend<=2u;blend++) {
            const uint32_t expected=ColorFromPalette(original,ix,(uint8_t)bt,
                                                     (TBlendType)blend)&0xffffffu;
            const uint32_t actual=pack(ot_wled_palette_sample(&built,(uint8_t)ix,
                        (uint8_t)bt,(ot_wled_palette_blend_t)blend));
            if(actual!=expected) {
              std::fprintf(stderr,"Palette mismatch id=%u index=%u brightness=%u blend=%u upstream=%06X CORE=%06X\n",
                            id,ix,bt,blend,(unsigned)expected,(unsigned)actual);
              return 1;
            }
            gradient_samples++;
          }
        }
      }
    }
    std::printf("WLED gradient catalog reference comparison passed: %u RGB samples across 60 palettes (13-72).\n",gradient_samples);

    unsigned checked=0;
    for(const RefCase& fx:refs) {
        const std::vector<unsigned> zone_lengths=
            (fx.id==89u || fx.id==44u) ?
            std::vector<unsigned>{2u,3u,7u,16u,31u,64u,255u,522u} :
            std::vector<unsigned>{2u,3u,7u,16u,31u};
        for(unsigned count: zone_lengths) {
            if(fx.id==82u && count<5u)continue; // upstream FX_FALLBACK_STATIC for short eyes
            for(unsigned speed:{0u,16u,127u,255u}) {
                for(unsigned intensity:{0u,64u,128u,200u,255u}) {
                    const std::vector<uint8_t> palette_ids=(fx.id==218u || fx.id==179u || fx.id==87u || fx.id==115u || fx.id==65u || fx.id==80u || fx.id==81u || fx.id==38u || fx.id==74u || fx.id==219u || fx.id==112u)?std::vector<uint8_t>{0u,2u,3u,4u,5u,6u,7u,8u,9u,10u,11u,12u,13u,31u,45u,61u,72u}:std::vector<uint8_t>{0u};
                    for(uint8_t pal:palette_ids) {
                    PinnedSeg seg(count);seg.palette=((fx.id==179u || fx.id==87u)&&pal==0u)?11u:(fx.id==65u&&pal==0u)?12u:(fx.id==38u&&pal==0u)?50u:(fx.id==219u&&pal==0u)?2u:pal;
                    if(fx.id==65u){seg.custom1=0u;seg.check1=true;seg.check3=true;}
                    PinnedEnv env;
                    seg.speed=(uint8_t)speed;seg.intensity=(uint8_t)intensity;
                    seg.colors[0]=0x007B3809u; // configured primary 123,56,9
                    seg.colors[1]=0x000358AEu; // secondary 3,88,174
                    seg.colors[2]=0u;         // missing CORE tertiary stays black
                    current_segment=&seg;current_environment=&env;
                    ref_rng_state=0x73a11c5du;
                    ot_wled_fx_config_t cfg{};
                    cfg.effect_id=fx.id;cfg.speed=seg.speed;
                    cfg.intensity=seg.intensity;cfg.brightness=255;
                    cfg.frame_time_ms=23;cfg.palette_id=pal;
                    cfg.foreground={123,56,9};cfg.background={3,88,174};
                    ot_wled_fx_state_t state{};
                    std::vector<ot_wled_rgb_t> actual(count);
                    /* Tetrix needs post-initial-fade frames to exercise falling
                     * bricks; the six baseline frame instants miss that path. */
                    const std::vector<uint32_t> frames=fx.id==44u ?
                        std::vector<uint32_t>{0u,17u,171u,389u,1500u,2024u,
                          2050u,2073u,2200u,2400u,2600u,3000u,5000u,10000u} :
                        std::vector<uint32_t>{0u,17u,171u,389u,1500u,65530u};
                    for(uint32_t t:frames) {
                        strip.now=t;
                        fx.effect();
                        if(fx.id==49 && t!=0u && env.allocation.size() && env.data==nullptr)
                            std::abort(); // validate source allocation invariants
                        env.call++;seg.call++; // upstream effect engine increments after frame
                        if(!ot_wled_fx_render(&cfg,&state,t,actual.data(),count)) {
                            std::fprintf(stderr,"CORE rejected pinned FX %u (%s)\n",fx.id,fx.name);
                            return 1;
                        }
                        for(size_t i=0;i<count;i++) {
                            if(pack(actual[i])!=(seg.pixels[i]&0xffffffu)) {
                                std::fprintf(stderr,
                                  "WLED 1:1 mismatch id=%u %s pal=%u len=%u speed=%u intensity=%u ms=%u px=%zu upstream=%06X CORE=%06X\n",
                                  fx.id,fx.name,(unsigned)pal,count,speed,intensity,t,i,
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
    }
    std::printf("WLED pinned-source golden-frame comparison passed: %u RGB samples, %zu distinct modes (RGB with default/explicit fixed palettes where enabled, gamma-off except Fairy/Fairytwinkle; synchronized test RNG)\n",
                checked,sizeof(refs)/sizeof(refs[0]));
    return 0;
}
