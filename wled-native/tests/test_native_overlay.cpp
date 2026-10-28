#include "../usermods/opentail_native/ot_native_overlay.h"
#include <cassert>
#include <cstdint>
#include <vector>
using namespace opentail;
using S = Span;
static bool eq(const S& a,const S& b) {
  return a.start==b.start && a.stop==b.stop && a.color==b.color;
}
static std::vector<S> render(const Overlay& o) {
  std::vector<S> v;
  o.forEachActiveSpan([&](S s){v.push_back(s);});
  return v;
}
int main() {
  Overlay o;
  Zone zones[]={
    {0,118,Park|Brake,0x080000,0xFF0000,0,0,0,true},
    {118,227,Left,0,0,0xFF8000,0,0,true},
    {227,269,Reverse,0,0,0,0,0xFFFFFF,true},
    {269,522,Park|Brake,0x080000,0xFF0000,0,0,0,true},
    {522,552,Right,0,0,0,0xFF8000,0,true}
  };
  assert(o.configure(zones,5,552));
  assert(o.zoneCount()==5);
  o.setVehicleInputsLocal(Brake|Left|Right);
  assert(render(o).empty()); // Disabled by default: no effects changed.
  o.setEnabled(true);
  auto spans=render(o); // Local vehicle inputs work without a BLE session.
  assert(spans.size()==4);
  o.setSessionAuthorized(true);
  assert(render(o).size()==4); // Pairing does not gate local vehicle inputs.
  o.setSessionAuthorized(false);
  assert(render(o).size()==4); // Disconnect must not disable road-signal overrides.
  o.setSessionAuthorized(true);
  spans=render(o);
  assert(spans.size()==4);
  assert(eq(spans[0],{0,118,0xFF0000}));
  assert(eq(spans[1],{118,227,0xFF8000}));
  assert(eq(spans[2],{269,522,0xFF0000}));
  assert(eq(spans[3],{522,552,0xFF8000}));
  uint32_t frame[552];
  for (auto& px : frame) px = 0x010101;
  o.applyToFrame(frame,552);
  assert(frame[0]==0xFF0000 && frame[117]==0xFF0000);
  assert(frame[118]==0xFF8000 && frame[226]==0xFF8000);
  assert(frame[227]==0x010101); // Unassigned zone remains WLED-rendered.
  assert(frame[269]==0xFF0000 && frame[521]==0xFF0000);
  assert(frame[522]==0xFF8000 && frame[551]==0xFF8000);
  o.applyToFrame(nullptr,552); // Null buffer is a no-op.
  o.setVehicleInputsLocal(Park);
  assert(render(o).empty()); // Parking does not preempt active shows.
  o.setShowActive(0,false);o.setShowActive(3,false);
  spans=render(o);assert(spans.size()==2);
  assert(eq(spans[0],{0,118,0x080000}));
  o.setVehicleInputsLocal(Reverse|Brake);spans=render(o);
  assert(spans.size()==3);
  assert(eq(spans[1],{227,269,0xFFFFFF}));
  Zone overlap[]={zones[0],zones[0]};
  assert(!o.configure(overlap,2,552));
  assert(o.zoneCount()==5); // Invalid profile cannot corrupt current one.
  Zone invalid[]={zones[0]}; invalid[0].stop=553;
  assert(!o.configure(invalid,1,552));
  Zone zero[]={zones[0]};zero[0].start=zero[0].stop;
  assert(!o.configure(zero,1,552));
  o.setSessionAuthorized(false);
  assert(render(o).size()==3); // BLE disconnect cannot suppress local reverse/brake signals.
  assert(o.inputs()==(Reverse|Brake));
  o.setVehicleInputsLocal(Brake);
  assert(render(o).size()==2); // Brake overlays the two tail zones.
  o.setVehicleInputsLocal(0);assert(render(o).empty());
  o.setVehicleInputsLocal(Brake);o.setEnabled(false);assert(render(o).empty());
  return 0;
}
