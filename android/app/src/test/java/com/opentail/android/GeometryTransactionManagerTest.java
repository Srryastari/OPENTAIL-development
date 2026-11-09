package com.opentail.android;

import org.junit.Test;
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
        manager.observeStatus(status(2,95,0,0x55,7,8,7,5,1,5));
        assertEquals(GeometryTransactionManager.State.COMMITTING,manager.state());
        assertNull("delayed geometry UPDATE must be rejected during COMMITTING",
            manager.queueUpdate(new byte[]{9},new byte[]{1,2,3}));
        assertNull("another COMMIT must not be emitted while COMMITTING",
            manager.requestCommit(status(2,95,0,0x55,7,8,7,5,1,5)));
        // Firmware retains the last rejection diagnostic after COMMITTING.
        // A stale error must not mask the authoritative terminal COMMITTED state.
        manager.observeStatus(status(3,95,0,0x55,8,8,8,5,1,5));
        assertEquals(GeometryTransactionManager.State.RECONCILING,manager.state());
        assertArrayEquals(new byte[]{1,2,3},manager.lastConfirmed().wire);
    }
}
