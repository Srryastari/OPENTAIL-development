#pragma once
// Hardware-independent, per-input debouncer for OpenTail local vehicle triggers.
// The board adapter supplies one packed sample of electrical pin levels every
// 10 ms. This module never reads GPIO, BLE, Wi-Fi or LED hardware.
#include <cstdint>

namespace opentail::inputs {
constexpr uint8_t kPark = 1u << 0;
constexpr uint8_t kBrake = 1u << 1;
constexpr uint8_t kLeft = 1u << 2;
constexpr uint8_t kRight = 1u << 3;
constexpr uint8_t kReverse = 1u << 4;
constexpr uint8_t kSupportedTriggers = kPark | kBrake | kLeft | kRight | kReverse;
constexpr unsigned kInputCount = 8;

class Sampler final {
 public:
  explicit Sampler(uint8_t debounceSamples = 4) :
      debounceSamples_(debounceSamples ? debounceSamples : 1) {
    // Locked default IN1..IN8 logical assignments: Park, Brake, Left, Right,
    // Reverse, Aux1, Aux2, Aux3. Auxiliary roles intentionally do not map to
    // the five road-light overlay bits in this module.
    mapping_[0] = kPark;
    mapping_[1] = kBrake;
    mapping_[2] = kLeft;
    mapping_[3] = kRight;
    mapping_[4] = kReverse;
    mapping_[5] = mapping_[6] = mapping_[7] = 0;
  }

  // mapping entries are 0 (disabled) or one supported trigger bit.
  // Invalid profiles are rejected transactionally.
  bool configureMapping(const uint8_t mapping[kInputCount]) {
    if (!mapping) return false;
    for (unsigned i = 0; i < kInputCount; ++i) {
      const uint8_t v = mapping[i];
      if (v && ((v & (v - 1u)) || (v & ~kSupportedTriggers))) return false;
    }
    for (unsigned i = 0; i < kInputCount; ++i) mapping_[i] = mapping[i];
    recomputeLogical();
    return true;
  }

  // electricalHighMask bit=1 means the corresponding GPIO currently reads HIGH.
  // activeLow=true means a LOW electrical level asserts the opto input.
  uint8_t sample(uint8_t electricalHighMask, bool activeLow) {
    const uint8_t active = activeLow
        ? static_cast<uint8_t>(~electricalHighMask)
        : electricalHighMask;
    for (unsigned i = 0; i < kInputCount; ++i) {
      const uint8_t bit = static_cast<uint8_t>(1u << i);
      const bool candidate = (active & bit) != 0;
      const bool stable = (stablePhysical_ & bit) != 0;
      if (candidate == stable) {
        counts_[i] = 0;
      } else if (++counts_[i] >= debounceSamples_) {
        if (candidate) stablePhysical_ |= bit;
        else stablePhysical_ &= static_cast<uint8_t>(~bit);
        counts_[i] = 0;
      }
    }
    recomputeLogical();
    return logical_;
  }

  uint8_t logicalMask() const { return logical_; }
  uint8_t stablePhysicalMask() const { return stablePhysical_; }
  void reset() {
    stablePhysical_ = logical_ = 0;
    for (unsigned i = 0; i < kInputCount; ++i) counts_[i] = 0;
  }

 private:
  void recomputeLogical() {
    uint8_t logical = 0;
    for (unsigned i = 0; i < kInputCount; ++i)
      if (stablePhysical_ & static_cast<uint8_t>(1u << i)) logical |= mapping_[i];
    logical_ = logical;
  }

  uint8_t debounceSamples_;
  uint8_t mapping_[kInputCount]{};
  uint8_t counts_[kInputCount]{};
  uint8_t stablePhysical_ = 0;
  uint8_t logical_ = 0;
};
}  // namespace opentail::inputs
