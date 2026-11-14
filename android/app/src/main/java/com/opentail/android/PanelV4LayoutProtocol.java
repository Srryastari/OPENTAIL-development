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

    /** Firmware Panel CFG opcode 3: begin a revision-bound immutable upload. */
    static byte[] beginSave(int lane,int baseRevision,byte[] wire) {
        lane(lane);revision(baseRevision);validateWireLane(lane,wire);
        int crc=PanelPreviewProtocol.crc16(wire);
        return new byte[]{3,3,(byte)lane,(byte)MAP_BYTES,(byte)(MAP_BYTES>>>8),
            (byte)crc,(byte)(crc>>>8),(byte)baseRevision,(byte)(baseRevision>>>8)};
    }

    /** Firmware Panel CFG opcode 4: exactly one 1..15 byte chunk. */
    static byte[] saveChunk(int lane,byte[] wire,int offset) {
        lane(lane);validateWireLane(lane,wire);
        if(offset<0||offset>=MAP_BYTES)
            throw new IllegalArgumentException("Invalid Panel upload offset");
        int count=Math.min(15,MAP_BYTES-offset);
        byte[] packet=new byte[5+count];
        packet[0]=3;packet[1]=4;packet[2]=(byte)lane;
        packet[3]=(byte)offset;packet[4]=(byte)(offset>>>8);
        System.arraycopy(wire,offset,packet,5,count);
        return packet;
    }

    static byte[] completeSave(int lane) {
        lane(lane);return new byte[]{3,5,(byte)lane};
    }

    static byte[] cancelUncommittedSave(int lane) {
        lane(lane);return new byte[]{3,6,(byte)lane};
    }

    /** A 4-byte write acknowledgement, never proof of durable Save. */
    static int parseSaveAck(int lane,int opcode,byte[] ack) {
        lane(lane);
        if(opcode<3||opcode>6||ack==null||ack.length!=4||
           (ack[0]&255)!=3||(ack[1]&255)!=(0x80|opcode)||
           (ack[2]&255)!=lane)
            throw new IllegalArgumentException("Stale or invalid Panel Save ACK");
        return ack[3]&255;
    }

    /** Firmware #127 validated-save responses. ATT ACK alone never means
     * the journal was promoted. Do not auto-retry an uncertain Save. */
    static String saveRejectionMessage(int code) {
        switch(code) {
            case 1:return "Invalid Panel v4 packet; check the selected output and reconnect.";
            case 2:return "Controller busy or another save/override is active. Keep the live layout; retry only after a fresh status read.";
            case 3:return "Saved revision is stale or the live session expired. Reconnect and reconcile the selected output.";
            case 4:return "Flash journal did not confirm persistence. Reconnect and verify readback before retrying.";
            case 5:return "Controller authorization is missing; authenticate before saving.";
            case 6:return "Panel upload checksum mismatch. Refresh the connection and verify the draft.";
            case 7:return "Invalid Panel v4 layout. Fix panel identities, ranges, counts or metadata.";
            case 8:return "Panel upload incomplete; reconcile before starting another Save.";
            case 9:return "Panel transaction rejected; refresh authoritative status before retry.";
            case 10:return "Live mapping has unaccepted geometry or stable IDs. Finish the physical preview edits before Save; no changes were persisted.";
            default:return "Unknown Panel v4 rejection ("+code+"); reconcile before retrying.";
        }
    }

    private static void validateWireLane(int lane,byte[] wire) {
        if(wire==null||wire.length!=MAP_BYTES||
           (wire[0]&255)!=WIRE_VERSION||(wire[1]&255)!=lane)
            throw new IllegalArgumentException("Invalid or cross-lane Panel v4 wire");
    }

    static byte[] startFreshChallenge(int lane,int confirmedRevision) {
        lane(lane);revision(confirmedRevision);
        return new byte[]{3,(byte)0xff,(byte)0xaa,(byte)lane,
            (byte)confirmedRevision,(byte)(confirmedRevision >>> 8)};
    }

    /** Firmware returns a 12-byte owner-authenticated lane/revision challenge. */
    static long parseStartFreshChallenge(int lane,int revision,byte[] response) {
        lane(lane);revision(revision);
        if(response==null||response.length!=12||
           (response[0]&255)!=3||(response[1]&255)!=255||
           (response[2]&255)!=170||(response[3]&255)!=lane||
           ((response[8]&255)|((response[9]&255)<<8))!=revision)
            throw new IllegalArgumentException("Stale or wrong-lane Panel reset challenge");
        int ttl=(response[10]&255)|((response[11]&255)<<8);
        if(ttl!=5000)throw new IllegalArgumentException("Invalid reset challenge lease");
        long challenge=(response[4]&255L)|((response[5]&255L)<<8)|
            ((response[6]&255L)<<16)|((response[7]&255L)<<24);
        if(challenge==0)throw new IllegalArgumentException("Invalid zero reset challenge");
        return challenge;
    }

    static int parseStartFreshAck(int lane,byte[] response) {
        lane(lane);
        if(response==null||response.length!=4||
           (response[0]&255)!=3||(response[1]&255)!=255||
           (response[2]&255)!=lane)
            throw new IllegalArgumentException("Invalid reset confirmation ACK");
        return response[3]&255;
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
