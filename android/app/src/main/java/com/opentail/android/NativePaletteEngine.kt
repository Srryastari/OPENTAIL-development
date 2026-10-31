package com.opentail.android

import androidx.compose.ui.graphics.Color
import org.json.JSONObject

/**
 * Native, data-only palette catalog. OpenTail owns the designer UI.
 * Fixed RGB swatches are shown ONLY for real stop metadata supplied in a
 * project (palettePreviews, using the /json/palx [index,R,G,B] shape).
 * Unknown palette IDs never receive invented gradient colors.
 */
internal data class NativePalette(
    val id: Int,
    val name: String,
    val stops: List<Pair<Float, Color>>?
)

internal object NativePaletteEngine {
    const val MAX_ID = 255
    private val knownNames = listOf(
        "Default", "Random Cycle", "Color 1", "Colors 1&2", "Color Gradient"
    )

    fun lookup(project: JSONObject, requested: Int): NativePalette {
        val id = requested.coerceIn(0, MAX_ID)
        val optionalNames = project.optJSONArray("paletteNames")
        val suppliedName = optionalNames?.optString(id)?.trim().orEmpty()
        val name = if (suppliedName.isNotBlank() && suppliedName.length <= 64)
            suppliedName else knownNames.getOrNull(id) ?: "Palette $id"
        val raw = project.optJSONObject("palettePreviews")?.optJSONArray(id.toString())
            ?: return NativePalette(id, name, null)
        if (raw.length() !in 1..64) return NativePalette(id, name, null)
        val stops = ArrayList<Pair<Float, Color>>(raw.length())
        for (i in 0 until raw.length()) {
            val entry = raw.optJSONArray(i) ?: return NativePalette(id, name, null)
            if (entry.length() != 4) return NativePalette(id, name, null)
            val parts = (0..3).map { index ->
                val value = entry.opt(index)
                val number = (value as? Number)?.toDouble()
                    ?: return NativePalette(id, name, null)
                if (number !in 0.0..255.0 || number % 1.0 != 0.0)
                    return NativePalette(id, name, null)
                number.toInt()
            }
            stops.add(parts[0] / 255f to Color(
                0xff000000.toInt() or (parts[1] shl 16) or (parts[2] shl 8) or parts[3]
            ))
        }
        stops.sortBy { it.first }
        // Compose gradients need endpoints for reliable fill; retain the
        // actual sample positions and colors, including repeated stops.
        val bounded = ArrayList<Pair<Float, Color>>(stops.size + 2)
        if (stops.first().first > 0f) bounded.add(0f to stops.first().second)
        bounded.addAll(stops)
        if (stops.last().first < 1f) bounded.add(1f to stops.last().second)
        if (bounded.size == 1) bounded.add(1f to bounded[0].second)
        return NativePalette(id, name, bounded)
    }

    /**
     * CORE #49 advertises 16 source-derived palette-capable FX, plus Embers.
     * A single Java policy also guards BLE transport, so preview cannot
     * disagree with the command that reaches firmware.
     */
    fun coreSupports(effectId: Int, paletteId: Int): Boolean =
        BleFxPalettePolicy.isWireCandidate(effectId, paletteId)

    fun corePreviewPalette(effectId: Int, selectedId: Int): Int =
        if (coreSupports(effectId, selectedId)) selectedId else 0

    fun effectiveStepPalette(stepPalette: Int, sharedEnabled: Boolean, sharedPalette: Int): Int =
        (if (sharedEnabled) sharedPalette else stepPalette).coerceIn(0, MAX_ID)
}
