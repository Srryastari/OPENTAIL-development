package com.opentail.android;

import java.util.Arrays;

/** Exact, little-endian client codec for the Issue #126 Panel/Zone BLE contract. */
final class PanelPreviewProtocol {
    static final int WIRE_VERSION = 3;
    static final int STATUS_VERSION = 2;
    static final int STATUS_BYTES = 61;
    static final int MAP_BYTES = 667;
    static final int TX_MIN_ATT_MTU = 23;
    static final int OP_BEGIN = 0x01, OP_UPDATE = 0x02, OP_COMMIT = 0x03,
                     OP_CANCEL = 0x04, OP_HEARTBEAT = 0x05;
    static final int ERR_COMMITTING_LOCKED = 5, ERR_SEQUENCE_GAP = 6,
                     ERR_SEQUENCE_OUT_OF_ORDER = 7, ERR_SEQUENCE_DUPLICATE = 8,
                     ERR_SEQUENCE_CONFLICT = 9, ERR_SYNC_REQUIRED = 10;
    static final int STATE_IDLE = 0, STATE_ACTIVE = 1, STATE_COMMITTING = 2,
                     STATE_COMMITTED = 3, STATE_CANCELLED = 4,
                     STATE_EXPIRED = 5, STATE_ERROR = 6;
    static final int FLAG_WARNING = 1, FLAG_HARD_FAULT = 2, FLAG_HAS_SEQUENCE = 4;
    static final int CAP_LIVE_PREVIEW = 1 << 1, CAP_SEQUENCED_PREVIEW = 1 << 2,
                     CAP_STATUS_V2 = 1 << 3;

    private PanelPreviewProtocol() {}

    static final class Capabilities {
        final int wireVersion, schemaVersion, maxPanels, maxZones, maxRanges;
        final int flags, maxPayload, chunkBytes, previewLeaseSeconds, brightnessCap;
        private Capabilities(byte[] b) {
            wireVersion=u8(b[0]); schemaVersion=u8(b[1]); maxPanels=u8(b[2]);
            maxZones=u8(b[3]); maxRanges=u8(b[4]); flags=u8(b[5]);
            maxPayload=u16(b,6); chunkBytes=u8(b[8]);
            previewLeaseSeconds=u8(b[9]); brightnessCap=u8(b[10]);
        }
        static Capabilities parse(byte[] b) {
            if (b==null || b.length!=11) throw new IllegalArgumentException("Panel capability descriptor must be 11 bytes");
            Capabilities c=new Capabilities(b);
            if(c.wireVersion!=WIRE_VERSION || c.schemaVersion<1 ||
               c.maxPanels<1 || c.maxZones<1 || c.maxRanges<1 || c.maxPayload<1 ||
               c.chunkBytes<1 || c.previewLeaseSeconds<1)
                throw new IllegalArgumentException("Unsupported or invalid Panel capability descriptor");
            return c;
        }
        boolean supportsPreviewV2() {
            int required=CAP_LIVE_PREVIEW|CAP_SEQUENCED_PREVIEW|CAP_STATUS_V2;
            return (flags&required)==required;
        }
    }

    static final class Status {
        final int version,state,lane,schemaVersion,baseRevision,pixelCapacity,leaseMs;
        final long sessionId,lastAcceptedSequence,candidateCrc32,activeRevision,
                   stagedRevision,renderRevision,leaseRemainingMs,lastRejectedSequence,
                   missingCount,outOfOrderCount,duplicateCount,conflictCount,
                   committingRejectCount;
        final boolean configPending,persistent,warningObserved,hardSyncFault,hasSequence;
        final int lastError,lastRejectedReason;
        private Status(byte[] b) {
            version=u8(b[0]); state=u8(b[1]); lane=u8(b[2]); schemaVersion=u8(b[3]);
            sessionId=u32(b,4); baseRevision=u16(b,8); pixelCapacity=u16(b,10);
            leaseMs=u16(b,12); lastAcceptedSequence=u32(b,14);
            candidateCrc32=u32(b,18); activeRevision=u16(b,22);
            stagedRevision=u16(b,24); renderRevision=u16(b,26);
            configPending=u8(b[28])!=0; persistent=u8(b[29])!=0;
            lastError=u8(b[30]); leaseRemainingMs=u32(b,31);
            lastRejectedSequence=u32(b,35); lastRejectedReason=u8(b[39]);
            missingCount=u32(b,40); outOfOrderCount=u32(b,44);
            duplicateCount=u32(b,48); conflictCount=u32(b,52);
            committingRejectCount=u32(b,56);
            int flags=u8(b[60]); warningObserved=(flags&FLAG_WARNING)!=0;
            hardSyncFault=(flags&FLAG_HARD_FAULT)!=0;
            hasSequence=(flags&FLAG_HAS_SEQUENCE)!=0;
        }
        static Status parse(byte[] b) {
            if (b==null || b.length!=STATUS_BYTES)
                throw new IllegalArgumentException("Panel status v2 must be exactly 61 bytes");
            Status s=new Status(b);
            if(s.version!=STATUS_VERSION || s.state>STATE_ERROR || s.lane>1 ||
               s.sessionId>0xffffffffL || s.lastAcceptedSequence>0xffffffffL ||
               s.lastRejectedSequence>0xffffffffL || s.leaseRemainingMs>0xffffffffL)
                throw new IllegalArgumentException("Unsupported or malformed Panel status v2");
            if(s.hasSequence != (s.lastAcceptedSequence!=0))
                throw new IllegalArgumentException("Panel status sequence flag disagrees with last accepted sequence");
            return s;
        }
        boolean isCommitting() { return state==STATE_COMMITTING; }
        boolean isTerminal() {
            return state==STATE_COMMITTED || state==STATE_CANCELLED ||
                   state==STATE_EXPIRED || state==STATE_ERROR;
        }
        boolean commitFullyApplied() {
            return state==STATE_COMMITTED && activeRevision==renderRevision &&
                   (activeRevision==baseRevision ||
                    activeRevision==((baseRevision+1L)&0xffffL));
        }

