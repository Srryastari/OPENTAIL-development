package com.opentail.android;

import android.content.Context;
import android.content.SharedPreferences;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import java.util.HashSet;
import java.util.Set;
import java.util.UUID;

/**
 * Local authoring model compatible with the web Studio's v4 project document.
 * No vehicle-specific zones, fixed GPIOs or network side effects exist here.
 * An export is a design, NOT a firmware-ready or deployed configuration.
 */
final class NativeProject {
    static final String PREFS = "opentail_native_project_v1";
    static final String KEY = "project_json";
    static final int MAX_ZONES = 200;
    static final int MAX_SEQUENCE = 50;
    static final int MAX_PRESETS = 40;
    static final String[] MAIN_TRIGGERS = {
            "Park", "Brake", "Left Turn", "Right Turn", "Hazards", "Reverse"
    };
    private final SharedPreferences prefs;
    private JSONObject data;
    // Last successfully committed project, not merely the last in-memory edit.
    // All editors share this rollback point when persistent storage fails.
    private String committedJson;

    NativeProject(Context context) {
        this(context.getSharedPreferences(PREFS, Context.MODE_PRIVATE));
    }

    // Package-private injection permits real JSON and storage-failure tests
    // without Android UI or a connected controller.
    NativeProject(SharedPreferences preferences) {
        prefs = preferences;
        try { data = validated(new JSONObject(prefs.getString(KEY, "{}"))); }
        catch (Exception ignored) { data = empty(); }
        committedJson = data.toString();
    }

    private static JSONObject empty() {
        JSONObject p = new JSONObject();
        try {
            p.put("version", 4);
            p.put("projectName", "Untitled project");
            p.put("layout", new JSONArray());
            p.put("removedSections", new JSONArray());
            p.put("profiles", new JSONObject());
            p.put("sequence", new JSONArray());
            p.put("savedPresets", new JSONArray());
            p.put("selectedPixels", new JSONObject());
            p.put("sequenceOptions", new JSONObject()
                    .put("shuffleLoop", false).put("favoritesOnly", false)
                    .put("sharedPaletteEnabled", false).put("sharedPaletteId", 0));
            p.put("effectFavorites", new JSONArray());
            p.put("hardwareProfile", new JSONObject().put("chip", "Addressable")
                    .put("order", "RGB"));
            JSONObject profiles = p.getJSONObject("profiles");
            for (String trigger : MAIN_TRIGGERS) profiles.put(trigger, defaultTrigger(trigger));
            for (String event : new String[]{"Startup", "Shutdown"})
                profiles.put(event, defaultTrigger(event));
        } catch (JSONException ignored) { }
        return p;
    }

    static JSONObject defaultTrigger(String name) throws JSONException {
        String color = "Reverse".equals(name) ? "#ffffff" :
                ("Left Turn".equals(name) || "Right Turn".equals(name) || "Hazards".equals(name))
                        ? "#ffbf00" : "#ff0000";
        return new JSONObject()
                .put("name", name)
                .put("kind", "automotive")
                .put("pattern", "Solid")
                .put("color", color)
                .put("secondary", "#000000")
                .put("brightness", 85)
                .put("speed", 60)
                .put("intensity", 60)
                .put("zones", new JSONArray())
                .put("sectionAnimations", new JSONObject());
    }

    // Palette IDs are unsigned bytes in the shared v4 sequence schema.
    // Reject malformed imported values instead of displaying a clamped ID
    // while silently exporting a different, invalid value.
    static boolean validPaletteId(Object value) {
        if (!(value instanceof Number)) return false;
        double number = ((Number)value).doubleValue();
        return number >= 0 && number <= 255 && number == Math.floor(number);
    }

    private static void validateSequenceStep(JSONObject step) throws JSONException {
        if (step.optString("fx", "").isEmpty()
                || step.optInt("ms", 0) < 50 || step.optInt("ms", 0) > 600000)
            throw new JSONException("Invalid sequence step");
        JSONObject settings = step.optJSONObject("settings");
        if (step.has("settings") && settings == null)
            throw new JSONException("Invalid sequence settings");
        if (settings != null && settings.has("pal") && !validPaletteId(settings.opt("pal")))
            throw new JSONException("Invalid sequence palette ID");
    }

