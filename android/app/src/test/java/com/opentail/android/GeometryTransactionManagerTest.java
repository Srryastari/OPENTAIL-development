package com.opentail.android;

import org.junit.Test;
import android.content.SharedPreferences;
import org.json.JSONObject;
import java.util.List;
import java.lang.reflect.Proxy;
import static org.junit.Assert.*;

public class GeometryTransactionManagerTest {
    private static void put16(byte[] b,int at,int v){b[at]=(byte)v;b[at+1]=(byte)(v>>>8);}
    private static void put32(byte[] b,int at,long v){for(int i=0;i<4;i++)b[at+i]=(byte)(v>>>(8*i));}
    private static PanelPreviewProtocol.Status status(int state,long session,long accepted,
            long crc,int active,int staged,int render,int error,int rejected,int flags) {
        byte[] b=new byte[61];b[0]=2;b[1]=(byte)state;b[2]=0;b[3]=3;
        put32(b,4,session);put16(b,8,7);put16(b,10,944);put16(b,12,5000);
        put32(b,14,accepted);put32(b,18,crc);put16(b,22,active);put16(b,24,staged);
        put16(b,26,render);b[30]=(byte)error;put32(b,35,rejected);b[39]=(byte)error;
        b[60]=(byte)flags;return PanelPreviewProtocol.Status.parse(b);
    }

    @Test public void freshUniversalOutputAssignmentsKeepIndependentPanelAndZoneIdentity() {
        NativeProject project=new NativeProject(new FakeProjectStorage().prefs);
        assertTrue(project.addZoneAt("O1",100,0));
        String leftId=project.zone(0).optString("id");
        assertTrue(project.mapZoneToOutputSlot(leftId,0,0));
        assertTrue(project.addZoneAt("O2",80,1));
        String rightId=project.zone(1).optString("id");
        assertTrue(project.mapZoneToOutputSlot(rightId,1,0));
        GeometryTransactionManager.LayoutMapping mapped=
            GeometryTransactionManager.mapLayout(project);
        assertEquals(100,mapped.lane(0).activePixels);
        assertEquals(80,mapped.lane(1).activePixels);
        assertEquals(1,mapped.lane(0).panels.size());
        assertEquals(1,mapped.lane(1).panels.size());
        assertEquals(1,mapped.lane(0).zones.size());
        assertEquals(1,mapped.lane(1).zones.size());
        assertNotEquals(mapped.lane(0).panels.get(0).id,mapped.lane(1).panels.get(0).id);
        assertNotEquals(mapped.lane(0).zones.get(0).id,mapped.lane(1).zones.get(0).id);
        assertTrue(mapped.lane(0).panels.get(0).name.length()>0);
        assertTrue(mapped.lane(1).panels.get(0).name.length()>0);
        assertEquals(0,mapped.lane(0).zones.get(0).start);
        assertEquals(0,mapped.lane(1).zones.get(0).start);
        assertEquals(100,mapped.lane(0).zones.get(0).count);
        assertEquals(80,mapped.lane(1).zones.get(0).count);
    }

    @Test public void blankUniversalLayoutHasNoSyntheticPanelOrPixels() {
        FakeProjectStorage store=new FakeProjectStorage();
        NativeProject project=new NativeProject(store.prefs);
        assertEquals(0,project.zoneCount());
        GeometryTransactionManager.LayoutMapping initial=GeometryTransactionManager.mapLayout(project);
        assertEquals(0,initial.lane(0).activePixels);
        assertEquals(0,initial.lane(1).activePixels);
        assertEquals(0,initial.lane(0).zones.size());
        assertEquals(0,initial.lane(1).panels.size());
        assertTrue(project.addZoneAt("Empty zone",0,0));
        GeometryTransactionManager.LayoutMapping localOnly=GeometryTransactionManager.mapLayout(project);
        assertEquals(0,localOnly.lane(0).activePixels);
        assertEquals(0,localOnly.lane(1).activePixels);
        assertEquals(0,project.pixelCount());
    }

    private static PanelPreviewProtocol.LaneSnapshot fullSnapshot(byte[] wire,int revision) {
        byte[] header=new byte[14];
        header[0]=3;header[1]=(byte)0x81;header[2]=wire[1];
        put16(header,4,wire.length);put16(header,6,PanelPreviewProtocol.crc16(wire));
        put16(header,8,revision);put16(header,10,revision);header[13]=1;
        PanelPreviewProtocol.SnapshotHeader parsed=
            PanelPreviewProtocol.SnapshotHeader.parse(header,wire[1]&255);
        parsed.verify(wire,wire[1]&255);
        return new PanelPreviewProtocol.LaneSnapshot(parsed,wire);
    }

    @Test public void acknowledgedLiveAndAtomicSaveUseIdenticalFullPhysicalGeometry() throws Exception {
        for(int lane=0;lane<2;lane++) {
            byte[] factory=PanelV4LayoutProtocol.expectedFactoryMap(lane);
            PanelPreviewProtocol.LaneSnapshot confirmed=fullSnapshot(factory,2);
            JSONObject draft=PanelV4DraftCodec.fromSnapshot(confirmed);
            GeometryTransactionManager.MappedLane zero=
                GeometryTransactionManager.fromV4SaveCandidate(lane,factory);
            assertEquals(0,zero.activePixels);
            assertTrue(zero.panels.isEmpty());assertTrue(zero.zones.isEmpty());

            // First live BEGIN 0->100 + one nonempty accepted UPDATE.
            draft.put("pixels",100);
            draft.getJSONArray("zones").put(new JSONObject()
                .put("name","Test").put("pixels",100).put("role","unassigned"));
            byte[] save=PanelV4DraftCodec.encode(lane,draft,confirmed);
            GeometryTransactionManager.MappedLane accepted=
                GeometryTransactionManager.fromV4SaveCandidate(lane,save);
            assertEquals(100,accepted.activePixels);
            assertEquals(0x8001+lane,accepted.panels.get(0).id);
            assertEquals(0x4081+lane*8,accepted.zones.get(0).id);
            assertEquals(PanelPreviewProtocol.ROLE_UNASSIGNED_WIRE,
                accepted.zones.get(0).roleFlags);
            assertFalse(GeometryTransactionManager.sameLiveGeometry(zero,accepted));
            assertTrue(GeometryTransactionManager.sameLiveGeometry(accepted,
                GeometryTransactionManager.fromV4SaveCandidate(lane,save)));

            // Names, strip order and calibration may change in one atomic
            // metadata Save without altering already accepted live topology.
            JSONObject metadata=new JSONObject(draft.toString());
            metadata.put("name","New output").put("panel_name","New panel")
                .put("color_order","RGB");
            metadata.getJSONArray("zones").getJSONObject(0).put("name","New zone");
            byte[] metadataWire=PanelV4DraftCodec.encode(lane,metadata,confirmed);
            assertTrue(GeometryTransactionManager.sameLiveGeometry(accepted,
                GeometryTransactionManager.fromV4SaveCandidate(lane,metadataWire)));

            // Previously the UPDATE planner ignored role-only edits, then
            // Firmware correctly rejected uploaded Save geometry (code 10).
            JSONObject changedRole=new JSONObject(draft.toString());
            changedRole.getJSONArray("zones").getJSONObject(0).put("role","brake");
            byte[] changedWire=PanelV4DraftCodec.encode(lane,changedRole,confirmed);
            GeometryTransactionManager.MappedLane desired=
                GeometryTransactionManager.fromV4SaveCandidate(lane,changedWire);
            assertFalse(GeometryTransactionManager.sameLiveGeometry(accepted,desired));
            List<PanelPreviewProtocol.UpdateDescriptor[]> batches=
                desired.descriptorBatches(accepted,247);
            assertEquals(1,batches.size());assertEquals(1,batches.get(0).length);
            assertEquals(3,batches.get(0)[0].roleFlags);
            assertEquals(100,batches.get(0)[0].count);
            assertTrue(GeometryTransactionManager.sameLiveGeometry(desired,desired));

            // An unaccepted range or a different stable 16-bit ID may not Save.
            JSONObject resized=new JSONObject(draft.toString()).put("pixels",80);
            resized.getJSONArray("zones").getJSONObject(0).put("pixels",80);
            assertFalse(GeometryTransactionManager.sameLiveGeometry(accepted,
                GeometryTransactionManager.fromV4SaveCandidate(lane,
                    PanelV4DraftCodec.encode(lane,resized,confirmed))));
            JSONObject shiftedId=new JSONObject(draft.toString());
            shiftedId.getJSONArray("zones").getJSONObject(0)
                .put("id",0x5000|(accepted.zones.get(0).id&0xff));
            assertFalse(GeometryTransactionManager.sameLiveGeometry(accepted,
                GeometryTransactionManager.fromV4SaveCandidate(lane,
                    PanelV4DraftCodec.encode(lane,shiftedId,confirmed))));
        }
    }


