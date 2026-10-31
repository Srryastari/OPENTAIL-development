package com.opentail.android;

/** Local RGB-only subset, not CORE telemetry or a full WLED renderer.
 * Adapted from CORE Effects #50 1767b144e193ed6978816899ebb5eba237ee6e2d,
 * core-firmware/main/wled_fx_compat.c: scaled, mix_channel, wheel_rgb,
 * wave16, wipe_frame and ot_wled_fx_render (IDs 0/1/3/9/12).
 * Upstream WLED 20e7a717664295eefc6ad77972cee1888d2da072; copyright
 * Harm Aldick and WLED contributors, EUPL-1.2-or-later.
 * See android/third_party/wled/LICENSE and README.md for scope/attribution.
 * Direct colors (palette 0) only; no gamma, bus brightness, zone arbitration,
 * custom options, audio or hardware-frame claims. One call per local frame.
 */
final class OfflineFxSketch {
    private long step;
    static boolean supports(int id) { return id==0||id==1||id==3||id==9||id==12; }
    static boolean supports(int id,int palette) { return supports(id)&&palette==0; }

    boolean render(int id,int palette,long elapsedMs,int fg,int bg,int speed,
                   int intensity,int brightness,int[] destination) {
        if(!supports(id,palette)||destination==null||destination.length==0||
           destination.length>65535||elapsedMs<0)return false;
        long time=elapsedMs&0xffffffffL;
        speed=Math.max(0,Math.min(255,speed));
        intensity=Math.max(0,Math.min(255,intensity));
        brightness=Math.max(0,Math.min(255,brightness));
        int count=destination.length;
        boolean on=true;
        // Use a nominal local FRAMETIME=16ms; CORE supplies its own runtime
        // frame interval. Preserve its sparse-frame catch-up rule.
        if(id==1 && brightness!=0) {
            long cycle=(255-speed)*20L;
            long onTime=16+((cycle*intensity)>>8);
            cycle+=32;
            long index=time/cycle;
            on=index!=step || time%cycle<=onTime;
            step=index;
        }
        long progress=id==3 ? (time%(750+(255-speed)*150L))*65535/
            (750+(255-speed)*150L) : 0;
        boolean back=progress>32767;
        long half=back?progress-32767:progress;
        int led=(int)((half*count)>>15);
        int mix=(int)Math.min(255,((half*count*2)&65535)/(intensity+1));
        int counter=(int)(time*((speed>>3)+10))&65535;
        int triangle=counter<32768?counter*2:65535-(counter-32768)*2;
        int base=(int)(((time*((speed>>2)+2))&65535)>>8);
        for(int i=0;i<count;i++) {
            int color;
            switch(id) {
                case 1: color=on?fg:bg;break;
                case 3:
                    if(count==1)color=fg;
                    else if(i<led)color=back?bg:fg;
                    else if(i==led)color=blend(back?fg:bg,back?bg:fg,mix);
                    else color=back?fg:bg;
                    break;
                case 9: color=wheel((i*(16<<(intensity/29))/count+base)&255);break;
                case 12: color=blend(bg,fg,triangle>>8);break;
                default: color=fg;
            }
            destination[i]=scale(color,brightness);
        }
        return true;
    }
    private static int scale(int color,int brightness) {
        return 0xff000000|(((((color>>16)&255)*brightness+127)/255)<<16)|
            (((((color>>8)&255)*brightness+127)/255)<<8)|
            (((color&255)*brightness+127)/255);
    }
    private static int blend(int a,int b,int mix) {
        int color=0xff000000;
        for(int shift=0;shift<=16;shift+=8)
            color|=(((((a>>shift)&255)*(256-mix)+
                ((b>>shift)&255)*(1+mix))>>8)&255)<<shift;
        return color;
    }
    private static int wheel(int hue) {
        int thirds=((hue<<8)&8191)*21846;
        int third=(thirds>>21)&255,twice=(thirds>>20)&255;
        int r,g,b;
        switch(hue>>5) {
            case 0:r=255-third;g=third;b=0;break;
            case 1:r=171;g=85+third;b=0;break;
            case 2:r=171-twice;g=170+third;b=0;break;
            case 3:r=0;g=255-third;b=third;break;
            case 4:r=0;g=171-twice;b=85+twice;break;
            case 5:r=third;g=0;b=255-third;break;
            case 6:r=85+third;g=0;b=171-third;break;
            default:r=170+third;g=0;b=85-third;
        }
        return 0xff000000|((r&255)<<16)|((g&255)<<8)|(b&255);
    }
}