    private static JSONObject validated(JSONObject candidate) throws JSONException {
        if (candidate.optInt("version", 0) < 1 || candidate.optInt("version", 0) > 4)
            throw new JSONException("Unsupported project format (expected OpenTail v1-v4)");
        if (!(candidate.opt("layout") instanceof JSONArray)
                || !(candidate.opt("profiles") instanceof JSONObject))
            throw new JSONException("Missing zone layout or profiles");
        JSONArray layout = candidate.getJSONArray("layout");
        if (layout.length() > MAX_ZONES) throw new JSONException("Too many design zones");
        Set<String> ids = new HashSet<>();
        long lastEnd = 0;
        for (int i = 0; i < layout.length(); i++) {
            JSONObject z = layout.getJSONObject(i);
            String id = z.optString("id", "");
            String name = z.optString("name", "");
            int start = z.optInt("start", -1), length = z.optInt("length", -1);
            if (id.isEmpty() || !ids.add(id) || name.isEmpty() || name.length() > 48
                    || start < 0 || length < 1 || length > 10000
                    || ((long) start + length) > 1000000)
                throw new JSONException("Invalid or overlapping zone layout");
            if (start < lastEnd) throw new JSONException("Overlapping zone indexes");
            lastEnd = (long) start + length;
        }
        if (candidate.optString("projectName", "Untitled project").length() > 64)
            throw new JSONException("Project name is too long");
        if (candidate.optJSONArray("sequence") != null
                && candidate.getJSONArray("sequence").length() > MAX_SEQUENCE)
            throw new JSONException("Too many sequence steps");
        JSONArray steps = candidate.optJSONArray("sequence");
        if (steps != null) {
            for (int i = 0; i < steps.length(); i++) {
                validateSequenceStep(steps.getJSONObject(i));
            }
        }
        JSONObject options = candidate.optJSONObject("sequenceOptions");
        if (candidate.has("sequenceOptions") && options == null)
            throw new JSONException("Invalid sequence options");
        if (options != null && options.has("sharedPaletteId")
                && !validPaletteId(options.opt("sharedPaletteId")))
            throw new JSONException("Invalid shared palette ID");
        JSONArray presets = candidate.optJSONArray("savedPresets");
        if (presets != null && presets.length() > MAX_PRESETS)
            throw new JSONException("Too many presets");
        // Preserve other v4 fields without interpreting legacy device state.
        JSONObject copy = new JSONObject(candidate.toString());
        if (copy.optJSONArray("sequence") == null) copy.put("sequence", new JSONArray());
        if (copy.optJSONArray("savedPresets") == null) copy.put("savedPresets", new JSONArray());
        if (copy.optJSONObject("selectedPixels") == null) copy.put("selectedPixels", new JSONObject());
        if (copy.optJSONObject("sequenceOptions") == null)
            copy.put("sequenceOptions", new JSONObject());
        if (copy.optJSONArray("effectFavorites") == null)
            copy.put("effectFavorites", new JSONArray());
        JSONObject profiles = copy.getJSONObject("profiles");
        for (String name : MAIN_TRIGGERS)
            if (!profiles.has(name)) profiles.put(name, defaultTrigger(name));
        for (String name : new String[]{"Startup", "Shutdown"})
            if (!profiles.has(name)) profiles.put(name, defaultTrigger(name));
        copy.put("version", 4);
        return copy;
    }

    synchronized boolean importJson(String input) {
        try {
            if (input.length() > 2000000) return false;
            JSONObject candidate = validated(new JSONObject(input));
            JSONObject previous = data;
            data = candidate;
            if (save()) return true;
            data = previous; // a failed disk commit must not replace the current project
            return false;
        } catch (Exception ex) { return false; }
    }

