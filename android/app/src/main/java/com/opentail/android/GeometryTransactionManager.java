package com.opentail.android;

import java.util.Arrays;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import org.json.JSONObject;

/**
 * Protocol-independent guard for Layout geometry edits.
 * The confirmed image is never replaced by a preview or unverified commit.
 */
final class GeometryTransactionManager {
    enum State { READY, BEGINNING, ACTIVE, UPDATE_PENDING, COMMIT_REQUESTED,
                 COMMITTING, RECONCILING, ERROR }

    static final class Snapshot {
        final int lane,revision;
        final byte[] wire;
        Snapshot(int lane,int revision,byte[] wire) {
            if(lane<0||lane>1||revision<0||revision>65535||wire==null)
                throw new IllegalArgumentException("Invalid authoritative geometry snapshot");
            this.lane=lane;this.revision=revision;this.wire=Arrays.copyOf(wire,wire.length);
        }
    }
    static final class Update {
        final long sequence;
        final byte[] packet;
        Update(long sequence,byte[] packet) {
            this.sequence=sequence;this.packet=Arrays.copyOf(packet,packet.length);
        }
    }

    /** Explicit Panel/Zone translation; no implicit housing or trunk inference. */
    static final class MappedPanel {
        final int id,lane,zoneCount;
        final String name;
        MappedPanel(int id,int lane,String name,int zoneCount) {
            this.id=id;this.lane=lane;this.name=name;this.zoneCount=zoneCount;
        }
    }
    static final class MappedZone {
        final int id,panelId,section,start,count,roleFlags;
        final String name;
        MappedZone(int id,int panelId,int section,int start,int count,String name) {
            this.id=id;this.panelId=panelId;this.section=section;this.start=start;
            this.count=count;this.name=name;
            this.roleFlags=PanelPreviewProtocol.ROLE_UNASSIGNED_WIRE;
        }
        PanelPreviewProtocol.UpdateDescriptor descriptor() {
            return new PanelPreviewProtocol.UpdateDescriptor(panelId,id,0,start,count,roleFlags);
        }
    }
    static final class MappedLane {
        final int lane,activePixels;
        final List<MappedPanel> panels;
        final List<MappedZone> zones;
        MappedLane(int lane,int activePixels,List<MappedPanel> panels,List<MappedZone> zones) {
            this.lane=lane;this.activePixels=activePixels;
            this.panels=Collections.unmodifiableList(new ArrayList<>(panels));
            this.zones=Collections.unmodifiableList(new ArrayList<>(zones));
        }
        PanelPreviewProtocol.UpdateDescriptor[] descriptors() {
            PanelPreviewProtocol.UpdateDescriptor[] out=
                new PanelPreviewProtocol.UpdateDescriptor[zones.size()];
            for(int i=0;i<zones.size();i++)out[i]=zones.get(i).descriptor();
            return out;
        }

        /** Split descriptors to the exact negotiated ATT write-command budget. */
        List<PanelPreviewProtocol.UpdateDescriptor[]> descriptorBatches(int mtu) {
            if(mtu<PanelPreviewProtocol.TX_MIN_ATT_MTU)
                throw new IllegalArgumentException("ATT MTU is below the BLE minimum");
            int perPacket=Math.min(4,(mtu-3-11)/8);
            if(perPacket<1)
                throw new IllegalArgumentException("ATT MTU cannot carry one Panel UPDATE descriptor");
            List<PanelPreviewProtocol.UpdateDescriptor[]> batches=new ArrayList<>();
            PanelPreviewProtocol.UpdateDescriptor[] all=descriptors();
            for(int offset=0;offset<all.length;offset+=perPacket) {
                int count=Math.min(perPacket,all.length-offset);
                batches.add(Arrays.copyOfRange(all,offset,offset+count));
            }
            return Collections.unmodifiableList(batches);
        }
    }
    static final class LayoutMapping {
        final MappedLane[] lanes;
        LayoutMapping(MappedLane[] lanes) { this.lanes=lanes; }
        MappedLane lane(int lane) {
            if(lane<0||lane>=lanes.length)throw new IllegalArgumentException("Invalid output lane");
            return lanes[lane];
        }
    }

