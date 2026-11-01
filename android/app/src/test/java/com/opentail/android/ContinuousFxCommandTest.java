package com.opentail.android;
import org.junit.Test;
import static org.junit.Assert.*;
public class ContinuousFxCommandTest {
 @Test public void capabilityFailsClosedAndRequiresExactToken(){
  assertFalse(ContinuousFxCommand.advertised(null));
  assertFalse(ContinuousFxCommand.advertised("OpenTail CORE old | BLE HMAC enrollment bench v2"));
  assertFalse(ContinuousFxCommand.advertised("OpenTail CORE new | continuous-fx-v10"));
  assertTrue(ContinuousFxCommand.advertised("OpenTail CORE new | BLE HMAC enrollment bench v2 | continuous-fx-v1"));
 }
 @Test public void wireHasDistinctOpcodeAndNoTtl(){
  byte[] p=ContinuousFxCommand.encode(32777,1,3,255,0,1,0,0,0,24,128,200,0);
  assertEquals(17,p.length);assertEquals(1,p[0]);assertEquals(10,p[1]);
  assertEquals(9,p[2]&255);assertEquals(128,p[3]&255);
  assertEquals(0,p[16]);assertEquals(1,p[4]);
 }
 @Test public void invalidCommandsCannotBeEncoded(){
  for(int chains:new int[]{0,4})try{
   ContinuousFxCommand.encode(0,chains,1,255,0,0,0,0,0,24,128,128,0);
   fail("bad target");
  }catch(IllegalArgumentException expected){}
  try {ContinuousFxCommand.encode(0,1,1,255,0,0,0,0,0,33,128,128,0);fail("brightness");}
  catch(IllegalArgumentException expected){}
  try {ContinuousFxCommand.encode(0,1,1,255,0,0,0,0,0,24,128,128,200);fail("unsupported palette");}
  catch(IllegalArgumentException expected){}
 }
}
