package com.opentail.android;

import org.junit.Test;
import static org.junit.Assert.*;

/** Exact Firmware #127 Panel v4 factory reset and zero-LED wire contract. */
public class PanelV4LayoutProtocolTest {
    @Test public void bothFactoryLanesAreTrulyZeroAndDistinct() {
        for(int lane=0;lane<2;lane++) {
            byte[] wire=PanelV4LayoutProtocol.expectedFactoryMap(lane);
            assertEquals(667,wire.length);
            assertEquals(3,wire[0]&255);
            assertEquals(lane,wire[1]&255);
            assertEquals(0,wire[2]&255);
            assertEquals(0,wire[3]&255);
            assertEquals(2,wire[4]&255);
            assertEquals(0,wire[36]&255); // gamma off
            assertEquals(0,wire[37]&255); // no panels
            assertEquals(0,wire[38]&255); // no zones
            for(int i=30;i<36;i++)assertEquals(255,wire[i]&255);
            for(int i=39;i<wire.length;i++)assertEquals(0,wire[i]&255);
            assertTrue(PanelV4LayoutProtocol.isFactoryMap(lane,wire));
            wire[37]=1;
            assertFalse(PanelV4LayoutProtocol.isFactoryMap(lane,wire));
        }
        assertFalse(PanelV4LayoutProtocol.isFactoryMap(1,
            PanelV4LayoutProtocol.expectedFactoryMap(0)));
    }

    @Test public void challengeAndConfirmationMatchFirmwareLittleEndianWire() {
        assertArrayEquals(new byte[]{3,(byte)255,(byte)170,1,0x34,0x12},
            PanelV4LayoutProtocol.startFreshChallenge(1,0x1234));
        assertArrayEquals(new byte[]{3,(byte)255,(byte)171,0,
            (byte)0xef,(byte)0xcd,(byte)0xab,(byte)0x89,0x78,0x56},
            PanelV4LayoutProtocol.startFreshConfirm(0,0x5678,0x89abcdefL));
    }

    @Test public void durableUploadUsesExact667ByteV4WireAndFifteenByteChunks() {
        byte[] wire=PanelV4LayoutProtocol.expectedFactoryMap(1);
        byte[] begin=PanelV4LayoutProtocol.beginSave(1,0x1234,wire);
        assertEquals(9,begin.length);
        assertEquals(3,begin[0]&255);assertEquals(3,begin[1]&255);
        assertEquals(1,begin[2]&255);
        assertEquals(667, (begin[3]&255)|((begin[4]&255)<<8));
        assertEquals(PanelPreviewProtocol.crc16(wire),
            (begin[5]&255)|((begin[6]&255)<<8));
        assertEquals(0x1234,(begin[7]&255)|((begin[8]&255)<<8));
        int offset=0;
        while(offset<wire.length) {
            byte[] chunk=PanelV4LayoutProtocol.saveChunk(1,wire,offset);
            assertTrue(chunk.length>=6&&chunk.length<=20);
            assertEquals(3,chunk[0]&255);assertEquals(4,chunk[1]&255);
            assertEquals(1,chunk[2]&255);
            assertEquals(offset,(chunk[3]&255)|((chunk[4]&255)<<8));
            for(int k=5;k<chunk.length;k++)assertEquals(wire[offset+k-5],chunk[k]);
            offset+=chunk.length-5;
        }
        assertEquals(667,offset);
        assertArrayEquals(new byte[]{3,5,1},PanelV4LayoutProtocol.completeSave(1));
        assertArrayEquals(new byte[]{3,6,1},PanelV4LayoutProtocol.cancelUncommittedSave(1));
        assertEquals(0,PanelV4LayoutProtocol.parseSaveAck(1,5,
            new byte[]{3,(byte)0x85,1,0}));
        assertEquals(3,PanelV4LayoutProtocol.parseSaveAck(1,5,
            new byte[]{3,(byte)0x85,1,3}));
        try { PanelV4LayoutProtocol.parseSaveAck(1,5,
            new byte[]{3,(byte)0x85,0,0});fail("wrong lane ACK"); }
        catch(IllegalArgumentException expected) {}
        try {PanelV4LayoutProtocol.beginSave(0,1,wire);fail("cross-lane Save");}
        catch(IllegalArgumentException expected) {}
    }

    @Test public void distinctFirmwareSaveNacksGiveActionableFailClosedReasons() {
        assertTrue(PanelV4LayoutProtocol.saveRejectionMessage(2).contains("busy"));
        assertTrue(PanelV4LayoutProtocol.saveRejectionMessage(3).contains("stale") ||
                   PanelV4LayoutProtocol.saveRejectionMessage(3).contains("expired"));
        assertTrue(PanelV4LayoutProtocol.saveRejectionMessage(7).contains("Invalid"));
        assertTrue(PanelV4LayoutProtocol.saveRejectionMessage(10).contains("unaccepted"));
        assertTrue(PanelV4LayoutProtocol.saveRejectionMessage(4).contains("Flash"));
        for(int code=1;code<=10;code++) {
            assertFalse(PanelV4LayoutProtocol.saveRejectionMessage(code).isEmpty());
            assertEquals(code,PanelV4LayoutProtocol.parseSaveAck(1,5,
                new byte[]{3,(byte)0x85,1,(byte)code}));
        }
    }

    @Test public void resetRejectsInvalidLaneRevisionAndChallenge() {
        try {PanelV4LayoutProtocol.startFreshChallenge(2,1);fail("lane");}
        catch(IllegalArgumentException expected) {}
        try {PanelV4LayoutProtocol.startFreshChallenge(0,-1);fail("revision");}
        catch(IllegalArgumentException expected) {}
        try {PanelV4LayoutProtocol.startFreshConfirm(0,3,0);fail("challenge");}
        catch(IllegalArgumentException expected) {}
        try {PanelV4LayoutProtocol.startFreshConfirm(0,3,0x100000000L);fail("overflow");}
        catch(IllegalArgumentException expected) {}
        assertFalse(PanelV4LayoutProtocol.isFactoryMap(0,null));
    }
}
