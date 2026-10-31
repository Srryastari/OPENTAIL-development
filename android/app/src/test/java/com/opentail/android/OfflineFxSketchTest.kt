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
            assertFalse(OfflineFxSketch().render(id,0,10,red,black,128,128,255,IntArray(30)))
        }
    }
    private fun color(id:Int,time:Long,brightness:Int=255):Int {
        val colors=IntArray(30)
        assertTrue(OfflineFxSketch().render(id,0,time,red,black,128,128,brightness,colors))
        return colors[5]
    }
    @Test fun blinkUsesCoreDutyCycle() {
        assertEquals(red,color(1,0))
        assertEquals(black,color(1,2000))
    }
    @Test fun fadeFrameChangesAndNeverDropsOpaqueAlpha() {
        val first=color(12,0)
        val second=color(12,500)
        assertNotEquals(first,second)
        assertEquals(0xff,first ushr 24)
        assertEquals(0xff,second ushr 24)
    }
    @Test fun buffersPalettesAndBrightnessAreStrict() {
        val renderer=OfflineFxSketch()
        assertFalse(renderer.render(1,0,0,red,black,128,128,255,IntArray(0)))
        assertFalse(renderer.render(1,0,-1,red,black,128,128,255,IntArray(30)))
        for(id in listOf(0,1,3,9,12)) {
            assertFalse(OfflineFxSketch.supports(id,1))
            assertFalse(OfflineFxSketch.supports(id,73))
            assertEquals(black,color(id,500,0))
        }
        assertEquals(0xff180000.toInt(),color(0,0,24))
    }
}