    @Test public void multiPanelMultiRangeLiveUpdateMatchesDurableWireWithoutFlattening() throws Exception {
        for(int lane=0;lane<2;lane++) {
            byte[] empty=PanelV4LayoutProtocol.expectedFactoryMap(lane);
            PanelPreviewProtocol.LaneSnapshot confirmed=fullSnapshot(empty,2);
            JSONObject draft=PanelV4DraftCodec.fromSnapshot(confirmed);
            draft.put("pixels",100).put("v4_advanced",true);
            int a=0x4081+lane*8,b=a+1,pa=0x8001+lane,pb=0x8003+lane;
            draft.put("panels",new org.json.JSONArray()
                .put(new JSONObject().put("id",pa).put("name","Panel A").put("lane",lane))
                .put(new JSONObject().put("id",pb).put("name","Panel B").put("lane",lane)));
            draft.getJSONArray("zones")
                .put(new JSONObject().put("id",a).put("panel_id",pa).put("name","Outer")
                    .put("role","unassigned").put("pixels",20).put("flags",0)
                    .put("ranges",new org.json.JSONArray()
                        .put(new JSONObject().put("start",0).put("count",10))
                        .put(new JSONObject().put("start",30).put("count",10))))
                .put(new JSONObject().put("id",b).put("panel_id",pb).put("name","Inner")
                    .put("role","brake").put("pixels",20).put("flags",0)
                    .put("ranges",new org.json.JSONArray()
                        .put(new JSONObject().put("start",60).put("count",20))));
            byte[] wire=PanelV4DraftCodec.encode(lane,draft,confirmed);
            PanelPreviewProtocol.LaneSnapshot stored=fullSnapshot(wire,3);
            JSONObject readback=PanelV4DraftCodec.fromSnapshot(stored);
            assertTrue(readback.getBoolean("v4_editable"));
            assertTrue(readback.getBoolean("v4_advanced"));
            assertArrayEquals(wire,PanelV4DraftCodec.encode(lane,readback,stored));
            GeometryTransactionManager.MappedLane baseline=
                GeometryTransactionManager.fromV4SaveCandidate(lane,wire);
            assertEquals(2,baseline.panels.size());
            assertEquals(2,baseline.zones.get(0).ranges.size());
            assertTrue(baseline.descriptorBatches(stored,247).isEmpty());
            // Fresh matching telemetry must admit the *real* range slots.
            // Previous preflight collapsed [0,10]+[30,10] into [0,20]
            // and refused every existing multi-range Zone.
            GeometryTransactionManager.MappedLane untouchedOther=
                GeometryTransactionManager.fromV4SaveCandidate(1-lane,
                    PanelV4LayoutProtocol.expectedFactoryMap(1-lane));
            GeometryTransactionManager.LayoutMapping both=new GeometryTransactionManager.LayoutMapping(
                lane==0 ? new GeometryTransactionManager.MappedLane[]{baseline,untouchedOther}
                    : new GeometryTransactionManager.MappedLane[]{untouchedOther,baseline});
            byte[] leftWire=lane==0?wire:PanelV4LayoutProtocol.expectedFactoryMap(0);
            byte[] rightWire=lane==1?wire:PanelV4LayoutProtocol.expectedFactoryMap(1);
            validateSnapshots(both,fullSnapshot(leftWire,7),fullSnapshot(rightWire,7));

            JSONObject moved=new JSONObject(readback.toString());
            moved.getJSONArray("zones").getJSONObject(0).getJSONArray("ranges")
                .getJSONObject(1).put("start",40);
            byte[] movedWire=PanelV4DraftCodec.encode(lane,moved,stored);
            GeometryTransactionManager.MappedLane shifted=
                GeometryTransactionManager.fromV4SaveCandidate(lane,movedWire);
            java.util.List<PanelPreviewProtocol.UpdateDescriptor[]> shiftedBatches=
                shifted.descriptorBatches(baseline,247);
            assertEquals(1,shiftedBatches.size());
            assertEquals(1,shiftedBatches.get(0).length);
            assertEquals(1,shiftedBatches.get(0)[0].rangeSlot);
            assertEquals(40,shiftedBatches.get(0)[0].start);
            assertEquals(a,shiftedBatches.get(0)[0].zoneId);
            assertFalse(GeometryTransactionManager.sameLiveGeometry(baseline,shifted));
            assertTrue(GeometryTransactionManager.sameLiveGeometry(shifted,
                GeometryTransactionManager.fromV4SaveCandidate(lane,movedWire)));

            JSONObject appended=new JSONObject(moved.toString());
            JSONObject z=appended.getJSONArray("zones").getJSONObject(0);
            z.getJSONArray("ranges").put(new JSONObject().put("start",85).put("count",5));
            z.put("pixels",25);
            byte[] appendedWire=PanelV4DraftCodec.encode(lane,appended,stored);
            GeometryTransactionManager.MappedLane withThird=
                GeometryTransactionManager.fromV4SaveCandidate(lane,appendedWire);
            java.util.List<PanelPreviewProtocol.UpdateDescriptor[]> appends=
                withThird.descriptorBatches(shifted,247);
            assertEquals(1,appends.size());
            assertEquals(2,appends.get(0)[0].rangeSlot);
            assertEquals(85,appends.get(0)[0].start);
            java.util.List<PanelPreviewProtocol.UpdateDescriptor[]> deletions=
                shifted.descriptorBatches(withThird,247);
            assertEquals(1,deletions.size());
            assertEquals(0x10,deletions.get(0)[0].roleFlags);
            assertEquals(2,deletions.get(0)[0].rangeSlot);

            JSONObject metadata=new JSONObject(readback.toString()).put("name","Custom O"+(lane+1));
            metadata.getJSONArray("zones").getJSONObject(0).put("name","Outer New");
            assertTrue(GeometryTransactionManager.sameLiveGeometry(baseline,
                GeometryTransactionManager.fromV4SaveCandidate(lane,
                    PanelV4DraftCodec.encode(lane,metadata,stored))));
        }
    }

    @Test public void physicalReorderRebuildsAffectedSuffixWithStableIds() throws Exception {
        // Multi-panel maps must not be silently flattened or reordered by
        // unrelated local project cards.
        byte[] empty=PanelV4LayoutProtocol.expectedFactoryMap(0);
        PanelPreviewProtocol.LaneSnapshot first=fullSnapshot(empty,1);
        JSONObject draft=PanelV4DraftCodec.fromSnapshot(first)
            .put("pixels",100).put("v4_advanced",true);
        draft.getJSONArray("zones")
            .put(new JSONObject().put("name","A").put("pixels",20).put("role","off")
                .put("id",0x4081).put("panel_id",0x8001).put("ranges",
                    new org.json.JSONArray().put(new JSONObject().put("start",0).put("count",20))))
            .put(new JSONObject().put("name","B").put("pixels",20).put("role","off")
                .put("id",0x4082).put("panel_id",0x8001).put("ranges",
                    new org.json.JSONArray().put(new JSONObject().put("start",60).put("count",20))));
        byte[] wire=PanelV4DraftCodec.encode(0,draft,first);
        GeometryTransactionManager.MappedLane prior=
            GeometryTransactionManager.fromV4SaveCandidate(0,wire);
        JSONObject swapped=new JSONObject(draft.toString());
        org.json.JSONArray zones=swapped.getJSONArray("zones");
        JSONObject firstZone=zones.getJSONObject(0),secondZone=zones.getJSONObject(1);
        swapped.put("zones",new org.json.JSONArray().put(secondZone).put(firstZone));
        GeometryTransactionManager.MappedLane future=
            GeometryTransactionManager.fromV4SaveCandidate(0,
                PanelV4DraftCodec.encode(0,swapped,fullSnapshot(wire,2)));
        java.util.List<PanelPreviewProtocol.UpdateDescriptor[]> batches=
            future.descriptorBatches(prior,247);
        // v2 has no reorder opcode, so only the affected suffix is deleted
        // then recreated with the exact same unambiguous full stable IDs.
        assertEquals(4,batches.size());
        assertEquals(0x20,batches.get(0)[0].roleFlags);
        assertEquals(0x20,batches.get(1)[0].roleFlags);
        assertEquals(0x4082,batches.get(2)[0].zoneId);
        assertEquals(0x4081,batches.get(3)[0].zoneId);
        assertEquals(0,batches.get(2)[0].roleFlags);
        assertTrue(GeometryTransactionManager.sameLiveGeometry(future,
            GeometryTransactionManager.fromV4SaveCandidate(0,
                PanelV4DraftCodec.encode(0,swapped,fullSnapshot(wire,2)))));
    }

    @Test public void savedO2RemainsIndependentFromUnsavedO1LiveCandidate() throws Exception {
        PanelPreviewProtocol.LaneSnapshot o1=fullSnapshot(
            PanelV4LayoutProtocol.expectedFactoryMap(0),5);
        PanelPreviewProtocol.LaneSnapshot o2=fullSnapshot(
            PanelV4LayoutProtocol.expectedFactoryMap(1),6);
        JSONObject left=PanelV4DraftCodec.fromSnapshot(o1);
        JSONObject right=PanelV4DraftCodec.fromSnapshot(o2);
        left.put("pixels",100);
        left.getJSONArray("zones").put(new JSONObject().put("name","Left")
            .put("pixels",100).put("role","unassigned"));
        right.put("pixels",80);
        right.getJSONArray("zones").put(new JSONObject().put("name","Right")
            .put("pixels",80).put("role","unassigned"));
        GeometryTransactionManager.MappedLane liveLeft=
            GeometryTransactionManager.fromV4SaveCandidate(0,PanelV4DraftCodec.encode(0,left,o1));
        GeometryTransactionManager.MappedLane liveRight=
            GeometryTransactionManager.fromV4SaveCandidate(1,PanelV4DraftCodec.encode(1,right,o2));
        GeometryTransactionManager.LayoutMapping both=new GeometryTransactionManager.LayoutMapping(
            new GeometryTransactionManager.MappedLane[]{liveLeft,liveRight});
        GeometryTransactionManager.LayoutMapping one=both.withLane(0,
            GeometryTransactionManager.fromV4SaveCandidate(0,o1.wire));
        assertEquals(0,one.lane(0).activePixels);
        assertEquals(80,one.lane(1).activePixels);
        assertTrue(GeometryTransactionManager.sameLiveGeometry(both.lane(1),one.lane(1)));
    }

