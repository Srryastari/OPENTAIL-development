package com.opentail.android

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

/** BLE palette gates match CORE's current ot_palette_1d_supported contract. */
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

    @Test fun otherEffectsNeverClaimEmbersPaletteSupport() {
        for (effect in listOf(0, 3, 9, 32769, 32787)) {
            for (palette in listOf(1, 2, 3, 4, 5, 255)) {
                assertFalse(NativePaletteEngine.coreSupports(effect, palette))
                assertEquals(0, NativePaletteEngine.corePreviewPalette(effect, palette))
            }
        }
    }
}
