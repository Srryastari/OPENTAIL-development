package com.opentail.android;

import org.junit.Test;
import static org.junit.Assert.*;

/** Golden admission boundaries from pinned firmware ot_palette_1d_supported(). */
public class BleFxPalettePolicyTest {
    private static final int[] PALETTE_EFFECTS = {
        38,65,66,74,80,81,87,90,101,107,112,115,179,185,218,219
    };
    @Test public void everyAdvertisedWledPaletteModeAcceptsFirmwareIdRange() {
        for (int effect : PALETTE_EFFECTS)
            for (int palette : new int[]{0,1,2,4,72,73,127,128,200})
                assertTrue("FX " + effect + ", palette " + palette,
                    BleFxPalettePolicy.isWireCandidate(effect,palette));
    }
    @Test public void originalEmbersRetainsItsNarrowPaletteContract() {
        for (int palette : new int[]{0,2,3,4})
            assertTrue(BleFxPalettePolicy.isWireCandidate(32768,palette));
        for (int palette : new int[]{1,5,72,200})
            assertFalse(BleFxPalettePolicy.isWireCandidate(32768,palette));
    }
    @Test public void unsupportedEffectPaletteCombinationsFailClosed() {
        for (int effect : new int[]{0,1,9,27,67,32769,32787})
            for (int palette : new int[]{1,2,73,200})
                assertFalse("FX " + effect + ", palette " + palette,
                    BleFxPalettePolicy.isWireCandidate(effect,palette));
    }
    @Test public void invalidWireIdRangesFailClosed() {
        for (int palette : new int[]{-1,201,254,255})
            assertFalse(BleFxPalettePolicy.isWireCandidate(65,palette));
        assertFalse(BleFxPalettePolicy.isWireCandidate(-1,0));
        assertFalse(BleFxPalettePolicy.isWireCandidate(65536,0));
    }
}
