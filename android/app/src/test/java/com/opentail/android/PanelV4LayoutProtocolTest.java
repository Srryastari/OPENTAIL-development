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
