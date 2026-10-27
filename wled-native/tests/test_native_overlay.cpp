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
  o.setInputs(Brake|Left|Right);
  assert(render(o).empty()); // Disabled by default: no effects changed.
  o.setEnabled(true);
  assert(render(o).empty()); // Enabled configuration is not authorization.
  o.setSessionAuthorized(true);
  assert(render(o).empty()); // Pre-authorization inputs must not be replayed.
  o.setInputs(Brake|Left|Right); // Fresh authenticated sample.
  auto spans=render(o);
  assert(spans.size()==4);
  assert(eq(spans[0],{0,118,0xFF0000}));
  assert(eq(spans[1],{118,227,0xFF8000}));
  assert(eq(spans[2],{269,522,0xFF0000}));
  assert(eq(spans[3],{522,552,0xFF8000}));
  o.setInputs(Park);
  assert(render(o).empty()); // Parking does not preempt active shows.
  o.setShowActive(0,false);o.setShowActive(3,false);
  spans=render(o);assert(spans.size()==2);
  assert(eq(spans[0],{0,118,0x080000}));
  o.setInputs(Reverse|Brake);spans=render(o);
  assert(spans.size()==3);
  assert(eq(spans[1],{227,269,0xFFFFFF}));
  Zone overlap[]={zones[0],zones[0]};
  assert(!o.configure(overlap,2,552));
  assert(o.zoneCount()==5); // Invalid profile cannot corrupt current one.
  Zone invalid[]={zones[0]}; invalid[0].stop=553;
  assert(!o.configure(invalid,1,552));
  Zone zero[]={zones[0]};zero[0].start=zero[0].stop;
  assert(!o.configure(zero,1,552));
  o.setSessionAuthorized(false);assert(render(o).empty());
  assert(o.inputs()==0); // Disconnect clears stale vehicle signals.
  o.setInputs(Brake);assert(render(o).empty()); // No unauthenticated replay.
  o.setSessionAuthorized(true);assert(render(o).empty()); // Must refresh inputs.
  o.setInputs(0);assert(render(o).empty());
  o.setInputs(Brake);o.setEnabled(false);assert(render(o).empty());
  return 0;
}
