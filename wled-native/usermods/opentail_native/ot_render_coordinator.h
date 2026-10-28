#pragma once
// OpenTail-owned unified render coordinator contract.
// Header-only, dependency-free C++17. Provider adapters are implemented by
// the pinned WLED runtime and OpenTail-native effects, but neither provider
// owns physical pixel writes. The coordinator validates zones and commits
// exactly one final frame through the configured output sink.
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace opentail::render {
constexpr std::size_t kMaxZones = 32;
constexpr std::size_t kMaxPixels = 65535;

enum class Provider : uint8_t { WledNative = 0, OpenTailNative = 1 };

struct Rgb {
  uint8_t r, g, b;
};
struct EffectState {
  uint16_t effectId;
  uint8_t speed, intensity, palette, brightness;
  Rgb colors[3]; // Exact declared slots map to WLED's native color slots.
  uint8_t options[3];
  uint8_t custom[3];
  bool reverse, mirror;
};
struct Zone {
  uint16_t start, stop; // Global logical pixel indices; stop exclusive.
  Provider provider;
  EffectState effect;
};
struct TriggerSpan {
  uint16_t start, stop;
  Rgb color;
};

class EffectProvider {
 public:
  virtual ~EffectProvider() = default;
  // Render into the supplied zone-local frame. Implementations must not write
  // hardware. WLED adapter must preserve native segment state and semantics.
  virtual bool render(const EffectState& effect, uint32_t nowMs,
                      Rgb* frame, std::size_t pixelCount) = 0;
};
class TriggerLayer {
 public:
  virtual ~TriggerLayer() = default;
  // Apply OpenTail arbitration to the already-rendered logical frame.
  // Stop/global-off is enforced by the coordinator, not overridden here.
  virtual void apply(Rgb* frame, std::size_t pixelCount) = 0;
};
class OutputSink {
 public:
  virtual ~OutputSink() = default;
  // The only hardware-commit point in this architecture.
  virtual bool commit(const Rgb* frame, std::size_t pixelCount) = 0;
};

class Coordinator final {
 public:
  bool configure(const Zone* zones, std::size_t count, std::size_t pixelCount) {
    if (!zones || count == 0 || count > kMaxZones ||
        pixelCount == 0 || pixelCount > kMaxPixels) return false;
    for (std::size_t i = 0; i < count; ++i) {
      if (zones[i].start >= zones[i].stop || zones[i].stop > pixelCount ||
          static_cast<uint8_t>(zones[i].provider) >
            static_cast<uint8_t>(Provider::OpenTailNative)) return false;
      for (std::size_t j = 0; j < i; ++j) {
        if (zones[i].start < zones[j].stop && zones[i].stop > zones[j].start)
          return false;
      }
    }
    for (std::size_t i = 0; i < count; ++i) zones_[i] = zones[i];
    zoneCount_ = count;
    pixelCount_ = pixelCount;
    return true;
  }
  void setEnabled(bool enabled) { enabled_ = enabled; }
  void setGlobalOff(bool off) { globalOff_ = off; }
  bool enabled() const { return enabled_; }
  std::size_t zoneCount() const { return zoneCount_; }

  bool renderFrame(uint32_t nowMs, EffectProvider& wled,
                   EffectProvider& native, TriggerLayer& triggers,
                   OutputSink& output) {
    if (!enabled_ || globalOff_ || !zoneCount_ || !pixelCount_) {
      // Explicitly commit black on Stop/global-off; never let triggers relight.
      if (pixelCount_) {
        for (std::size_t i = 0; i < pixelCount_; ++i) frame_[i] = Rgb{0,0,0};
        return output.commit(frame_, pixelCount_);
      }
      return false;
    }
    for (std::size_t i = 0; i < pixelCount_; ++i) frame_[i] = Rgb{0,0,0};
    for (std::size_t i = 0; i < zoneCount_; ++i) {
      const Zone& z = zones_[i];
      EffectProvider& provider = z.provider == Provider::WledNative ? wled : native;
      if (!provider.render(z.effect, nowMs, frame_ + z.start,
                           static_cast<std::size_t>(z.stop - z.start))) return false;
    }
    triggers.apply(frame_, pixelCount_);
    // Exactly one output commit per successful rendered frame.
    return output.commit(frame_, pixelCount_);
  }

 private:
  Zone zones_[kMaxZones]{};
  Rgb frame_[kMaxPixels]{};
  std::size_t zoneCount_ = 0;
  std::size_t pixelCount_ = 0;
  bool enabled_ = false;
  bool globalOff_ = false;
};
} // namespace opentail::render