    synchronized String exportJson() {
        try { return new JSONObject(data.toString()).put("version", 4).toString(2); }
        catch (JSONException e) { return data.toString(); }
    }

    synchronized boolean save() {
        String next = data.toString();
        boolean persisted;
        try { persisted = prefs.edit().putString(KEY, next).commit(); }
        catch (RuntimeException ex) { persisted = false; }
        if (persisted) {
            committedJson = next;
            return true;
        }
        // Pixel, layout, trigger and event editors mutate nested JSON before
        // calling save(). A failed commit must restore ALL those shared fields,
        // not leave a false in-memory success that leaks into exports/presets.
        restoreCommitted();
        return false;
    }

    private void restoreCommitted() {
        try { data = new JSONObject(committedJson); }
        catch (JSONException impossible) { throw new IllegalStateException("Corrupt committed project snapshot", impossible); }
    }

    synchronized String name() { return data.optString("projectName", "Untitled project"); }
    synchronized boolean name(String name) {
        if (name == null) return false;
        String n = name.trim();
        if (n.isEmpty()) n = "Untitled project";
        try { data.put("projectName", n.substring(0, Math.min(64, n.length()))); return save(); }
        catch (JSONException ignored) { restoreCommitted(); return false; }
    }

    /** v4 Studio-compatible per-zone zero-based colors, local-only (never BLE). */
    synchronized String pixelColor(String id,int pixel) {
        int ix=indexOfZone(id);
        if(ix<0||pixel<0||pixel>=zone(ix).optInt("length",0))return "#000000";
        JSONObject all=data.optJSONObject("selectedPixels");
        JSONObject entries=all==null?null:all.optJSONObject(id);
        String hex=entries==null?"#000000":entries.optString(Integer.toString(pixel),"#000000");
        return hex.matches("#[0-9A-Fa-f]{6}")?hex:"#000000";
    }
    synchronized boolean setPixelColor(String id,int pixel,String hex) {
        int ix=indexOfZone(id);
        if(ix<0||pixel<0||pixel>=zone(ix).optInt("length",0)||
           hex==null||!hex.matches("#[0-9A-Fa-f]{6}"))return false;
        try {
            JSONObject all=data.optJSONObject("selectedPixels");
            if(all==null){all=new JSONObject();data.put("selectedPixels",all);}
            JSONObject entries=all.optJSONObject(id);
            if(entries==null){entries=new JSONObject();all.put(id,entries);}
            entries.put(Integer.toString(pixel),hex);
            return save();
        }catch(JSONException e){restoreCommitted();return false;}
    }
    synchronized boolean clearPixelColor(String id,int pixel) {
        int zoneIndex=indexOfZone(id);
        if(zoneIndex<0 || pixel<0 || pixel>=zone(zoneIndex).optInt("length",0)) return false;
        JSONObject all=data.optJSONObject("selectedPixels");
        JSONObject entries=all==null?null:all.optJSONObject(id);
        if(entries==null)return true;
        entries.remove(Integer.toString(pixel));
        return save();
    }
    synchronized JSONArray zones() { return data.optJSONArray("layout"); }
    synchronized int zoneCount() { return zones().length(); }
    synchronized int pixelCount() {
        int count = 0;
        for (int i = 0; i < zoneCount(); i++)
            count += zones().optJSONObject(i).optInt("length", 0);
        return count;
    }

    synchronized JSONObject zone(int index) { return zones().optJSONObject(index); }
    synchronized int indexOfZone(String id) {
        for (int i = 0; i < zoneCount(); i++)
            if (id.equals(zone(i).optString("id"))) return i;
        return -1;
    }

    private void reindex() throws JSONException {
        long end = 0;
        for (int i = 0; i < zoneCount(); i++) {
            JSONObject z = zone(i);
            int length = z.getInt("length");
            boolean manual = "manual".equals(z.optString("offsetMode"));
            long start = manual ? Math.max(end, z.optLong("start", end)) : end;
            if (start + length > 1000000) throw new JSONException("Pixel index ceiling exceeded");
            z.put("start", start);
            end = start + length;
        }
    }

