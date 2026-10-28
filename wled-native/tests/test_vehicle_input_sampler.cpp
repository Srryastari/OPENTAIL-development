#include "../usermods/opentail_native/ot_vehicle_input_sampler.h"
#include <cassert>
#include <cstdint>
using namespace opentail::inputs;

int main() {
  Sampler s(4);
  // All inputs idle HIGH on an active-low optocoupler board.
  for (int i=0;i<8;i++) assert(s.sample(0xff,true)==0);
  // IN2 (Brake) goes LOW. Debounce must reject the first three samples.
  for (int i=0;i<3;i++) assert(s.sample(0xfd,true)==0);
  assert(s.sample(0xfd,true)==kBrake);
  assert(s.stablePhysicalMask()==(1u<<1));
  // A one-sample bounce does not release Brake.
  assert(s.sample(0xff,true)==kBrake);
  assert(s.sample(0xfd,true)==kBrake);
  assert(s.sample(0xfd,true)==kBrake);
  assert(s.sample(0xfd,true)==kBrake);
  // Four consecutive inactive samples release it.
  for (int i=0;i<3;i++) assert(s.sample(0xff,true)==kBrake);
  assert(s.sample(0xff,true)==0);

  // Simultaneous signals remain simultaneous; the overlay arbitrates priority.
  for (int i=0;i<4;i++) s.sample(0xf1,true); // IN1, IN2, IN3 active
  assert(s.logicalMask()==(kPark|kBrake|kLeft));
  assert(s.stablePhysicalMask()==0x0e);

  // A configurable profile can disable an input or remap a physical input.
  const uint8_t map[kInputCount]={0,kReverse,kBrake,kLeft,kRight,0,0,0};
  assert(s.configureMapping(map));
  assert(s.logicalMask()==(kReverse|kBrake|kLeft)); // current stable state remapped immediately
  const uint8_t invalid[kInputCount]={3,0,0,0,0,0,0,0};
  assert(!s.configureMapping(invalid));
  assert(s.logicalMask()==(kReverse|kBrake|kLeft)); // invalid profile is transactional

  // Active-high hardware is supported without changing the mapping.
  s.reset();
  for(int i=0;i<4;i++) s.sample(0x00,false);
  for(int i=0;i<3;i++) assert(s.sample(0x02,false)==0);
  assert(s.sample(0x02,false)==kReverse);

  // A reset clears stale inputs; BLE/session state is not part of this class.
  s.reset();
  assert(s.logicalMask()==0 && s.stablePhysicalMask()==0);
  return 0;
}
