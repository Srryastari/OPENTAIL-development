#pragma once
// Original OpenTail effect recipes over bounded Particle1D.
// Logical 1D sections only. Caller must resolve automotive trigger priority BEFORE presenting frames.
#include "Particle1D.h"
#include <algorithm>
#include <cstdint>
namespace opentail::fx {
struct EffectParams {
  uint8_t speed=128, intensity=128, width=1, brightness=255;
  bool reverse=false;
  RGB color{255,120,0};
};
class Effects1D {
 public:
  explicit Effects1D(size_t particleCapacity=64, uint32_t seed=0x13579BDFu)
      : particles_(particleCapacity), rng_(seed?seed:1) {}
  void reset(){particles_.reset();emissionAccumulator_=0;}
  size_t activeCount() const{return particles_.activeCount();}
  // dtMs is elapsed frame time; all random generation is deterministic for a given seed.
  // Emission is bounded: no more than min(capacity, 32) emit attempts in one frame.
  void tickSparks(uint32_t dtMs,size_t length,const EffectParams& p){
    if(!length){reset();return;}
    // Average rate 1..21 particles per second; intensity determines density.
    const uint32_t rate=1+(static_cast<uint32_t>(p.intensity)*20)/255;
    emissionAccumulator_+=static_cast<uint64_t>(dtMs)*rate;
    uint32_t emits=static_cast<uint32_t>(std::min<uint64_t>(emissionAccumulator_/1000, std::min<size_t>(particles_.capacity(),32)));
    emissionAccumulator_%=1000;
    while(emits--){
      uint32_t pixel=next()%static_cast<uint32_t>(std::min<size_t>(length,UINT32_MAX/256));
      int32_t velocity=(static_cast<int32_t>(p.speed)+1)*16;
      if(next()&1) velocity=-velocity;
      particles_.emit(static_cast<int32_t>(pixel*256),velocity,200+(next()%1300),p.color);
    }
    particles_.step(dtMs,length,EdgeMode::Kill);
  }
  void tickDrift(uint32_t dtMs,size_t length,const EffectParams& p){
    if(!length){reset();return;}
    emissionAccumulator_+=static_cast<uint64_t>(dtMs)*(1+(static_cast<uint32_t>(p.intensity)*10)/255);
    uint32_t emits=static_cast<uint32_t>(std::min<uint64_t>(emissionAccumulator_/1000, std::min<size_t>(particles_.capacity(),32)));
    emissionAccumulator_%=1000;
    while(emits--){
      uint32_t pixel=next()%static_cast<uint32_t>(std::min<size_t>(length,UINT32_MAX/256));
      int32_t v=(static_cast<int32_t>(p.speed)+1)*4;
      if(next()&1) v=-v;
      particles_.emit(static_cast<int32_t>(pixel*256),v,1500+(next()%3000),p.color);
    }
    particles_.step(dtMs,length,EdgeMode::Wrap);
  }
  // Render additively. Optional fade retains previous trails; fade=255 clears immediately.
  void render(RGB* pixels,size_t length,const EffectParams& p,uint8_t fade=255) const {
    if(!pixels) return;
    for(size_t i=0;i<length;++i){
      pixels[i].r=static_cast<uint8_t>((static_cast<uint16_t>(pixels[i].r)*(255-fade))/255);
      pixels[i].g=static_cast<uint8_t>((static_cast<uint16_t>(pixels[i].g)*(255-fade))/255);
      pixels[i].b=static_cast<uint8_t>((static_cast<uint16_t>(pixels[i].b)*(255-fade))/255);
    }
    particles_.render(pixels,length,p.brightness,p.reverse);
  }
 private:
  uint32_t next(){rng_^=rng_<<13;rng_^=rng_>>17;rng_^=rng_<<5;return rng_;}
  Particle1D particles_;
  uint32_t rng_;
  uint64_t emissionAccumulator_=0;
};
} // namespace opentail::fx
