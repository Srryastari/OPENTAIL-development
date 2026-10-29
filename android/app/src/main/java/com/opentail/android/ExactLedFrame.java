package com.opentail.android;

import java.util.Arrays;

/**
 * Pixel-exact 1D frame data, not an effect simulator.
 *
 * One array entry is one real logical LED index (0-based). It deliberately has
 * no clock, guessed effect implementation, downsampling, or hardcoded strip
 * layout. Frame provenance is explicit: PROJECT is local paint, CORE is an
 * actual renderer snapshot supplied by the controller.
 *
 * CORE snapshot transport is NOT available in the current BLE protocol.
 * Do not construct a CORE frame from guessed effect names or parameters.
 */
final class ExactLedFrame {
    static final int MAX_PIXELS = 65535; // representation ceiling, NOT device allocation
    enum Origin { PROJECT, CORE }

    final Origin origin;
    final int output;
    final long frameNumber;
    private final int[] argb;

    private ExactLedFrame(Origin origin,int output,long frameNumber,int[] values) {
        this.origin=origin;
        this.output=output;
        this.frameNumber=frameNumber;
        this.argb=values;
    }

    static ExactLedFrame fromProject(int[] colors) {
        return new ExactLedFrame(Origin.PROJECT,-1,0,checked(colors));
    }

    /** Exactly reconstruct the local zero-based 1D address map.
     * Honor auto/manual offsets, configured gaps and zone reversal. Local
     * paint is NOT a renderer FX frame or a reading of physical LED output. */
    static ExactLedFrame fromProjectModel(NativeProject project) {
        if(project==null)throw new IllegalArgumentException("missing project");
        int end=0;
        for(int z=0;z<project.zoneCount();z++) {
            org.json.JSONObject zone=project.zone(z);
            long start=zone.optLong("start",-1);
            long length=zone.optLong("length",-1);
            if(start<0||length<0||start+length>MAX_PIXELS)
                throw new IllegalArgumentException("design exceeds preview capacity");
            end=Math.max(end,(int)(start+length));
        }
        int[] colors=new int[end];
        Arrays.fill(colors,0xff000000);
        for(int z=0;z<project.zoneCount();z++) {
            org.json.JSONObject zone=project.zone(z);
            int start=zone.optInt("start");
            int length=zone.optInt("length");
            boolean reverse=zone.optBoolean("reverse",false);
            String id=zone.optString("id");
            for(int local=0;local<length;local++) {
                int address=start+(reverse?(length-1-local):local);
                String hex=project.pixelColor(id,local);
                colors[address]=0xff000000|Integer.parseInt(hex.substring(1),16);
            }
        }
        return fromProject(colors);
    }

    /** Accept only complete, validated, RGB888 renderer frame snapshots.
     * Byte 0..2 maps directly to physical pixel 0. No BRG/GRB guessing: CORE
     * must deliver canonical logical RGB after its renderer/arbitration stage.
     */
    static ExactLedFrame fromCoreRgb888(int output,long sequence,byte[] rgb) {
        if(output<0 || output>255 || sequence<0 || rgb==null ||
           rgb.length==0 || rgb.length%3!=0 || rgb.length/3>MAX_PIXELS)
            throw new IllegalArgumentException("invalid CORE RGB888 frame");
        int[] colors=new int[rgb.length/3];
        for(int i=0,j=0;i<colors.length;i++,j+=3)
            colors[i]=0xff000000|((rgb[j]&255)<<16)|
                ((rgb[j+1]&255)<<8)|(rgb[j+2]&255);
        return new ExactLedFrame(Origin.CORE,output,sequence,colors);
    }

    private static int[] checked(int[] input) {
        if(input==null || input.length>MAX_PIXELS)
            throw new IllegalArgumentException("invalid LED pixel count");
        int[] copy=Arrays.copyOf(input,input.length);
        for(int i=0;i<copy.length;i++)copy[i]=0xff000000|(copy[i]&0xffffff);
        return copy;
    }

    int size(){return argb.length;}
    int colorAt(int index){return argb[index];}
    int[] copyColors(){return Arrays.copyOf(argb,argb.length);}
}