    /**
     * Maps existing local Output/section links to CORE's hierarchical schema.
     * Panel IDs/names must be explicit project metadata; a design zone is never
     * guessed to be a housing or trunk Panel. Existing UI structure is unchanged.
     * Current Layout sections are contiguous single-range zones; multi-range
     * authoring remains disabled until the UI has explicit range metadata.
     */
    private static boolean isExactGs400Layout(List<JSONObject>[] assigned) {
        int[] expected={118,109,42,253,101};
        for(int lane=0;lane<2;lane++) {
            if(assigned[lane].size()!=5)return false;
            List<JSONObject> sections=new ArrayList<>(assigned[lane]);
            Collections.sort(sections,(a,b)->Integer.compare(
                a.optInt("physicalSection",-1),b.optInt("physicalSection",-1)));
            for(int i=0;i<expected.length;i++)
                if(sections.get(i).optInt("physicalSection",-1)!=i||
                   sections.get(i).optInt("length",-1)!=expected[i])return false;
        }
        return true;
    }

    static LayoutMapping mapLayout(NativeProject project) {
        if(project==null)throw new IllegalArgumentException("Layout project is required");
        @SuppressWarnings("unchecked")
        List<JSONObject>[] assigned=new List[]{new ArrayList<>(),new ArrayList<>()};
        boolean anyExplicitPanel=false,allExplicitPanel=true,partialPanelMetadata=false;
        for(int i=0;i<project.zoneCount();i++) {
            JSONObject zone=project.zone(i);
            int lane=zone.optInt("physicalOutput",-1);
            if(lane<0)continue;
            if(lane>1)throw new IllegalArgumentException("Physical output is outside the two-lane contract");
            boolean hasId=zone.has("physicalPanelId"),hasName=zone.has("physicalPanelName");
            boolean explicit=hasId&&hasName;
            partialPanelMetadata|=hasId!=hasName;
            anyExplicitPanel|=explicit;
            allExplicitPanel&=explicit;
            assigned[lane].add(zone);
        }
        if(partialPanelMetadata)
            throw new IllegalStateException("Panel identity requires both ID and name");
        boolean exactGs400Profile=!anyExplicitPanel&&isExactGs400Layout(assigned);
        if(anyExplicitPanel&&!allExplicitPanel)
            throw new IllegalStateException("Every assigned section must have an explicit CORE Panel identity");
        if(!anyExplicitPanel&&!exactGs400Profile)
            throw new IllegalStateException("Layout has no explicit CORE Panel identity and does not match the exact owner-approved GS400 section map");
        MappedLane[] lanes=new MappedLane[2];
        Map<Integer,Integer> globalPanelLanes=new HashMap<>();
        for(int lane=0;lane<2;lane++) {
            List<JSONObject> sections=assigned[lane];
            Collections.sort(sections,(a,b)->Integer.compare(
                a.optInt("physicalSection",-1),b.optInt("physicalSection",-1)));
            if(sections.size()>8)
                throw new IllegalArgumentException("Output "+(lane+1)+" exceeds CORE's eight-zone limit");
            Map<Integer,String> panelNames=new HashMap<>();
            Map<Integer,Integer> panelCounts=new HashMap<>();
            List<MappedZone> zones=new ArrayList<>();
            int cursor=0;
            for(JSONObject source:sections) {
                int section=source.optInt("physicalSection",-1);
                int panelId=exactGs400Profile
                    ? lane*2+(section==4?2:1):source.optInt("physicalPanelId",-1);
                String panelName=exactGs400Profile
                    ? (lane==0?"Left ":"Right ")+(section==4?"Trunk":"Housing")
                    : source.optString("physicalPanelName","").trim();
                int count=source.optInt("length",-1);
                if(section<0||section>7||panelId<1||panelId>127||
                   panelName.isEmpty()||count<1)
                    throw new IllegalArgumentException("Invalid physical section or Panel identity");
                Integer priorLane=globalPanelLanes.get(panelId);
                if(priorLane!=null&&priorLane.intValue()!=lane)
                    throw new IllegalArgumentException("CORE Panel IDs are controller-wide and cannot cross outputs");
                if(priorLane==null)globalPanelLanes.put(panelId,lane);
                String priorName=panelNames.get(panelId);
                if(priorName!=null&&!priorName.equals(panelName))
                    throw new IllegalArgumentException("One Panel ID has conflicting names");
                if(priorName==null)panelNames.put(panelId,panelName);
                panelCounts.put(panelId,panelCounts.containsKey(panelId)?
                    panelCounts.get(panelId)+1:1);
                long end=(long)cursor+count;
                if(end>2000)
                    throw new IllegalArgumentException("Output "+(lane+1)+" exceeds the 2,000-pixel Android limit");
                int zoneId=128+lane*8+section+1;
                zones.add(new MappedZone(zoneId,panelId,section,cursor,count,source.optString("name")));
                cursor=(int)end;
            }
            if(panelNames.size()>4)
                throw new IllegalArgumentException("Output "+(lane+1)+" exceeds CORE's four-Panel limit");
            List<MappedPanel> panels=new ArrayList<>();
            for(Map.Entry<Integer,String> entry:panelNames.entrySet())
                panels.add(new MappedPanel(entry.getKey(),lane,entry.getValue(),
                    panelCounts.get(entry.getKey())));
            Collections.sort(panels,(a,b)->Integer.compare(a.id,b.id));
            lanes[lane]=new MappedLane(lane,cursor,panels,zones);
        }
        return new LayoutMapping(lanes);
    }

