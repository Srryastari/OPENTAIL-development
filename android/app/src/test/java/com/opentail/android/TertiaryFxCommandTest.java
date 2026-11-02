package com.opentail.android;
import org.junit.Test;
import static org.junit.Assert.*;

public class TertiaryFxCommandTest {
 @Test public void exactVersionTokenRequired() {
   assertTrue(TertiaryFxCommand.advertised("OpenTail CORE 0.1 |continuous-fx-v1|fx-tertiary-v1"));
   assertFalse(TertiaryFxCommand.advertised("OpenTail CORE 0.1 |continuous-fx-v1"));
   assertFalse(TertiaryFxCommand.advertised("OpenTail CORE 0.1 |fx-tertiary-v11"));
   assertFalse(TertiaryFxCommand.advertised(null));
 }
 @Test public void explicitBlackAndColorSlotAreEncoded() {
   byte[] black=TertiaryFxCommand.encode(54,1,1,255,0,0,0,255,0,24,255,128,0,0xff000000);
   assertEquals(20,black.length);assertEquals(1,black[0]);assertEquals(11,black[1]);
   assertEquals(54,black[2]&255);assertEquals(1,black[16]);
   assertEquals(0,black[17]);assertEquals(0,black[18]);assertEquals(0,black[19]);
   byte[] colored=TertiaryFxCommand.encode(54,2,4,12,34,56,78,90,123,24,0,255,0,0xffaabbcc);
   assertEquals(0xaa,colored[17]&255);assertEquals(0xbb,colored[18]&255);assertEquals(0xcc,colored[19]&255);
   assertEquals(2,colored[4]);assertEquals(4,colored[5]);
 }
 @Test public void rejectedEffectsNeverSilentlyFallback() {
   for(int id:new int[]{10,11,38,40,48,60,91,95,0,32781})
       assertFalse(TertiaryFxCommand.supported(id));
   for(int id:new int[]{28,29,34,50,52,54,55,56,84,87,103})
       assertTrue(TertiaryFxCommand.supported(id));
   try{TertiaryFxCommand.encode(38,1,1,1,2,3,4,5,6,24,128,128,0,0);fail();}
   catch(IllegalArgumentException expected){}
   try{TertiaryFxCommand.encode(54,1,1,1,2,3,4,5,6,33,128,128,0,0);fail();}
   catch(IllegalArgumentException expected){}
 }
}
