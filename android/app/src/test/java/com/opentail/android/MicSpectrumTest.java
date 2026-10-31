package com.opentail.android;
import org.junit.Test;
import static org.junit.Assert.*;
public class MicSpectrumTest {
    @Test public void resolvesMeasuredSineAndSilence(){
        short[] pcm=new short[MicSpectrum.SIZE];
        for(int i=0;i<pcm.length;i++)pcm[i]=(short)(2048*Math.sin(2*Math.PI*440*i/MicSpectrum.SAMPLE_RATE));
        float[] peak=new MicSpectrum().analyze(pcm);
        assertEquals(440,peak[0],5);
        assertTrue(peak[1]>1700 && peak[1]<2200);
        peak=new MicSpectrum().analyze(new short[MicSpectrum.SIZE]);
        assertEquals(0,peak[1],0);
    }
}