    /**
     * Validate the proposed identity/ranges against both authoritative CORE maps
     * before BEGIN. Existing geometry is never implicitly deleted or overwritten
     * by a guessed ID; only an exact existing Zone identity may be updated in place.
     */
    static void validateMappingAgainstSnapshots(LayoutMapping mapping,
            PanelPreviewProtocol.LaneSnapshot[] snapshots,
            PanelPreviewProtocol.Status[] statuses) {
        if(mapping==null||snapshots==null||snapshots.length!=2||
           statuses==null||statuses.length!=2)
            throw new IllegalArgumentException("Both authoritative output snapshots and statuses are required");
        PanelPreviewProtocol.MapIdentity[] current=new PanelPreviewProtocol.MapIdentity[2];
        Map<Integer,PanelPreviewProtocol.MapPanel> currentPanels=new HashMap<>();
        Map<Integer,Integer> panelLanes=new HashMap<>();
        Map<Integer,PanelPreviewProtocol.MapZone> currentZones=new HashMap<>();
        Map<Integer,Integer> zoneLanes=new HashMap<>();
        for(int lane=0;lane<2;lane++) {
            PanelPreviewProtocol.LaneSnapshot snapshot=snapshots[lane];
            PanelPreviewProtocol.Status status=statuses[lane];
            if(snapshot==null||status==null||snapshot.lane!=lane||status.lane!=lane||
               snapshot.pending||!snapshot.persistent||status.state!=PanelPreviewProtocol.STATE_IDLE||
               status.sessionId!=0||status.configPending||!status.persistent||
               status.hardSyncFault||status.warningObserved||
               snapshot.activeRevision!=status.activeRevision||
               snapshot.stagedRevision!=status.stagedRevision||
               status.activeRevision!=status.renderRevision)
                throw new IllegalArgumentException("CORE output "+(lane+1)+
                    " is not in a stable, persistent, idle geometry state");
            current[lane]=PanelPreviewProtocol.MapIdentity.parse(snapshot);
            if(current[lane].lane!=lane)
                throw new IllegalArgumentException("Authoritative map lane identity mismatch");
            if(status.pixelCapacity!=current[lane].activePixels)
                throw new IllegalArgumentException("CORE status pixel capacity disagrees with authoritative Panel map");
            for(PanelPreviewProtocol.MapPanel panel:current[lane].panels) {
                if(currentZones.containsKey(panel.id))
                    throw new IllegalArgumentException("CORE Panel/Zone ID namespace collision");
                if(currentPanels.put(panel.id,panel)!=null)
                    throw new IllegalArgumentException("Duplicate controller-wide Panel ID");
                panelLanes.put(panel.id,lane);
            }
            for(PanelPreviewProtocol.MapZone zone:current[lane].zones) {
                if(currentPanels.containsKey(zone.id))
                    throw new IllegalArgumentException("CORE Panel/Zone ID namespace collision");
                if(currentZones.put(zone.id,zone)!=null)
                    throw new IllegalArgumentException("Duplicate controller-wide Zone ID");
                zoneLanes.put(zone.id,lane);
            }
        }
        for(int lane=0;lane<2;lane++) {
            MappedLane proposed=mapping.lane(lane);
            PanelPreviewProtocol.MapIdentity active=current[lane];
            if(proposed.activePixels>active.activePixels)
                throw new IllegalArgumentException("Output "+(lane+1)+
                    " layout exceeds CORE's authoritative active pixel capacity");
            for(MappedPanel panel:proposed.panels) {
                if(currentZones.containsKey(panel.id))
                    throw new IllegalArgumentException("Proposed Panel ID collides with an existing Zone ID");
                PanelPreviewProtocol.MapPanel prior=currentPanels.get(panel.id);
                if(prior!=null&&panelLanes.get(panel.id)!=lane)
                    throw new IllegalArgumentException("Proposed Panel ID conflicts with authoritative Panel identity");
            }
            for(MappedZone zone:proposed.zones) {
                if(currentPanels.containsKey(zone.id))
                    throw new IllegalArgumentException("Proposed Zone ID collides with an existing Panel ID");
                PanelPreviewProtocol.MapZone prior=currentZones.get(zone.id);
                if(prior!=null) {
                    if(zoneLanes.get(zone.id)!=lane||prior.panelId!=zone.panelId||
                       prior.ranges.size()!=1)
                        throw new IllegalArgumentException("Existing Zone ID has a different Panel or multi-range geometry");
                }
                long end=(long)zone.start+zone.count;
                for(PanelPreviewProtocol.MapZone existing:active.zones) {
                    if(existing.id==zone.id)continue;
                    for(PanelPreviewProtocol.MapRange range:existing.ranges)
                        if(zone.start<range.start+range.count&&range.start<end)
                            throw new IllegalArgumentException("Proposed geometry overlaps an existing CORE Zone");
                }
            }
        }
    }