    @Test public void lastConfirmedGeometryDoesNotChangeDuringPreviewOrCommit() {
        byte[] original={3,0,1,2};byte[] edited={3,0,1,9};
        GeometryTransactionManager manager=new GeometryTransactionManager(
            new GeometryTransactionManager.Snapshot(0,7,original));
        assertTrue(manager.requestBegin());
        assertTrue(manager.acceptBegin(status(1,77,0,0x1111,7,7,7,0,0,0)));
        PanelPreviewProtocol.UpdateDescriptor d=
            new PanelPreviewProtocol.UpdateDescriptor(1,1,0,10,5,0);
        byte[] packet=PanelPreviewProtocol.update(0,77,1,
            new PanelPreviewProtocol.UpdateDescriptor[]{d},247);
        assertNotNull(manager.queueUpdate(edited,packet));
        assertFalse(manager.requestCommit(status(1,77,0,0x2222,7,7,7,0,0,0))!=null);
        assertTrue(manager.acknowledgeUpdate(status(1,77,1,0x2222,7,7,7,0,0,4)));
        byte[] commit=manager.requestCommit(status(1,77,1,0x2222,7,7,7,0,0,4));
        assertNotNull(commit);assertEquals(0x22,commit[7]&255);
        manager.commitWriteCompleted();
        assertEquals(GeometryTransactionManager.State.COMMIT_REQUESTED,manager.state());
        manager.observeStatus(status(2,77,1,0x2222,7,8,7,0,0,4));
        assertEquals(GeometryTransactionManager.State.COMMITTING,manager.state());
        assertArrayEquals(original,manager.lastConfirmed().wire);
        assertTrue(manager.rejectUpdateDuringCommit(PanelPreviewProtocol.ERR_COMMITTING_LOCKED));
        assertEquals(GeometryTransactionManager.State.COMMITTING,manager.state());
        assertArrayEquals(original,manager.lastConfirmed().wire);
        manager.observeStatus(status(3,77,1,0x2222,8,8,8,0,1,4));
        assertEquals(GeometryTransactionManager.State.RECONCILING,manager.state());
        assertTrue(manager.reconcile(status(3,77,1,0x2222,8,8,8,0,1,4),
            new GeometryTransactionManager.Snapshot(0,8,edited)));
        assertEquals(GeometryTransactionManager.State.READY,manager.state());
        assertArrayEquals(edited,manager.lastConfirmed().wire);
    }

    @Test public void disconnectAndHardSequenceFaultRequireAuthoritativeReload() {
        byte[] original={1,2,3};
        GeometryTransactionManager manager=new GeometryTransactionManager(
            new GeometryTransactionManager.Snapshot(0,7,original));
        assertTrue(manager.requestBegin());
        assertTrue(manager.acceptBegin(status(1,91,0,0x10,7,7,7,0,0,0)));
        manager.connectionLost();
        assertEquals(GeometryTransactionManager.State.RECONCILING,manager.state());
        assertTrue(manager.syncUncertain());
        assertArrayEquals(original,manager.lastConfirmed().wire);
        assertFalse(manager.reconcile(status(1,91,0,0x10,7,7,7,0,0,0),
            new GeometryTransactionManager.Snapshot(0,7,original)));
        assertTrue(manager.reconcile(status(4,91,0,0x10,7,7,7,0,0,0),
            new GeometryTransactionManager.Snapshot(0,7,original)));
        assertFalse(manager.syncUncertain());
    }

    @Test public void duplicateWarningCanResumeOnlyAfterMatchingAuthoritativeSequenceAndCrc() {
        byte[] original={3,0,1,2};byte[] edited={3,0,1,9};
        GeometryTransactionManager manager=new GeometryTransactionManager(
            new GeometryTransactionManager.Snapshot(0,7,original));
        assertTrue(manager.requestBegin());
        assertTrue(manager.acceptBegin(status(1,55,0,0x1111,7,7,7,0,0,0)));
        PanelPreviewProtocol.UpdateDescriptor d=
            new PanelPreviewProtocol.UpdateDescriptor(1,1,0,10,5,0);
        byte[] packet=PanelPreviewProtocol.update(0,55,1,
            new PanelPreviewProtocol.UpdateDescriptor[]{d},247);
        assertNotNull(manager.queueUpdate(edited,packet));
        assertTrue(manager.acknowledgeUpdate(status(1,55,1,0x2222,7,7,7,0,0,4)));
        manager.markSynchronizationUncertain("Duplicate sequence observed");
        assertEquals(GeometryTransactionManager.State.RECONCILING,manager.state());
        assertNull(manager.queueUpdate(edited,packet));
        assertTrue(manager.reconcile(status(1,55,1,0x2222,7,7,7,0,0,5),
            new GeometryTransactionManager.Snapshot(0,7,original)));
        assertEquals(GeometryTransactionManager.State.ACTIVE,manager.state());
        assertFalse(manager.syncUncertain());
        assertArrayEquals(edited,manager.candidateCopy());
        assertArrayEquals(original,manager.lastConfirmed().wire);
    }

    @Test public void hardFaultRequiresCancelAndVerifiedReloadBeforeNewSession() {
        byte[] original={1,2,3};
        GeometryTransactionManager manager=new GeometryTransactionManager(
            new GeometryTransactionManager.Snapshot(0,7,original));
        assertTrue(manager.requestBegin());
        assertTrue(manager.acceptBegin(status(1,92,0,0x10,7,7,7,0,0,0)));
        PanelPreviewProtocol.Status fault=status(1,92,0,0x10,7,7,7,10,0,2);
        manager.observeStatus(fault);
        assertEquals(GeometryTransactionManager.State.RECONCILING,manager.state());
        assertFalse(manager.reconcile(fault,new GeometryTransactionManager.Snapshot(0,7,original)));
        PanelPreviewProtocol.Status cancelled=status(4,92,0,0x10,7,7,7,10,0,2);
        assertTrue(manager.reconcile(cancelled,new GeometryTransactionManager.Snapshot(0,7,original)));
        assertEquals(GeometryTransactionManager.State.READY,manager.state());
        assertFalse(manager.syncUncertain());
    }

    @Test public void staleRendererRevisionCannotConfirmGeometry() {
        byte[] original={1,2,3};
        GeometryTransactionManager manager=new GeometryTransactionManager(
            new GeometryTransactionManager.Snapshot(0,7,original));
        assertTrue(manager.requestBegin());
        assertTrue(manager.acceptBegin(status(1,93,0,0x10,7,7,7,0,0,0)));
        manager.markSynchronizationUncertain("Renderer readback is stale");
        assertFalse(manager.reconcile(status(1,93,0,0x10,7,7,6,0,0,0),
            new GeometryTransactionManager.Snapshot(0,7,original)));
        assertEquals(GeometryTransactionManager.State.RECONCILING,manager.state());
        assertTrue(manager.syncUncertain());
    }

    @Test public void cancelledTerminalSnapshotReconcilesOutstandingCommitRequest() {
        byte[] original={1,2,3};
        GeometryTransactionManager manager=new GeometryTransactionManager(
            new GeometryTransactionManager.Snapshot(0,7,original));
        assertTrue(manager.requestBegin());
        PanelPreviewProtocol.Status active=status(1,94,0,0x10,7,7,7,0,0,0);
        assertTrue(manager.acceptBegin(active));
        assertNotNull(manager.requestCommit(active));
        assertEquals(GeometryTransactionManager.State.COMMIT_REQUESTED,manager.state());
        PanelPreviewProtocol.Status cancelled=status(4,94,0,0x10,7,7,7,0,0,0);
        assertTrue(manager.reconcile(cancelled,new GeometryTransactionManager.Snapshot(0,7,original)));
        assertEquals(GeometryTransactionManager.State.READY,manager.state());
    }

    @Test public void committingRejectDoesNotAbortOrDemoteActiveCommit() {
        GeometryTransactionManager manager=new GeometryTransactionManager(
            new GeometryTransactionManager.Snapshot(0,7,new byte[]{1}));
        assertTrue(manager.requestBegin());
        assertTrue(manager.acceptBegin(status(1,5,0,0x44,7,7,7,0,0,0)));
        assertNotNull(manager.requestCommit(status(1,5,0,0x44,7,7,7,0,0,0)));
        manager.commitWriteCompleted();
        manager.observeStatus(status(2,5,0,0x44,7,8,7,5,0,1));
        assertEquals(GeometryTransactionManager.State.COMMITTING,manager.state());
        assertArrayEquals(new byte[]{1},manager.lastConfirmed().wire);
    }

