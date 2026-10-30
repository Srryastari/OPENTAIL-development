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
}