    synchronized boolean addZone(String label, int length) {
        return addZoneAt(label, length, zoneCount());
    }

    /** Insert and reindex once. An "Add next" edit must not commit an
     * appended zone and then fail during a second reorder transaction. */
    synchronized boolean addZoneAt(String label, int length, int insertAt) {
        if (zoneCount() >= MAX_ZONES || length < 1 || length > 10000
                || insertAt < 0 || insertAt > zoneCount() || label == null
                || label.length() > 48) return false;
        try {
            JSONObject z = new JSONObject()
                    .put("id", "zone_" + UUID.randomUUID().toString().replace("-", ""))
                    .put("name", label.isEmpty() ? "Zone " + (zoneCount()+1) : label)
                    .put("start", 0).put("length", length)
                    .put("offsetMode", "auto").put("reverse", false)
                    .put("functions", new JSONArray()).put("controllerId", "default");
            JSONArray layout = new JSONArray();
            for (int i = 0; i <= zoneCount(); i++) {
                if (i == insertAt) layout.put(z);
                if (i < zoneCount()) layout.put(zone(i));
            }
            data.put("layout", layout);
            reindex();
            return save();
        } catch (JSONException e) { restoreCommitted(); return false; }
    }

    /** Preview every dependent zero-based zone offset without mutating saved
     * layouts. Keep identical manual-gap rules to reindex(); neither slider
     * dragging nor typing can send BLE commands or persist a draft. */
    synchronized String previewZoneOffsets(String id, int suggestedLength) {
        int selected=indexOfZone(id);
        if(selected<0 || suggestedLength<1 || suggestedLength>10000)
            return "Pixel count must be between 1 and 10,000";
        long end=0;
        StringBuilder summary=new StringBuilder();
        for(int i=0;i<zoneCount();i++){
            JSONObject zone=zone(i);
            if(zone==null)continue;
            int len=i==selected?suggestedLength:zone.optInt("length",0);
            boolean manual="manual".equals(zone.optString("offsetMode"));
            long start=manual?Math.max(end,zone.optLong("start",end)):end;
            if(start+len>1000000)return "Pixel index ceiling exceeded";
            if(i>=selected&&i<selected+5){
                if(summary.length()>0)summary.append("\n");
                summary.append(zone.optString("name","Zone "+(i+1)))
                       .append(": ").append(start).append("–")
                       .append(start+len-1).append(" · next ").append(start+len);
            }
            end=start+len;
        }
        if(zoneCount()>selected+5)summary.append("\n…additional zones reindex on Save");
        return summary.toString();
    }

    synchronized boolean editZone(String id, String label, int length, boolean reverse) {
        int index = indexOfZone(id);
        if (index < 0 || length < 1 || length > 10000 || label.trim().isEmpty()
                || label.trim().length() > 48) return false;
        try {
            JSONObject z = zone(index);
            z.put("name", label.trim()).put("length", length).put("reverse", reverse);
            JSONObject colors=data.optJSONObject("selectedPixels");
            JSONObject painted=colors==null?null:colors.optJSONObject(id);
            if(painted!=null){
                JSONArray keys=painted.names();
                if(keys!=null)for(int i=0;i<keys.length();i++){
                    String key=keys.optString(i);
                    try {if(Integer.parseInt(key)>=length)painted.remove(key);}
                    catch(NumberFormatException ex){painted.remove(key);}
                }
            }
            reindex();return save();
        } catch (JSONException e) { restoreCommitted(); return false; }
    }

