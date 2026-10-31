package com.opentail.android;

/** Frame-stable settings shared by the editor's scoped preferences and preview.
 * Pure reader so parameter propagation is executable in local JVM tests.
 */
final class OfflinePreviewSettings {
    interface Reader {int get(String key,int fallback);}
    static int[] read(int effect,Reader prefs) {
        String scope="wled."+effect+".";
        return new int[]{
            prefs.get(scope+"color0",prefs.get(scope+"foreground",prefs.get("foreground",0xffffffff))),
            prefs.get(scope+"color1",prefs.get(scope+"background",prefs.get("background",0xff000000))),
            prefs.get(scope+"speed",prefs.get("speed",128)),
            prefs.get(scope+"intensity",prefs.get("intensity",128)),
            prefs.get(scope+"brightness",prefs.get("brightness",160)),
            prefs.get(scope+"palette",prefs.get("paletteId",0))};
    }
}
