package com.opentail.android;

import android.content.SharedPreferences;
import org.json.JSONObject;
import org.json.JSONArray;
import org.junit.Test;
import java.lang.reflect.Proxy;
import static org.junit.Assert.*;

/** Pure Layout -> CORE Panel/Zone identity mapping tests. */
public class PanelGeometryMapperTest {
    private static final class Storage {
        String persisted,pending;
        final SharedPreferences prefs;
        Storage() {
            SharedPreferences.Editor editor=(SharedPreferences.Editor)Proxy.newProxyInstance(
                SharedPreferences.Editor.class.getClassLoader(),
                new Class<?>[]{SharedPreferences.Editor.class},(proxy,method,args)->{
                    switch(method.getName()) {
                        case "putString": pending=(String)args[1];return proxy;
                        case "commit": persisted=pending;return true;
                        case "apply": persisted=pending;return null;
                        case "clear": pending=null;return proxy;
                        case "toString": return "FakeEditor";
                        default:
                            if(SharedPreferences.Editor.class.isAssignableFrom(method.getReturnType()))return proxy;
                            return false;
                    }
                });
            prefs=(SharedPreferences)Proxy.newProxyInstance(
                SharedPreferences.class.getClassLoader(),new Class<?>[]{SharedPreferences.class},
                (proxy,method,args)->{
                    switch(method.getName()) {
                        case "getString": return persisted==null?args[1]:persisted;
                        case "edit": return editor;
                        case "contains": return persisted!=null;
                        case "toString": return "FakePrefs";
                        default: return method.getReturnType().isPrimitive()?false:null;
                    }
                });
        }
    }

    private static NativeProject zone(NativeProject p,String name,int pixels,
                                      int lane,int section,int panel,String panelName) throws Exception {
        assertTrue(p.addZone(name,pixels));
        String id=p.zone(p.zoneCount()-1).optString("id");
        assertTrue(p.mapZoneToOutputSlot(id,lane,section));
        JSONObject document=new JSONObject(p.exportJson());
        JSONObject z=document.getJSONArray("layout").getJSONObject(p.zoneCount()-1);
        z.put("physicalPanelId",panel).put("physicalPanelName",panelName);
        assertTrue("valid explicit panel metadata",p.importJson(document.toString()));
        return p;
    }

    private static NativeProject gs400Layout() throws Exception {
        NativeProject p=new NativeProject(new Storage().prefs);
        // Four logical sections share each housing Panel; trunk is its own Panel.
        zone(p,"Left Top",118,0,0,1,"Left Housing");
        zone(p,"Left Amber",109,0,1,1,"Left Housing");
        zone(p,"Left Reverse",42,0,2,1,"Left Housing");
        zone(p,"Left Bottom",253,0,3,1,"Left Housing");
        zone(p,"Left Trunk",101,0,4,2,"Left Trunk");
        zone(p,"Right Top",118,1,0,3,"Right Housing");
        zone(p,"Right Amber",109,1,1,3,"Right Housing");
        zone(p,"Right Reverse",42,1,2,3,"Right Housing");
        zone(p,"Right Bottom",253,1,3,3,"Right Housing");
        zone(p,"Right Trunk",101,1,4,4,"Right Trunk");
        return p;
    }

