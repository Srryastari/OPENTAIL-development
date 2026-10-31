package com.opentail.android

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * Source-authoritative WLED+ design metadata review coverage.
 * Complements NativeEffectParameters source/CORE contract checks:
 * slot IDs are never compacted to match only visible color circles.
 */
class ApprovedWledEffectsMetadataTest {
    @Test fun glitterEditsRealThirdColorSlotNotFirst() {
        val d=ApprovedEditorMetadata.definition("Glitter",null)
        assertEquals(listOf(2),d.roles.map{it.slot})
        assertEquals(listOf("Gl"),d.roles.map{it.label})
        assertTrue(d.palette)
        assertTrue(d.params.any{it.label=="Overlay" && it.isSwitch})
    }

    @Test fun solidGlitterPreservesSparseBgAndThirdGlSlot() {
        val d=ApprovedEditorMetadata.definition("Solid Glitter",null)
        assertEquals(listOf(0,2),d.roles.map{it.slot})
        assertEquals(listOf("Bg","Gl"),d.roles.map{it.label})
        assertFalse(d.palette)
    }

    @Test fun scannerDualAndTwoDotsPreserveNamedParametersAndThreeColors() {
        val scanner=ApprovedEditorMetadata.definition("Scanner Dual",null)
        assertEquals(listOf("Fx","Bg","Cs"),scanner.roles.map{it.label})
        assertTrue(scanner.params.any{it.label=="Trail"})
        assertTrue(scanner.params.any{it.label=="Delay"})
        assertTrue(scanner.params.any{it.label=="Dual" && it.isSwitch})
        assertTrue(scanner.params.any{it.label=="Bi-delay" && it.isSwitch})
        val dots=ApprovedEditorMetadata.definition("Two Dots",null)
        assertEquals(listOf("1","2","Bg"),dots.roles.map{it.label})
        assertTrue(dots.params.any{it.label=="Dot size"})
        assertTrue(dots.params.any{it.label=="Overlay" && it.isSwitch})
    }

    @Test fun blinkTwinkleAndSolidUseSourceMetadata() {
        assertEquals(listOf("Speed","Duty cycle"),
            ApprovedEditorMetadata.definition("Blink",null).params.map{it.label})
        assertEquals(listOf("Speed","Intensity"),
            ApprovedEditorMetadata.definition("Twinkle",null).params.map{it.label})
        assertTrue(ApprovedEditorMetadata.definition("Solid",null).params.isEmpty())
        assertEquals(listOf("Fx"),
            ApprovedEditorMetadata.definition("Solid",null).roles.map{it.label})
    }

    @Test fun unknownEffectsFallbackOnlyToPinnedCoreFlags() {
        val unsupported=ApprovedEditorMetadata.definition("Unknown sample",null)
        assertTrue(unsupported.roles.isEmpty())
        assertTrue(unsupported.params.isEmpty())
        assertFalse(unsupported.palette)
        val flags=EffectParameters(true,true,true,true,true)
        val d=ApprovedEditorMetadata.definition("Unknown sample",flags)
        assertEquals(listOf("Fx","Bg"),d.roles.map{it.label})
        assertEquals(listOf("Speed","Intensity"),d.params.map{it.label})
        assertTrue(d.palette)
    }
}
