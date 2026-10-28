#pragma once
// OpenTail-native 1D effect engine for the pinned WLED logical-frame seam.
// It never touches a LED bus. Configuration and authorization must come from
// the trusted OpenTail control plane; all output remains fail-closed by default.
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace opentail::nativefx {
constexpr std::size_t kMaxZones = 32;
enum class Id : uint8_t {
  Embers = 0,
  MaterializeSparks = 1,
  RandomMaterialize = 2,
  SmoothFade = 3,
  PlasmaSweep = 4
};
struct Zone {
  uint16_t start, stop; // global pixel indices; stop exclusive
  Id effect;
  uint32_t foreground;  // logical packed 0x00RRGGBB
  uint32_t background;
  uint16_t periodMs;
  uint8_t intensity;    // 0..255
};
inline uint32_t pack(uint8_t r,uint8_t g,uint8_t b) {
  return (uint32_t(r)<<16)|(uint32_t(g)<<8)|uint32_t(b);
}
inline uint8_t channel(uint32_t c,unsigned shift) {
  return uint8_t((c>>shift)&0xffu);
}
inline uint32_t blend(uint32_t a,uint32_t b,uint8_t amount) {
  const unsigned inv=255u-amount;
  return pack(uint8_t((channel(a,16)*inv+channel(b,16)*amount)/255u),
              uint8_t((channel(a,8)*inv+channel(b,8)*amount)/255u),
              uint8_t((channel(a,0)*inv+channel(b,0)*amount)/255u));
}
inline uint32_t hash32(uint32_t x) {
  x ^= x >> 16; x *= 0x7feb352du;
  x ^= x >> 15; x *= 0x846ca68bu;
  x ^= x >> 16; return x;
}
inline uint32_t wheel(uint8_t p) {
  p=uint8_t(255u-p);
  if(p<85) return pack(uint8_t(255u-p*3u),0,uint8_t(p*3u));
  if(p<170) { p=uint8_t(p-85u); return pack(0,uint8_t(p*3u),uint8_t(255u-p*3u)); }
  p=uint8_t(p-170u); return pack(uint8_t(p*3u),uint8_t(255u-p*3u),0);
}
class Engine final {
 public:
  bool configure(const Zone* zones,std::size_t count,std::size_t pixelCount) {
    if(!zones||count==0||count>kMaxZones||pixelCount==0||pixelCount>65535) return false;
    for(std::size_t i=0;i<count;++i) {
      const Zone& z=zones[i];
      if(z.start>=z.stop||z.stop>pixelCount||z.periodMs==0||
         static_cast<uint8_t>(z.effect)>static_cast<uint8_t>(Id::PlasmaSweep)) return false;
      for(std::size_t j=0;j<i;++j)
        if(z.start<zones[j].stop&&z.stop>zones[j].start) return false;
    }
    for(std::size_t i=0;i<count;++i) zones_[i]=zones[i];
    count_=count; pixelCount_=pixelCount; return true;
  }
  void setEnabled(bool v) { enabled_=v; }
  void setAuthorized(bool v) { authorized_=v; if(!v) enabled_=false; }
  bool enabled() const { return enabled_; }
  std::size_t zoneCount() const { return count_; }
  void apply(uint32_t* frame,std::size_t count,uint32_t nowMs) const {
    if(!frame||!enabled_||!authorized_||count<pixelCount_) return;
    for(std::size_t zi=0;zi<count_;++zi) {
      const Zone& z=zones_[zi];
      const std::size_t len=std::size_t(z.stop-z.start);
      const uint32_t phase=nowMs/z.periodMs;
      for(std::size_t local=0;local<len;++local) {
        const uint8_t x=uint8_t((local*255u)/(len>1?len-1:1));
        const uint32_t noise=hash32(uint32_t(local)^phase*0x9e3779b9u^uint32_t(zi)*0x85ebca6bu);
        uint32_t out=z.background;
        switch(z.effect) {
          case Id::Embers: {
            const uint8_t heat=uint8_t((noise&255u)*z.intensity/255u);
            out=blend(z.background,z.foreground,heat); break;
          }
          case Id::MaterializeSparks: {
            const uint8_t sparkle=uint8_t((noise>>24)&255u);
            const uint8_t threshold=uint8_t(255u-z.intensity);
            if(sparkle>=threshold) out=blend(z.background,z.foreground,uint8_t(160u+(sparkle>>2)));
            else out=blend(z.background,z.foreground,uint8_t((sparkle*z.intensity)/1020u));
            break;
          }
          case Id::RandomMaterialize: {
            const uint8_t sparkle=uint8_t((noise>>16)&255u);
            const uint8_t threshold=uint8_t(255u-z.intensity);
            if(sparkle>=threshold) out=(sparkle>245u)?0x00ffffffu:wheel(uint8_t(x+phase*7u+local*13u));
            else out=z.background;
            break;
          }
          case Id::SmoothFade: {
            const float wave=(std::sin(float(nowMs%uint32_t(z.periodMs*2u))*3.14159265f/float(z.periodMs))+1.0f)*0.5f;
            const uint8_t amount=uint8_t(wave*float(z.intensity));
            out=blend(z.background,z.foreground,amount); break;
          }
          case Id::PlasmaSweep: {
            const float wave=(std::sin((float(local)*0.32f)+(float(nowMs)/float(z.periodMs)))+1.0f)*0.5f;
            const uint8_t amount=uint8_t(wave*float(z.intensity));
            out=blend(z.background,z.foreground,amount); break;
          }
        }
        frame[z.start+local]=out;
      }
    }
  }
 private:
  Zone zones_[kMaxZones]{};
  std::size_t count_=0,pixelCount_=0;
  bool enabled_=false,authorized_=false;
};
} // namespace opentail::nativefx
