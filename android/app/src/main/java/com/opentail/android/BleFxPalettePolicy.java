package com.opentail.android;

/** BLE FX v1 palette byte admission, aligned with CORE ot_palette_1d_supported.
 * CORE still decides whether a particular effect advertises palette support.
 */
final class BleFxPalettePolicy {
    private BleFxPalettePolicy() { }

    static boolean isWireCandidate(int effectId, int paletteId) {
        if (effectId < 0 || effectId > 65535 || paletteId < 0 || paletteId > 200)
            return false;
        if (paletteId == 0) return true;
        // Original Embers supports 2..4, unlike source-derived palette effects.
        return effectId == 32768 ? paletteId >= 2 && paletteId <= 4 : true;
    }
}
