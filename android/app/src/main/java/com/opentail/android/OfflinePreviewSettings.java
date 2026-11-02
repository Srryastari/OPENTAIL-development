package com.opentail.android;

/** Frame-stable settings shared by the editor's scoped preferences and preview.
 * Pure reader so parameter propagation is executable in local JVM tests.
 */
final class OfflinePreviewSettings {
    interface Reader {int get(String key,int fallback);}
    interface Writer {void put(String key,int value);}
    static void snapshot(int effect,Reader prefs,Writer writer) {
        if(effect<0)return;
        int[] values=read(effect,prefs);
        String scope="wled."+effect+".";
        String[] keys={"foreground","background","speed","intensity","brightness","palette"};
        for(int i=0;i<keys.length;i++)writer.put(scope+keys[i],values[i]);
        writer.put(scope+"accent",prefs.get(scope+"color2",prefs.get(scope+"accent",
            prefs.get("effectId",effect)==effect?prefs.get("accent",0xffffffff):0xffffffff)));
    }
    static int[] read(int effect,Reader prefs) {
        String scope="wled."+effect+".";
        Reader source=(key,fallback)->prefs.get("effectId",-1)==effect ? prefs.get(key,fallback) : fallback;
        return new int[]{
            prefs.get(scope+"color0",prefs.get(scope+"foreground",source.get("foreground",0xffffffff))),
            prefs.get(scope+"color1",prefs.get(scope+"background",source.get("background",0xff000000))),
            prefs.get(scope+"speed",source.get("speed",128)),
            prefs.get(scope+"intensity",source.get("intensity",128)),
            prefs.get(scope+"brightness",source.get("brightness",160)),
            prefs.get(scope+"palette",source.get("paletteId",0))};
    }
}
