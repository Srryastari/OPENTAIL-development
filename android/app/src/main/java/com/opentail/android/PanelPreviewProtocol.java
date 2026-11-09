package com.opentail.android;

import java.util.Arrays;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import java.util.List;

/** Exact, little-endian client codec for the Issue #126 Panel/Zone BLE contract. */
final class PanelPreviewProtocol {
    static final int WIRE_VERSION = 3;
    static final int STATUS_VERSION = 2;
    static final int STATUS_BYTES = 61;
    static final int MAP_BYTES = 667;
    static final int TX_MIN_ATT_MTU = 23;
    // Firmware's four-bit UPDATE role sentinel for OT_PZ_ROLE_UNASSIGNED.
    // PR #127 must normalize 0x0f to 0xff before accepting/storing the role.
    static final int ROLE_UNASSIGNED_WIRE = 0x0f;
    // Firmware PR #127 exact HEAD f646b399 normalizes this sentinel; host proof PASS.
    static final boolean CORE_UNASSIGNED_ROLE_UPDATE_SUPPORTED = true;
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

    static final class MapRange {
        final int start,count;
        MapRange(int start,int count){this.start=start;this.count=count;}
    }
    static final class MapPanel {
        final int id,lane,zoneCount;
        final String name;
        MapPanel(int id,int lane,String name,int zoneCount) {
            this.id=id;this.lane=lane;this.name=name;this.zoneCount=zoneCount;
        }
    }
    static final class MapZone {
        final int id,panelId,role,flags;
        final String name;
        final List<MapRange> ranges;
        MapZone(int id,int panelId,int role,int flags,String name,List<MapRange> ranges) {
            this.id=id;this.panelId=panelId;this.role=role;this.flags=flags;this.name=name;
            this.ranges=Collections.unmodifiableList(new ArrayList<>(ranges));
        }
    }
    static final class MapIdentity {
        final int lane,activePixels;
        final List<MapPanel> panels;
        final List<MapZone> zones;
        private MapIdentity(int lane,int activePixels,List<MapPanel> panels,List<MapZone> zones) {
            this.lane=lane;this.activePixels=activePixels;
            this.panels=Collections.unmodifiableList(new ArrayList<>(panels));
            this.zones=Collections.unmodifiableList(new ArrayList<>(zones));
        }
        static MapIdentity parse(LaneSnapshot snapshot) {
            if(snapshot==null||snapshot.wire==null||snapshot.wire.length!=MAP_BYTES||
               crc16(snapshot.wire)!=snapshot.crc16)
                throw new IllegalArgumentException("Panel map snapshot is missing or failed CRC verification");
            byte[] b=snapshot.wire;
            if(u8(b[0])!=WIRE_VERSION||u8(b[1])!=snapshot.lane)
                throw new IllegalArgumentException("Panel map version/lane mismatch");
            int active=u16(b,2),panelCount=u8(b[37]),zoneCount=u8(b[38]);
            if(panelCount>4||zoneCount>8||active>2048||
               (active==0&&(panelCount!=0||zoneCount!=0)))
                throw new IllegalArgumentException("Panel map counts or active pixel length are invalid");
            List<MapPanel> panels=new ArrayList<>();
            List<MapZone> zones=new ArrayList<>();
            Map<Integer,Integer> panelAliases=new HashMap<>();
            Map<Integer,Integer> zoneAliases=new HashMap<>();
            int panelBase=39;
            for(int i=0;i<4;i++) {
                int at=panelBase+i*29,id=u16(b,at),lane=u8(b[at+2]);
                String name=wireName(b,at+3,25);
                int count=u8(b[at+28]);
                if(i>=panelCount) {
                    if(id!=0||lane!=0||count!=0||!name.isEmpty())
                        throw new IllegalArgumentException("Unused Panel map slot is not zero-filled");
                    continue;
                }
                if(id==0||lane!=snapshot.lane||name.isEmpty()||count>8)
                    throw new IllegalArgumentException("Malformed Panel identity in authoritative map");
                for(MapPanel prior:panels)if(prior.id==id)
                    throw new IllegalArgumentException("Duplicate Panel ID in authoritative map");
                int alias=id&0xff;
                if(alias==0||panelAliases.put(alias,id)!=null)
                    throw new IllegalArgumentException("Ambiguous Panel low-byte alias in authoritative map");
                panels.add(new MapPanel(id,lane,name,count));
            }
            int zoneBase=155;
            int[] actualPanelCounts=new int[panels.size()];
            for(int i=0;i<8;i++) {
                int at=zoneBase+i*64,id=u16(b,at),panelId=u16(b,at+2);
                String name=wireName(b,at+4,25);
                int role=u8(b[at+29]),flags=u8(b[at+30]),rangeCount=u8(b[at+31]);
                if(i>=zoneCount) {
                    boolean nonzero=id!=0||panelId!=0||!name.isEmpty()||role!=0||flags!=0||rangeCount!=0;
                    for(int r=0;r<8;r++)nonzero|=u16(b,at+32+r*4)!=0||u16(b,at+34+r*4)!=0;
                    if(nonzero)throw new IllegalArgumentException("Unused Zone map slot is not zero-filled");
                    continue;
                }
                if(id==0||panelId==0||name.isEmpty()||rangeCount>8||
                   (role>10&&role!=0xff)||(flags&~1)!=0||
                   (rangeCount==0?flags!=1:flags!=0))
                    throw new IllegalArgumentException("Malformed Zone identity in authoritative map");
                for(MapZone prior:zones)if(prior.id==id)
                    throw new IllegalArgumentException("Duplicate Zone ID in authoritative map");
                int alias=id&0xff;
                if(alias==0||zoneAliases.put(alias,id)!=null)
                    throw new IllegalArgumentException("Ambiguous Zone low-byte alias in authoritative map");
                int panelIndex=-1;
                for(int j=0;j<panels.size();j++)if(panels.get(j).id==panelId)panelIndex=j;
                if(panelIndex<0)throw new IllegalArgumentException("Zone references missing Panel identity");
                actualPanelCounts[panelIndex]++;
                List<MapRange> ranges=new ArrayList<>();
                for(int r=0;r<8;r++) {
                    int start=u16(b,at+32+r*4),count=u16(b,at+34+r*4);
                    if(r>=rangeCount) {
                        if(start!=0||count!=0)throw new IllegalArgumentException("Unused Zone range is not zero-filled");
                    } else {
                        if(count==0||(long)start+count>active)
                            throw new IllegalArgumentException("Zone range exceeds active output pixels");
                        ranges.add(new MapRange(start,count));
                    }
                }
                zones.add(new MapZone(id,panelId,role,flags,name,ranges));
            }
            for(int i=0;i<panels.size();i++)
                if(panels.get(i).zoneCount!=actualPanelCounts[i])
                    throw new IllegalArgumentException("Panel zone count disagrees with authoritative map");
            for(int i=0;i<zones.size();i++)for(int a=0;a<zones.get(i).ranges.size();a++) {
                MapRange x=zones.get(i).ranges.get(a);
                for(int j=i;j<zones.size();j++)for(int r=(j==i?a+1:0);r<zones.get(j).ranges.size();r++) {
                    MapRange y=zones.get(j).ranges.get(r);
                    if(x.start<y.start+y.count&&y.start<x.start+x.count)
                        throw new IllegalArgumentException("Authoritative Panel ranges overlap");
                }
            }
            return new MapIdentity(snapshot.lane,active,panels,zones);
        }
        private static String wireName(byte[] b,int at,int length) {
            StringBuilder value=new StringBuilder();
            boolean terminated=false;
            for(int i=0;i<length;i++) {
                int ch=u8(b[at+i]);
                if(ch==0){
                    terminated=true;
                    for(int tail=i+1;tail<length;tail++)
                        if(u8(b[at+tail])!=0)
                            throw new IllegalArgumentException("Panel map name has nonzero bytes after terminator");
                    break;
                }
                if(ch<32||ch>126)throw new IllegalArgumentException("Panel map name is not printable ASCII");
                value.append((char)ch);
            }
            if(!terminated)throw new IllegalArgumentException("Panel map name is not NUL-terminated");
            return value.toString();
        }
    }

