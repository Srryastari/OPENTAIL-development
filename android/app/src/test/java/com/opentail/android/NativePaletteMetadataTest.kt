package com.opentail.android

import org.json.JSONArray
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * Authoring-only palette metadata fidelity. No fake pixels or assumption that
 * an installed CORE supports the source project's palette ID.
 */
class NativePaletteMetadataTest {
    private fun stop(index: Number, r: Number, g: Number, b: Number): JSONArray =
        JSONArray().put(index).put(r).put(g).put(b)

    @Test fun exactProjectRgbStopsAndOffsetsArePreserved() {
        val project = JSONObject()
            .put("palettePreviews", JSONObject().put("4",
                JSONArray().put(stop(0, 255, 0, 0))
                    .put(stop(128, 0, 255, 0))
                    .put(stop(255, 0, 0, 255))))
        val palette = NativePaletteEngine.lookup(project, 4)
        val stops = palette.stops ?: error("Real fixed-color stops must be visible")
        assertEquals(3, stops.size)
        assertEquals(0f, stops[0].first, 0f)
        assertEquals(128f / 255f, stops[1].first, 0.000001f)
        assertEquals(1f, stops[2].first, 0f)
        assertEquals(1f, stops[0].second.red, 0.000001f)
        assertEquals(1f, stops[1].second.green, 0.000001f)
        assertEquals(1f, stops[2].second.blue, 0.000001f)
    }

    @Test fun missingDynamicAndMalformedStopsNeverInventSwatches() {
        val project = JSONObject()
        assertNull(NativePaletteEngine.lookup(project, 72).stops)
        project.put("palettePreviews", JSONObject().put("72",
            JSONArray().put(stop(0, 255, 30.5, 0))))
        assertNull(NativePaletteEngine.lookup(project, 72).stops)
        project.put("palettePreviews", JSONObject().put("72",
            JSONArray().put(JSONArray().put(0).put(255).put(0))))
        assertNull(NativePaletteEngine.lookup(project, 72).stops)
        project.put("palettePreviews", JSONObject().put("72",
            JSONArray().put(stop(256, 255, 0, 0))))
        assertNull(NativePaletteEngine.lookup(project, 72).stops)
    }

    @Test fun projectSuppliedNamesOverrideOnlyTheirOwnIds() {
        val names = JSONArray()
        for (id in 0..6) names.put(if (id == 6) "Verified Project Palette" else "")
        val project = JSONObject().put("paletteNames", names)
        assertEquals("Verified Project Palette", NativePaletteEngine.lookup(project, 6).name)
        assertEquals("Palette 7", NativePaletteEngine.lookup(project, 7).name)
        assertEquals("Default", NativePaletteEngine.lookup(project, 0).name)
        assertTrue(NativePaletteEngine.lookup(project, 255).stops == null)
    }
    @Test fun pinnedCoreFixedColorPalettesUseActualForegroundAndBackground() {
        val project = JSONObject()
        assertNull(NativePaletteEngine.lookup(project, 4).stops)
        val solid = NativePaletteEngine.lookup(project, 2, 0x00ff00, 0x000000)
        val split = NativePaletteEngine.lookup(project, 3, 0xff0000, 0x0000ff)
        val gradient = NativePaletteEngine.lookup(project, 4, 0xff0000, 0x0000ff)
        assertEquals(2, solid.stops!!.size)
        assertEquals(4, split.stops!!.size)
        assertEquals(127f / 255f, split.stops!![1].first, 0.000001f)
        assertEquals(128f / 255f, split.stops!![2].first, 0.000001f)
        assertEquals(2, gradient.stops!!.size)
        assertEquals(1f, gradient.stops!![0].second.red, 0.00001f)
        assertEquals(1f, gradient.stops!![1].second.blue, 0.00001f)
        assertNull(NativePaletteEngine.lookup(project, 72, 0xff0000, 0x000000).stops)
    }

    @Test fun onlyVerifiedCustomReadbackCanRenderDevicePalette() {
        val project = JSONObject()
        val confirmed = NativePaletteEngine.lookup(project, 200, verifiedCustomStops =
            "0:#FF0000,128:#00FF00,255:#0000FF")
        assertEquals(3, confirmed.stops!!.size)
        assertEquals(128f/255f, confirmed.stops!![1].first, 0.000001f)
        assertNull(NativePaletteEngine.lookup(project, 200,
            verifiedCustomStops = "0:#FF0000,128:#00FF00").stops)
        assertNull(NativePaletteEngine.lookup(project, 200,
            verifiedCustomStops = "0:#FF0000,0:#00FF00,255:#0000FF").stops)
        assertNull(NativePaletteEngine.lookup(project, 199).stops)
    }

    @Test fun opaqueArgbAsSignedIntIsARealColorNotMissingMetadata() {
        val project=JSONObject()
        val white=NativePaletteEngine.lookup(project,2,0xffffffff.toInt(),0xff000000.toInt())
        assertEquals(2,white.stops!!.size)
        assertEquals(1f,white.stops!![0].second.red,0.000001f)
        val redBlue=NativePaletteEngine.lookup(project,4,
            0xffee0000.toInt(),0xff0000ee.toInt())
        assertEquals(2,redBlue.stops!!.size)
        assertEquals(0xee/255f,redBlue.stops!![0].second.red,0.000001f)
        assertEquals(0xee/255f,redBlue.stops!![1].second.blue,0.000001f)
        assertNull(NativePaletteEngine.lookup(project,2).stops)
        assertNull(NativePaletteEngine.lookup(project,4,0xffffffff.toInt()).stops)
    }

}
