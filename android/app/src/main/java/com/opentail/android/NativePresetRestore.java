package com.opentail.android;

import android.content.SharedPreferences;
import org.json.JSONArray;
import org.json.JSONObject;
import org.json.JSONException;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * The effect preference write MUST succeed before replacing the local project.
 * A failed commit cannot leave a newly loaded design with old FX controls.
 * No external hardware/NVS writes are performed.
 */
final class NativePresetRestore {
    private static final String[] FX_KEYS = {
        "speed","intensity","brightness","foreground","background","accent","paletteId"
    };
    private NativePresetRestore(){ }

    static boolean load(NativeProject project, SharedPreferences prefs, JSONObject preset) {
        if (project == null || prefs == null || preset == null) return false;
        final JSONObject draft;
        final LinkedHashMap<String,Integer> updates=new LinkedHashMap<>();
        try {
            draft=new JSONObject(preset.getString("studio"));
            // Preset activation must never destroy the user's preset library.
            draft.put("savedPresets",new JSONArray(project.presets().toString()));
            if (preset.has("fxId")) {
                int id=preset.getInt("fxId");
                if (id < 0 || id > 65535) return false;
                updates.put("effectId",id);
            }
            JSONObject values=preset.optJSONObject("effectSettings");
            if (values!=null) for(String key:FX_KEYS) if(values.has(key)) {
                Object raw=values.get(key);
                if(!(raw instanceof Number)) return false;
                int v=((Number)raw).intValue();
                if(("speed".equals(key)||"intensity".equals(key)||"brightness".equals(key))
                        && (v<0||v>255)) return false;
                if("paletteId".equals(key)&&(v<0||v>NativePaletteEngine.MAX_ID))
                    return false;
                updates.put(key,v);
            }
            JSONObject scoped=preset.optJSONObject("effectEditorSettings");
            if(scoped!=null) {
                java.util.Iterator<String> keys=scoped.keys();
                while(keys.hasNext()) {
                    String key=keys.next();
                    if(!key.matches("wled\\.\\d{1,5}\\.(?:speed|intensity|brightness|foreground|background|accent|palette|color[0-2]|param[2-7]|temp[0-2])"))
                        return false;
                    Object raw=scoped.get(key);
                    if(!(raw instanceof Number)) return false;
                    updates.put(key,((Number)raw).intValue());
                }
            }
        } catch (Exception ex) { return false; }

        // Snapshot only touched keys, preserving absent vs explicit values.
        final Map<String,Integer> previous=new LinkedHashMap<>();
        final List<String> absent=new ArrayList<>();
        for(String key:updates.keySet()) {
            if(prefs.contains(key)) previous.put(key,prefs.getInt(key,0));
            else absent.add(key);
        }
        SharedPreferences.Editor editor=prefs.edit();
        for(Map.Entry<String,Integer> item:updates.entrySet())
            editor.putInt(item.getKey(),item.getValue());
        if(!editor.commit())return false;  // project is untouched
        if(project.importJson(draft.toString()))return true;

        // Second-stage project storage failure: restore previous editor values.
        SharedPreferences.Editor rollback=prefs.edit();
        for(String key:absent)rollback.remove(key);
        for(Map.Entry<String,Integer> item:previous.entrySet())
            rollback.putInt(item.getKey(),item.getValue());
        rollback.commit();
        return false;
    }
}
