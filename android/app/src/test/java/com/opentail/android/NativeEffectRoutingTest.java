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
    @Test public void startupAndShutdownHaveDistinctDedicatedChoices() {
        int[] startup = {32769,32770,32771};
        int[] shutdown = {32773,32787};
        for (int id : startup) {
            assertTrue(has(NativeEffectCatalog.autoEventChoices("Startup"), id));
            assertFalse(has(NativeEffectCatalog.autoEventChoices("Shutdown"), id));
            assertFalse(has(NativeEffectCatalog.generalEffects(), id));
        }
        for (int id : shutdown) {
            assertTrue(has(NativeEffectCatalog.autoEventChoices("Shutdown"), id));
            assertFalse(has(NativeEffectCatalog.autoEventChoices("Startup"), id));
            assertFalse(has(NativeEffectCatalog.generalEffects(), id));
        }
        // Standard WLED effects and Strobe remain eligible in BOTH contexts.
        assertTrue(has(NativeEffectCatalog.autoEventChoices("Startup"), 23));
        assertTrue(has(NativeEffectCatalog.autoEventChoices("Shutdown"), 23));
        assertTrue(has(NativeEffectCatalog.autoEventChoices("Startup"), 0));
        assertTrue(has(NativeEffectCatalog.autoEventChoices("Shutdown"), 0));
        assertTrue(NativeEffectCatalog.autoEventChoices("Unrecognized").isEmpty());
    }
    @Test public void noProprietaryShowModeEffectLeaksIntoEitherAutoEvent() {
        for (int id=32768;id<=32787;id++) {
            if (!NativeEffectCatalog.autoEventOnly(id)) {
                assertFalse(has(NativeEffectCatalog.autoEventChoices("Startup"),id));
                assertFalse(has(NativeEffectCatalog.autoEventChoices("Shutdown"),id));
                assertTrue(has(NativeEffectCatalog.generalEffects(),id));
            }
        }
    }
    @Test public void exactNamesFailClosedForAutoEventPersistence() {
        assertTrue(NativeEffectCatalog.isAllowedForAutoEvent("Startup","Materialize Sparks"));
        assertFalse(NativeEffectCatalog.isAllowedForAutoEvent("Shutdown","Materialize Sparks"));
        assertTrue(NativeEffectCatalog.isAllowedForAutoEvent("Shutdown","Singularity Shutdown"));
        assertFalse(NativeEffectCatalog.isAllowedForAutoEvent("Startup","Singularity Shutdown"));
        assertFalse(NativeEffectCatalog.isAllowedForAutoEvent("Startup","Crystal Growth"));
        assertFalse(NativeEffectCatalog.isAllowedForAutoEvent("Shutdown","Embers"));
        assertFalse(NativeEffectCatalog.isAllowedForAutoEvent("Startup","Unknown Effect"));
        assertFalse(NativeEffectCatalog.isAllowedForAutoEvent("Settings","Solid"));
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
}
