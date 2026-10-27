// Optional WLED-native OpenTail adapter (bench only): native WLED FX engine.
// No mode replacement, effect registry interception, output-pin changes or NVS writes.
// Built ONLY in the explicit opentail_s3_n16r8 PlatformIO environment.
#include "wled.h"
#include "ot_native_overlay.h"

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
    // Required WLED Usermod interface, intentionally no runtime work until
    // authenticated BLE + saved Output Manager profile plumbing is verified.
  }
  void handleOverlayDraw() override {
    // WLED has already rendered effects. No effect phase/state is reset.
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
  // A future authenticated CORE BLE bridge must supply the validated profile
  // and trigger mask. Deliberately no unsecured HTTP vehicle-trigger writes.
};
static OpenTailNativeUsermod opentail_native;
REGISTER_USERMOD(opentail_native);