    private Snapshot lastConfirmed;
    private State state=State.READY;
    private long sessionId,nextSequence=1,pendingSequence;
    private byte[] candidate,pendingPacket;
    private long candidateCrc32;
    private int targetRevision=-1;
    private boolean syncUncertain;
    private String lastError;

    GeometryTransactionManager(Snapshot initial) {
        if(initial==null)throw new IllegalArgumentException("Initial authoritative geometry is required");
        lastConfirmed=copy(initial);
    }

    synchronized long sessionId() { return sessionId; }
    synchronized State state() { return state; }
    synchronized Snapshot lastConfirmed() { return copy(lastConfirmed); }
    synchronized byte[] candidateCopy() { return candidate==null?null:Arrays.copyOf(candidate,candidate.length); }
    synchronized boolean syncUncertain() { return syncUncertain; }
    synchronized String lastError() { return lastError; }

    synchronized void markSynchronizationUncertain(String reason) {
        syncUncertain=true;
        lastError=reason==null?"Geometry synchronization is uncertain":reason;
        if(state!=State.COMMITTING && state!=State.COMMIT_REQUESTED)
            state=State.RECONCILING;
    }

    synchronized boolean requestBegin() {
        if(state!=State.READY||syncUncertain)return false;
        state=State.BEGINNING;lastError=null;
        candidate=Arrays.copyOf(lastConfirmed.wire,lastConfirmed.wire.length);
        nextSequence=1;pendingSequence=0;pendingPacket=null;sessionId=0;
        return true;
    }

    synchronized boolean acceptBegin(PanelPreviewProtocol.Status status) {
        if(state!=State.BEGINNING||status==null||status.state!=PanelPreviewProtocol.STATE_ACTIVE||
           status.lane!=lastConfirmed.lane||status.sessionId==0||
           status.baseRevision!=lastConfirmed.revision||status.pixelCapacity==0)return false;
        sessionId=status.sessionId;candidateCrc32=status.candidateCrc32;
        state=State.ACTIVE;return true;
    }

