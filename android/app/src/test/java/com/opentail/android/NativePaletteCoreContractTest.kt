package com.opentail.android

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

/** Confirmed by pinned CORE #49 palette admission and FX supports_palette metadata. */
class NativePaletteCoreContractTest {
    @Test fun defaultPaletteRemainsValidForEveryEffect() {
        for (effect in listOf(0, 3, 9, 32768, 32787)) {
            assertTrue(NativePaletteEngine.coreSupports(effect, 0))
            assertEquals(0, NativePaletteEngine.corePreviewPalette(effect, 0))
        }
    }

    @Test fun embersExposesOnlyImplementedColorPaletteIds() {
        for (palette in listOf(2, 3, 4)) {
            assertTrue(NativePaletteEngine.coreSupports(32768, palette))
            assertEquals(palette, NativePaletteEngine.corePreviewPalette(32768, palette))
        }
        for (palette in listOf(-1, 1, 5, 128, 255, 256)) {
            assertFalse(NativePaletteEngine.coreSupports(32768, palette))
            assertEquals(0, NativePaletteEngine.corePreviewPalette(32768, palette))
        }
    }

    @Test fun sourcePaletteEffectIdsMirrorPinnedCoreMetadata() {
        val supported = listOf(38,65,66,74,80,81,87,90,101,107,112,115,179,185,218,219)
        for (effect in supported) {
            for (palette in listOf(1,2,4,72,73,128,200)) {
                assertTrue(NativePaletteEngine.coreSupports(effect, palette))
                assertEquals(palette, NativePaletteEngine.corePreviewPalette(effect, palette))
            }
        }
    }

    @Test fun nonPaletteEffectsAreNotAdvertised() {
        for (effect in listOf(0,3,9,27,67,32769,32787))
            for (palette in listOf(1,2,4,72,128,200)) {
                assertFalse(NativePaletteEngine.coreSupports(effect, palette))
                assertEquals(0, NativePaletteEngine.corePreviewPalette(effect, palette))
            }
    }
}
