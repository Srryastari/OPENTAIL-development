package com.opentail.android;

/** Pinned to CORE #49 ot_palette_1d_supported() and 120-effect metadata.
 * This is transport admission, not a promise of visual palette parity.
 */
final class BleFxPalettePolicy {
    private BleFxPalettePolicy() { }

    static boolean isWireCandidate(int effectId, int paletteId) {
        if (effectId < 0 || effectId > 65535 || paletteId < 0 || paletteId > 200)
            return false;
        if (paletteId == 0) return true;
        // The original Embers recipe supports only Colors 1, 1&2 and Gradient.
        if (effectId == 32768) return paletteId >= 2 && paletteId <= 4;
        // The source-derived modes below are the 16 whose CORE metadata
        // currently advertises supports_palette=true; all other IDs fail closed.
        switch (effectId) {
            case 38: case 65: case 66: case 74: case 80: case 81:
            case 87: case 90: case 101: case 107: case 112: case 115:
            case 179: case 185: case 218: case 219:
                return true;
            default:
                return false;
        }
    }
}
