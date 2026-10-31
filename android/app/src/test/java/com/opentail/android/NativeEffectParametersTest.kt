package com.opentail.android

import org.junit.Assert.*
import org.junit.Test

class NativeEffectParametersTest {
    @Test fun everyAuthoringEffectHasPinnedCoreMetadata() {
        NativeEffectCatalog.all().forEach { assertNotNull(it.name, NativeEffectParameters.forId(it.id)) }
    }
    @Test fun solidDoesNotExposeUnsupportedMotionControls() {
        val solid = NativeEffectParameters.forId(0)!!
        assertTrue(solid.foreground)
        assertFalse(solid.background)
        assertFalse(solid.speed)
        assertFalse(solid.intensity)
        assertFalse(solid.palette)
    }
    @Test fun eventAndEmbersColorsRespectTheirDifferentMetadata() {
        assertTrue(NativeEffectParameters.forId(32769)!!.background)
        assertFalse(NativeEffectParameters.forId(32768)!!.background)
    }
}
