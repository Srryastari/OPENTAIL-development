package com.opentail.android

import org.junit.Assert.*
import org.junit.Test

class OfflineFxSketchTest {
    private val red=0xffff0000.toInt()
    private val black=0xff000000.toInt()
    @Test fun localSketchIsExplicitlyBoundedToKnownEffects() {
        for(id in listOf(0,1,3,9,12)) assertTrue(OfflineFxSketch.supports(id))
        for(id in listOf(-1,32769,23,120)) {
            assertFalse(OfflineFxSketch.supports(id))
            assertNull(OfflineFxSketch.colorAt(id,0,30,10,red,black))
        }
    }
    @Test fun blinkAndWipeActuallyAnimateWithoutCORE() {
        assertEquals(red,OfflineFxSketch.colorAt(1,5,30,0,red,black))
        assertEquals(black,OfflineFxSketch.colorAt(1,5,30,16,red,black))
        assertEquals(black,OfflineFxSketch.colorAt(3,12,30,0,red,black))
        assertEquals(red,OfflineFxSketch.colorAt(3,12,30,24,red,black))
    }
    @Test fun fadeFrameChangesAndNeverDropsOpaqueAlpha() {
        val first=OfflineFxSketch.colorAt(12,5,30,0,red,black)!!
        val second=OfflineFxSketch.colorAt(12,5,30,16,red,black)!!
        assertNotEquals(first,second)
        assertEquals(0xff,first ushr 24)
        assertEquals(0xff,second ushr 24)
    }
    @Test fun indexesAreStrictAndNeverAliasRealCoreFrames() {
        assertNull(OfflineFxSketch.colorAt(1,-1,30,0,red,black))
        assertNull(OfflineFxSketch.colorAt(1,30,30,0,red,black))
        assertNull(OfflineFxSketch.colorAt(1,0,0,0,red,black))
    }
}
