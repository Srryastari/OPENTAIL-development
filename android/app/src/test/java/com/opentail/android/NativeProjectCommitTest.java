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

    @Test public void triggerStyleBatchIsAtomicAndRejectsIncompleteSelection() throws Exception {
        Storage store = new Storage();
        NativeProject project = new NativeProject(store.prefs);
        Set<String> selected = new HashSet<>(Arrays.asList("Brake", "Left Turn"));
        int oldCommits = store.commits;
        assertTrue(project.styleTriggers(selected, "Solid", "#112233"));
        assertEquals(oldCommits + 1, store.commits);
        assertEquals("#112233", project.trigger("Brake").getString("color"));
        assertEquals("#112233", project.trigger("Left Turn").getString("color"));
        assertFalse(project.styleTriggers(new HashSet<>(Arrays.asList("Brake", "UNKNOWN")), "Solid", "#ffffff"));
        assertFalse(project.styleTriggers(selected, "Solid", "invalid"));
        assertEquals(oldCommits + 1, store.commits);

        String before = project.exportJson();
        store.fail = true;
        assertFalse(project.styleTriggers(selected, "Wipe", "#abcdef"));
        assertEquals(before, project.exportJson());
        assertEquals("#112233", project.trigger("Brake").getString("color"));
        assertEquals("Solid", project.trigger("Left Turn").getString("pattern"));
    }

    @Test public void autoEventsAreValidatedAndCommittedAtomically() throws Exception {
        Storage store = new Storage();
        NativeProject project = new NativeProject(store.prefs);
        int commits = store.commits;
        assertFalse(project.setAutoEvent("Brake", "eventUiEnabled", true));
        assertFalse(project.setAutoEvent("Startup", "eventUiFadeMs", -1));
        assertFalse(project.setAutoEvent("Shutdown", "eventDuration", Double.NaN));
        assertFalse(project.setAutoEvent("Startup", "notAProperty", true));
        assertEquals(commits, store.commits);
        assertTrue(project.setAutoEvent("Startup", "eventFx", "Solid"));
        assertEquals(commits + 1, store.commits);
        String previous = project.exportJson();
        store.fail = true;
        assertFalse(project.setAutoEvent("Startup", "eventUiEnabled", true));
        assertEquals(previous, project.exportJson());
        assertFalse(project.setAutoEvent("Shutdown", "eventUiFadeMs", 120));
        assertEquals(previous, project.exportJson());
    }

    @Test public void importedPixelOffsetsAndDurationsRejectFractionalOrTextNumbers() throws Exception {
        Storage storage = new Storage();
        NativeProject project = new NativeProject(storage.prefs);
        assertTrue(project.addZoneAt("Universal", 8, 0));
        String original = project.exportJson();
        int commits = storage.commits;

        for (Object invalid : new Object[]{1.5, "1", -1, 1000000}) {
            JSONObject copy = new JSONObject(original);
            copy.getJSONArray("layout").getJSONObject(0).put("start", invalid);
            assertFalse("Invalid offset " + invalid, project.importJson(copy.toString()));
            assertEquals(original, project.exportJson());
        }
        for (Object invalid : new Object[]{8.5, "8", 0, 10001}) {
            JSONObject copy = new JSONObject(original);
            copy.getJSONArray("layout").getJSONObject(0).put("length", invalid);
            assertFalse("Invalid zone length " + invalid, project.importJson(copy.toString()));
            assertEquals(original, project.exportJson());
        }
        for (Object invalid : new Object[]{50.5, "100", 49, 600001}) {
            JSONObject copy = new JSONObject(original);
            copy.put("sequence", new JSONArray().put(new JSONObject()
                .put("fx", "Solid").put("ms", invalid)));
            assertFalse("Invalid sequence duration " + invalid, project.importJson(copy.toString()));
            assertEquals(original, project.exportJson());
        }
        assertEquals("Rejected imports must not write storage", commits, storage.commits);

        JSONObject valid = new JSONObject(original);
        valid.getJSONArray("layout").getJSONObject(0).put("start", 999992L);
        assertTrue(project.importJson(valid.toString()));
        assertEquals(999992, project.zone(0).getInt("start"));
    }

    @Test public void malformedTriggerProfilesNeverOverwriteSavedDataOnRead() throws Exception {
        Storage storage = new Storage();
        NativeProject project = new NativeProject(storage.prefs);
        String original = project.exportJson();
        int commits = storage.commits;
        for (Object invalid : new Object[]{new JSONArray(), "malformed", JSONObject.NULL}) {
            JSONObject imported = new JSONObject(original);
            imported.getJSONObject("profiles").put("Brake", invalid);
            assertFalse(project.importJson(imported.toString()));
            assertEquals(original, project.exportJson());
            assertEquals(commits, storage.commits);
        }

        // Unknown lookups are read-only and cannot silently add an entry.
        assertNotNull(project.trigger("Unknown local profile"));
        assertEquals(original, project.exportJson());
        assertEquals(commits, storage.commits);

        // Missing built-in keys in older project versions are still restored.
        JSONObject legacy = new JSONObject(original);
        legacy.put("version", 3);
        legacy.getJSONObject("profiles").remove("Brake");
        assertTrue(project.importJson(legacy.toString()));
        assertEquals("Solid", project.trigger("Brake").getString("pattern"));
        assertEquals(4, new JSONObject(project.exportJson()).getInt("version"));
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
