// Experimental WLED-native OpenTail adapter. The compositor hook applies
// OpenTail overlays inside WLED's logical frame before gamma and bus mapping.
// Authenticated BLE/profile integration remains incomplete; disabled by default.
#include "wled.h"
#include "ot_native_overlay.h"
#include "ot_render_coordinator.h"
#include "ot_wled_render_hook.h"

class OpenTailNativeUsermod final : public Usermod {
  opentail::Overlay overlay;
  bool enabled = false;
 public:
  void setup() override {
    // Do not seize WLED's default data pins, buttons or PC817 GPIOs.
    overlay.setEnabled(enabled);
  }
  void loop() override {
    // Authenticated OpenTail BLE/profile bridge still needs integration.
    // No network-originated trigger writes are accepted here.
  }
  void applyToFrame(uint32_t* pixels, std::size_t count) const {
    if (!enabled || !pixels || !count || bri == 0) return;
    overlay.applyToFrame(pixels, count);
  }
  void addToConfig(JsonObject& root) override {
    JsonObject cfg=root.createNestedObject("OpenTailNative");
    cfg["enabled"]=enabled;
  }
  bool readFromConfig(JsonObject& root) override {
    JsonObject cfg=root["OpenTailNative"];
    enabled=cfg["enabled"] | false;
    overlay.setEnabled(enabled);
    return !cfg.isNull();
  }
  // Future authenticated CORE bridge must configure validated zones and inputs.
  // Until then, overlay remains unconfigured and unauthorized (fail closed).
};
static OpenTailNativeUsermod opentail_native;
REGISTER_USERMOD(opentail_native);

// Called by the tiny pinned-source patch at WLED's real frame-compositor seam.
// This is not strip.setPixelColor(): it edits the logical frame before WLED's
// own correction/mapping/bus conversion, preserving one physical output owner.
extern "C" void opentailNativePostRender(uint32_t* pixels, std::size_t count) {
  opentail_native.applyToFrame(pixels, count);
}