    synchronized boolean removeZone(String id) {
        int index = indexOfZone(id);
        if (index < 0) return false;
        JSONArray remaining = new JSONArray();
        for (int i = 0; i < zoneCount(); i++)
            if (i != index) remaining.put(zone(i));
        try {
            data.put("layout", remaining);
            JSONObject all=data.optJSONObject("selectedPixels");
            if(all!=null)all.remove(id);
            // Only remove assignments for deleted zone; preserve all other triggers.
            JSONObject profiles = data.getJSONObject("profiles");
            JSONArray names = profiles.names();
            if (names != null) for (int i = 0; i < names.length(); i++) {
                JSONObject p = profiles.optJSONObject(names.getString(i));
                if (p == null) continue;
                JSONArray assigned = p.optJSONArray("zones"), filtered = new JSONArray();
                if (assigned != null) for (int j = 0; j < assigned.length(); j++)
                    if (!id.equals(assigned.optString(j))) filtered.put(assigned.optString(j));
                p.put("zones", filtered);
                JSONObject sections = p.optJSONObject("sectionAnimations");
                if (sections != null) sections.remove(id);
            }
            reindex(); return save();
        } catch (JSONException e) { restoreCommitted(); return false; }
    }

    synchronized boolean moveZone(int from, int to) {
        if (from < 0 || to < 0 || from >= zoneCount() || to >= zoneCount()) return false;
        java.util.ArrayList<JSONObject> reordered = new java.util.ArrayList<>();
        for (int k = 0; k < zoneCount(); k++) reordered.add(zone(k));
        JSONObject selected = reordered.remove(from);
        reordered.add(to, selected);
        JSONArray changed = new JSONArray();
        for (JSONObject zone : reordered) changed.put(zone);
        try { data.put("layout", changed); reindex();return save(); }
        catch (JSONException e) { restoreCommitted(); return false; }
    }

    synchronized JSONObject profiles() { return data.optJSONObject("profiles"); }
    synchronized JSONObject trigger(String name) {
        JSONObject p = profiles().optJSONObject(name);
        if (p != null) return p;
        try { p = defaultTrigger(name); profiles().put(name, p);save(); }
        catch (JSONException ignored) { }
        return p;
    }

    synchronized boolean customTrigger(String name) {
        String n=name.trim();
        if (!n.matches("[a-zA-Z0-9 _-]{1,32}") || profiles().has(n)) return false;
        try { profiles().put(n, defaultTrigger(n).put("kind", "trigger-custom")); return save(); }
        catch (JSONException e) { restoreCommitted(); return false; }
    }

    synchronized boolean assign(String trigger, String zoneId, boolean enabled) {
        return assignMany(java.util.Collections.singleton(trigger), zoneId, enabled);
    }

    /** One durable transaction for a multi-selected trigger assignment. */
    synchronized boolean assignMany(Set<String> triggers, String zoneId, boolean enabled) {
        if (indexOfZone(zoneId) < 0 || triggers == null || triggers.isEmpty()) return false;
        for (String name : triggers)
            if (name == null || profiles().optJSONObject(name) == null) return false;
        try {
            JSONObject draft = new JSONObject(data.toString());
            JSONObject all = draft.getJSONObject("profiles");
            for (String name : triggers) {
                JSONObject p = all.getJSONObject(name);
                JSONArray old = p.optJSONArray("zones"), next = new JSONArray();
                if (old != null) for (int i = 0; i < old.length(); i++)
                    if (!zoneId.equals(old.getString(i))) next.put(old.getString(i));
                if (enabled) next.put(zoneId);
                p.put("zones", next);
            }
            data = draft;
            return save();
        } catch (JSONException e) { return false; }
    }

    /** Multi-trigger style updates are one validated local transaction.
     * CORE arbitration is unaffected; this only persists the v4 designer. */
    synchronized boolean styleTriggers(Set<String> triggers, String effect, String color) {
        if (triggers == null || triggers.isEmpty() || effect == null || effect.isEmpty()
                || color == null || !color.matches("#[0-9A-Fa-f]{6}")) return false;
        for (String name : triggers)
            if (name == null || profiles().optJSONObject(name) == null) return false;
        try {
            JSONObject draft = new JSONObject(data.toString());
            JSONObject all = draft.getJSONObject("profiles");
            for (String name : triggers)
                all.getJSONObject(name).put("pattern", effect).put("color", color);
            data = draft;
            return save();
        } catch (JSONException e) { return false; }
    }

