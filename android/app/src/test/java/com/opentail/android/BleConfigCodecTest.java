package com.opentail.android;

import org.json.JSONArray;
import org.json.JSONObject;
import org.junit.Test;
import static org.junit.Assert.*;

public class BleConfigCodecTest {
    private JSONObject output(Object pixels, Object... lengths) throws Exception {
        JSONArray zones = new JSONArray();
        for (Object length : lengths) zones.put(new JSONObject()
            .put("name", "Zone").put("role", "off").put("pixels", length));
        return new JSONObject().put("name", "Output").put("pixels", pixels)
            .put("color_order", "GRB").put("zones", zones);
    }
    private void rejected(JSONObject json) throws Exception {
        try { BleConfigCodec.encode(json); fail("Invalid geometry accepted: " + json); }
        catch (IllegalArgumentException expected) { }
    }
    @Test public void boundaryRoundTripsPreserveWireFormat() throws Exception {
        for (int count : new int[]{1, 1999, 2000}) {
            byte[] wire = BleConfigCodec.encode(output(count, count));
            assertEquals(261, wire.length);
            assertEquals(1, wire[0]);
            assertEquals(count, (wire[1]&255) | ((wire[2]&255)<<8));
            assertEquals(count, BleConfigCodec.decode(wire, 0).getInt("pixels"));
        }
        assertEquals(0x29b1, BleConfigCodec.crc16("123456789".getBytes("US-ASCII")));
    }
    @Test public void rejectsInvalidCountsBeforeIntegerCoercion() throws Exception {
        for (Object count : new Object[]{-1, 0, 2001, 65535, 4294967297L, 1.5, "2000",
                new java.math.BigDecimal("2000.000000000000000001")}) {
            rejected(output(count, count));
        }
        for (Object count : new Object[]{-1, 2001, 4294967297L, 0.5, "1"})
            rejected(output(1, count));
    }
    @Test public void checksCumulativeBudgetAndPreservesZeroPlaceholders() throws Exception {
        BleConfigCodec.encode(output(2000, 1000, 1000));
        BleConfigCodec.encode(output(2000, 0, 2000));
        rejected(output(2000, 1000, 1001));
        rejected(output(2001, 1000, 1001));
        rejected(output(2000, 1999));
    }
    @Test public void rejectsOversizedDeviceReadback() throws Exception {
        byte[] wire = BleConfigCodec.encode(output(2000, 2000));
        for (int count : new int[]{0, 2001, 65535}) {
            wire[1]=(byte)count; wire[2]=(byte)(count>>8);
            wire[62]=(byte)count; wire[63]=(byte)(count>>8);
            try { BleConfigCodec.decode(wire, 0); fail("Oversized readback accepted"); }
            catch (Exception expected) { }
        }
    }
}