    @Test public void committingRejectionDoesNotMaskTerminalCommitStatus() {
        GeometryTransactionManager manager=new GeometryTransactionManager(
            new GeometryTransactionManager.Snapshot(0,7,new byte[]{1,2,3}));
        assertTrue(manager.requestBegin());
        assertTrue(manager.acceptBegin(status(1,95,0,0x55,7,7,7,0,0,0)));
        PanelPreviewProtocol.Status active=status(1,95,0,0x55,7,7,7,0,0,0);
        assertNotNull(manager.requestCommit(active));
        assertNull("duplicate COMMIT must not be emitted",manager.requestCommit(active));
        manager.observeStatus(status(2,95,0,0x55,7,8,7,5,1,1));
        assertEquals(GeometryTransactionManager.State.COMMITTING,manager.state());
        assertNull("delayed geometry UPDATE must be rejected during COMMITTING",
            manager.queueUpdate(new byte[]{9},new byte[]{1,2,3}));
        assertNull("another COMMIT must not be emitted while COMMITTING",
            manager.requestCommit(status(2,95,0,0x55,7,8,7,5,1,1)));
        // Firmware retains the last rejection diagnostic after COMMITTING.
        // A stale error must not mask the authoritative terminal COMMITTED state.
        manager.observeStatus(status(3,95,0,0x55,8,8,8,5,1,1));
        assertEquals(GeometryTransactionManager.State.RECONCILING,manager.state());
        assertArrayEquals(new byte[]{1,2,3},manager.lastConfirmed().wire);
    }

    private static final class FakeProjectStorage {
        String persisted,pending;
        final SharedPreferences prefs;
        FakeProjectStorage() {
            SharedPreferences.Editor editor=(SharedPreferences.Editor)Proxy.newProxyInstance(
                SharedPreferences.Editor.class.getClassLoader(),
                new Class<?>[]{SharedPreferences.Editor.class},(proxy,method,args)->{
                    switch(method.getName()) {
                        case "putString": pending=(String)args[1];return proxy;
                        case "commit": persisted=pending;return true;
                        case "apply": persisted=pending;return null;
                        case "clear": pending=null;return proxy;
                        default:
                            if(SharedPreferences.Editor.class.isAssignableFrom(method.getReturnType()))return proxy;
                            return false;
                    }
                });
            prefs=(SharedPreferences)Proxy.newProxyInstance(
                SharedPreferences.class.getClassLoader(),new Class<?>[]{SharedPreferences.class},
                (proxy,method,args)->{
                    switch(method.getName()) {
                        case "getString": return persisted==null?args[1]:persisted;
                        case "edit": return editor;
                        case "contains": return persisted!=null;
                        default: return method.getReturnType().isPrimitive()?false:null;
                    }
                });
        }
    }

    private static NativeProject mappedZone(NativeProject project,String name,int pixels,
            int lane,int section,int panel,String panelName) throws Exception {
        assertTrue(project.addZone(name,pixels));
        String id=project.zone(project.zoneCount()-1).optString("id");
        assertTrue(project.mapZoneToOutputSlot(id,lane,section));
        JSONObject document=new JSONObject(project.exportJson());
        document.getJSONArray("layout").getJSONObject(project.zoneCount()-1)
            .put("physicalPanelId",panel).put("physicalPanelName",panelName);
        assertTrue(project.importJson(document.toString()));
        return project;
    }

    @Test public void gs400PanelZoneIdentityMapsFourHousingSectionsAndSeparateTrunk() throws Exception {
        NativeProject project=new NativeProject(new FakeProjectStorage().prefs);
        mappedZone(project,"Left Top",118,0,0,1,"Left Housing");
        mappedZone(project,"Left Amber",109,0,1,1,"Left Housing");
        mappedZone(project,"Left Reverse",42,0,2,1,"Left Housing");
        mappedZone(project,"Left Bottom",253,0,3,1,"Left Housing");
        mappedZone(project,"Left Trunk",101,0,4,2,"Left Trunk");
        mappedZone(project,"Right Top",118,1,0,3,"Right Housing");
        mappedZone(project,"Right Amber",109,1,1,3,"Right Housing");
        mappedZone(project,"Right Reverse",42,1,2,3,"Right Housing");
        mappedZone(project,"Right Bottom",253,1,3,3,"Right Housing");
        mappedZone(project,"Right Trunk",101,1,4,4,"Right Trunk");

        GeometryTransactionManager.LayoutMapping mapping=GeometryTransactionManager.mapLayout(project);
        GeometryTransactionManager.MappedLane left=mapping.lane(0),right=mapping.lane(1);
        assertEquals(623,left.activePixels);assertEquals(623,right.activePixels);
        assertEquals(2,left.panels.size());assertEquals(2,right.panels.size());
        assertEquals(5,left.zones.size());assertEquals(5,right.zones.size());
        assertEquals("Left Housing",left.panels.get(0).name);
        assertEquals(4,left.panels.get(0).zoneCount);
        assertEquals("Left Trunk",left.panels.get(1).name);
        assertEquals(0,left.zones.get(0).start);
        assertEquals(118,left.zones.get(1).start);
        assertEquals(227,left.zones.get(2).start);
        assertEquals(269,left.zones.get(3).start);
        assertEquals(522,left.zones.get(4).start);
        assertEquals(101,left.zones.get(4).count);
        assertEquals(129,left.zones.get(0).id);
        assertEquals(137,right.zones.get(0).id);
        assertEquals(1,left.zones.get(0).panelId);
        assertEquals(2,left.zones.get(4).panelId);
        assertEquals(3,right.zones.get(0).panelId);
        assertEquals(4,right.zones.get(4).panelId);
        assertEquals(PanelPreviewProtocol.ROLE_UNASSIGNED_WIRE,left.zones.get(0).roleFlags);
        PanelPreviewProtocol.UpdateDescriptor[] descriptors=left.descriptors();
        assertEquals(5,descriptors.length);
        assertEquals(129,descriptors[0].zoneId);
        assertEquals(0,descriptors[0].rangeSlot);
        assertEquals(118,descriptors[0].count);
    }

    private static void physicalZone(NativeProject project,String name,int pixels,
            int lane,int section) throws Exception {
        // The GS400 housing/trunk split is now an explicit owner layout,
        // never a hidden fallback forced onto arbitrary universal sections.
        int panel=lane*2+(section==4?2:1);
        String panelName=(lane==0?"Left ":"Right ")+(section==4?"Trunk":"Housing");
        mappedZone(project,name,pixels,lane,section,panel,panelName);
    }

    @Test public void exactOwnerApprovedGs400LayoutGetsDeterministicHousingAndTrunkPanels() throws Exception {
        NativeProject project=new NativeProject(new FakeProjectStorage().prefs);
        int[] pixels={118,109,42,253,101};
        String[] sections={"Top","Amber","Reverse","Bottom","Trunk"};
        for(int lane=0;lane<2;lane++)for(int i=0;i<5;i++)
            physicalZone(project,(lane==0?"Left ":"Right ")+sections[i],pixels[i],lane,i);
        GeometryTransactionManager.LayoutMapping mapping=GeometryTransactionManager.mapLayout(project);
        assertEquals(623,mapping.lane(0).activePixels);
        assertEquals(623,mapping.lane(1).activePixels);
        assertEquals(1,mapping.lane(0).zones.get(0).panelId);
        assertEquals(1,mapping.lane(0).zones.get(3).panelId);
        assertEquals(2,mapping.lane(0).zones.get(4).panelId);
        assertEquals(3,mapping.lane(1).zones.get(0).panelId);
        assertEquals(4,mapping.lane(1).zones.get(4).panelId);
        assertEquals("Left Housing",mapping.lane(0).panels.get(0).name);
        assertEquals("Left Trunk",mapping.lane(0).panels.get(1).name);
    }

    @Test public void gs400PanelIdentityRemainsStableWhenPixelCountsAreEdited() throws Exception {
        NativeProject project=new NativeProject(new FakeProjectStorage().prefs);
        int[] edited={120,110,40,250,103}; // Same owner-approved sections; total remains 623.
        String[] sections={"Top","Amber","Reverse","Bottom","Trunk"};
        for(int lane=0;lane<2;lane++)for(int i=0;i<5;i++)
            physicalZone(project,(lane==0?"Left ":"Right ")+sections[i],edited[i],lane,i);
        GeometryTransactionManager.LayoutMapping mapping=GeometryTransactionManager.mapLayout(project);
        GeometryTransactionManager.MappedLane left=mapping.lane(0);
        assertEquals(623,left.activePixels);
        assertEquals(2,left.panels.size());
        assertEquals("Left Housing",left.panels.get(0).name);
        assertEquals(4,left.panels.get(0).zoneCount);
        assertEquals("Left Trunk",left.panels.get(1).name);
        assertEquals(120,left.zones.get(0).count);
        assertEquals(520,left.zones.get(4).start);
        assertEquals(103,left.zones.get(4).count);
    }

    @Test public void geometryMappingNeverGuessesMissingPanelIdentity() throws Exception {
        NativeProject project=new NativeProject(new FakeProjectStorage().prefs);
        assertTrue(project.addZone("Unassigned panel",10));
        String id=project.zone(0).optString("id");
        assertTrue(project.mapZoneToOutputSlot(id,0,0));
        // The interactive universal assignment legitimately chooses a stable
        // Panel. A malformed imported document with the identity removed
        // must still fail closed rather than invent one during CORE mapping.
        JSONObject imported=new JSONObject(project.exportJson());
        JSONObject zone=imported.getJSONArray("layout").getJSONObject(0);
        zone.remove("physicalPanelId");zone.remove("physicalPanelName");
        assertTrue(project.importJson(imported.toString()));
        try { GeometryTransactionManager.mapLayout(project);fail("Panel identity must not be guessed"); }
        catch(IllegalStateException expected) {
            assertTrue(expected.getMessage().contains("explicit CORE Panel identity"));
        }
    }

