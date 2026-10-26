#include "../Effects1D.h"
#include <cassert>
#include <vector>
using namespace opentail::fx;
int main(){
  Effects1D engine(16,42);
  EffectParams params;
  params.intensity=255; params.color={255,20,0};
  std::vector<RGB> leds(100);
  for(int i=0;i<100;++i){
    engine.tickSparks(16,leds.size(),params);
    engine.render(leds.data(),leds.size(),params,32);
  }
  assert(engine.activeCount()<=16);
  bool illuminated=false;
  for(auto p:leds) if(p.r){illuminated=true;break;}
  assert(illuminated);
  engine.reset();
  assert(engine.activeCount()==0);
  for(int i=0;i<100;++i){
    engine.tickDrift(16,leds.size(),params);
    engine.render(leds.data(),leds.size(),params,64);
  }
  assert(engine.activeCount()<=16);
  engine.tickDrift(16,0,params);
  assert(engine.activeCount()==0);
  return 0;
}
