package com.opentail.android;

import android.content.SharedPreferences;
import org.json.JSONArray;
import org.json.JSONObject;
import org.junit.Test;

import java.lang.reflect.Proxy;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

import static org.junit.Assert.*;

/** Real JSON + deterministic SharedPreferences commit failures, no phone needed. */
public class NativeProjectCommitTest {
    private static final class Storage {
        String persisted;
        boolean fail;
        int commits;
        final SharedPreferences prefs;

        Storage() {
            final SharedPreferences.Editor editor = (SharedPreferences.Editor) Proxy.newProxyInstance(
                SharedPreferences.Editor.class.getClassLoader(),
                new Class<?>[]{SharedPreferences.Editor.class},
                (proxy, method, args) -> {
                    switch (method.getName()) {
                        case "putString":
                            pending = (String) args[1];
                            return proxy;
                        case "commit":
                            commits++;
                            if (fail) return false;
                            persisted = pending;
                            return true;
                        case "apply":
                            if (!fail) persisted = pending;
                            return null;
                        case "clear":
                            pending = null;
                            return proxy;
                        case "toString":
                            return "FakeEditor";
                        default:
                            if (SharedPreferences.Editor.class.isAssignableFrom(method.getReturnType()))
                                return proxy;
                            return false;
                    }
                });
            prefs = (SharedPreferences) Proxy.newProxyInstance(
                SharedPreferences.class.getClassLoader(),
                new Class<?>[]{SharedPreferences.class},
                (proxy, method, args) -> {
                    switch (method.getName()) {
                        case "getString":
                            return persisted == null ? args[1] : persisted;
                        case "edit":
                            return editor;
                        case "contains":
                            return persisted != null;
                        case "toString":
                            return "FakePrefs";
                        default:
                            return method.getReturnType().isPrimitive() ? false : null;
                    }
                });
        }
        String pending;
    }

    @Test public void failedCommitRestoresEveryNestedEditAndExport() throws Exception {
        Storage store = new Storage();
        NativeProject project = new NativeProject(store.prefs);
        assertTrue(project.addZoneAt("Left", 4, 0));
        String id = project.zone(0).getString("id");
        assertTrue(project.setPixelColor(id, 1, "#AABBCC"));
        String before = project.exportJson();
        String diskBefore = store.persisted;
        store.fail = true;

        assertFalse(project.editZone(id, "Unsaved", 5, true));
        assertEquals(before, project.exportJson());
        assertFalse(project.setPixelColor(id, 2, "#FF0000"));
        assertEquals(before, project.exportJson());
        assertFalse(project.clearPixelColor(id, 1));
        assertEquals(before, project.exportJson());
        assertFalse(project.name("Unsaved project name"));
        assertEquals(before, project.exportJson());
        assertFalse(project.removeZone(id));
        assertEquals(before, project.exportJson());
        assertEquals(diskBefore, store.persisted);
    }

    @Test public void triggerMultiSelectCommitsOnceOrNotAtAll() throws Exception {
        Storage store = new Storage();
        NativeProject project = new NativeProject(store.prefs);
        assertTrue(project.addZoneAt("Main", 10, 0));
        String id = project.zone(0).getString("id");
        Set<String> two = new HashSet<>(Arrays.asList("Brake", "Left Turn"));
        int previousCommits = store.commits;
        assertTrue(project.assignMany(two, id, true));
        assertEquals(previousCommits + 1, store.commits);
        for (String name : two) {
            JSONArray zones = project.trigger(name).getJSONArray("zones");
            assertEquals(1, zones.length());
            assertEquals(id, zones.getString(0));
        }
        String before = project.exportJson();
        store.fail = true;
        assertFalse(project.assignMany(new HashSet<>(Arrays.asList("Brake", "Right Turn")), id, false));
        assertEquals(before, project.exportJson());
        assertEquals(1, project.trigger("Brake").getJSONArray("zones").length());
        assertEquals(0, project.trigger("Right Turn").getJSONArray("zones").length());
    }

    @Test public void failedReindexBeforeCommitCannotLeavePartialLayout() throws Exception {
        Storage store = new Storage();
        NativeProject project = new NativeProject(store.prefs);
        assertTrue(project.addZoneAt("Near ceiling", 5, 0));
        JSONObject design = new JSONObject(project.exportJson());
        JSONObject zone = design.getJSONArray("layout").getJSONObject(0);
        zone.put("offsetMode", "manual");
        zone.put("start", 999995);
        assertTrue(project.importJson(design.toString()));
        String before = project.exportJson();
        int previousCommits = store.commits;
        assertFalse(project.addZoneAt("Would exceed ceiling", 30, 1));
        assertEquals(previousCommits, store.commits);
        assertEquals(before, project.exportJson());
        assertEquals(1, project.zoneCount());
    }
}
