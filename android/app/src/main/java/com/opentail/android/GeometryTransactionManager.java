package com.opentail.android;

import java.util.Arrays;

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

    synchronized State state() { return state; }
    synchronized Snapshot lastConfirmed() { return copy(lastConfirmed); }
    synchronized byte[] candidateCopy() { return candidate==null?null:Arrays.copyOf(candidate,candidate.length); }
    synchronized boolean syncUncertain() { return syncUncertain; }
    synchronized String lastError() { return lastError; }
    synchronized long sessionId() { return sessionId; }

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
        candidate=Arrays.copyOf(candidateWire,candidateWire.length);
        pendingSequence=nextSequence;
        pendingPacket=Arrays.copyOf(encodedPacket,encodedPacket.length);
        state=State.UPDATE_PENDING;
        return new Update(pendingSequence,pendingPacket);
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
        if(status.lastRejectedReason==PanelPreviewProtocol.ERR_COMMITTING_LOCKED ||
           status.committingRejectCount>0 && status.state==PanelPreviewProtocol.STATE_COMMITTING) {
            // A delayed UPDATE rejection is not a commit failure. Preserve the
            // confirmed image and continue observing this exact transaction.
            if(status.state==PanelPreviewProtocol.STATE_COMMITTING)state=State.COMMITTING;
            return;
        }
        if(status.hardSyncFault) {
            requireReconciliation("CORE geometry sequence fault requires cancel and authoritative reload");
            return;
        }
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
        }
    }

    synchronized void connectionLost() {
        if(state!=State.READY)requireReconciliation("BLE disconnected during geometry transaction");
    }

    synchronized boolean reconcile(PanelPreviewProtocol.Status status,Snapshot authoritative) {
        if(status==null||authoritative==null||authoritative.lane!=lastConfirmed.lane||
           status.lane!=lastConfirmed.lane||authoritative.revision!=status.activeRevision||
           status.hardSyncFault||status.state==PanelPreviewProtocol.STATE_COMMITTING||
           (status.state==PanelPreviewProtocol.STATE_COMMITTED &&
            status.activeRevision!=status.renderRevision))return false;

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
           state==State.RECONCILING && candidate==null && status.sessionId!=sessionId) {
            lastConfirmed=copy(authoritative);
            candidate=null;pendingPacket=null;pendingSequence=0;sessionId=0;
            targetRevision=-1;syncUncertain=false;lastError=null;state=State.READY;
            return true;
        }
        if(state==State.COMMIT_REQUESTED||state==State.COMMITTING)return false;
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
