// Experimental WLED-native OpenTail adapter. The compositor hook applies
// OpenTail overlays inside WLED's logical frame before gamma and bus mapping.
// Authenticated BLE/profile integration remains incomplete; disabled by default.
#include "wled.h"
#include "ot_native_overlay.h"
#include "ot_native_effects.h"
#include "ot_render_coordinator.h"
#include "ot_wled_render_hook.h"

class OpenTailNativeUsermod final : public Usermod {
  opentail::Overlay overlay;
  opentail::nativefx::Engine nativeEffects;
  bool enabled = false;
 public:
  void setup() override {
    // Do not seize WLED's default data pins, buttons or PC817 GPIOs.
    overlay.setEnabled(enabled);
    nativeEffects.setEnabled(false); // No effect zones until trusted profile arrives.
  }
  void loop() override {
    // Authenticated OpenTail BLE/profile bridge still needs integration.
    // No network-originated trigger writes are accepted here.
  }
  void applyToFrame(uint32_t* pixels, std::size_t count) const {
    if (!enabled || !pixels || !count || bri == 0) return;
    // Native effect zones, when configured and authorized, replace only their
    // assigned logical pixels. Vehicle-trigger overlays are applied last.
    nativeEffects.apply(pixels, count, millis());
    overlay.applyToFrame(pixels, count);
  }
  void addToJsonInfo(JsonObject& root) override {
    // Advertise native effects to clients from the running firmware, while
    // keeping them disabled until an authenticated profile configures zones.
    JsonObject ot = root.createNestedObject("OpenTailNative");
    ot["enabled"] = enabled;
    ot["nativeEffectsActive"] = nativeEffects.enabled();
    JsonArray effects = ot.createNestedArray("effects");
    addEffectInfo(effects, "embers", "Embers", "Intensity,Period", 180, 120);
    addEffectInfo(effects, "materialize_sparks", "Materialize Sparks", "Intensity,Period", 100, 120);
    addEffectInfo(effects, "random_materialize", "Random Materialize", "Intensity,Period", 80, 120);
    addEffectInfo(effects, "smooth_fade", "Smooth Fade", "Intensity,Period", 220, 500);
    addEffectInfo(effects, "plasma_sweep", "Plasma Sweep", "Intensity,Period", 200, 300);
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
  // Until then, overlay and native effects remain unconfigured and unauthorized.
 private:
  static void addEffectInfo(JsonArray& effects, const char* key, const char* name,
                            const char* params, uint8_t intensity, uint16_t periodMs) {
    JsonObject effect = effects.createNestedObject();
    effect["key"] = key;
    effect["name"] = name;
    effect["provider"] = "opentail";
    effect["oneD"] = true;
    effect["supportsPalette"] = false;
    effect["requiresAudio"] = false;
    effect["intensityDefault"] = intensity;
    effect["periodMsDefault"] = periodMs;
    JsonArray parameters = effect.createNestedArray("parameters");
    const char* cursor = params;
    const char* comma = nullptr;
    while (*cursor) {
      comma = cursor;
      while (*comma && *comma != ',') ++comma;
      String label;
      for (const char* p = cursor; p < comma; ++p) label += *p;
      parameters.add(label);
      cursor = *comma ? comma + 1 : comma;
    }
    JsonArray colors = effect.createNestedArray("colorSlots");
    colors.add("Foreground");
    colors.add("Background");
  }
};
static OpenTailNativeUsermod opentail_native;
REGISTER_USERMOD(opentail_native);

// Called by the tiny pinned-source patch at WLED's real frame-compositor seam.
// This is not strip.setPixelColor(): it edits the logical frame before WLED's
// own correction/mapping/bus conversion, preserving one physical output owner.
extern "C" void opentailNativePostRender(uint32_t* pixels, std::size_t count) {
  opentail_native.applyToFrame(pixels, count);
}
