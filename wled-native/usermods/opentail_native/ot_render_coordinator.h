#pragma once
// OpenTail-owned unified render coordinator contract.
// Header-only, dependency-free C++17. WLED and OpenTail-native providers render
// into a caller-owned logical frame; only this coordinator invokes OutputSink.
#include <cstddef>
#include <cstdint>

namespace opentail::render {
constexpr std::size_t kMaxZones = 32;
constexpr std::size_t kMaxPixels = 65535;
enum class Provider : uint8_t { WledNative = 0, OpenTailNative = 1 };
struct Rgb { uint8_t r, g, b; };
struct EffectState {
  uint16_t effectId;
  uint8_t speed, intensity, palette, brightness;
  Rgb colors[3]; // WLED native segment color slots; metadata defines which are used.
  uint8_t options[3], custom[3];
  bool reverse, mirror;
};
struct Zone {
  uint16_t start, stop; // Global logical pixel indices; stop exclusive.
  Provider provider;
  EffectState effect;
};
class EffectProvider {
 public:
  virtual ~EffectProvider() = default;
  // Render zone-local pixels only. Provider implementations must not write hardware.
  virtual bool render(const EffectState& effect, uint32_t nowMs,
                      Rgb* frame, std::size_t pixelCount) = 0;
};
class TriggerLayer {
 public:
  virtual ~TriggerLayer() = default;
  virtual void apply(Rgb* frame, std::size_t pixelCount) = 0;
};
class OutputSink {
 public:
  virtual ~OutputSink() = default;
  // The sole hardware-commit point in this architecture.
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
      for (std::size_t j = 0; j < i; ++j)
        if (zones[i].start < zones[j].stop && zones[i].stop > zones[j].start)
          return false;
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
  std::size_t pixelCount() const { return pixelCount_; }

  bool renderFrame(uint32_t nowMs, EffectProvider& wled, EffectProvider& native,
                   TriggerLayer& triggers, OutputSink& output,
                   Rgb* frame, std::size_t frameCapacity) const {
    if (!frame || frameCapacity < pixelCount_ || !pixelCount_) return false;
    // Stop/global-off is authoritative; no trigger layer is allowed to relight.
    if (!enabled_ || globalOff_ || !zoneCount_) {
      for (std::size_t i = 0; i < pixelCount_; ++i) frame[i] = Rgb{0,0,0};
      return output.commit(frame, pixelCount_);
    }
    for (std::size_t i = 0; i < pixelCount_; ++i) frame[i] = Rgb{0,0,0};
    for (std::size_t i = 0; i < zoneCount_; ++i) {
      const Zone& z = zones_[i];
      EffectProvider& provider = z.provider == Provider::WledNative ? wled : native;
      if (!provider.render(z.effect, nowMs, frame + z.start,
                           static_cast<std::size_t>(z.stop - z.start))) {
        // Fail closed on provider error: never leave a stale illuminated frame.
        for (std::size_t p = 0; p < pixelCount_; ++p) frame[p] = Rgb{0,0,0};
        (void)output.commit(frame, pixelCount_);
        return false;
      }
    }
    triggers.apply(frame, pixelCount_);
    // Exactly one output commit per successfully rendered frame.
    return output.commit(frame, pixelCount_);
  }
 private:
  Zone zones_[kMaxZones]{};
  std::size_t zoneCount_ = 0, pixelCount_ = 0;
  bool enabled_ = false, globalOff_ = false;
};
} // namespace opentail::render
