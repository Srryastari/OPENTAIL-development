package com.opentail.android;

import org.json.JSONArray;
import org.json.JSONObject;
import org.junit.Test;
import static org.junit.Assert.*;

/** New CORE outputs may be genuinely unconfigured (0 pixels and 0 zones).
 * Existing configured BLE v1 frames must keep their old strict validation. */
public class BleConfigUnconfiguredOutputTest {
    private static byte[] blankOutput() {
        byte[] bytes = new byte[BleConfigCodec.LENGTH];
        bytes[0] = 1;  // BLE output v1, not a new wire format
        bytes[3] = 2;  // GRB
        byte[] name = "Pixel OUT 1".getBytes(java.nio.charset.StandardCharsets.US_ASCII);
        System.arraycopy(name, 0, bytes, 5, name.length);
        for (int i = 30; i < 36; i++) bytes[i] = (byte)255;
        return bytes; // pixels=0, zones=0
    }
    @Test public void acceptsTrueEmptyOutputReadback() throws Exception {
        JSONObject decoded = BleConfigCodec.decode(blankOutput(),0);
        assertEquals(0,decoded.getInt("pixels"));
        assertEquals(0,decoded.getInt("zone_count"));
        assertEquals(0,decoded.getJSONArray("zones").length());
        assertEquals("GRB",decoded.getString("color_order"));
    }
    @Test public void rejectsHalfConfiguredFrames() throws Exception {
        byte[] pixelsWithoutZones=blankOutput();
        pixelsWithoutZones[1]=1;
        try { BleConfigCodec.decode(pixelsWithoutZones,0);fail("pixels>0 require a zone"); }
        catch (Exception expected) { }
        byte[] zonesWithoutPixels=blankOutput();
        zonesWithoutPixels[4]=1;
        try { BleConfigCodec.decode(zonesWithoutPixels,0);fail("zero pixels require zero zones"); }
        catch (Exception expected) { }
    }
    @Test public void existingConfiguredOutputRoundTrips() throws Exception {
        JSONObject configured=new JSONObject();
        configured.put("pixels",522).put("name","Pixel OUT 1").put("color_order","GRB");
        JSONArray zones=new JSONArray();
        zones.put(new JSONObject().put("name","Main").put("pixels",522).put("role","off"));
        configured.put("zones",zones);
        JSONObject readback=BleConfigCodec.decode(BleConfigCodec.encode(configured),0);
        assertEquals(522,readback.getInt("pixels"));
        assertEquals(1,readback.getInt("zone_count"));
        assertEquals(522,readback.getJSONArray("zones").getJSONObject(0).getInt("pixels"));
    }
    @Test public void emptyReadbackDoesNotAutoSaveAsActiveGeometry() throws Exception {
        JSONObject empty=BleConfigCodec.decode(blankOutput(),0);
        try { BleConfigCodec.encode(empty); fail("empty outputs are read-only until configured"); }
        catch (IllegalArgumentException expected) { }
    }
}