    /** Only the newest not-yet-sent candidate is retained by the caller. */
    synchronized Update queueUpdate(byte[] candidateWire,byte[] encodedPacket) {
        if(state!=State.ACTIVE||syncUncertain||pendingPacket!=null||
           candidateWire==null||encodedPacket==null)return null;
        if(encodedPacket.length<19||encodedPacket[0]!=(byte)0xfe||encodedPacket[1]!=2||
           (encodedPacket.length-11)%8!=0||(encodedPacket.length-11)/8<1||
           (encodedPacket.length-11)/8>4||encodedPacket[2]!=(byte)lastConfirmed.lane||
           PanelPreviewProtocol.u32(encodedPacket,3)!=sessionId||
           PanelPreviewProtocol.u32(encodedPacket,7)!=nextSequence)
            throw new IllegalArgumentException("Panel UPDATE packet does not match the active lane/session/sequence");
        if(!PanelPreviewProtocol.CORE_UNASSIGNED_ROLE_UPDATE_SUPPORTED) {
            for(int at=18;at<encodedPacket.length;at+=8)
                if((encodedPacket[at]&0x0f)==PanelPreviewProtocol.ROLE_UNASSIGNED_WIRE)
                    throw new IllegalStateException("CORE does not yet accept the UNASSIGNED Panel UPDATE role sentinel");
        }
        candidate=Arrays.copyOf(candidateWire,candidateWire.length);
        pendingSequence=nextSequence;
        pendingPacket=Arrays.copyOf(encodedPacket,encodedPacket.length);
        state=State.UPDATE_PENDING;
        return new Update(pendingSequence,pendingPacket);
    }

    /** Build the next authenticated, sequenced UPDATE from CORE's exact wire contract. */
    synchronized Update queueUpdate(byte[] candidateWire,
            PanelPreviewProtocol.UpdateDescriptor[] descriptors,int attMtu) {
        if(state!=State.ACTIVE||syncUncertain||pendingPacket!=null||
           candidateWire==null||descriptors==null||descriptors.length==0)return null;
        byte[] packet=PanelPreviewProtocol.update(lastConfirmed.lane,sessionId,
            nextSequence,descriptors,attMtu);
        return queueUpdate(candidateWire,packet);
    }

    synchronized boolean acknowledgeUpdate(PanelPreviewProtocol.Status status) {
        if(status==null)return false;
        if(state==State.COMMITTING||status.state==PanelPreviewProtocol.STATE_COMMITTING)
            return false;
        if(state!=State.UPDATE_PENDING)return false;
        if(status.lane!=lastConfirmed.lane||status.sessionId!=sessionId||
           status.lastAcceptedSequence!=pendingSequence) {
            if(status.lastRejectedSequence==pendingSequence || status.hardSyncFault) {
                requireReconciliation("CORE did not authoritatively accept the pending geometry UPDATE");
            }
            return false;
        }
        candidateCrc32=status.candidateCrc32;
        pendingPacket=null;pendingSequence=0;
        nextSequence=next(nextSequence);
        state=State.ACTIVE;return true;
    }

    synchronized byte[] requestCommit(PanelPreviewProtocol.Status status) {
        if(state!=State.ACTIVE||syncUncertain||pendingPacket!=null||status==null||
           status.state!=PanelPreviewProtocol.STATE_ACTIVE||status.lane!=lastConfirmed.lane||
           status.sessionId!=sessionId||status.hardSyncFault||
           status.lastAcceptedSequence!=(nextSequence==1?0:previous(nextSequence))||
           status.candidateCrc32!=candidateCrc32)return null;
        targetRevision=lastConfirmed.revision+1;
        state=State.COMMIT_REQUESTED;
        return PanelPreviewProtocol.commit(lastConfirmed.lane,sessionId,status.candidateCrc32);
    }

    synchronized void commitWriteCompleted() {
        // ATT write completion is transport delivery only. CORE status must
        // explicitly report COMMITTING before the manager enters that state.
        // A successful write response is never commit confirmation.
    }

    synchronized void observeStatus(PanelPreviewProtocol.Status status) {
        if(status==null)return;
        // COMMITTING and terminal transaction states take precedence over the
        // retained last-rejection diagnostic. CORE intentionally keeps that
        // diagnostic after a delayed UPDATE is rejected during COMMITTING.
        if(status.state==PanelPreviewProtocol.STATE_COMMITTING) {
            if(status.sessionId==sessionId)state=State.COMMITTING;
            return;
        }
        if(status.state==PanelPreviewProtocol.STATE_COMMITTED) {
            if(status.commitFullyApplied())state=State.RECONCILING;
            else requireReconciliation("CORE reported COMMITTED before active/render revisions agreed");
            return;
        }
        if(status.state==PanelPreviewProtocol.STATE_CANCELLED ||
           status.state==PanelPreviewProtocol.STATE_EXPIRED ||
           status.state==PanelPreviewProtocol.STATE_ERROR) {
            requireReconciliation("Panel transaction reached terminal state; reload authoritative geometry");
            return;
        }
        if(status.lastRejectedReason==PanelPreviewProtocol.ERR_COMMITTING_LOCKED ||
           status.committingRejectCount>0) {
            // A delayed UPDATE rejection is not a commit failure. Preserve the
            // confirmed image and continue observing the transaction. A stale
            // rejection must never mask a later authoritative terminal status.
            return;
        }
        if(status.hardSyncFault) {
            requireReconciliation("CORE geometry sequence fault requires cancel and authoritative reload");
        }
    }