    @Test public void fiveSectionsOnBothOutputsNeverInferVehicleSpecificPanels() throws Exception {
        NativeProject project=new NativeProject(new FakeProjectStorage().prefs);
        for(int lane=0;lane<2;lane++)for(int section=0;section<5;section++)
            mappedZone(project,"Fixture "+lane+"-"+section,section+1,
                lane,section,lane+1,"User Panel "+lane);

        // Explicit user metadata works for any fixture shape and LED counts.
        GeometryTransactionManager.LayoutMapping valid=GeometryTransactionManager.mapLayout(project);
        assertEquals(15,valid.lane(0).activePixels);
        assertEquals(15,valid.lane(1).activePixels);

        // A legacy import that merely happens to have five sections per output
        // must never silently acquire GS400 Housing/Trunk identities.
        JSONObject imported=new JSONObject(project.exportJson());
        org.json.JSONArray zones=imported.getJSONArray("layout");
        for(int i=0;i<zones.length();i++) {
            JSONObject zone=zones.getJSONObject(i);
            zone.remove("physicalPanelId");
            zone.remove("physicalPanelName");
        }
        assertTrue(project.importJson(imported.toString()));
        try {
            GeometryTransactionManager.mapLayout(project);
            fail("Missing Panel identities must not be inferred from section count");
        } catch(IllegalStateException expected) {
            assertTrue(expected.getMessage().contains("explicit CORE Panel identity"));
        }
    }

    @Test public void geometryMappingRejectsPanelIdReuseAcrossOutputs() throws Exception {
        NativeProject project=new NativeProject(new FakeProjectStorage().prefs);
        mappedZone(project,"Left",10,0,0,7,"Left Housing");
        mappedZone(project,"Right",10,1,0,7,"Right Housing");
        try { GeometryTransactionManager.mapLayout(project);fail("Panel IDs are controller-wide"); }
        catch(IllegalArgumentException expected) {
            assertTrue(expected.getMessage().contains("controller-wide"));
        }
    }

    @Test public void geometryMappingRejectsPixelAndPanelCapacityOverflow() throws Exception {
        NativeProject tooLong=new NativeProject(new FakeProjectStorage().prefs);
        mappedZone(tooLong,"Too long",2001,0,0,1,"Housing");
        try { GeometryTransactionManager.mapLayout(tooLong);fail("pixel capacity"); }
        catch(IllegalArgumentException expected) {
            assertTrue(expected.getMessage().contains("2,000-pixel"));
        }
        NativeProject tooMany=new NativeProject(new FakeProjectStorage().prefs);
        for(int i=0;i<5;i++)mappedZone(tooMany,"Panel "+i,1,0,i,i+1,"Panel "+i);
        try { GeometryTransactionManager.mapLayout(tooMany);fail("panel capacity"); }
        catch(IllegalArgumentException expected) {
            assertTrue(expected.getMessage().contains("four-Panel"));
        }
    }

    @Test public void geometryMappingRejectsConflictingPanelNamesAndWireIds() throws Exception {
        NativeProject conflict=new NativeProject(new FakeProjectStorage().prefs);
        mappedZone(conflict,"Top",10,0,0,1,"Housing");
        mappedZone(conflict,"Bottom",10,0,1,1,"Trunk");
        try { GeometryTransactionManager.mapLayout(conflict);fail("conflicting Panel names"); }
        catch(IllegalArgumentException expected) {
            assertTrue(expected.getMessage().contains("conflicting names"));
        }
        NativeProject invalid=new NativeProject(new FakeProjectStorage().prefs);
        mappedZone(invalid,"Invalid",10,0,0,128,"Housing");
        try { GeometryTransactionManager.mapLayout(invalid);fail("Panel ID wire range"); }
        catch(IllegalArgumentException expected) {
            assertTrue(expected.getMessage().contains("Panel identity"));
        }
    }

    private static void putName(byte[] wire,int at,String value) {
        byte[] bytes=value.getBytes(java.nio.charset.StandardCharsets.US_ASCII);
        System.arraycopy(bytes,0,wire,at,Math.min(bytes.length,24));
    }
    private static PanelPreviewProtocol.LaneSnapshot panelSnapshot(int lane,int pixels,
            int panelId,String panelName,int zoneId,int zonePanel,String zoneName,
            int rangeStart,int rangeCount) {
        return panelSnapshot(lane,pixels,panelId,panelName,zoneId,zonePanel,zoneName,
            rangeStart,rangeCount,0xff);
    }
    private static PanelPreviewProtocol.LaneSnapshot panelSnapshot(int lane,int pixels,
            int panelId,String panelName,int zoneId,int zonePanel,String zoneName,
            int rangeStart,int rangeCount,int role) {
        byte[] wire=new byte[PanelPreviewProtocol.MAP_BYTES];
        wire[0]=3;wire[1]=(byte)lane;put16(wire,2,pixels);
        if(panelId!=0) {
            wire[37]=1;wire[38]=1;
            put16(wire,39,panelId);wire[41]=(byte)lane;
            putName(wire,42,panelName);wire[67]=1;
            put16(wire,155,zoneId);put16(wire,157,zonePanel);
            putName(wire,159,zoneName);wire[184]=(byte)role;wire[186]=1;
            put16(wire,187,rangeStart);put16(wire,189,rangeCount);
        }
        byte[] header=new byte[14];
        header[0]=3;header[1]=(byte)0x81;header[2]=(byte)lane;
        put16(header,4,PanelPreviewProtocol.MAP_BYTES);
        put16(header,6,PanelPreviewProtocol.crc16(wire));
        put16(header,8,7);put16(header,10,7);header[13]=1;
        PanelPreviewProtocol.SnapshotHeader parsed=
            PanelPreviewProtocol.SnapshotHeader.parse(header,lane);
        parsed.verify(wire,lane);
        return new PanelPreviewProtocol.LaneSnapshot(parsed,wire);
    }

    private static PanelPreviewProtocol.LaneSnapshot panelSnapshotWithTwo(
            int lane,int pixels,int[] panelIds,int[] zoneIds,int[] parentIds) {
        if(panelIds.length!=2||zoneIds.length!=2||parentIds.length!=2)
            throw new IllegalArgumentException("Two map identities required");
        byte[] wire=new byte[PanelPreviewProtocol.MAP_BYTES];
        wire[0]=3;wire[1]=(byte)lane;put16(wire,2,pixels);wire[37]=2;wire[38]=2;
        for(int i=0;i<2;i++) {
            int panelAt=39+i*29;
            put16(wire,panelAt,panelIds[i]);wire[panelAt+2]=(byte)lane;
            putName(wire,panelAt+3,"Panel "+i);wire[panelAt+28]=1;
            int zoneAt=155+i*64;
            put16(wire,zoneAt,zoneIds[i]);put16(wire,zoneAt+2,parentIds[i]);
            putName(wire,zoneAt+4,"Zone "+i);wire[zoneAt+29]=0;
            wire[zoneAt+30]=0;wire[zoneAt+31]=1;
            put16(wire,zoneAt+32,i*10);put16(wire,zoneAt+34,10);
        }
        byte[] header=new byte[14];
        header[0]=3;header[1]=(byte)0x81;header[2]=(byte)lane;
        put16(header,4,PanelPreviewProtocol.MAP_BYTES);
        put16(header,6,PanelPreviewProtocol.crc16(wire));
        put16(header,8,7);put16(header,10,7);header[13]=1;
        PanelPreviewProtocol.SnapshotHeader parsed=PanelPreviewProtocol.SnapshotHeader.parse(header,lane);
        parsed.verify(wire,lane);
        return new PanelPreviewProtocol.LaneSnapshot(parsed,wire);
    }

    private static PanelPreviewProtocol.Status idlePanelStatus(int lane,int revision,int pixels) {
        byte[] b=new byte[61];b[0]=2;b[1]=0;b[2]=(byte)lane;b[3]=3;
        put16(b,8,revision);put16(b,10,pixels);put16(b,12,5000);
        put16(b,22,revision);put16(b,24,revision);put16(b,26,revision);
        b[29]=1;
        return PanelPreviewProtocol.Status.parse(b);
    }
    private static PanelPreviewProtocol.Status laneStatus(int lane,int state,long session,
            int baseRevision,int pixels,int active,int staged,int render) {
        byte[] b=new byte[61];b[0]=2;b[1]=(byte)state;b[2]=(byte)lane;b[3]=3;
        put32(b,4,session);put16(b,8,baseRevision);put16(b,10,pixels);put16(b,12,5000);
        put16(b,22,active);put16(b,24,staged);put16(b,26,render);b[29]=1;put32(b,31,5000);
        return PanelPreviewProtocol.Status.parse(b);
    }

    private static void validateSnapshots(GeometryTransactionManager.LayoutMapping mapping,
            PanelPreviewProtocol.LaneSnapshot left,PanelPreviewProtocol.LaneSnapshot right) {
        GeometryTransactionManager.validateMappingAgainstSnapshots(mapping,
            new PanelPreviewProtocol.LaneSnapshot[]{left,right},
            new PanelPreviewProtocol.Status[]{
                idlePanelStatus(0,7,PanelPreviewProtocol.MapIdentity.parse(left).activePixels),
                idlePanelStatus(1,7,PanelPreviewProtocol.MapIdentity.parse(right).activePixels)});
    }

    @Test public void authoritativeMapRoleAndUpdateWireSentinelAreDistinctValues() {
        PanelPreviewProtocol.LaneSnapshot snapshot=panelSnapshot(0,623,1,"Housing",
            129,1,"Top",0,10);
        PanelPreviewProtocol.MapIdentity map=PanelPreviewProtocol.MapIdentity.parse(snapshot);
        assertEquals(0xff,map.zones.get(0).role);
        assertEquals(0x0f,PanelPreviewProtocol.ROLE_UNASSIGNED_WIRE);
    }

