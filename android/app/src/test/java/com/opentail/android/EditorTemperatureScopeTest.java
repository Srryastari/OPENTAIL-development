package com.opentail.android;

import java.util.HashMap;
import java.util.Map;
import org.junit.Test;
import static org.junit.Assert.*;

/** Real preference-key contract shared by the four approved WLED+ screens. */
public class EditorTemperatureScopeTest {
    @Test public void preservesExistingShowModesSetting() {
        assertEquals("wled.32775.temp2",
            EditorTemperatureScope.preferenceKey("show-modes",32775,2));
    }

    @Test public void crossContextTemperatureChangesCannotLeak() {
        String[] screens={"show-modes","triggers:Brake",
            "auto-event:Startup","auto-event:Shutdown"};
        Map<String,Integer> persisted=new HashMap<>();
        for(int i=0;i<screens.length;i++) {
            String key=EditorTemperatureScope.preferenceKey(screens[i],9,0);
            assertFalse("Context key collision: "+screens[i], persisted.containsKey(key));
            persisted.put(key,21+i*15);
        }
        for(int i=0;i<screens.length;i++)
            assertEquals(Integer.valueOf(21+i*15),persisted.get(
                EditorTemperatureScope.preferenceKey(screens[i],9,0)));
    }

    @Test public void roleEffectAndTriggerGroupAreIndependent() {
        String a=EditorTemperatureScope.preferenceKey("triggers:Brake",9,0);
        assertNotEquals(a,EditorTemperatureScope.preferenceKey("triggers:Brake",9,1));
        assertNotEquals(a,EditorTemperatureScope.preferenceKey("triggers:Brake",10,0));
        assertNotEquals(a,EditorTemperatureScope.preferenceKey("triggers:Brake|Turn",9,0));
        assertNotEquals(a,EditorTemperatureScope.preferenceKey("triggers:Turn|Brake",9,0));
    }

    @Test public void handlesDifferentScopeNamesWithoutAmbiguousKeys() {
        assertNotEquals(
            EditorTemperatureScope.preferenceKey("auto-event:Startup",9,1),
            EditorTemperatureScope.preferenceKey("auto-event:Shutdown",9,1));
        assertNotEquals(
            EditorTemperatureScope.preferenceKey("triggers:a.wled.1",99,0),
            EditorTemperatureScope.preferenceKey("triggers:a",1,0));
    }
}
