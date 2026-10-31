package com.opentail.android;

import org.junit.Test;
import static org.junit.Assert.*;

/** Real CORE has >127 supported effect IDs. Never truncate a BLE GATT long-read. */
public class BleFxCapabilitiesTest {
    private static byte[] catalog(int count) {
        byte[] result = new byte[2 + count * 2];
        result[0] = 1; result[1] = (byte)count;
        for (int i = 0; i < count; i++) {
            int id = i < 120 ? i : 32768 + i - 120;
            result[2 + 2*i] = (byte)id;
            result[3 + 2*i] = (byte)(id >> 8);
        }
        return result;
    }
    @Test public void acceptsFullCoreCatalog() {
        String full = BleFxCapabilities.decode(catalog(140));
        assertTrue(full.startsWith("[0,1,2,"));
        assertTrue(full.contains("32768"));
        assertTrue(full.endsWith("32787]"));
        assertEquals(140, full.substring(1,full.length()-1).split(",").length);
    }
    @Test public void acceptsMaximumWireCount() {
        assertEquals(254, BleFxCapabilities.decode(catalog(254)).split(",").length);
    }
    @Test public void rejectsCorruptionAndTruncation() {
        try { BleFxCapabilities.decode(new byte[]{1, (byte)140, 0, 0}); fail(); }
        catch (IllegalArgumentException expected) { }
        try { BleFxCapabilities.decode(new byte[]{2, 0}); fail(); }
        catch (IllegalArgumentException expected) { }
        try { BleFxCapabilities.decode(new byte[]{1, (byte)255}); fail(); }
        catch (IllegalArgumentException expected) { }
        try { BleFxCapabilities.decode(new byte[]{1,2, 7,0,7,0}); fail(); }
        catch (IllegalArgumentException expected) { }
    }
}
