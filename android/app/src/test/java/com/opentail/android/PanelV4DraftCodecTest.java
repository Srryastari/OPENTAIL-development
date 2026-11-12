package com.opentail.android;

import org.junit.Test;
import org.json.JSONObject;
import org.json.JSONArray;
import static org.junit.Assert.*;

public class PanelV4DraftCodecTest {
    private static void put16(byte[] b,int at,int n){b[at]=(byte)n;b[at+1]=(byte)(n>>>8);}
    private static void name(byte[] b,int at,String s) {
        byte[] chars=s.getBytes(java.nio.charset.StandardCharsets.US_ASCII);
        System.arraycopy(chars,0,b,at,chars.length);
    }
    private static PanelPreviewProtocol.LaneSnapshot snapshot(byte[] wire,int revision) {
        byte[] header=new byte[14];
        header[0]=3;header[1]=(byte)0x81;header[2]=wire[1];
        put16(header,4,wire.length);put16(header,6,PanelPreviewProtocol.crc16(wire));
        put16(header,8,revision);put16(header,10,revision);
        header[13]=1;
        PanelPreviewProtocol.SnapshotHeader parsed=
            PanelPreviewProtocol.SnapshotHeader.parse(header,wire[1]&255);
        parsed.verify(wire,wire[1]&255);
        return new PanelPreviewProtocol.LaneSnapshot(parsed,wire);
    }
    @Test public void factoryZeroMapNeedsNoSyntheticPlaceholder() throws Exception {
        for(int lane=0;lane<2;lane++) {
            byte[] before=PanelV4LayoutProtocol.expectedFactoryMap(lane);
            JSONObject draft=PanelV4DraftCodec.fromSnapshot(snapshot(before,1));
            assertEquals(0,draft.getInt("pixels"));
            assertEquals(0,draft.getJSONArray("zones").length());
            assertEquals("Pixel OUT "+(lane+1),draft.getString("name"));
            assertTrue(draft.getBoolean("v4_editable"));
            assertArrayEquals(before,PanelV4DraftCodec.encode(lane,draft,snapshot(before,1)));
        }
    }
    @Test public void firstRealZoneRetainsFull16BitIdentityAfterReadback() throws Exception {
        byte[] empty=PanelV4LayoutProtocol.expectedFactoryMap(0);
        PanelPreviewProtocol.LaneSnapshot first=snapshot(empty,4);
        JSONObject draft=PanelV4DraftCodec.fromSnapshot(first);
        draft.put("pixels",100);
        draft.getJSONArray("zones").put(new JSONObject().put("pixels",100)
            .put("name","Brake").put("role","brake"));
        byte[] wire=PanelV4DraftCodec.encode(0,draft,first);
        assertEquals(100,(wire[2]&255)|((wire[3]&255)<<8));
        assertEquals(1,wire[37]&255);assertEquals(1,wire[38]&255);
        assertEquals(0x8001,(wire[39]&255)|((wire[40]&255)<<8));
        assertEquals(0x4001,(wire[155]&255)|((wire[156]&255)<<8));
        assertEquals(3,wire[184]&255); // BRAKE role
        JSONObject confirmed=PanelV4DraftCodec.fromSnapshot(snapshot(wire,5));
        assertEquals(0x4001,confirmed.getJSONArray("zones").getJSONObject(0).getInt("id"));
        assertArrayEquals(wire,PanelV4DraftCodec.encode(0,confirmed,snapshot(wire,5)));
    }
    @Test public void rejectsComplexGeometryRatherThanFlatteningItsRanges() throws Exception {
        byte[] wire=PanelV4LayoutProtocol.expectedFactoryMap(0);
        put16(wire,2,100);wire[37]=1;wire[38]=1;
        put16(wire,39,0x8001);name(wire,42,"Panel 1");wire[67]=1;
        put16(wire,155,0x4001);put16(wire,157,0x8001);
        name(wire,159,"Brake");wire[184]=3;wire[186]=2;
        put16(wire,187,0);put16(wire,189,50);
        put16(wire,191,60);put16(wire,193,40);
        PanelPreviewProtocol.LaneSnapshot confirmed=snapshot(wire,12);
        JSONObject draft=PanelV4DraftCodec.fromSnapshot(confirmed);
        assertFalse(draft.getBoolean("v4_editable"));
        try {PanelV4DraftCodec.encode(0,draft,confirmed);fail("multi-range flatten");}
        catch(IllegalArgumentException expected) {}
    }
}
