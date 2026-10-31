package com.opentail.android;

import android.content.SharedPreferences;
import org.json.JSONObject;
import org.junit.Test;
import java.lang.reflect.Proxy;
import java.util.HashMap;
import java.util.Map;
import static org.junit.Assert.*;

/** Real JSON with independently failing project and effect preference stores. */
public class NativePresetRestoreTest {
    static final class MemoryStore {
        final Map<String,Object> values=new HashMap<>();
        boolean fail;
        final SharedPreferences prefs;
        MemoryStore() {
            prefs=(SharedPreferences)Proxy.newProxyInstance(
                SharedPreferences.class.getClassLoader(),new Class<?>[]{SharedPreferences.class},
                (p,method,args)->{
                    switch(method.getName()){
                        case "getString":return values.getOrDefault(args[0],args[1]);
                        case "getInt":return values.getOrDefault(args[0],args[1]);
                        case "getAll":return new HashMap<>(values);
                        case "contains":return values.containsKey(args[0]);
                        case "edit":return editor();
                        case "toString":return "MemoryStore";
                        default:return method.getReturnType().isPrimitive()?false:null;
                    }
                });
        }
        Object editor() {
            final Map<String,Object> staged=new HashMap<>();
            final java.util.Set<String> remove=new java.util.HashSet<>();
            final boolean[] clear={false};
            return Proxy.newProxyInstance(
                SharedPreferences.Editor.class.getClassLoader(),
                new Class<?>[]{SharedPreferences.Editor.class},
                (p,method,args)->{
                    switch(method.getName()){
                        case "putString":
                        case "putInt":staged.put((String)args[0],args[1]);return p;
                        case "remove":remove.add((String)args[0]);return p;
                        case "clear":clear[0]=true;return p;
                        case "commit":
                        case "apply":
                            if(fail)return "commit".equals(method.getName())?false:null;
                            if(clear[0]) values.clear();
                            for(String key:remove)values.remove(key);
                            values.putAll(staged);
                            return "commit".equals(method.getName())?true:null;
                        case "toString":return "MemoryEditor";
                        default:return SharedPreferences.Editor.class.isAssignableFrom(method.getReturnType())?p:false;
                    }
                });
        }
    }

    private JSONObject preset(NativeProject model,int effectId) throws Exception {
        return new JSONObject().put("studio",model.exportJson()).put("fxId",effectId)
            .put("effectSettings",new JSONObject().put("brightness",208).put("paletteId",4))
            .put("effectEditorSettings",new JSONObject().put("wled.9.speed",201)
                  .put("wled.9.color2",0xff70e040));
    }

    @Test public void effectCommitFailureMustNotImportNewDesignOrChangeSelection() throws Exception {
        MemoryStore projectStore=new MemoryStore(),fxStore=new MemoryStore();
        NativeProject project=new NativeProject(projectStore.prefs);
        JSONObject saved=preset(project,9);
        assertTrue(project.addZoneAt("Existing",12,0));
        String before=project.exportJson();
        fxStore.values.put("effectId",3);
        fxStore.fail=true;
        assertFalse(NativePresetRestore.load(project,fxStore.prefs,saved));
        assertEquals(before,project.exportJson());
        assertEquals(3,fxStore.prefs.getInt("effectId",0));
        assertFalse(fxStore.prefs.contains("wled.9.speed"));
    }

    @Test public void successfulLoadRestoresBothDesignAndScopedEditor() throws Exception {
        MemoryStore projectStore=new MemoryStore(),fxStore=new MemoryStore();
        NativeProject project=new NativeProject(projectStore.prefs);
        JSONObject saved=preset(project,9);
        assertTrue(project.addZoneAt("Unsaved",8,0));
        assertTrue(NativePresetRestore.load(project,fxStore.prefs,saved));
        assertEquals(0,project.zoneCount());
        assertEquals(9,fxStore.prefs.getInt("effectId",-1));
        assertEquals(208,fxStore.prefs.getInt("brightness",0));
        assertEquals(4,fxStore.prefs.getInt("paletteId",0));
        assertEquals(201,fxStore.prefs.getInt("wled.9.speed",0));
        assertEquals(0xff70e040,fxStore.prefs.getInt("wled.9.color2",0));
    }

    @Test public void projectCommitFailureRestoresOldPrefs() throws Exception {
        MemoryStore projectStore=new MemoryStore(),fxStore=new MemoryStore();
        NativeProject project=new NativeProject(projectStore.prefs);
        JSONObject saved=preset(project,9);
        fxStore.values.put("effectId",4);
        fxStore.values.put("wled.9.speed",70);
        String before=project.exportJson();
        projectStore.fail=true;
        assertFalse(NativePresetRestore.load(project,fxStore.prefs,saved));
        assertEquals(before,project.exportJson());
        assertEquals(4,fxStore.prefs.getInt("effectId",0));
        assertEquals(70,fxStore.prefs.getInt("wled.9.speed",0));
        assertFalse(fxStore.prefs.contains("paletteId"));
    }

    @Test public void invalidEditorPreferenceRejectsBeforeAnyCommit() throws Exception {
        MemoryStore projectStore=new MemoryStore(),fxStore=new MemoryStore();
        NativeProject project=new NativeProject(projectStore.prefs);
        JSONObject saved=preset(project,9);
        saved.getJSONObject("effectEditorSettings").put("wled.9.fakeWireOpcode",123);
        String before=project.exportJson();
        assertFalse(NativePresetRestore.load(project,fxStore.prefs,saved));
        assertEquals(before,project.exportJson());
        assertTrue(fxStore.values.isEmpty());
    }
}
