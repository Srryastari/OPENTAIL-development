package com.opentail.android;
import org.junit.Test;
import static org.junit.Assert.*;

public class TertiaryFxCommandTest {
 @Test public void capabilityTiersRemainBackwardCompatible() {
   String legacy="OpenTail CORE 0.1 |continuous-fx-v1|fx-tertiary-v1";
   String current="OpenTail CORE 0.2 |continuous-fx-v1|fx-tertiary-v1|fx-tertiary13-v1";
   assertTrue(TertiaryFxCommand.advertised(legacy));
   assertTrue(TertiaryFxCommand.advertised(current));
   assertFalse(TertiaryFxCommand.advertised13(legacy));
   assertTrue(TertiaryFxCommand.advertised13(current));
   assertFalse(TertiaryFxCommand.advertised("OpenTail CORE 0.1 |fx-tertiary-v11"));
   assertFalse(TertiaryFxCommand.advertised(null));
 }
 @Test public void allThirteenSourceProvenModesAreAdmittedOnlyByNewFirmware() {
   int[] supported={28,29,34,38,50,52,54,55,56,84,87,95,103};
   int[] legacy={28,29,34,50,52,54,55,56,84,87,103};
   int[] unsupported={10,11,40,48,60,91};
   String oldInfo="OpenTail CORE 0.1 |fx-tertiary-v1";
   String newInfo="OpenTail CORE 0.2 |fx-tertiary-v1|fx-tertiary13-v1";
   for(int id:supported)assertTrue("source renderer mode "+id,TertiaryFxCommand.supported(id));
   for(int id:legacy)assertTrue("legacy BLE mode "+id,TertiaryFxCommand.supportedForFirmware(id,0,oldInfo));
   assertFalse(TertiaryFxCommand.supportedForFirmware(38,4,oldInfo));
   assertFalse(TertiaryFxCommand.supportedForFirmware(95,0,oldInfo));
   for(int id:supported)
     assertTrue("13-mode BLE admission "+id,
       TertiaryFxCommand.supportedForFirmware(id,id==38?4:0,newInfo));
   for(int id:unsupported){
     assertFalse("unsupported renderer mode "+id,TertiaryFxCommand.supported(id));
     assertFalse("unsupported BLE mode "+id,TertiaryFxCommand.supportedForFirmware(id,0,newInfo));
   }
 }
 @Test public void auroraAndPopcornEncodeExplicitBlack() {
   byte[] aurora=TertiaryFxCommand.encode(38,1,1,255,0,0,0,255,0,24,255,128,4,0x000000);
   assertEquals(20,aurora.length);assertEquals(11,aurora[1]&255);
   assertEquals(38,aurora[2]&255);assertEquals(1,aurora[16]&255);
   assertEquals(0,aurora[17]&255);assertEquals(0,aurora[18]&255);assertEquals(0,aurora[19]&255);
   byte[] popcorn=TertiaryFxCommand.encode(95,2,4,12,34,56,78,90,123,24,0,255,0,0x000000);
   assertEquals(95,popcorn[2]&255);assertEquals(1,popcorn[16]&255);
   assertEquals(0,popcorn[17]&255);assertEquals(0,popcorn[18]&255);assertEquals(0,popcorn[19]&255);
 }
 @Test public void invalidAuroraPaletteAndUnsupportedIdsFailClosed() {
   for(int id:new int[]{10,11,40,48,60,91,0,32781})
     assertFalse(TertiaryFxCommand.supported(id));
   try{TertiaryFxCommand.encode(38,1,1,1,2,3,4,5,6,24,128,128,5,0);fail();}
   catch(IllegalArgumentException expected){}
   try{TertiaryFxCommand.encode(54,1,1,1,2,3,4,5,6,33,128,128,0,0);fail();}
   catch(IllegalArgumentException expected){}
 }
}