    @Test public void assignedCoreRoleSurvivesGeometryUpdateAndOnlyChangedDescriptorIsSent() throws Exception {
        NativeProject project=new NativeProject(new FakeProjectStorage().prefs);
        mappedZone(project,"Top",11,0,0,1,"Housing");
        GeometryTransactionManager.LayoutMapping raw=GeometryTransactionManager.mapLayout(project);
        PanelPreviewProtocol.LaneSnapshot left=panelSnapshot(0,623,1,"Housing",129,1,"Top",0,10,3);
        PanelPreviewProtocol.LaneSnapshot right=panelSnapshot(1,623,0,"",0,0,"",0,0);
        GeometryTransactionManager.LayoutMapping mapped=raw.withRolesFromSnapshots(
            new PanelPreviewProtocol.LaneSnapshot[]{left,right});
        assertEquals(3,mapped.lane(0).zones.get(0).roleFlags);
        List<PanelPreviewProtocol.UpdateDescriptor[]> batches=mapped.lane(0).descriptorBatches(left,23);
        assertEquals(1,batches.size());assertEquals(1,batches.get(0).length);
        assertEquals(3,batches.get(0)[0].roleFlags);assertEquals(11,batches.get(0)[0].count);
    }

    @Test public void liveGeometryDiffDeletesRemovedZoneBeforeApplyingShiftedRemainingZone() throws Exception {
        NativeProject beforeProject=new NativeProject(new FakeProjectStorage().prefs);
        mappedZone(beforeProject,"Top",10,0,0,1,"Housing");
        mappedZone(beforeProject,"Amber",10,0,1,1,"Housing");
        GeometryTransactionManager.MappedLane before=GeometryTransactionManager.mapLayout(beforeProject).lane(0);
        NativeProject afterProject=new NativeProject(new FakeProjectStorage().prefs);
        mappedZone(afterProject,"Amber",10,0,1,1,"Housing");
        GeometryTransactionManager.MappedLane after=GeometryTransactionManager.mapLayout(afterProject).lane(0).withRolesFrom(before);
        List<PanelPreviewProtocol.UpdateDescriptor[]> batches=after.descriptorBatches(before,23);
        assertEquals(2,batches.size());
        assertEquals(129,batches.get(0)[0].zoneId);assertEquals(0x20,batches.get(0)[0].roleFlags);
        assertEquals(0,batches.get(0)[0].count);
        assertEquals(130,batches.get(1)[0].zoneId);assertEquals(0,batches.get(1)[0].start);
        assertEquals(10,batches.get(1)[0].count);
    }

    @Test public void mtuPlannerOrdersSafeBoundaryShrinkBeforeExpansionAndRejectsOversizedCycles() {
        List<GeometryTransactionManager.MappedPanel> panels=java.util.Collections.singletonList(
            new GeometryTransactionManager.MappedPanel(1,0,"Housing",2));
        GeometryTransactionManager.MappedLane before=new GeometryTransactionManager.MappedLane(0,20,
            panels,java.util.Arrays.asList(
                new GeometryTransactionManager.MappedZone(129,1,0,0,10,"Top",3),
                new GeometryTransactionManager.MappedZone(130,1,1,10,10,"Amber",4)));
        GeometryTransactionManager.MappedLane shifted=new GeometryTransactionManager.MappedLane(0,20,
            panels,java.util.Arrays.asList(
                new GeometryTransactionManager.MappedZone(129,1,0,0,11,"Top",3),
                new GeometryTransactionManager.MappedZone(130,1,1,11,9,"Amber",4)));
        List<PanelPreviewProtocol.UpdateDescriptor[]> safe=shifted.descriptorBatches(before,23);
        assertEquals(2,safe.size());assertEquals(130,safe.get(0)[0].zoneId);
        assertEquals(129,safe.get(1)[0].zoneId);
        GeometryTransactionManager.MappedLane swapped=new GeometryTransactionManager.MappedLane(0,20,
            panels,java.util.Arrays.asList(
                new GeometryTransactionManager.MappedZone(129,1,0,10,10,"Top",3),
                new GeometryTransactionManager.MappedZone(130,1,1,0,10,"Amber",4)));
        try {swapped.descriptorBatches(before,23);fail("MTU 23 cannot atomically carry a two-zone swap");}
        catch(IllegalStateException expected){assertTrue(expected.getMessage().contains("simultaneous descriptors"));}
        List<PanelPreviewProtocol.UpdateDescriptor[]> wide=swapped.descriptorBatches(before,247);
        assertEquals(1,wide.size());assertEquals(2,wide.get(0).length);
    }

    @Test public void secondLaneCanBeginWhileFirstLaneIsActiveButNotWhileItIsCommitting() throws Exception {
        NativeProject project=new NativeProject(new FakeProjectStorage().prefs);
        mappedZone(project,"Left Top",10,0,0,1,"Left Housing");
        mappedZone(project,"Right Top",10,1,0,3,"Right Housing");
        GeometryTransactionManager.LayoutMapping mapping=GeometryTransactionManager.mapLayout(project);
        PanelPreviewProtocol.LaneSnapshot left=panelSnapshot(0,623,1,"Left Housing",129,1,"Left Top",0,10);
        PanelPreviewProtocol.LaneSnapshot right=panelSnapshot(1,623,3,"Right Housing",137,3,"Right Top",0,10);
        PanelPreviewProtocol.Status active=laneStatus(0,PanelPreviewProtocol.STATE_ACTIVE,77,7,623,7,7,7);
        PanelPreviewProtocol.Status idle=laneStatus(1,PanelPreviewProtocol.STATE_IDLE,0,7,623,7,7,7);
        GeometryTransactionManager.validateMappingAgainstSnapshots(mapping,
            new PanelPreviewProtocol.LaneSnapshot[]{left,right},
            new PanelPreviewProtocol.Status[]{active,idle});
        PanelPreviewProtocol.Status committing=laneStatus(0,PanelPreviewProtocol.STATE_COMMITTING,77,7,623,7,8,7);
        try {
            GeometryTransactionManager.validateMappingAgainstSnapshots(mapping,
                new PanelPreviewProtocol.LaneSnapshot[]{left,right},
                new PanelPreviewProtocol.Status[]{committing,idle});
            fail("COMMITTING must block a second lane BEGIN");
        } catch(IllegalArgumentException expected) {
            assertTrue(expected.getMessage().contains("stable, persistent geometry"));
        }
    }

    @Test public void proposedGs400MappingMustMatchFreshAuthoritativeMapAndFitCapacity() throws Exception {
        NativeProject project=new NativeProject(new FakeProjectStorage().prefs);
        // Reuse the existing GS400 fixture setup through the helper below.
        mappedZone(project,"Left Top",118,0,0,1,"Left Housing");
        mappedZone(project,"Left Amber",109,0,1,1,"Left Housing");
        mappedZone(project,"Left Reverse",42,0,2,1,"Left Housing");
        mappedZone(project,"Left Bottom",253,0,3,1,"Left Housing");
        mappedZone(project,"Left Trunk",101,0,4,2,"Left Trunk");
        mappedZone(project,"Right Top",118,1,0,3,"Right Housing");
        mappedZone(project,"Right Amber",109,1,1,3,"Right Housing");
        mappedZone(project,"Right Reverse",42,1,2,3,"Right Housing");
        mappedZone(project,"Right Bottom",253,1,3,3,"Right Housing");
        mappedZone(project,"Right Trunk",101,1,4,4,"Right Trunk");
        GeometryTransactionManager.LayoutMapping mapping=GeometryTransactionManager.mapLayout(project);
        PanelPreviewProtocol.LaneSnapshot left=panelSnapshot(0,623,1,"Left Housing",
            129,1,"Left Top",0,118);
        PanelPreviewProtocol.LaneSnapshot right=panelSnapshot(1,623,0,"",0,0,"",0,0);
        validateSnapshots(mapping,left,right);
    }

    @Test public void proposedGeometryCannotOverlapAnUnrelatedAuthoritativeZone() throws Exception {
        NativeProject project=new NativeProject(new FakeProjectStorage().prefs);
        mappedZone(project,"Left Top",118,0,0,1,"Left Housing");
        GeometryTransactionManager.LayoutMapping mapping=GeometryTransactionManager.mapLayout(project);
        PanelPreviewProtocol.LaneSnapshot occupied=panelSnapshot(0,623,5,"Existing",
            200,5,"Existing Zone",0,10);
        PanelPreviewProtocol.LaneSnapshot empty=panelSnapshot(1,623,0,"",0,0,"",0,0);
        try {
            validateSnapshots(mapping,occupied,empty);
            fail("existing range overlap must block preview");
        } catch(IllegalArgumentException expected) {
            assertTrue(expected.getMessage().contains("overlaps an existing CORE Zone"));
        }
    }

