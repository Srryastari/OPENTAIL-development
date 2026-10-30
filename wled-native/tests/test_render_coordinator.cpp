#include "../usermods/opentail_native/ot_render_coordinator.h"
#include <cassert>
#include <cstddef>
#include <cstdint>
using namespace opentail::render;

struct FakeProvider final : EffectProvider {
  unsigned calls = 0;
  bool success = true;
  uint16_t lastId = 0;
  Rgb value{0,0,0};
  bool render(const EffectState& effect, uint32_t, Rgb* frame,
              std::size_t count) override {
    ++calls; lastId = effect.effectId;
    for (std::size_t i=0; i<count; ++i) frame[i] = value;
    return success;
  }
};
struct FakeTriggers final : TriggerLayer {
  unsigned calls = 0;
  void apply(Rgb* frame, std::size_t count) override {
    ++calls;
    if (count > 1) frame[1] = Rgb{255,180,0};
  }
};
struct FakeOutput final : OutputSink {
  unsigned commits = 0;
  Rgb last[8]{};
  std::size_t count = 0;
  bool commit(const Rgb* frame, std::size_t n) override {
    ++commits; count=n;
    for (std::size_t i=0; i<n && i<8; ++i) last[i]=frame[i];
    return true;
  }
};
static EffectState effect(uint16_t id) {
  return EffectState{id,128,128,0,255,{{255,0,0},{0,0,0},{0,0,0}},
                     {0,0,0},{0,0,0},false,false};
}
int main() {
  Coordinator c;
  const Zone zones[] = {
    {0,2,Provider::WledNative,effect(7)},
    {2,4,Provider::OpenTailNative,effect(900)}
  };
  assert(c.configure(zones,2,4));
  const Zone overlap[] = {
    {0,3,Provider::WledNative,effect(7)},
    {2,4,Provider::OpenTailNative,effect(900)}
  };
  assert(!c.configure(overlap,2,4));
  assert(c.zoneCount()==2); // Invalid update is transactional.
  FakeProvider wled, native;
  wled.value=Rgb{10,20,30}; native.value=Rgb{40,50,60};
  FakeTriggers triggers; FakeOutput output; Rgb frame[4]{};
  assert(!c.renderFrame(10,wled,native,triggers,output,frame,3));
  assert(output.commits==0);
  c.setEnabled(true);
  assert(c.renderFrame(10,wled,native,triggers,output,frame,4));
  assert(wled.calls==1 && wled.lastId==7);
  assert(native.calls==1 && native.lastId==900);
  assert(triggers.calls==1 && output.commits==1);
  assert(output.count==4);
  assert(output.last[0].r==10 && output.last[0].g==20);
  assert(output.last[1].r==255 && output.last[1].g==180);
  assert(output.last[2].r==40 && output.last[3].b==60);
  c.setGlobalOff(true);
  assert(c.renderFrame(20,wled,native,triggers,output,frame,4));
  assert(output.commits==2 && triggers.calls==1);
  for (std::size_t i=0;i<4;++i)
    assert(output.last[i].r==0 && output.last[i].g==0 && output.last[i].b==0);
  c.setGlobalOff(false);
  native.success=false;
  assert(!c.renderFrame(30,wled,native,triggers,output,frame,4));
  assert(output.commits==3); // Provider failure explicitly commits black.
  for (std::size_t i=0;i<4;++i)
    assert(output.last[i].r==0 && output.last[i].g==0 && output.last[i].b==0);
  return 0;
}
