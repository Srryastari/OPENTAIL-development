package com.opentail.android;

/** Frame-stable settings shared by the editor's scoped preferences and preview.
 * Pure reader so parameter propagation is executable in local JVM tests.
 */
final class OfflinePreviewSettings {
    interface Reader {int get(String key,int fallback);}
    static int[] read(int effect,Reader prefs) {
        String scope="wled."+effect+".";
        Reader source=(key,fallback)->prefs.get("effectId",effect)==effect ? prefs.get(key,fallback) : fallback;
        return new int[]{
            prefs.get(scope+"color0",prefs.get(scope+"foreground",source.get("foreground",0xffffffff))),
            prefs.get(scope+"color1",prefs.get(scope+"background",source.get("background",0xff000000))),
            prefs.get(scope+"speed",source.get("speed",128)),
            prefs.get(scope+"intensity",source.get("intensity",128)),
            prefs.get(scope+"brightness",source.get("brightness",160)),
            prefs.get(scope+"palette",source.get("paletteId",0))};
    }
}