    static final class UpdateDescriptor {
        /* IDs remain authoritative uint16 values in Android. The eight-byte
         * UPDATE wire carries only each ID's unique low-byte alias. */
        final int panelId,zoneId,rangeSlot,start,count,roleFlags;
        UpdateDescriptor(int panelId,int zoneId,int rangeSlot,int start,int count,int roleFlags) {
            if(panelId<0||panelId>65535||zoneId<0||zoneId>65535||rangeSlot<0||rangeSlot>255||
               start<0||start>65535||count<0||count>65535||roleFlags<0||roleFlags>255)
                throw new IllegalArgumentException("Panel UPDATE descriptor value out of range");
            if((roleFlags&0xc0)!=0)throw new IllegalArgumentException("Reserved role/action bits must be zero");
            int action=(roleFlags&0x30)>>>4;
            if((panelId&0xff)==0||(action==3?zoneId!=0:(zoneId&0xff)==0))
                throw new IllegalArgumentException("Panel UPDATE identity has a reserved zero low-byte alias");
            this.panelId=panelId;this.zoneId=zoneId;this.rangeSlot=rangeSlot;
            this.start=start;this.count=count;this.roleFlags=roleFlags;
        }
        byte[] encode() {
            return new byte[]{(byte)(panelId&0xff),(byte)(zoneId&0xff),(byte)rangeSlot,
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
