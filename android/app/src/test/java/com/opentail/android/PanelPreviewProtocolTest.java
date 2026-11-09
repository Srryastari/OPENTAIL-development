package com.opentail.android;

import org.junit.Test;
import static org.junit.Assert.*;

public class PanelPreviewProtocolTest {
    private static void put16(byte[] b,int at,int v){b[at]=(byte)v;b[at+1]=(byte)(v>>>8);}
    private static void put32(byte[] b,int at,long v){for(int i=0;i<4;i++)b[at+i]=(byte)(v>>>(8*i));}
    private static byte[] status(int state,long session,long accepted,long crc,int flags) {
        byte[] b=new byte[61];b[0]=2;b[1]=(byte)state;b[2]=0;b[3]=3;
        put32(b,4,session);put16(b,8,7);put16(b,10,944);put16(b,12,5000);
        put32(b,14,accepted);put32(b,18,crc);put16(b,22,7);put16(b,24,7);put16(b,26,7);
        b[29]=1;b[60]=(byte)flags;return b;
    }

    @Test public void capabilityNegotiationRequiresAllPanelV2Features() {
        byte[] caps={3,3,4,8,8,0x0f,(byte)0x9b,2,15,5,32};
        PanelPreviewProtocol.Capabilities parsed=PanelPreviewProtocol.Capabilities.parse(caps);
        assertTrue(parsed.supportsPreviewV2());
        caps[5]=0x07;
        assertFalse(PanelPreviewProtocol.Capabilities.parse(caps).supportsPreviewV2());
    }

    @Test public void statusV2DecodesAllSequenceAndTransactionFields() {
        byte[] b=status(2,0x89abcdefL,17,0x12345678L,7);
        put32(b,35,16);b[39]=5;put32(b,40,2);put32(b,44,3);
        put32(b,48,4);put32(b,52,5);put32(b,56,6);
        PanelPreviewProtocol.Status s=PanelPreviewProtocol.Status.parse(b);
        assertEquals(2,s.state);assertEquals(0x89abcdefL,s.sessionId);
        assertEquals(17,s.lastAcceptedSequence);assertEquals(0x12345678L,s.candidateCrc32);
        assertEquals(16,s.lastRejectedSequence);assertEquals(5,s.lastRejectedReason);
        assertEquals(2,s.missingCount);assertEquals(3,s.outOfOrderCount);
        assertEquals(4,s.duplicateCount);assertEquals(5,s.conflictCount);
        assertEquals(6,s.committingRejectCount);assertTrue(s.hardSyncFault);
        assertTrue(s.warningObserved);assertTrue(s.hasSequence);
    }

    @Test public void statusRejectsWrongLengthVersionAndInconsistentSequenceFlag() {
        try { PanelPreviewProtocol.Status.parse(new byte[60]); fail("length"); }
        catch(IllegalArgumentException expected) {}
        byte[] b=status(1,1,1,0,0);b[0]=1;
        try { PanelPreviewProtocol.Status.parse(b); fail("version"); }
        catch(IllegalArgumentException expected) {}
        b=status(1,1,1,0,0);
        try { PanelPreviewProtocol.Status.parse(b); fail("sequence flag"); }
        catch(IllegalArgumentException expected) {}
    }

    @Test public void controlAndUpdatePacketsMatchLittleEndianWireContract() {
        assertArrayEquals(new byte[]{(byte)0xfe,1,1,7,0,(byte)0xb0,3,3},
            PanelPreviewProtocol.begin(1,7,944,3));
        assertArrayEquals(new byte[]{(byte)0xfe,4,0,(byte)0xef,(byte)0xcd,(byte)0xab,(byte)0x89},
            PanelPreviewProtocol.cancel(0,0x89abcdefL));
        PanelPreviewProtocol.UpdateDescriptor d=
            new PanelPreviewProtocol.UpdateDescriptor(2,3,1,0x1234,0x0020,0x05);
        byte[] update=PanelPreviewProtocol.update(1,0x89abcdefL,9,
            new PanelPreviewProtocol.UpdateDescriptor[]{d},23);
        assertEquals(19,update.length);
        assertEquals(0xfe,update[0]&255);assertEquals(2,update[1]&255);
        assertEquals(1,update[2]&255);assertEquals(0xef,update[3]&255);
        assertEquals(9,update[7]&255);assertEquals(2,update[11]&255);
        assertEquals(0x34,update[14]&255);assertEquals(0x12,update[15]&255);
        try { PanelPreviewProtocol.update(0,1,0,
            new PanelPreviewProtocol.UpdateDescriptor[]{d},247);fail("sequence zero"); }
        catch(IllegalArgumentException expected) {}
        try { PanelPreviewProtocol.update(0,1,1,
            new PanelPreviewProtocol.UpdateDescriptor[]{d,d},23);fail("MTU"); }
        catch(IllegalArgumentException expected) {}
    }

    @Test public void mapSnapshotHeaderAndCrcMustMatchAuthoritativeBytes() {
        byte[] wire=new byte[PanelPreviewProtocol.MAP_BYTES];wire[0]=3;wire[1]=1;
        int crc=PanelPreviewProtocol.crc16(wire);
        byte[] h=new byte[14];h[0]=3;h[1]=(byte)0x81;h[2]=1;h[3]=0;
        put16(h,4,PanelPreviewProtocol.MAP_BYTES);put16(h,6,crc);
        put16(h,8,12);put16(h,10,12);h[13]=1;
        PanelPreviewProtocol.SnapshotHeader header=PanelPreviewProtocol.SnapshotHeader.parse(h,1);
        header.verify(wire,1);assertEquals(12,header.activeRevision);
        wire[22]=1;
        try { header.verify(wire,1);fail("crc mismatch"); }
        catch(IllegalArgumentException expected) {}
    }

    @Test public void crc16MatchesCcittFalseKnownVector() {
        assertEquals(0x29b1,PanelPreviewProtocol.crc16("123456789".getBytes(java.nio.charset.StandardCharsets.US_ASCII)));
    }
}
