package com.opentail.android;

import org.junit.Test;
import static org.junit.Assert.*;

/** Byte-level compatibility with docs/TRIGGER-BLE-CONTRACT-V1.md on trigger PR #24. */
public class NativeTriggerMappingCodecTest {
    @Test public void decodesExactFifteenByteReadbackIncludingAuxAndPendingApply() {
        byte[] packet = {1, 0, 0x34, 0x12, (byte)0xa5, (byte)0xe7, 0,
                         1, 2, 3, 4, 5, 6, 7, 8};
        NativeTriggerMappingCodec.Snapshot response = NativeTriggerMappingCodec.decode(packet);
        assertEquals(0x1234, response.revision);
        assertEquals(0xa5, response.rawPhysicalMask);
        assertEquals(0xe7, response.effectiveMask);
        assertFalse(response.persisted); // staged until asserted inputs release
        assertArrayEquals(new int[]{1,2,3,4,5,6,7,8}, response.functions);
        assertTrue(response.matches(new int[]{1,2,3,4,5,6,7,8}));
    }

    @Test public void encodesExactlyElevenBytesWithRevisionAndDuplicateFunctions() {
        int[] functions = {0, 2, 2, 3, 4, 5, 6, 8};
        byte[] request = NativeTriggerMappingCodec.encode(0x1234, functions);
        assertArrayEquals(new byte[]{1, 0x34, 0x12, 0, 2, 2, 3, 4, 5, 6, 8}, request);
        assertEquals(11, request.length);
        request[4] = 7;
        assertEquals(2, functions[1]); // never mutate the caller's model
    }

    @Test public void rejectsMalformedVersionsAndInvalidFunctionIds() {
        assertThrows(IllegalArgumentException.class, () -> NativeTriggerMappingCodec.decode(new byte[14]));
        byte[] good = {1, 0, 1, 0, 0, 0, 1, 1,2,3,4,5,6,7,8};
        final byte[] invalidVersion = good.clone();invalidVersion[0] = 2;
        assertThrows(IllegalArgumentException.class, () -> NativeTriggerMappingCodec.decode(invalidVersion));
        final byte[] invalidStatus = good.clone();invalidStatus[1] = 5;
        assertThrows(IllegalArgumentException.class, () -> NativeTriggerMappingCodec.decode(invalidStatus));
        final byte[] invalidId = good.clone();invalidId[14] = 9;
        assertThrows(IllegalArgumentException.class, () -> NativeTriggerMappingCodec.decode(invalidId));
        assertThrows(IllegalArgumentException.class, () -> NativeTriggerMappingCodec.encode(-1, new int[8]));
        assertThrows(IllegalArgumentException.class, () -> NativeTriggerMappingCodec.encode(65536, new int[8]));
        assertThrows(IllegalArgumentException.class, () -> NativeTriggerMappingCodec.encode(65535, new int[8]));
        assertThrows(IllegalArgumentException.class, () -> NativeTriggerMappingCodec.encode(0, new int[8]));
        assertThrows(IllegalArgumentException.class, () -> NativeTriggerMappingCodec.encode(1, new int[7]));
        int[] bad = new int[8]; bad[6] = 9;
        assertThrows(IllegalArgumentException.class, () -> NativeTriggerMappingCodec.encode(1, bad));
    }

    @Test public void requiresAuthoritativeAppliedReadbackNotJustAttWriteAck() {
        int[] wanted = {1,2,3,4,5,6,7,8};
        byte[] read = {1,0,2,0,0,0,1,1,2,3,4,5,6,7,8};
        assertTrue(NativeTriggerMappingCodec.decode(read).confirmsApplied(1,wanted));
        read[1] = 3; // stale edit rejected
        assertFalse(NativeTriggerMappingCodec.decode(read).confirmsApplied(1,wanted));
        read[1] = 0;read[6] = 0; // persisted, but not yet active
        assertFalse(NativeTriggerMappingCodec.decode(read).confirmsApplied(1,wanted));
        read[6] = 1;read[2] = 1; // unchanged revision
        assertFalse(NativeTriggerMappingCodec.decode(read).confirmsApplied(1,wanted));
        read[2] = 2;read[14] = 7; // revision advanced, wrong mapping
        assertFalse(NativeTriggerMappingCodec.decode(read).confirmsApplied(1,wanted));
        // Firmware #49 rejects exhausted tokens instead of recycling 65535 -> 0.
        read[14] = 8;read[2] = 0;read[3] = 0;
        assertFalse(NativeTriggerMappingCodec.decode(read).confirmsApplied(65535,wanted));
        assertFalse(NativeTriggerMappingCodec.decode(read).confirmsApplied(0,wanted));
    }

    @Test public void exposesBusyStaleAndNvsFailureAsFailuresNotSuccess() {
        byte[] wire = {1, 3, 2, 0, 0, 0, 1, 1,2,3,4,5,6,7,8};
        assertEquals(3, NativeTriggerMappingCodec.decode(wire).status);
        wire[1] = 4;
        assertEquals(4, NativeTriggerMappingCodec.decode(wire).status);
        wire[1] = 2;
        assertEquals(2, NativeTriggerMappingCodec.decode(wire).status);
    }
}