    @Test public void proposedPanelIdsCannotCollideWithExistingZonesOrDifferentPanels() throws Exception {
        NativeProject project=new NativeProject(new FakeProjectStorage().prefs);
        mappedZone(project,"Left Top",118,0,0,1,"Left Housing");
        GeometryTransactionManager.LayoutMapping mapping=GeometryTransactionManager.mapLayout(project);
        PanelPreviewProtocol.LaneSnapshot zoneCollision=panelSnapshot(0,623,7,"Other",
            1,7,"Existing Zone",300,10);
        PanelPreviewProtocol.LaneSnapshot empty=panelSnapshot(1,623,0,"",0,0,"",0,0);
        try {
            validateSnapshots(mapping,zoneCollision,empty);
            fail("Panel/Zone ID collision must block preview");
        } catch(IllegalArgumentException expected) {
            assertTrue(expected.getMessage().contains("collides with an existing Zone ID"));
        }
        PanelPreviewProtocol.LaneSnapshot emptyLeft=panelSnapshot(0,623,0,"",0,0,"",0,0);
        PanelPreviewProtocol.LaneSnapshot panelCollision=panelSnapshot(1,623,1,"Other",
            200,1,"Other Zone",300,10);
        try {
            validateSnapshots(mapping,emptyLeft,panelCollision);
            fail("conflicting existing Panel identity must block preview");
        } catch(IllegalArgumentException expected) {
            assertTrue("Unexpected Panel collision rejection: "+expected.getMessage(),
                expected.getMessage()!=null&&expected.getMessage().contains("conflicts with authoritative Panel"));
        }
    }

    @Test public void unsavedCapacityExpansionIsAllowedButOver2000Fails() throws Exception {
        NativeProject project=new NativeProject(new FakeProjectStorage().prefs);
        mappedZone(project,"Left Top",118,0,0,1,"Left Housing");
        GeometryTransactionManager.LayoutMapping mapping=GeometryTransactionManager.mapLayout(project);
        PanelPreviewProtocol.LaneSnapshot small=panelSnapshot(0,100,0,"",0,0,"",0,0);
        PanelPreviewProtocol.LaneSnapshot empty=panelSnapshot(1,623,0,"",0,0,"",0,0);
        // Firmware advertises the live-capacity bit separately; geometry
        // preflight must validate against the new proposed capacity, not NVS.
        validateSnapshots(mapping,small,empty);
        NativeProject oversized=new NativeProject(new FakeProjectStorage().prefs);
        mappedZone(oversized,"Too long",2001,0,0,1,"Left Housing");
        try {
            GeometryTransactionManager.LayoutMapping tooLarge=GeometryTransactionManager.mapLayout(oversized);
            validateSnapshots(tooLarge,small,empty);
            fail("Android 2000-LED limit must block preview");
        }catch(IllegalArgumentException expected){
            assertTrue("Unexpected pixel limit message: "+expected.getMessage(),
                expected.getMessage().contains("2000-LED") ||
                expected.getMessage().contains("2,000-pixel"));
        }
    }

    @Test public void transactionManagerOwnsContiguousUpdateSequenceNumbers() {
        GeometryTransactionManager manager=new GeometryTransactionManager(
            new GeometryTransactionManager.Snapshot(0,7,new byte[]{1,2,3}));
        assertTrue(manager.requestBegin());
        assertTrue(manager.acceptBegin(status(1,88,0,0x10,7,7,7,0,0,0)));
        PanelPreviewProtocol.UpdateDescriptor descriptor=
            new PanelPreviewProtocol.UpdateDescriptor(1,129,0,0,10,0);
        GeometryTransactionManager.Update first=manager.queueUpdate(
            new byte[]{1},new PanelPreviewProtocol.UpdateDescriptor[]{descriptor},23);
        assertNotNull(first);
        assertEquals(1,PanelPreviewProtocol.u32(first.packet,7));
        assertEquals(19,first.packet.length);
        assertTrue(manager.acknowledgeUpdate(status(1,88,1,0x20,7,7,7,0,0,4)));
        GeometryTransactionManager.Update second=manager.queueUpdate(
            new byte[]{2},new PanelPreviewProtocol.UpdateDescriptor[]{descriptor},23);
        assertNotNull(second);
        assertEquals(2,PanelPreviewProtocol.u32(second.packet,7));
        assertEquals(19,second.packet.length);
    }

    @Test public void committedStatusRequiresAndAcceptsAuthoritativeMapReadback() {
        GeometryTransactionManager manager=new GeometryTransactionManager(
            new GeometryTransactionManager.Snapshot(0,7,new byte[]{1,2,3}));
        assertTrue(manager.requestBegin());
        assertTrue(manager.acceptBegin(status(1,88,0,0x10,7,7,7,0,0,0)));
        PanelPreviewProtocol.UpdateDescriptor descriptor=
            new PanelPreviewProtocol.UpdateDescriptor(1,129,0,0,10,0);
        assertNotNull(manager.queueUpdate(new byte[]{4},
            new PanelPreviewProtocol.UpdateDescriptor[]{descriptor},23));
        assertTrue(manager.acknowledgeUpdate(status(1,88,1,0x20,7,7,7,0,0,4)));
        assertNotNull(manager.requestCommit(status(1,88,1,0x20,7,7,7,0,0,4)));
        manager.observeStatus(status(2,88,1,0x20,7,7,7,0,0,4));
        assertEquals(GeometryTransactionManager.State.COMMITTING,manager.state());
        PanelPreviewProtocol.Status committed=status(3,88,1,0x20,8,8,8,0,0,4);
        manager.observeStatus(committed);
        assertEquals(GeometryTransactionManager.State.RECONCILING,manager.state());
        assertFalse(manager.reconcile(committed,
            new GeometryTransactionManager.Snapshot(0,7,new byte[]{9})));
        assertTrue(manager.reconcile(committed,
            new GeometryTransactionManager.Snapshot(0,8,new byte[]{9})));
        assertEquals(GeometryTransactionManager.State.READY,manager.state());
    }

    @Test public void rawUpdateCannotBypassManagerLaneSessionOrSequenceOwnership() {
        GeometryTransactionManager manager=new GeometryTransactionManager(
            new GeometryTransactionManager.Snapshot(0,7,new byte[]{1,2,3}));
        assertTrue(manager.requestBegin());
        assertTrue(manager.acceptBegin(status(1,90,0,0x10,7,7,7,0,0,0)));
        PanelPreviewProtocol.UpdateDescriptor descriptor=
            new PanelPreviewProtocol.UpdateDescriptor(1,129,0,0,10,0);
        byte[] wrongSession=PanelPreviewProtocol.update(0,91,1,
            new PanelPreviewProtocol.UpdateDescriptor[]{descriptor},23);
        try { manager.queueUpdate(new byte[]{1},wrongSession);fail("session mismatch"); }
        catch(IllegalArgumentException expected) {}
        byte[] wrongLane=PanelPreviewProtocol.update(1,90,1,
            new PanelPreviewProtocol.UpdateDescriptor[]{descriptor},23);
        try { manager.queueUpdate(new byte[]{1},wrongLane);fail("lane mismatch"); }
        catch(IllegalArgumentException expected) {}
        byte[] wrongSequence=PanelPreviewProtocol.update(0,90,2,
            new PanelPreviewProtocol.UpdateDescriptor[]{descriptor},23);
        try { manager.queueUpdate(new byte[]{1},wrongSequence);fail("sequence mismatch"); }
        catch(IllegalArgumentException expected) {}
        assertEquals(GeometryTransactionManager.State.ACTIVE,manager.state());
    }

    @Test public void transactionManagerAllowsUnassignedRoleAfterFirmwareHostProof() {
        GeometryTransactionManager manager=new GeometryTransactionManager(
            new GeometryTransactionManager.Snapshot(0,7,new byte[]{1,2,3}));
        assertTrue(manager.requestBegin());
        assertTrue(manager.acceptBegin(status(1,89,0,0x10,7,7,7,0,0,0)));
        PanelPreviewProtocol.UpdateDescriptor unassigned=
            new PanelPreviewProtocol.UpdateDescriptor(1,129,0,0,10,
                PanelPreviewProtocol.ROLE_UNASSIGNED_WIRE);
        GeometryTransactionManager.Update update=manager.queueUpdate(new byte[]{1},
            new PanelPreviewProtocol.UpdateDescriptor[]{unassigned},23);
        assertNotNull(update);
        assertEquals(1,PanelPreviewProtocol.u32(update.packet,7));
        assertEquals(PanelPreviewProtocol.ROLE_UNASSIGNED_WIRE,update.packet[18]&0x0f);
        assertEquals(GeometryTransactionManager.State.UPDATE_PENDING,manager.state());
    }

