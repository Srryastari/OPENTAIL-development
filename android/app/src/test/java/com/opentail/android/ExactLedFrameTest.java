package com.opentail.android;

import static org.junit.Assert.*;
import org.junit.Test;
import java.util.Arrays;

/** Tests address fidelity; does NOT certify physical CORE FX frame parity. */
public class ExactLedFrameTest {
    @Test public void everyPixelIsPresentWithNoFixed28Or32DotSampling() {
        int[] source=new int[1246];
        for(int i=0;i<source.length;i++)
            source[i]=0xff000000|((i&255)<<16)|(((i/3)&255)<<8)|(i%251);
        ExactLedFrame frame=ExactLedFrame.fromProject(source);
        assertEquals(1246,frame.size());
        assertEquals(ExactLedFrame.Origin.PROJECT,frame.origin);
        for(int i=0;i<1246;i++)
            assertEquals("index "+i,source[i],frame.colorAt(i));
        source[42]=0;
        assertNotEquals(0,frame.colorAt(42)); // defensive snapshot copy
        int[] copy=frame.copyColors();
        copy[44]=0;
        assertEquals(source[44],frame.colorAt(44));
    }

    @Test public void reverseAndManualGapsPreserveExactGlobalOffsets() {
        int[] physical=new int[10];
        Arrays.fill(physical,0xff000000);
        ExactLedFrame.paintZone(physical,0,
            new int[]{0x00ff0000,0x0000ff00,0x000000ff},false);
        ExactLedFrame.paintZone(physical,6,
            new int[]{0x00ff9900,0x0000aaff,0x00001234},true);
        assertEquals(0xffff0000,physical[0]);
        assertEquals(0xff00ff00,physical[1]);
        assertEquals(0xff0000ff,physical[2]);
        for(int i=3;i<6;i++)assertEquals(0xff000000,physical[i]);
        assertEquals(0xff001234,physical[6]);
        assertEquals(0xff00aaff,physical[7]);
        assertEquals(0xffff9900,physical[8]);
        assertEquals(0xff000000,physical[9]);
    }

    @Test public void coreRgb888DecoderNeverSwizzlesOrDownsamples() {
        byte[] rgb=new byte[]{(byte)255,(byte)12,(byte)0,
             (byte)3,(byte)5,(byte)251,0,0,0};
        ExactLedFrame frame=ExactLedFrame.fromCoreRgb888(1,91,rgb);
        assertEquals(ExactLedFrame.Origin.CORE,frame.origin);
        assertEquals(1,frame.output);
        assertEquals(91,frame.frameNumber);
        assertEquals(3,frame.size());
        assertEquals(0xffff0c00,frame.colorAt(0));
        assertEquals(0xff0305fb,frame.colorAt(1));
        assertEquals(0xff000000,frame.colorAt(2));
        rgb[0]=0;
        assertEquals(0xffff0c00,frame.colorAt(0));
    }

    @Test public void invalidAndTruncatedDataCannotBecomeValidFrames() {
        for(byte[] bytes: new byte[][] {null,new byte[0],new byte[1],new byte[5]}) {
            try {ExactLedFrame.fromCoreRgb888(0,0,bytes);fail("accepted partial");}
            catch(IllegalArgumentException expected){}
        }
        try{ExactLedFrame.fromCoreRgb888(-1,0,new byte[3]);fail("output");}
        catch(IllegalArgumentException expected){}
        try{ExactLedFrame.fromCoreRgb888(0,-1,new byte[3]);fail("sequence");}
        catch(IllegalArgumentException expected){}
        try{ExactLedFrame.fromProject(new int[ExactLedFrame.MAX_PIXELS+1]);fail("size");}
        catch(IllegalArgumentException expected){}
        try{ExactLedFrame.paintZone(new int[3],2,new int[2],false);fail("bounds");}
        catch(IllegalArgumentException expected){}
    }
}