        /** Compact owner-facing sequence evidence for the existing System warning. */
        private static String rejectionReasonName(int code) {
            switch(code) {
                case 0: return "NONE";
                case 1: return "INVALID";
                case 2: return "BUSY";
                case 3: return "STALE";
                case 4: return "INTERNAL";
                case 5: return "COMMITTING_LOCKED";
                case 6: return "SEQUENCE_GAP";
                case 7: return "SEQUENCE_OUT_OF_ORDER";
                case 8: return "SEQUENCE_DUPLICATE";
                case 9: return "SEQUENCE_CONFLICT";
                case 10: return "SYNC_REQUIRED";
                default: return "UNKNOWN_" + code;
            }
        }

        String diagnosticSummary() {
            return "Panel " + (lane+1) + " · session " + sessionId +
                " · accepted " + lastAcceptedSequence +
                " · rejected " + lastRejectedSequence + " (" + rejectionReasonName(lastRejectedReason) + ")" +
                " · last error " + rejectionReasonName(lastError) +
                " · missing " + missingCount + " · out-of-order " + outOfOrderCount +
                " · duplicates " + duplicateCount + " · conflicts " + conflictCount +
                " · COMMITTING rejects " + committingRejectCount +
                (warningObserved ? " · warning latched" : "") +
                (hardSyncFault ? " · hard sync fault" : "");
        }
    }

    static final class SnapshotHeader {
        final int lane,wireLength,crc16,activeRevision,stagedRevision;
        final boolean pending,persistent;
        private SnapshotHeader(byte[] b) {
            lane=u8(b[2]); wireLength=u16(b,4); crc16=u16(b,6);
            activeRevision=u16(b,8); stagedRevision=u16(b,10);
            pending=u8(b[12])!=0; persistent=u8(b[13])!=0;
        }
        static SnapshotHeader parse(byte[] b,int expectedLane) {
            if(b==null || b.length!=14 || u8(b[0])!=WIRE_VERSION ||
               u8(b[1])!=0x81 || u8(b[2])!=expectedLane || u8(b[3])!=0)
                throw new IllegalArgumentException("Invalid authoritative Panel snapshot header");
            SnapshotHeader h=new SnapshotHeader(b);
            if(h.wireLength!=MAP_BYTES)
                throw new IllegalArgumentException("Unexpected Panel map wire length");
            return h;
        }
        void verify(byte[] wire,int expectedLane) {
            if(wire==null || wire.length!=MAP_BYTES || u8(wire[0])!=WIRE_VERSION ||
               u8(wire[1])!=expectedLane || crc16(wire)!=crc16)
                throw new IllegalArgumentException("Authoritative Panel map failed lane/version/CRC verification");
        }
    }

    static final class LaneSnapshot {
        final int lane,activeRevision,stagedRevision,crc16;
        final boolean pending,persistent;
        final byte[] wire;
        LaneSnapshot(SnapshotHeader h,byte[] bytes) {
            lane=h.lane;activeRevision=h.activeRevision;stagedRevision=h.stagedRevision;
            crc16=h.crc16;pending=h.pending;persistent=h.persistent;
            wire=Arrays.copyOf(bytes,bytes.length);
        }
    }

