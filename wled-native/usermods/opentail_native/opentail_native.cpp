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
    revokeNativeEffects(); // No effect zones until trusted profile arrives.
  }
  void loop() override {
    // Authenticated OpenTail BLE/profile bridge still needs integration.
    // No network-originated trigger writes are accepted here.
  }
  bool configureNativeEffects(const opentail::nativefx::Zone* zones,
                              std::size_t zoneCount, std::size_t pixelCount) {
    if (!enabled || !nativeEffects.configure(zones, zoneCount, pixelCount)) {
      revokeNativeEffects();
      return false;
    }
    nativeEffects.setAuthorized(true);
    nativeEffects.setEnabled(true);
    return true;
  }
  void revokeNativeEffects() {
    nativeEffects.setEnabled(false);
    nativeEffects.setAuthorized(false);
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
    revokeNativeEffects(); // Reboot/config reload never restores authorization.
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

// Internal trusted-control-plane ABI. This is NOT an HTTP endpoint and must
// only be called by the authenticated OpenTail session/profile bridge.
// The bridge must call revoke on disconnect, auth expiry, or Stop/global-off.
extern "C" {
struct OpenTailNativeZoneConfigV1 {
  uint16_t start;
  uint16_t stop;
  uint8_t effect_id;       // 0..4, stable IDs documented by ot_native_effects.h
  uint8_t foreground_r, foreground_g, foreground_b;
  uint8_t background_r, background_g, background_b;
  uint16_t period_ms;
  uint8_t intensity;
};
bool opentailNativeConfigureZonesV1(const OpenTailNativeZoneConfigV1* zones,
                                    size_t zone_count, size_t pixel_count,
                                    bool authenticated_session) {
  if (!authenticated_session || !zones || zone_count == 0 ||
      zone_count > opentail::nativefx::kMaxZones) {
    opentail_native.revokeNativeEffects();
    return false;
  }
  opentail::nativefx::Zone validated[opentail::nativefx::kMaxZones]{};
  for (size_t i = 0; i < zone_count; ++i) {
    if (zones[i].effect_id > static_cast<uint8_t>(opentail::nativefx::Id::PlasmaSweep) ||
        zones[i].period_ms == 0) {
      opentail_native.revokeNativeEffects();
      return false;
    }
    validated[i] = {
      zones[i].start, zones[i].stop,
      static_cast<opentail::nativefx::Id>(zones[i].effect_id),
      opentail::nativefx::pack(zones[i].foreground_r, zones[i].foreground_g, zones[i].foreground_b),
      opentail::nativefx::pack(zones[i].background_r, zones[i].background_g, zones[i].background_b),
      zones[i].period_ms, zones[i].intensity
    };
  }
  return opentail_native.configureNativeEffects(validated, zone_count, pixel_count);
}
void opentailNativeRevokeEffectsV1() {
  opentail_native.revokeNativeEffects();
}
}


// Called by the tiny pinned-source patch at WLED's real frame-compositor seam.
// This is not strip.setPixelColor(): it edits the logical frame before WLED's
// own correction/mapping/bus conversion, preserving one physical output owner.
extern "C" void opentailNativePostRender(uint32_t* pixels, std::size_t count) {
  opentail_native.applyToFrame(pixels, count);
}
