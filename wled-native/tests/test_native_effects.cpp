#include "../usermods/opentail_native/ot_native_effects.h"
#include <cassert>
#include <cstdint>
using namespace opentail::nativefx;
int main() {
  Engine e;
  const Zone zones[]={
    {0,8,Id::Embers,0x00ff2000u,0x00000000u,120,180},
    {8,16,Id::MaterializeSparks,0x0000aaffu,0x00000000u,120,200},
    {16,24,Id::RandomMaterialize,0x000000ffu,0x00000000u,120,255},
    {24,32,Id::SmoothFade,0x00ffffffu,0x00000000u,500,220},
    {32,40,Id::PlasmaSweep,0x0000ff00u,0x00000000u,300,200}
  };
  assert(e.configure(zones,5,40));
  uint32_t frame[40];
  for(auto& p:frame)p=0x00112233u;
  e.apply(frame,40,1000);
  for(auto p:frame)assert(p==0x00112233u); // disabled by default
  e.setEnabled(true);
  e.apply(frame,40,1000);
  for(auto p:frame)assert(p==0x00112233u); // not authorized
  e.setAuthorized(true);
  e.setEnabled(true);
  e.apply(frame,40,1000);
  bool changed=false;
  for(auto p:frame)if(p!=0x00112233u)changed=true;
  assert(changed);
  for(int i=0;i<8;i++)assert((frame[i]&0x00ff0000u)!=0 || frame[i]==0u);
  uint32_t at1000[40],at1120[40];
  e.apply(at1000,40,1000); e.apply(at1120,40,1120);
  bool animated=false;
  for(int i=0;i<40;i++)if(at1000[i]!=at1120[i])animated=true;
  assert(animated);
  // Invalid profile updates are transactional.
  const Zone overlap[]={{0,10,Id::Embers,0x00ff0000u,0,120,100},
                        {9,20,Id::SmoothFade,0x00ffffffu,0,500,100}};
  assert(!e.configure(overlap,2,40));
  assert(e.zoneCount()==5);
  const Zone outOfRange[]={{0,41,Id::Embers,0x00ff0000u,0,120,100}};
  assert(!e.configure(outOfRange,1,40));
  // Both logical color slots must affect Random Materialize output.
  Engine fgA,fgB,bgA,bgB;
  const Zone redFg={0,128,Id::RandomMaterialize,0x00ff0000u,0x00000020u,120,120};
  const Zone greenFg={0,128,Id::RandomMaterialize,0x0000ff00u,0x00000020u,120,120};
  const Zone blueBg={0,128,Id::RandomMaterialize,0x00ff0000u,0x00000020u,120,120};
  const Zone yellowBg={0,128,Id::RandomMaterialize,0x00ff0000u,0x00002000u,120,120};
  assert(fgA.configure(&redFg,1,128)&&fgB.configure(&greenFg,1,128));
  assert(bgA.configure(&blueBg,1,128)&&bgB.configure(&yellowBg,1,128));
  fgA.setAuthorized(true);fgA.setEnabled(true);fgB.setAuthorized(true);fgB.setEnabled(true);
  bgA.setAuthorized(true);bgA.setEnabled(true);bgB.setAuthorized(true);bgB.setEnabled(true);
  uint32_t redFrame[128]{},greenFrame[128]{},blueFrame[128]{},yellowFrame[128]{};
  fgA.apply(redFrame,128,1200);fgB.apply(greenFrame,128,1200);
  bgA.apply(blueFrame,128,1200);bgB.apply(yellowFrame,128,1200);
  bool foregroundChangesOutput=false,backgroundChangesOutput=false;
  for(int i=0;i<128;i++){
    if(redFrame[i]!=greenFrame[i])foregroundChangesOutput=true;
    if(blueFrame[i]!=yellowFrame[i])backgroundChangesOutput=true;
  }
  assert(foregroundChangesOutput && backgroundChangesOutput);
  // Zero intensity must suppress sparks and white/rainbow accents entirely.
  Engine zeroIntensity;
  const Zone zeroZones[]={
    {0,16,Id::MaterializeSparks,0x00ffffffu,0x00123456u,120,0},
    {16,32,Id::RandomMaterialize,0x00ffffffu,0x00654321u,120,0}
  };
  assert(zeroIntensity.configure(zeroZones,2,32));
  zeroIntensity.setAuthorized(true);zeroIntensity.setEnabled(true);
  uint32_t zeroFrame[32]{};
  zeroIntensity.apply(zeroFrame,32,0xffffffffu);
  for(int i=0;i<16;i++)assert(zeroFrame[i]==0x00123456u);
  for(int i=16;i<32;i++)assert(zeroFrame[i]==0x00654321u);
  e.setAuthorized(false);
  for(auto& p:frame)p=0x00112233u;
  e.apply(frame,40,2000);
  for(auto p:frame)assert(p==0x00112233u); // disconnect/unauthorized fails closed
  e.apply(nullptr,40,2000);
  return 0;
}