    static final class UpdateDescriptor {
        final int panelId,zoneId,rangeSlot,start,count,roleFlags;
        UpdateDescriptor(int panelId,int zoneId,int rangeSlot,int start,int count,int roleFlags) {
            if(panelId<0||panelId>255||zoneId<0||zoneId>255||rangeSlot<0||rangeSlot>255||
               start<0||start>65535||count<0||count>65535||roleFlags<0||roleFlags>255)
                throw new IllegalArgumentException("Panel UPDATE descriptor value out of range");
            if((roleFlags&0xc0)!=0)throw new IllegalArgumentException("Reserved role/action bits must be zero");
            this.panelId=panelId;this.zoneId=zoneId;this.rangeSlot=rangeSlot;
            this.start=start;this.count=count;this.roleFlags=roleFlags;
        }
        byte[] encode() {
            return new byte[]{(byte)panelId,(byte)zoneId,(byte)rangeSlot,
                (byte)start,(byte)(start>>>8),(byte)count,(byte)(count>>>8),(byte)roleFlags};
        }
    }

    static byte[] begin(int lane,int baseRevision,int capacity,int schemaVersion) {
        checkLane(lane);
        if(baseRevision<1||baseRevision>65534||capacity<1||capacity>65535||
           schemaVersion<1||schemaVersion>255)throw new IllegalArgumentException("Invalid Panel BEGIN values");
        return new byte[]{(byte)0xfe,(byte)OP_BEGIN,(byte)lane,
            (byte)baseRevision,(byte)(baseRevision>>>8),(byte)capacity,
            (byte)(capacity>>>8),(byte)schemaVersion};
    }
    static byte[] commit(int lane,long sessionId,long candidateCrc32) {
        checkLane(lane); checkU32(sessionId); checkU32(candidateCrc32);
        if(sessionId==0)throw new IllegalArgumentException("Session ID zero is reserved");
        byte[] p=new byte[11];p[0]=(byte)0xfe;p[1]=(byte)OP_COMMIT;p[2]=(byte)lane;
        putU32(p,3,sessionId);putU32(p,7,candidateCrc32);return p;
    }
    static byte[] cancel(int lane,long sessionId) { return control(OP_CANCEL,lane,sessionId); }
    static byte[] heartbeat(int lane,long sessionId) { return control(OP_HEARTBEAT,lane,sessionId); }
    private static byte[] control(int opcode,int lane,long sessionId) {
        checkLane(lane);checkU32(sessionId);
        if(sessionId==0)throw new IllegalArgumentException("Session ID zero is reserved");
        byte[] p=new byte[7];p[0]=(byte)0xfe;p[1]=(byte)opcode;p[2]=(byte)lane;
        putU32(p,3,sessionId);return p;
    }
    static byte[] update(int lane,long sessionId,long sequence,UpdateDescriptor[] descriptors,int mtu) {
        checkLane(lane);checkU32(sessionId);checkU32(sequence);
        if(sessionId==0||sequence==0)throw new IllegalArgumentException("Session and sequence zero are reserved");
        if(descriptors==null||descriptors.length<1||descriptors.length>4)
            throw new IllegalArgumentException("UPDATE requires one to four descriptors");
        if(mtu<23 || 11+8*descriptors.length>mtu-3)
            throw new IllegalArgumentException("UPDATE does not fit the negotiated ATT MTU");
        byte[] p=new byte[11+8*descriptors.length];p[0]=(byte)0xfe;p[1]=(byte)OP_UPDATE;p[2]=(byte)lane;
        putU32(p,3,sessionId);putU32(p,7,sequence);
        for(int i=0;i<descriptors.length;i++)
            System.arraycopy(descriptors[i].encode(),0,p,11+i*8,8);
        return p;
    }

    static int crc16(byte[] bytes) {
        int crc=0xffff;
        for(byte x:bytes){crc^=(x&255)<<8;for(int i=0;i<8;i++)
            crc=((crc&0x8000)!=0)?((crc<<1)^0x1021)&0xffff:(crc<<1)&0xffff;}
        return crc;
    }
    static boolean sameStatusIdentity(Status a,Status b) {
        return a!=null&&b!=null&&a.lane==b.lane&&a.sessionId==b.sessionId;
    }
    static boolean sameBytes(byte[] a,byte[] b) { return Arrays.equals(a,b); }
    static int u8(byte b) { return b&255; }
    static int u16(byte[] b,int at) { return u8(b[at])|(u8(b[at+1])<<8); }
    static long u32(byte[] b,int at) {
        return ((long)u8(b[at]))|((long)u8(b[at+1])<<8)|
               ((long)u8(b[at+2])<<16)|((long)u8(b[at+3])<<24);
    }
    static void putU32(byte[] b,int at,long value) {
        checkU32(value);for(int i=0;i<4;i++)b[at+i]=(byte)(value>>>(8*i));
    }
    private static void checkU32(long v) {
        if(v<0||v>0xffffffffL)throw new IllegalArgumentException("Expected uint32");
    }
    private static void checkLane(int lane) {
        if(lane<0||lane>1)throw new IllegalArgumentException("Lane must be 0 or 1");
    }
}
