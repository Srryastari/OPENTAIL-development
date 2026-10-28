// Experimental WLED-native OpenTail adapter. This file is not yet the complete
// OpenTail render coordinator or authenticated BLE bridge; keep disabled by default.
#include "wled.h"
#include "ot_native_overlay.h"
#include "ot_render_coordinator.h"

class OpenTailNativeUsermod final : public Usermod {
  opentail::Overlay overlay;
  bool enabled = false;
 public:
  void setup() override {
    // Do not seize WLED's default data pins, buttons or PC817 GPIOs.
    // Authenticated OpenTail BLE/profile integration is not yet wired in.
    overlay.setEnabled(enabled);
  }
  void loop() override {
    // The shared coordinator contract is in ot_render_coordinator.h.
    // Provider adapters, render-loop ownership and BLE/profile transport still
    // require integration into this pinned WLED runtime before activation.
  }
  void handleOverlayDraw() override {
    // Current bench-only overlay hook: WLED has already rendered effects.
    // This does NOT yet combine the OpenTail provider with the WLED render loop.
    // Respect WLED global off; do not reilluminate after Stop.
    if (!enabled || bri == 0) return;
    overlay.forEachActiveSpan([](opentail::Span s) {
      for (uint16_t p=s.start; p<s.stop; ++p) strip.setPixelColor(p,s.color);
    });
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
  // No unsecured HTTP vehicle-trigger writes.
};
static OpenTailNativeUsermod opentail_native;
REGISTER_USERMOD(opentail_native);
