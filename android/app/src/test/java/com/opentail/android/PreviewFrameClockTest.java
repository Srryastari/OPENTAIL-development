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

 @Test public void pauseAndResumeDoNotJumpTheAnimationPhase(){
   PreviewFrameClock c=new PreviewFrameClock(60);
   c.onFrame(0);
   assertEquals(16,c.onFrame(16_666_667L).elapsedMs);
   c.suspendTimestamp();
   PreviewFrameClock.Sample resumed=c.onFrame(5_000_000_000L);
   assertTrue(resumed.advance);
   assertEquals(16,resumed.elapsedMs);
   assertEquals(0,c.skippedFrames());
   assertEquals(33,c.onFrame(5_016_666_667L).elapsedMs);
 }
 @Test public void effectSwitchResetsOnlyTheNewRendererClock(){
   PreviewFrameClock first=new PreviewFrameClock();
   first.onFrame(0);first.onFrame(100_000_000L);
   PreviewFrameClock second=new PreviewFrameClock();
   assertEquals(0,second.onFrame(100_000_000L).elapsedMs);
   assertEquals(100,first.onFrame(100_000_000L).elapsedMs);
   assertEquals(16,second.onFrame(116_666_667L).elapsedMs);
 }
}
