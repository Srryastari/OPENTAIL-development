package com.opentail.android;

import java.util.List;
import org.junit.Test;
import static org.junit.Assert.*;

/** Preserve owner-approved Show Modes originals vs dedicated Auto Events. */
public class NativeEffectRoutingTest {
    private static boolean has(List<NativeEffectCatalog.Effect> effects,int id) {
        for (NativeEffectCatalog.Effect effect : effects) if (effect.id == id) return true;
        return false;
    }
    @Test public void generalOriginalsStayInUnifiedEffectsNotAutoEvents() {
        int[] showOnly = {
            97,32768,32772,32774,32775,32776,32777,32778,32779,
            32780,32781,32782,32783,32784,32785,32786
        };
        for(int id : showOnly) {
            assertTrue("Missing Show Modes effect "+id,
                has(NativeEffectCatalog.generalEffects(),id));
            assertFalse("Show Modes original leaked into Auto Events "+id,
                has(NativeEffectCatalog.autoEventChoices(),id));
        }
    }
    @Test public void dedicatedTransitionsStayInAutoEventsOnly() {
        int[] transitions={32769,32770,32771,32773,32787};
        for(int id : transitions) {
            assertTrue(has(NativeEffectCatalog.autoEventChoices(),id));
            assertFalse(has(NativeEffectCatalog.generalEffects(),id));
        }
        assertTrue(has(NativeEffectCatalog.autoEventChoices(),23));
        assertTrue(has(NativeEffectCatalog.generalEffects(),23));
    }
    @Test public void startupShutdownTransitionsNeverCrossContexts() {
        int[] startup={32769,32770,32771};
        int[] shutdown={32773,32787};
        List<NativeEffectCatalog.Effect> entry=NativeEffectCatalog.autoEventChoices("Startup");
        List<NativeEffectCatalog.Effect> exit=NativeEffectCatalog.autoEventChoices("Shutdown");
        assertEquals("Startup standard effects plus 3 transitions",122,entry.size());
        assertEquals("Shutdown standard effects plus 2 transitions",121,exit.size());
        for(int id:startup) {
            assertTrue("Missing Startup "+id,has(entry,id));
            assertFalse("Startup animation incorrectly in Shutdown "+id,has(exit,id));
        }
        for(int id:shutdown) {
            assertTrue("Missing Shutdown "+id,has(exit,id));
            assertFalse("Shutdown animation incorrectly in Startup "+id,has(entry,id));
        }
        for(int id:new int[]{0,9,23,65}) {
            assertTrue(has(entry,id));assertTrue(has(exit,id));
        }
        assertFalse(has(entry,32777)); assertFalse(has(exit,32777));
        assertFalse(has(entry,97)); assertFalse(has(exit,97));
        assertEquals(124,NativeEffectCatalog.autoEventChoices().size());
    }
    @Test public void allProprietaryEffectIdsRemainRegisteredAndSearchable() {
        for(int id=32768;id<=32787;id++) {
            NativeEffectCatalog.Effect found=NativeEffectCatalog.byId(id);
            assertEquals("Unregistered original "+id,id,found.id);
            assertFalse(found.name.trim().isEmpty());
            assertEquals(id,NativeEffectCatalog.byName(found.name).id);
            if(!NativeEffectCatalog.autoEventOnly(id))
                assertTrue("Proprietary FX invisible in Show Modes: "+id,
                    has(NativeEffectCatalog.generalEffects(),id));
        }
        assertEquals("Crystal Growth",NativeEffectCatalog.byId(32777).name);
        assertTrue(has(NativeEffectCatalog.generalEffects(),32777));
        try {
            NativeEffectCatalog.autoEventChoices("NotARealEvent");
            fail("Unknown event must not reuse a wrong effect list");
        } catch (IllegalArgumentException expected) {}
    }
}
