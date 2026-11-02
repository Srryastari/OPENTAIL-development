package com.opentail.android;
import org.junit.Test;
import static org.junit.Assert.*;

public class PreviewFrameClockTest {
 @Test public void monotonicFrameTimeAt30_60_120Hz(){
   for(int fps:new int[]{30,60,120}){
     PreviewFrameClock clock=new PreviewFrameClock(fps);
     long period=1_000_000_000L/fps;
     for(int i=0;i<=fps;i++){
       PreviewFrameClock.Sample s=clock.onFrame(i*period);
       assertTrue(s.advance);
       assertTrue(s.frameMs>=1 && s.frameMs<=34);
       assertEquals(0L,s.skippedIntervals);
     }
     assertTrue("fps="+fps,Math.abs(clock.onFrame(fps*period).elapsedMs-1000)<2);
     assertEquals(fps+1,clock.acceptedFrames());
   }
 }
 @Test public void recomposeAndBackwardsTimestampCannotDoubleAdvance(){
   PreviewFrameClock clock=new PreviewFrameClock();
   assertTrue(clock.onFrame(1_000_000_000L).advance);
   assertFalse(clock.onFrame(1_000_000_000L).advance);
   assertFalse(clock.onFrame(999_999_999L).advance);
   PreviewFrameClock.Sample next=clock.onFrame(1_016_666_667L);
   assertTrue(next.advance);assertEquals(16,next.elapsedMs);
   assertEquals(2,clock.acceptedFrames());
 }
 @Test public void outputsRemainIndependentEvenWhenOtherOutputStalls(){
   PreviewFrameClock o1=new PreviewFrameClock(60),o2=new PreviewFrameClock(60);
   o1.onFrame(0);o2.onFrame(0);
   o1.onFrame(16_666_667L);o2.onFrame(16_666_667L);
   o1.onFrame(33_333_334L); // O2 is skipped in the simulated display path.
   assertEquals(3,o1.acceptedFrames());
   assertEquals(2,o2.acceptedFrames());
   PreviewFrameClock.Sample nextO2=o2.onFrame(50_000_001L);
   assertTrue(nextO2.skippedIntervals>=1);
   assertEquals(0,o1.skippedFrames());
   assertEquals(1,o2.skippedFrames());
   assertEquals(3,o2.acceptedFrames());
 }
 @Test public void stalledFramesPreserveElapsedWithoutAnArtificialTtl(){
   PreviewFrameClock clock=new PreviewFrameClock();
   clock.onFrame(0);
   PreviewFrameClock.Sample s=clock.onFrame(2_000_000_000L);
   assertEquals(2000L,s.elapsedMs);
   assertTrue(s.skippedIntervals>=100);
   assertEquals(2000,s.frameMs);
 }
}
