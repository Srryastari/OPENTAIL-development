package com.opentail.android;
/** Explicit volatile controller playback; no refresh timer or NVS transaction. */
final class ContinuousFxCommand {
 static final int OPCODE=10;
 static boolean advertised(String info) {
  if(info==null||!info.startsWith("OpenTail CORE "))return false;
  for(String part:info.split("\\|"))if(part.trim().equals("continuous-fx-v1"))return true;
  return false;
 }
 static byte[] encode(int id,int chains,int zones,int r,int g,int b,
   int br,int bg,int bb,int brightness,int speed,int intensity,int palette) {
  if(id<0||id>65535||chains<1||chains>3||zones<1||zones>255||
     brightness<0||brightness>32||speed<0||speed>255||intensity<0||intensity>255||
     !BleFxPalettePolicy.isWireCandidate(id,palette))
   throw new IllegalArgumentException("Unsupported continuous FX");
  for(int value:new int[]{r,g,b,br,bg,bb})if(value<0||value>255)
   throw new IllegalArgumentException("Invalid RGB");
  return new byte[]{1,OPCODE,(byte)id,(byte)(id>>8),(byte)chains,(byte)zones,
    (byte)r,(byte)g,(byte)b,(byte)br,(byte)bg,(byte)bb,(byte)brightness,
    (byte)speed,(byte)intensity,(byte)palette,0};
 }
}
