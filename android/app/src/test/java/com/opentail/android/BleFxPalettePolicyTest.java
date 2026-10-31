package com.opentail.android;

import org.junit.Test;
import static org.junit.Assert.*;

/** CORE BLE FX v1 admits palette IDs up to 200 on supported WLED effects. */
public class BleFxPalettePolicyTest {
    @Test public void paletteCapableSourceEffectsAreNotTruncatedToZero() {
        for (int palette : new int[]{0,1,2,4,72,73,127,128,200}) {
            assertTrue("WLED FX palette ID " + palette,
                BleFxPalettePolicy.isWireCandidate(65,palette));
        }
    }

    @Test public void originalEmbersRetainsItsNarrowPaletteContract() {
        for (int palette : new int[]{0,2,3,4})
            assertTrue(BleFxPalettePolicy.isWireCandidate(32768,palette));
        for (int palette : new int[]{1,5,72,200})
            assertFalse(BleFxPalettePolicy.isWireCandidate(32768,palette));
    }

    @Test public void rejectsOutOfRangeValuesButLeavesCapabilityDecisionToCore() {
        for (int palette : new int[]{-1,201,254,255})
            assertFalse(BleFxPalettePolicy.isWireCandidate(65,palette));
        assertFalse(BleFxPalettePolicy.isWireCandidate(-1,0));
        assertFalse(BleFxPalettePolicy.isWireCandidate(65536,0));
        // The authoritative CORE metadata decides if original effect 32769
        // supports a palette; Android should not invent a false restriction.
        assertTrue(BleFxPalettePolicy.isWireCandidate(32769,72));
    }
}
