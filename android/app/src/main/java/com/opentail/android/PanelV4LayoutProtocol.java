package com.opentail.android;

import java.nio.charset.StandardCharsets;
import java.util.Arrays;

/**
 * Owner-authorized Panel v4 durable-layout contract from Firmware #127.
 *
 * This is intentionally NOT the legacy BleConfigCodec/Output v1 format.
 * Live-preview COMMIT (0xFE) must never be mistaken for a durable Panel
 * configuration transaction or the authenticated Start Fresh reset.
 */
final class PanelV4LayoutProtocol {
    static final int WIRE_VERSION = 3;
    static final int MAP_BYTES = 667;
    static final int MAX_PIXELS_PER_OUTPUT = 2000;
    static final int RESET_REQUEST_BYTES = 6;
    static final int RESET_CONFIRM_BYTES = 10;

    private PanelV4LayoutProtocol() {}

    private static void lane(int lane) {
        if(lane < 0 || lane > 1) throw new IllegalArgumentException("Invalid Panel output lane");
    }
    private static void revision(int revision) {
        if(revision < 0 || revision > 65535)
            throw new IllegalArgumentException("Invalid authoritative Panel revision");
    }

    static byte[] startFreshChallenge(int lane,int confirmedRevision) {
        lane(lane);revision(confirmedRevision);
        return new byte[]{3,(byte)0xff,(byte)0xaa,(byte)lane,
            (byte)confirmedRevision,(byte)(confirmedRevision >>> 8)};
    }

    static byte[] startFreshConfirm(int lane,int confirmedRevision,long challenge) {
        lane(lane);revision(confirmedRevision);
        if(challenge <= 0 || challenge > 0xffffffffL)
            throw new IllegalArgumentException("Invalid or expired reset challenge");
        return new byte[]{3,(byte)0xff,(byte)0xab,(byte)lane,
            (byte)challenge,(byte)(challenge >>> 8),
            (byte)(challenge >>> 16),(byte)(challenge >>> 24),
            (byte)confirmedRevision,(byte)(confirmedRevision >>> 8)};
    }

    /**
     * Empty factory layout for comparison with authoritative readback.
     * NEVER transmit it as a reset or infer successful persistence from it.
     * Firmware owns the journaled reset, its challenge and full readback.
     */
    static byte[] expectedFactoryMap(int lane) {
        lane(lane);
        byte[] map=new byte[MAP_BYTES];
        map[0]=WIRE_VERSION;
        map[1]=(byte)lane;
        // bytes 2..3: active pixels = 0; byte 4: GRB = 2.
        map[4]=2;
        byte[] name=("Pixel OUT "+(lane+1)).getBytes(StandardCharsets.UTF_8);
        System.arraycopy(name,0,map,5,name.length);
        // Firmware's six RGB/white gains are 255; gamma and counts remain zero.
        Arrays.fill(map,30,36,(byte)0xff);
        return map;
    }

    static boolean isFactoryMap(int lane,byte[] confirmedWire) {
        return confirmedWire!=null&&Arrays.equals(expectedFactoryMap(lane),confirmedWire);
    }
}
