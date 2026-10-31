package com.opentail.android

import org.junit.Assert.assertEquals
import org.junit.Test

/** Phone regression: a saved Rainbow palette must not blank a later effect.
 * These tests do not mutate the locked NativePaletteEngine or preferences.
 */
class OfflinePreviewPalettePolicyTest {
    @Test fun nonPaletteEffectsIgnoreStaleSelectedPaletteForRendering() {
        for (id in listOf(1, 26, 91, 32769, 32770, 32771, 32772, 32773,
            32774, 32775, 32776, 32777, 32778, 32779, 32780, 32781,
            32782, 32783, 32784, 32785, 32786, 32787)) {
            assertEquals("effect $id",0,OfflinePreviewPalettePolicy.rendererId(id,13))
            assertEquals("effect $id",0,OfflinePreviewPalettePolicy.rendererId(id,200))
        }
    }

    @Test fun paletteCompatibleEffectsRetainOwnerSelectedPalette() {
        for (id in listOf(38, 65, 90, 101, 107, 185, 218)) {
            assertEquals("effect $id",13,OfflinePreviewPalettePolicy.rendererId(id,13))
            assertEquals("effect $id",200,OfflinePreviewPalettePolicy.rendererId(id,200))
        }
    }

    @Test fun anUnassignedPaletteStillUsesCoreDefault() {
        for (id in listOf(1, 26, 38, 91, 32777, 32781))
            assertEquals(0,OfflinePreviewPalettePolicy.rendererId(id,0))
    }
    @Test fun missingOrUnsupportedCustomPaletteDoesNotBlockPlayback() {
        for (id in listOf(73, 128, 200, 201, 255, -1, 256))
            assertEquals(0,OfflinePreviewPalettePolicy.availableRendererId(id,false))
        for (id in 0..72)
            assertEquals(id,OfflinePreviewPalettePolicy.availableRendererId(id,false))
        for (id in 73..200)
            assertEquals(id,OfflinePreviewPalettePolicy.availableRendererId(id,true))
        assertEquals(0,OfflinePreviewPalettePolicy.availableRendererId(255,true))
    }
}
