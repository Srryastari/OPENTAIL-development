package com.opentail.android;

/** Authenticated 20-byte opcode-11 continuous FX command.
 * The legacy 11-mode capability remains backward compatible; the explicit
 * fx-tertiary13-v1 token unlocks the two additional source-proven consumers.
 */
final class TertiaryFxCommand {
    static final int OPCODE=11;
    static final String LEGACY_CAPABILITY="fx-tertiary-v1";
    static final String CAPABILITY_13="fx-tertiary13-v1";
    private static final int[] SOURCE_VERIFIED_11={28,29,34,50,52,54,55,56,84,87,103};
    private static final int[] SOURCE_VERIFIED_13={28,29,34,38,50,52,54,55,56,84,87,95,103};

    private static boolean contains(int[] values,int id){
        for(int value:values)if(value==id)return true;
        return false;
    }
    private static boolean hasToken(String info,String token){
        if(info==null||!info.startsWith("OpenTail CORE "))return false;
        for(String part:info.split("\\|"))
            if(part.trim().equals(token))return true;
        return false;
    }
    /** Source-level renderer support, used by local JNI preview. */
    static boolean supported(int id){return contains(SOURCE_VERIFIED_13,id);}
    /** Any third-color-capable firmware, including the legacy 11-mode tier. */
    static boolean advertised(String info){
        return hasToken(info,LEGACY_CAPABILITY)||hasToken(info,CAPABILITY_13);
    }
    static boolean advertised13(String info){return hasToken(info,CAPABILITY_13);}
    /** BLE admission is capability-tiered; old firmware never receives IDs 38/95. */
    static boolean supportedForFirmware(int id,int palette,String info){
        if(advertised13(info))
            return supported(id)&&(id!=38||palette==4);
        return hasToken(info,LEGACY_CAPABILITY)&&contains(SOURCE_VERIFIED_11,id);
    }
    static byte[] encode(int id,int chains,int zones,int r,int g,int b,
        int br,int bg,int bb,int brightness,int speed,int intensity,int palette,
        int tertiary) {
        if(!supported(id))throw new IllegalArgumentException("Tertiary effect not source verified");
        if(id==38&&palette!=4)
            throw new IllegalArgumentException("Aurora third color requires source-authentic palette 4");
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