    @Test public void gs400LayoutMapsSectionsToHousingAndSeparateTrunkPanels() throws Exception {
        GeometryTransactionManager.LayoutMapping m=
            GeometryTransactionManager.mapLayout(gs400Layout());
        GeometryTransactionManager.MappedLane left=m.lane(0),right=m.lane(1);
        assertEquals(623,left.activePixels);assertEquals(623,right.activePixels);
        assertEquals(2,left.panels.size());assertEquals(2,right.panels.size());
        assertEquals(5,left.zones.size());assertEquals(5,right.zones.size());
        assertEquals("Left Housing",left.panels.get(0).name);
        assertEquals(4,left.panels.get(0).zoneCount);
        assertEquals("Left Trunk",left.panels.get(1).name);
        assertEquals(1,left.panels.get(1).zoneCount);
        assertEquals(0,left.zones.get(0).start);
        assertEquals(118,left.zones.get(1).start);
        assertEquals(227,left.zones.get(2).start);
        assertEquals(269,left.zones.get(3).start);
        assertEquals(522,left.zones.get(4).start);
        assertEquals(101,left.zones.get(4).count);
        assertEquals(129,left.zones.get(0).id);
        assertEquals(137,right.zones.get(0).id);
        assertEquals(1,left.zones.get(0).panelId);
        assertEquals(2,left.zones.get(4).panelId);
        assertEquals(3,right.zones.get(0).panelId);
        assertEquals(4,right.zones.get(4).panelId);
        assertEquals(0x0f,left.zones.get(0).roleFlags);
        PanelPreviewProtocol.UpdateDescriptor[] descriptors=left.descriptors();
        assertEquals(5,descriptors.length);
        assertEquals(129,descriptors[0].zoneId);
        assertEquals(0,descriptors[0].rangeSlot);
        assertEquals(118,descriptors[0].count);
    }

    @Test public void physicalSectionCannotBePromotedWithoutExplicitPanelIdentity() {
        NativeProject p=new NativeProject(new Storage().prefs);
        assertTrue(p.addZone("Unassigned panel",10));
        String id=p.zone(0).optString("id");
        assertTrue(p.mapZoneToOutputSlot(id,0,0));
        try { GeometryTransactionManager.mapLayout(p);fail("Panel identity must not be guessed"); }
        catch(IllegalStateException expected) {
            assertTrue(expected.getMessage().contains("explicit CORE Panel identity"));
        }
    }

    @Test public void panelIdsAreControllerWideAndCannotBeReusedAcrossOutputs() throws Exception {
        NativeProject p=new NativeProject(new Storage().prefs);
        zone(p,"Left",10,0,0,7,"Left Housing");
        zone(p,"Right",10,1,0,7,"Right Housing");
        try { GeometryTransactionManager.mapLayout(p);fail("Panel IDs are controller-wide"); }
        catch(IllegalArgumentException expected) {
            assertTrue(expected.getMessage().contains("controller-wide"));
        }
    }

    @Test public void rejectsPixelCapacityAndPanelOrZoneCountOverflow() throws Exception {
        NativeProject p=new NativeProject(new Storage().prefs);
        zone(p,"Too long",2001,0,0,1,"Housing");
        try { GeometryTransactionManager.mapLayout(p);fail("pixel capacity"); }
        catch(IllegalArgumentException expected) {
            assertTrue(expected.getMessage().contains("2,000-pixel"));
        }

        NativeProject tooMany=new NativeProject(new Storage().prefs);
        for(int i=0;i<5;i++)zone(tooMany,"Panel "+i,1,0,i,i+1,"Panel "+i);
        try { GeometryTransactionManager.mapLayout(tooMany);fail("panel capacity"); }
        catch(IllegalArgumentException expected) {
            assertTrue(expected.getMessage().contains("four-Panel"));
        }
    }

    @Test public void rejectsConflictingPanelNamesAndPanelIdsOutsideWireRange() throws Exception {
        NativeProject p=new NativeProject(new Storage().prefs);
        zone(p,"Top",10,0,0,1,"Housing");
        zone(p,"Bottom",10,0,1,1,"Trunk");
        try { GeometryTransactionManager.mapLayout(p);fail("conflicting panel names"); }
        catch(IllegalArgumentException expected) {
            assertTrue(expected.getMessage().contains("conflicting names"));
        }

        NativeProject invalid=new NativeProject(new Storage().prefs);
        zone(invalid,"Invalid",10,0,0,128,"Housing");
        try { GeometryTransactionManager.mapLayout(invalid);fail("panel ID wire range"); }
        catch(IllegalArgumentException expected) {
            assertTrue(expected.getMessage().contains("Panel identity"));
        }
    }
}
