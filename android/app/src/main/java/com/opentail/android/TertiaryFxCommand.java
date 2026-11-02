package com.opentail.android;

/** Firmware #96 exact 20-byte authenticated continuous FX tertiary packet.
 * Legacy opcode 10 remains unchanged; no fallback when tertiary is requested.
 */
final class TertiaryFxCommand {
    static final int OPCODE=11;
    private static final int[] SOURCE_VERIFIED={28,29,34,38,50,52,54,55,56,84,87,95,103};
    static boolean supported(int id){
        for(int value:SOURCE_VERIFIED)if(value==id)return true;
        return false;
    }
    /** Source support is 13 IDs; Aurora RGB3 is effective only with palette 4. */
    static boolean supported(int id,int palette){
        return supported(id) && (id!=38 || palette==4);
    }
    static boolean advertised(String info){
        if(info==null||!info.startsWith("OpenTail CORE "))return false;
        for(String part:info.split("\\|"))
            if(part.trim().equals("fx-tertiary-v1"))return true;
        return false;
    }
    /** Extra INFO token distinguishes new 13-ID CORE from older 11-ID CORE. */
    static boolean advertised13(String info){
        if(!advertised(info))return false;
        for(String part:info.split("\\|"))
            if(part.trim().equals("fx-tertiary13-v1"))return true;
        return false;
    }
    static byte[] encode(int id,int chains,int zones,int r,int g,int b,
        int br,int bg,int bb,int brightness,int speed,int intensity,int palette,
        int tertiary) {
        if(!supported(id,palette))
            throw new IllegalArgumentException("Tertiary effect/palette not source verified");
        // Reuse existing strict validation: target masks, RGB, brightness and palette.
        byte[] legacy=ContinuousFxCommand.encode(id,chains,zones,r,g,b,
            br,bg,bb,brightness,speed,intensity,palette);
        byte[] out=new byte[20];
        System.arraycopy(legacy,0,out,0,16);
        out[1]=OPCODE;
        out[16]=1; // explicit third present: #000000 is valid, not omitted.
        out[17]=(byte)(tertiary>>16);
        out[18]=(byte)(tertiary>>8);
        out[19]=(byte)tertiary;
        return out;
    }
}
