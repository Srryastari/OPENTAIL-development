#pragma once
// OpenTail standalone 1D particle adapter (original implementation, not a verbatim WLED copy).
// Design reference: WLED wled00/FXparticleSystem.{h,cpp} (EUPL-1.2-or-later).
// This module does not modify vehicle triggers or safety lighting.
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace opentail::fx {
struct RGB { uint8_t r=0, g=0, b=0; };
struct Particle {
  // Fixed-point position in 1/256 pixel units, signed velocity in units per second.
  int32_t x256=0, velocity256=0;
  uint32_t ageMs=0, lifetimeMs=0;
  RGB color{};
  bool active=false;
};
enum class EdgeMode : uint8_t { Kill, Wrap, Bounce };
class Particle1D {
 public:
  explicit Particle1D(size_t capacity=64) : particles_(capacity) {}
  void reset() { for (auto& p: particles_) p=Particle{}; }
  size_t capacity() const { return particles_.size(); }
  size_t activeCount() const {
    size_t count=0; for (const auto& p: particles_) if(p.active) ++count; return count;
  }
  bool emit(int32_t x256,int32_t velocity256,uint32_t lifetimeMs,RGB color) {
    if(lifetimeMs==0) return false;
    for(auto& p:particles_) if(!p.active) {
      p={x256,velocity256,0,lifetimeMs,color,true}; return true;
    }
    return false; // bounded allocation; no heap allocation during frames
  }
  void step(uint32_t dtMs,size_t count,EdgeMode edge=EdgeMode::Kill,int32_t acceleration256=0) {
    if(!count) {reset(); return;}
    const int64_t length=static_cast<int64_t>(count)*256;
    for(auto& p:particles_) {
      if(!p.active) continue;
      if(dtMs>=p.lifetimeMs-p.ageMs){p.active=false;continue;}
      p.ageMs+=dtMs;
      int64_t v=static_cast<int64_t>(p.velocity256)+static_cast<int64_t>(acceleration256)*dtMs/1000;
      v=std::clamp(v,static_cast<int64_t>(INT32_MIN),static_cast<int64_t>(INT32_MAX));
      p.velocity256=static_cast<int32_t>(v);
      int64_t x=static_cast<int64_t>(p.x256)+v*dtMs/1000;
      if(edge==EdgeMode::Wrap){x=((x%length)+length)%length;}
      else if(edge==EdgeMode::Bounce) {
        // Mirror position at edges, handling any number of reflections.
        const int64_t max=length-1, period=2*max;
        x=((x%period)+period)%period;
        if(x>max){x=period-x;p.velocity256=-p.velocity256;}
      } else if(x<0 || x>=length){p.active=false;continue;}
      p.x256=static_cast<int32_t>(std::clamp(x,static_cast<int64_t>(INT32_MIN),static_cast<int64_t>(INT32_MAX)));
    }
  }
  // Additively paint particles onto caller-owned RGB buffer (no memory allocation).
  // Buffer is indexed by a logical 1D section, not a 2D coordinate map.
  void render(RGB* pixels,size_t count,uint8_t brightness=255,bool reverse=false) const {
    if(!pixels) return;
    for(const auto& p:particles_) {
      if(!p.active || p.x256<0) continue;
      size_t i=static_cast<size_t>(p.x256/256);
      if(i>=count) continue;
      if(reverse) i=count-1-i;
      const uint32_t remaining=p.lifetimeMs-p.ageMs;
      const uint32_t opacity=static_cast<uint32_t>((static_cast<uint64_t>(brightness)*remaining)/p.lifetimeMs);
      auto add=[opacity](uint8_t old,uint8_t channel)->uint8_t {
        return static_cast<uint8_t>(std::min<uint32_t>(255,old+(static_cast<uint32_t>(channel)*opacity)/255));
      };
      pixels[i].r=add(pixels[i].r,p.color.r);
      pixels[i].g=add(pixels[i].g,p.color.g);
      pixels[i].b=add(pixels[i].b,p.color.b);
    }
  }
 private:
  std::vector<Particle> particles_; // allocated only on construction
};
} // namespace opentail::fx
