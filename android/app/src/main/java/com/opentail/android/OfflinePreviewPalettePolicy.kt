package com.opentail.android

/** Offline-renderer admission, not the palette catalog or transport policy.
 * Preserve selected/saved palette IDs; the CORE render call must use zero when
 * the selected effect's actual metadata declares no palette support.
 */
internal object OfflinePreviewPalettePolicy {
    fun rendererId(effectId: Int, savedPaletteId: Int): Int =
        if (NativeEffectParameters.forId(effectId)?.palette == true) savedPaletteId else 0
}