    @Test public void panelDescriptorBatchesRespectNegotiatedAttMtuAndFirmwareLimit() throws Exception {
        NativeProject project=new NativeProject(new FakeProjectStorage().prefs);
        for(int i=0;i<5;i++)
            mappedZone(project,"Section "+i,10,0,i,1,"Housing");
        GeometryTransactionManager.MappedLane lane=GeometryTransactionManager.mapLayout(project).lane(0);

        List<PanelPreviewProtocol.UpdateDescriptor[]> at23=lane.descriptorBatches(23);
        assertEquals(5,at23.size());
        for(PanelPreviewProtocol.UpdateDescriptor[] batch:at23) {
            assertEquals(1,batch.length);
            assertTrue(PanelPreviewProtocol.update(0,9,1,batch,23).length<=20);
        }

        List<PanelPreviewProtocol.UpdateDescriptor[]> at31=lane.descriptorBatches(31);
        assertEquals(3,at31.size());
        assertEquals(2,at31.get(0).length);
        assertEquals(2,at31.get(1).length);
        assertEquals(1,at31.get(2).length);
        for(PanelPreviewProtocol.UpdateDescriptor[] batch:at31)
            assertTrue(PanelPreviewProtocol.update(0,9,1,batch,31).length<=28);

        List<PanelPreviewProtocol.UpdateDescriptor[]> at46=lane.descriptorBatches(46);
        assertEquals(2,at46.size());
        assertEquals(4,at46.get(0).length);
        assertEquals(1,at46.get(1).length);
        for(PanelPreviewProtocol.UpdateDescriptor[] batch:at46)
            assertTrue(PanelPreviewProtocol.update(0,9,1,batch,46).length<=43);

        try { lane.descriptorBatches(22);fail("below minimum ATT MTU"); }
        catch(IllegalArgumentException expected) {}
    }
    @Test public void migratedSixteenBitIdsAreReadBackAndPreviewedThroughAliases() {
        PanelPreviewProtocol.LaneSnapshot migrated=panelSnapshot(
            0,20,0x8001,"Panel 1",0x4001,0x8001,"Migrated Top",0,20,0);
        GeometryTransactionManager.MappedLane local=new GeometryTransactionManager.MappedLane(
            0,20,java.util.Collections.singletonList(
                new GeometryTransactionManager.MappedPanel(1,0,"Panel 1",1)),
            java.util.Collections.singletonList(
                new GeometryTransactionManager.MappedZone(129,1,0,1,19,"Migrated Top",0)));
        GeometryTransactionManager.MappedLane resolved=local.withRolesFromSnapshot(migrated);
        assertEquals(0x8001,resolved.panels.get(0).id);
        assertEquals(0x4001,resolved.zones.get(0).id);
        assertEquals(0x8001,resolved.zones.get(0).panelId);
        List<PanelPreviewProtocol.UpdateDescriptor[]> batches=resolved.descriptorBatches(migrated,23);
        assertEquals(1,batches.size());
        byte[] packet=PanelPreviewProtocol.update(0,77,1,batches.get(0),23);
        assertEquals(1,packet[11]&255);
        assertEquals(1,packet[12]&255);
        assertEquals(0,packet[13]&255);
        assertEquals(1,packet[14]&255);
        assertEquals(19,packet[16]&255);
        GeometryTransactionManager.MappedLane redraft=new GeometryTransactionManager.MappedLane(
            0,20,java.util.Collections.singletonList(
                new GeometryTransactionManager.MappedPanel(1,0,"Panel 1",1)),
            java.util.Collections.singletonList(
                new GeometryTransactionManager.MappedZone(129,1,0,2,18,"Migrated Top",0)))
            .withRolesFrom(resolved);
        assertEquals(0x8001,redraft.panels.get(0).id);
        assertEquals(0x4001,redraft.zones.get(0).id);
    }

    @Test public void ambiguousPanelAndZoneLowByteAliasesFailClosed() {
        PanelPreviewProtocol.LaneSnapshot ambiguousPanels=panelSnapshotWithTwo(
            0,20,new int[]{0x8001,0x8101},new int[]{0x4001,0x4002},
            new int[]{0x8001,0x8101});
        try {
            PanelPreviewProtocol.MapIdentity.parse(ambiguousPanels);
            fail("duplicate Panel low-byte aliases must be rejected");
        } catch(IllegalArgumentException expected) {
            assertTrue(expected.getMessage().contains("Ambiguous Panel low-byte alias"));
        }
        PanelPreviewProtocol.LaneSnapshot ambiguousZones=panelSnapshotWithTwo(
            0,20,new int[]{0x8001,0x8002},new int[]{0x4001,0x4101},
            new int[]{0x8001,0x8002});
        try {
            PanelPreviewProtocol.MapIdentity.parse(ambiguousZones);
            fail("duplicate Zone low-byte aliases must be rejected");
        } catch(IllegalArgumentException expected) {
            assertTrue(expected.getMessage().contains("Ambiguous Zone low-byte alias"));
        }
    }

    @Test public void migratedZonePanelMoveDeletesThenRecreatesUsingTheSameFullZoneId() {
        PanelPreviewProtocol.LaneSnapshot migrated=panelSnapshot(
            0,20,0x8001,"Panel 1",0x4001,0x8001,"Migrated Trunk",0,20,0);
        GeometryTransactionManager.MappedLane local=new GeometryTransactionManager.MappedLane(
            0,20,java.util.Arrays.asList(
                new GeometryTransactionManager.MappedPanel(1,0,"Housing",0),
                new GeometryTransactionManager.MappedPanel(2,0,"Trunk",1)),
            java.util.Collections.singletonList(
                new GeometryTransactionManager.MappedZone(129,2,4,0,20,"Migrated Trunk",0)));
        GeometryTransactionManager.MappedLane resolved=local.withRolesFromSnapshot(migrated);
        assertEquals(0x8002,resolved.panels.get(1).id);
        assertEquals(0x4001,resolved.zones.get(0).id);
        assertEquals(0x8002,resolved.zones.get(0).panelId);
        List<PanelPreviewProtocol.UpdateDescriptor[]> batches=resolved.descriptorBatches(migrated,23);
        assertEquals(2,batches.size());
        byte[] delete=PanelPreviewProtocol.update(0,77,1,batches.get(0),23);
        byte[] upsert=PanelPreviewProtocol.update(0,77,2,batches.get(1),23);
        assertEquals(1,delete[11]&255);assertEquals(1,delete[12]&255);
        assertEquals(0x20,delete[18]&255);
        assertEquals(2,upsert[11]&255);assertEquals(1,upsert[12]&255);
        assertEquals(0,upsert[18]&255);
    }

    @Test public void rapidZoneEditsRemainSerializedAndCannotCommitUnacknowledgedGeometry() {
        byte[] confirmed={3,1,4};
        GeometryTransactionManager manager=new GeometryTransactionManager(
            new GeometryTransactionManager.Snapshot(0,7,confirmed));
        assertTrue(manager.requestBegin());
        assertTrue(manager.acceptBegin(status(1,107,0,0x10,7,7,7,0,0,0)));
        PanelPreviewProtocol.UpdateDescriptor descriptor=
            new PanelPreviewProtocol.UpdateDescriptor(0x8001,0x4001,0,0,15,0);
        for(int sequence=1;sequence<=8;sequence++) {
            byte[] current={(byte)sequence};
            GeometryTransactionManager.Update sent=manager.queueUpdate(current,
                new PanelPreviewProtocol.UpdateDescriptor[]{descriptor},23);
            assertNotNull("serialized edit "+sequence,sent);
            assertEquals(sequence,PanelPreviewProtocol.u32(sent.packet,7));
            assertNull("overlapping write must not replace a pending packet",
                manager.queueUpdate(new byte[]{99},
                    new PanelPreviewProtocol.UpdateDescriptor[]{descriptor},23));
            assertNull("unacknowledged edit cannot be committed",
                manager.requestCommit(status(1,107,sequence-1,0x10+sequence,7,7,7,0,0,
                    sequence==1?0:4)));
            assertArrayEquals("shadow UPDATE must not replace confirmed map",
                confirmed,manager.lastConfirmed().wire);
            assertTrue("authoritative acknowledgment "+sequence,
                manager.acknowledgeUpdate(status(1,107,sequence,0x10+sequence,7,7,7,0,0,4)));
        }
        assertArrayEquals(new byte[]{8},manager.candidateCopy());
        assertNotNull(manager.requestCommit(status(1,107,8,0x18,7,7,7,0,0,4)));
        manager.observeStatus(status(2,107,8,0x18,7,8,7,5,9,4));
        assertEquals(GeometryTransactionManager.State.COMMITTING,manager.state());
        assertNull("delayed UPDATE during COMMITTING must be refused",
            manager.queueUpdate(new byte[]{10},
                new PanelPreviewProtocol.UpdateDescriptor[]{descriptor},23));
        assertArrayEquals(confirmed,manager.lastConfirmed().wire);
        manager.observeStatus(status(3,107,8,0x18,8,8,8,5,9,4));
        assertArrayEquals(confirmed,manager.lastConfirmed().wire);
        assertTrue(manager.reconcile(status(3,107,8,0x18,8,8,8,5,9,4),
            new GeometryTransactionManager.Snapshot(0,8,new byte[]{8})));
        assertArrayEquals(new byte[]{8},manager.lastConfirmed().wire);
    }

    @Test public void disconnectDropsPreviewAndFreshSimulatedReadbackReplacesOldManager() {
        byte[] oldWire={7,7,7};
        GeometryTransactionManager old=new GeometryTransactionManager(
            new GeometryTransactionManager.Snapshot(0,7,oldWire));
        assertTrue(old.requestBegin());
        assertTrue(old.acceptBegin(status(1,108,0,0x10,7,7,7,0,0,0)));
        old.connectionLost();
        assertEquals(GeometryTransactionManager.State.RECONCILING,old.state());
        assertArrayEquals(oldWire,old.lastConfirmed().wire);
        assertFalse(old.requestBegin());

        // Simulate the GATT reconnect lifecycle: old session managers are
        // discarded; the verified new snapshot is the sole geometry authority.
        PanelPreviewProtocol.LaneSnapshot migrated=panelSnapshot(
            0,20,0x8001,"Housing",0x4001,0x8001,"Top",0,20,0);
        assertEquals(0x8001,PanelPreviewProtocol.MapIdentity.parse(migrated).panels.get(0).id);
        GeometryTransactionManager fresh=new GeometryTransactionManager(
            new GeometryTransactionManager.Snapshot(0,7,migrated.wire));
        assertEquals(GeometryTransactionManager.State.READY,fresh.state());
        assertArrayEquals(migrated.wire,fresh.lastConfirmed().wire);
        assertFalse(java.util.Arrays.equals(old.lastConfirmed().wire,
            fresh.lastConfirmed().wire));
        assertTrue(fresh.requestBegin());
        assertTrue(fresh.acceptBegin(status(1,109,0,0x10,7,7,7,0,0,0)));
    }

}
