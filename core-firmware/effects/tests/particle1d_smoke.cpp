#include "../Particle1D.h"
#include <cassert>
#include <vector>
using namespace opentail::fx;
int main() {
  Particle1D ps(2);
  assert(ps.capacity()==2 && ps.activeCount()==0);
  assert(ps.emit(0,2560,1000,{255,0,0}));
  assert(ps.emit(256,0,1000,{0,255,0}));
  assert(!ps.emit(0,0,1000,{0,0,255})); // fixed capacity
  std::vector<RGB> px(10);
  ps.step(100,10); ps.render(px.data(),px.size());
  assert(px[1].r>0 && px[1].g>0);
  ps.step(1000,10); assert(ps.activeCount()==0);
  assert(ps.emit(-256,0,1000,{0,0,255}));
  ps.step(10,10,EdgeMode::Wrap);
  std::fill(px.begin(),px.end(),RGB{});
  ps.render(px.data(),px.size(),255,true);
  assert(px[0].b>0); // wrap negative coordinate to last pixel then reverse
  ps.step(10,0); assert(ps.activeCount()==0);
  assert(!ps.emit(0,0,0,{1,2,3}));
  return 0;
}
