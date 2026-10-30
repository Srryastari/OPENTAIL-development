package com.opentail.android;

import java.util.Arrays;

/**
 * Pure wire codec for CORE trigger team's optional authenticated GATT v1
 * characteristic 55a56022-4915-7a92-cf44-b8b7102eaf2c.
 * Logical IDs (0..8) are functions, never GPIO numbers.
 */
final class NativeTriggerMappingCodec {
    static final int VERSION = 1;
    static final int INPUTS = 8;
    static final int READ_BYTES = 15;
    static final int WRITE_BYTES = 11;

    static final class Snapshot {
        final int status;
        final int revision;
        final int rawPhysicalMask;
        final int effectiveMask;
        final boolean persisted;
        final int[] functions;

        private Snapshot(int status, int revision, int physical, int effective,
                         boolean persisted, int[] functions) {
            this.status = status;
            this.revision = revision;
            this.rawPhysicalMask = physical;
            this.effectiveMask = effective;
            this.persisted = persisted;
            this.functions = functions.clone();
        }
        boolean matches(int[] expected) {
            return Arrays.equals(functions, expected);
        }
        /** ATT success is not a save acknowledgment: CORE must report a
         * newer revision, the exact logical map, and active persistence. */
        boolean confirmsApplied(int baseRevision, int[] expected) {
            return status == 0 && persisted &&
                revision == ((baseRevision + 1) & 0xffff) && matches(expected);
        }
    }

    static Snapshot decode(byte[] wire) {
        if (wire == null || wire.length != READ_BYTES ||
            (wire[0] & 255) != VERSION || (wire[1] & 255) > 4 ||
            (wire[6] & 255) > 1) throw new IllegalArgumentException("Invalid CORE trigger map response");
        int[] mapping = new int[INPUTS];
        for (int i = 0; i < INPUTS; i++) {
            mapping[i] = wire[7 + i] & 255;
            if (mapping[i] > 8) throw new IllegalArgumentException("Unknown trigger function");
        }
        return new Snapshot(wire[1] & 255, (wire[2] & 255) | ((wire[3] & 255) << 8),
            wire[4] & 255, wire[5] & 255, wire[6] == 1, mapping);
    }

    static byte[] encode(int expectedRevision, int[] functions) {
        if (expectedRevision < 0 || expectedRevision > 65535 ||
            functions == null || functions.length != INPUTS)
            throw new IllegalArgumentException("Invalid trigger mapping request");
        byte[] wire = new byte[WRITE_BYTES];
        wire[0] = VERSION;
        wire[1] = (byte)expectedRevision;
        wire[2] = (byte)(expectedRevision >>> 8);
        for (int i = 0; i < INPUTS; i++) {
            if (functions[i] < 0 || functions[i] > 8)
                throw new IllegalArgumentException("Unknown trigger function");
            wire[3 + i] = (byte)functions[i];
        }
        return wire;
    }
    private NativeTriggerMappingCodec() {}
}
