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
    // Exact pinned CORE palette_1d semantics: 2..5 derive from user color
    // slots. All other built-in gradients carry their own source RGB.
    fun usesEditableColorSlots(id: Int): Boolean = id in 2..5
    fun isImmutableFixedGradient(id: Int): Boolean = id == 0 || id in 6..72
    private val knownNames = listOf(
        "Default", "Random Cycle", "Color 1", "Colors 1&2", "Color Gradient"
    )

    fun lookup(project: JSONObject, requested: Int, foreground: Int? = null,
               background: Int? = null, verifiedCustomStops: String? = null,
               accent: Int? = null): NativePalette {
        val id = requested.coerceIn(0, MAX_ID)
        val optionalNames = project.optJSONArray("paletteNames")
        val suppliedName = optionalNames?.optString(id)?.trim().orEmpty()
        val name = if (suppliedName.isNotBlank() && suppliedName.length <= 64)
            suppliedName else knownNames.getOrNull(id) ?: "Palette $id"
        // Only show source-verified colors. A successful BLE v3 QUERY gives
        // the authoritative stored stops for one custom ID (200 - slot).
        if (id in 73..200 && verifiedCustomStops != null) {
            parseVerifiedCustom(verifiedCustomStops)?.let { return NativePalette(id, name, it) }
        }
        // Built-in palettes have immutable, source-authentic gradients.
        // Do not let a saved project preview or Main/Set Color recolor them.
        // IDs 2..5 are explicitly defined in terms of user color slots.
        val fixedPalette = id == 0 || id in 2..72
        val colorsAvailable = id == 0 || id >= 6 ||
            (foreground != null && (id == 2 || background != null))
        if (fixedPalette && colorsAvailable) {
            val fixed = runCatching {
                OfflineCoreRenderer.fixedPalette(id,
                    foreground ?: -1, background ?: 0xff000000.toInt(),
                    accent ?: 0xff000000.toInt())
            }.getOrNull()
            if (fixed != null && fixed.size == 16)
                return NativePalette(id, name, fixed.mapIndexed { index, rgb ->
                    (index.toFloat() / 15f) to Color(rgb)
                })
        }
        // Never substitute a project-defined gradient for a source-defined
        // immutable WLED palette. If the exact JNI builder is unavailable,
        // leave the swatch unknown, not misleading. Editable 2..5 retain
        // their pinned CORE-derived local fallback for non-JNI JVM tests.
        if (isImmutableFixedGradient(id))
            return NativePalette(id, name, null)
        val raw = project.optJSONObject("palettePreviews")?.optJSONArray(id.toString())
            ?: return NativePalette(id, name, coreFixedStops(id, foreground, background))
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

    // This mapping comes from pinned CORE #49 palette_1d.c, not guessed
    // WLED gradient artwork. Random Cycle and other dynamic IDs stay unknown.
    private fun coreFixedStops(id: Int, fg: Int?, bg: Int?): List<Pair<Float, Color>>? {
        // Opaque ARGB colors are negative signed Ints. Never use fg < 0 or
        // bg < 0 as an absence test: 0xFFFFFFFF (white) is a valid color.
        if (fg == null) return null
        val foreground = Color(0xff000000.toInt() or (fg and 0xffffff))
        if (id == 2) return listOf(0f to foreground, 1f to foreground)
        if (bg == null) return null
        val background = Color(0xff000000.toInt() or (bg and 0xffffff))
        return when (id) {
            3 -> listOf(0f to foreground, (127f / 255f) to foreground,
                        (128f / 255f) to background, 1f to background)
            4 -> listOf(0f to foreground, 1f to background)
            else -> null
        }
    }

    private fun parseVerifiedCustom(text: String): List<Pair<Float, Color>>? {
        val records = text.split(',')
        if (records.size !in 2..16) return null
        val result = ArrayList<Pair<Float, Color>>(records.size)
        var previous = -1
        val pattern = Regex("(\\d{1,3}):#([0-9A-Fa-f]{6})")
        for (record in records) {
            val match = pattern.matchEntire(record.trim()) ?: return null
            val position = match.groupValues[1].toIntOrNull() ?: return null
            if (position <= previous || position > 255) return null
            val rgb = match.groupValues[2].toLong(16).toInt()
            result.add(position / 255f to Color(0xff000000.toInt() or rgb))
            previous = position
        }
        if (result.first().first != 0f || previous != 255) return null
        return result
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
