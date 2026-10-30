package com.opentail.android;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import org.junit.Test;

/** Shared v4 palette fields are unsigned integers, not best-effort strings. */
public class NativePaletteValidationTest {
    @Test public void acceptsOnlyIntegralPaletteIdsBetweenZeroAnd255() {
        assertTrue(NativeProject.validPaletteId(0));
        assertTrue(NativeProject.validPaletteId(1));
        assertTrue(NativeProject.validPaletteId(254));
        assertTrue(NativeProject.validPaletteId(255L));
        assertTrue(NativeProject.validPaletteId(128.0));

        assertFalse(NativeProject.validPaletteId(-1));
        assertFalse(NativeProject.validPaletteId(256));
        assertFalse(NativeProject.validPaletteId(42.5));
        assertFalse(NativeProject.validPaletteId("42"));
        assertFalse(NativeProject.validPaletteId(true));
        assertFalse(NativeProject.validPaletteId(null));
        assertFalse(NativeProject.validPaletteId(Double.NaN));
        assertFalse(NativeProject.validPaletteId(Double.POSITIVE_INFINITY));
    }
}