    synchronized void connectionLost() {
        if(state!=State.READY)requireReconciliation("BLE disconnected during geometry transaction");
    }

    synchronized boolean reconcile(PanelPreviewProtocol.Status status,Snapshot authoritative) {
        if(status==null||authoritative==null||authoritative.lane!=lastConfirmed.lane||
           status.lane!=lastConfirmed.lane||authoritative.revision!=status.activeRevision||
           (status.hardSyncFault&&status.state!=PanelPreviewProtocol.STATE_CANCELLED)||
           status.state==PanelPreviewProtocol.STATE_COMMITTING||
           status.activeRevision!=status.renderRevision)return false;

        // A recoverable duplicate may resume the same active preview only
        // when CORE confirms its exact accepted sequence and candidate CRC.
        if(status.state==PanelPreviewProtocol.STATE_ACTIVE && sessionId!=0 &&
           status.sessionId==sessionId && candidate!=null &&
           state!=State.COMMIT_REQUESTED && state!=State.COMMITTING) {
            if(pendingPacket!=null) {
                if(status.lastAcceptedSequence!=pendingSequence)return false;
                pendingPacket=null;pendingSequence=0;
                nextSequence=next(status.lastAcceptedSequence);
                candidateCrc32=status.candidateCrc32;
            } else {
                long expected=nextSequence==1?0:previous(nextSequence);
                if(status.lastAcceptedSequence!=expected ||
                   status.candidateCrc32!=candidateCrc32)return false;
            }
            lastConfirmed=copy(authoritative);
            syncUncertain=false;lastError=null;state=State.ACTIVE;
            return true;
        }

        // A new session after cancellation/reload can return the manager to
        // READY, but the old hard-fault session itself cannot be cleared.
        if(status.state==PanelPreviewProtocol.STATE_ACTIVE &&
           state==State.RECONCILING && candidate==null && status.sessionId==sessionId)
            return false;
        if(status.state==PanelPreviewProtocol.STATE_ACTIVE &&
           state==State.RECONCILING && candidate==null && status.sessionId!=sessionId) {
            lastConfirmed=copy(authoritative);
            candidate=null;pendingPacket=null;pendingSequence=0;sessionId=0;
            targetRevision=-1;syncUncertain=false;lastError=null;state=State.READY;
            return true;
        }
        if(status.state==PanelPreviewProtocol.STATE_ACTIVE &&
           (state==State.COMMIT_REQUESTED||state==State.COMMITTING))return false;
        lastConfirmed=copy(authoritative);
        candidate=null;pendingPacket=null;pendingSequence=0;sessionId=0;
        targetRevision=-1;syncUncertain=false;lastError=null;state=State.READY;
        return true;
    }

    synchronized boolean rejectUpdateDuringCommit(int firmwareError) {
        if(state!=State.COMMITTING && state!=State.COMMIT_REQUESTED)return false;
        if(firmwareError==PanelPreviewProtocol.ERR_COMMITTING_LOCKED) {
            state=State.COMMITTING;
            lastError="Delayed geometry UPDATE rejected while COMMITTING";
            return true;
        }
        return false;
    }

    private void requireReconciliation(String reason) {
        syncUncertain=true;state=State.RECONCILING;lastError=reason;
        candidate=null;pendingPacket=null;pendingSequence=0;
    }
    private static Snapshot copy(Snapshot s) { return new Snapshot(s.lane,s.revision,s.wire); }
    private static long next(long n) { return n==0xffffffffL?1L:n+1L; }
    private static long previous(long n) { return n==1L?0xffffffffL:n-1L; }
}