    /** Startup/shutdown authoring only; physical ignition is CORE-owned. */
    synchronized boolean setAutoEvent(String event, String key, Object value) {
        if (!("Startup".equals(event) || "Shutdown".equals(event))) return false;
        switch (key) {
            case "eventUiEnabled":
                if (!(value instanceof Boolean)) return false;
                break;
            case "eventUiFadeMs":
                if (!(value instanceof Integer) || (Integer)value < 0 || (Integer)value > 5000)
                    return false;
                break;
            case "eventDuration":
                if (!(value instanceof Number)) return false;
                double seconds = ((Number)value).doubleValue();
                if (!Double.isFinite(seconds) || seconds < 0.2 || seconds > 30.0) return false;
                break;
            case "eventFx":
                if (!(value instanceof String) || ((String)value).isEmpty()
                        || ((String)value).length() > 64) return false;
                break;
            default:
                return false;
        }
        try {
            JSONObject draft = new JSONObject(data.toString());
            draft.getJSONObject("profiles").getJSONObject(event).put(key, value);
            data = draft;
            return save();
        } catch (JSONException ex) { return false; }
    }

    synchronized boolean deleteTrigger(String name) {
        JSONObject p = profiles().optJSONObject(name);
        if (p == null || !"trigger-custom".equals(p.optString("kind"))) return false;
        profiles().remove(name);return save();
    }

    synchronized JSONArray sequence() { return data.optJSONArray("sequence"); }
    synchronized JSONObject sequenceOptions() { return data.optJSONObject("sequenceOptions"); }
    synchronized boolean setSequence(JSONArray steps) {
        if (steps.length() > MAX_SEQUENCE) return false;
        JSONArray previous = data.optJSONArray("sequence");
        try {
            for (int i = 0; i < steps.length(); i++)
                validateSequenceStep(steps.getJSONObject(i));
            data.put("sequence", new JSONArray(steps.toString()));
            if (save()) return true;
        } catch (JSONException ignored) { }
        // Failed writes must not leave an unsaved sequence in the live model.
        try { data.put("sequence", previous == null ? new JSONArray() : previous); }
        catch (JSONException ignored) { }
        return false;
    }
    synchronized boolean updateSequenceOption(String key, Object value) {
        boolean palette = "sharedPaletteId".equals(key);
        if (palette) {
            if (!(value instanceof Integer) || (Integer)value < 0 || (Integer)value > 255)
                return false;
        } else if (!("shuffleLoop".equals(key) || "repeat".equals(key) ||
                     "favoritesOnly".equals(key) || "sharedPaletteEnabled".equals(key)) ||
                   !(value instanceof Boolean)) return false;
        JSONObject previous = data.optJSONObject("sequenceOptions");
        try {
            JSONObject next = previous == null ? new JSONObject() : new JSONObject(previous.toString());
            next.put(key, value);
            data.put("sequenceOptions", next);
            if (save()) return true;
        } catch (JSONException ignored) { }
        try { data.put("sequenceOptions", previous == null ? new JSONObject() : previous); }
        catch (JSONException ignored) { }
        return false;
    }
    synchronized JSONArray presets() { return data.optJSONArray("savedPresets"); }
    synchronized boolean setPresets(JSONArray presets) {
        if (presets.length() > MAX_PRESETS) return false;
        JSONArray previous = data.optJSONArray("savedPresets");
        try {
            data.put("savedPresets", new JSONArray(presets.toString()));
            if (save()) return true;
            // A failed storage commit must not leave unsaved presets in RAM.
            data.put("savedPresets", previous == null ? new JSONArray() : previous);
        } catch (JSONException e) {
            try { data.put("savedPresets", previous == null ? new JSONArray() : previous); }
            catch (JSONException ignored) { }
        }
        return false;
    }
    synchronized JSONObject raw() {return data;}
}
