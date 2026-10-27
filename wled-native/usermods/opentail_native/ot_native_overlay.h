#pragma once
// OpenTail WLED-native bench overlay. Pure C++17; no WLED/Arduino dependencies.
// WLED owns effect rendering/state; OpenTail only overrides assigned pixels.
// No writes without explicit enable, valid zones and an active signal.
#include <cstddef>
#include <cstdint>
namespace opentail {
constexpr std::size_t kMaxOverlayZones = 32;
enum Trigger : uint8_t { Park = 1, Brake = 2, Left = 4, Right = 8, Reverse = 16 };
struct Zone {
  uint16_t start, stop; // Absolute LED indices, stop exclusive.
  uint8_t role;
  uint32_t park, brake, left, right, reverse; // User-selected WLED RGBW32 colors.
  bool showActive; // Parking is an idle baseline, not a show interrupter.
};
struct Span { uint16_t start, stop; uint32_t color; };
class Overlay final {
 public:
  bool configure(const Zone* zones, std::size_t count, uint16_t ledCount) {
    if (!zones || count == 0 || count > kMaxOverlayZones || !ledCount) return false;
    for (std::size_t i=0; i<count; ++i) {
      if (zones[i].start >= zones[i].stop || zones[i].stop > ledCount ||
          (zones[i].role & ~uint8_t(31u))) return false;
      for (std::size_t j=0; j<i; ++j)
        if (zones[i].start < zones[j].stop && zones[i].stop > zones[j].start) return false;
    }
    // Transactional: invalid config leaves live profile intact.
    for (std::size_t i=0; i<count; ++i) zones_[i]=zones[i];
    count_=count; return true;
  }
  void setEnabled(bool v) { enabled_=v; }
  bool enabled() const { return enabled_; }
  void setInputs(uint8_t mask) { inputs_=mask & 31u; }
  uint8_t inputs() const { return inputs_; }
  void setShowActive(std::size_t zoneIndex,bool active) {
    if(zoneIndex < count_) zones_[zoneIndex].showActive=active;
  }
  std::size_t zoneCount() const { return count_; }
  template<class Fn> void forEachActiveSpan(Fn fn) const {
    if (!enabled_ || !inputs_) return;
    for (std::size_t i=0; i<count_; ++i) {
      const Zone& z=zones_[i];
      const uint8_t present=z.role & inputs_;
      uint32_t color=0;
      if (present & Reverse) color=z.reverse;
      else if (present & Left) color=z.left;
      else if (present & Right) color=z.right;
      else if (present & Brake) color=z.brake;
      else if ((present & Park) && !z.showActive) color=z.park;
      else continue;
      fn(Span{z.start,z.stop,color});
    }
  }
 private:
  Zone zones_[kMaxOverlayZones]{};
  std::size_t count_=0;
  uint8_t inputs_=0;
  bool enabled_=false;
};
}
