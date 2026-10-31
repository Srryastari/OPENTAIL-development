package com.opentail.android;

import java.util.HashSet;
import java.util.Set;

/**
 * CORE BLE v1 supported-effect catalog. Wire format:
 * byte version, byte count (0..254), then count little-endian uint16 IDs.
 * A real CORE reports 120 WLED-derived IDs plus original effects; never
 * truncate the catalog to a signed-byte (127) limit.
 */
final class BleFxCapabilities {
    private BleFxCapabilities() { }

    static String decode(byte[] raw) {
        if (raw == null || raw.length < 2 || (raw[0] & 255) != 1)
            throw new IllegalArgumentException("unsupported catalog header");
        int n = raw[1] & 255;
        if (n > 254 || raw.length != 2 + n * 2)
            throw new IllegalArgumentException("catalog length/count mismatch");
        StringBuilder ids = new StringBuilder("[");
        Set<Integer> seen = new HashSet<>();
        for (int i = 0; i < n; i++) {
            int id = (raw[2 + i * 2] & 255) | ((raw[3 + i * 2] & 255) << 8);
            if (!seen.add(id))
                throw new IllegalArgumentException("duplicate effect ID");
            if (i != 0) ids.append(',');
            ids.append(id);
        }
        return ids.append(']').toString();
    }
}
