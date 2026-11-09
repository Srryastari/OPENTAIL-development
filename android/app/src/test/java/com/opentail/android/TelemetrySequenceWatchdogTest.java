package com.opentail.android;

import org.junit.Test;
import static org.junit.Assert.*;

public class TelemetrySequenceWatchdogTest {
    private static void put16(byte[] b,int at,int v){b[at]=(byte)v;b[at+1]=(byte)(v>>>8);}
    private static void put32(byte[] b,int at,long v){for(int i=0;i<4;i++)b[at+i]=(byte)(v>>>(8*i));}
    private static PanelPreviewProtocol.Status status(int state,long session,long seq,
        long missing,long outOfOrder,long duplicates,long conflicts,long committing,
        int rejectedReason,int flags) {
        byte[] b=new byte[61];b[0]=2;b[1]=(byte)state;b[2]=0;b[3]=3;
        put32(b,4,session);put16(b,8,7);put16(b,10,944);put16(b,12,5000);
        put32(b,14,seq);put32(b,18,0x1234);put16(b,22,7);put16(b,24,7);put16(b,26,7);
        b[29]=1;b[39]=(byte)rejectedReason;
        put32(b,40,missing);put32(b,44,outOfOrder);put32(b,48,duplicates);
        put32(b,52,conflicts);put32(b,56,committing);b[60]=(byte)flags;
        return PanelPreviewProtocol.Status.parse(b);
    }

    @Test public void contiguousAcceptedSequencesRemainHealthy() {
        TelemetrySequenceWatchdog w=new TelemetrySequenceWatchdog();
        assertFalse(w.observe(status(1,10,1,0,0,0,0,0,0,4)).warning);
        TelemetrySequenceWatchdog.Decision d=w.observe(status(1,10,2,0,0,0,0,0,0,4));
        assertFalse(d.warning);assertFalse(d.hardFault);assertFalse(d.requiresReconciliation);
    }

    @Test public void missingSequenceCounterLatchesHardFault() {
        TelemetrySequenceWatchdog w=new TelemetrySequenceWatchdog();
        w.observe(status(1,10,1,0,0,0,0,0,0,4));
        TelemetrySequenceWatchdog.Decision d=w.observe(status(1,10,1,2,0,0,0,0,6,7));
        assertTrue(d.warning);assertTrue(d.hardFault);assertTrue(d.requiresReconciliation);
        assertFalse(w.reconcile(status(4,10,1,2,0,0,0,0,6,7),true));
        PanelPreviewProtocol.Status recovered=status(4,11,0,0,0,0,0,0,0,0);
        w.observe(recovered);
        assertFalse(w.reconcile(recovered,false));
        assertTrue(w.reconcile(recovered,true));
        assertFalse(w.snapshot().warning);
    }

    @Test public void acceptedSequenceJumpAndOutOfOrderCounterAreHardFaults() {
        TelemetrySequenceWatchdog jump=new TelemetrySequenceWatchdog();
        jump.observe(status(1,20,1,0,0,0,0,0,0,4));
        assertTrue(jump.observe(status(1,20,3,0,0,0,0,0,0,4)).hardFault);
        TelemetrySequenceWatchdog order=new TelemetrySequenceWatchdog();
        order.observe(status(1,21,2,0,0,0,0,0,0,4));
        assertTrue(order.observe(status(1,21,2,0,1,0,0,0,7,7)).hardFault);
    }

    @Test public void duplicateRequiresReadbackButCanReconcileSameSession() {
        TelemetrySequenceWatchdog w=new TelemetrySequenceWatchdog();
        PanelPreviewProtocol.Status first=status(1,30,1,0,0,0,0,0,0,4);
        w.observe(first);
        PanelPreviewProtocol.Status duplicate=status(1,30,1,0,0,1,0,0,8,5);
        TelemetrySequenceWatchdog.Decision d=w.observe(duplicate);
        assertTrue(d.warning);assertFalse(d.hardFault);assertTrue(d.requiresReconciliation);
        assertTrue(w.reconcile(duplicate,true));
        assertFalse(w.snapshot().warning);
        assertFalse(w.observe(duplicate).warning); // firmware's warning bit remains latched; local reconciliation is authoritative.
    }

    @Test public void sameRejectedReasonWithNewRejectedSequenceIsDetected() {
        TelemetrySequenceWatchdog w=new TelemetrySequenceWatchdog();
        w.observe(status(1,64,1,0,0,0,0,0,0,4));
        byte[] b=new byte[61];b[0]=2;b[1]=1;b[2]=0;b[3]=3;
        put32(b,4,64);put16(b,8,7);put16(b,10,944);put16(b,12,5000);
        put32(b,14,1);put32(b,18,0x1234);put16(b,22,7);put16(b,24,7);put16(b,26,7);
        put32(b,35,2);b[39]=5;b[60]=5;
        TelemetrySequenceWatchdog.Decision d=w.observe(PanelPreviewProtocol.Status.parse(b));
        assertTrue(d.warning);assertFalse(d.hardFault);
        assertEquals(2L,w.snapshot().lastRejectedSequence);
    }

    @Test public void disconnectDuringActivePreviewRequiresFreshSessionReconciliation() {
        TelemetrySequenceWatchdog w=new TelemetrySequenceWatchdog();
        PanelPreviewProtocol.Status active=status(1,51,0,0,0,0,0,0,0,0);
        w.observe(active);
        TelemetrySequenceWatchdog.Decision disconnected=w.markDisconnected();
        assertTrue(disconnected.warning);assertTrue(disconnected.hardFault);
        PanelPreviewProtocol.Status stillActive=status(1,51,0,0,0,0,0,0,0,0);
        assertTrue(w.observe(stillActive).hardFault);
        assertFalse(w.reconcile(stillActive,true));
        PanelPreviewProtocol.Status fresh=status(0,0,0,0,0,0,0,0,0,0);
        w.observe(fresh);
        assertTrue(w.reconcile(fresh,true));
        assertFalse(w.snapshot().warning);
    }

    @Test public void malformedStatusLatchesWarningAndActiveSessionFault() {
        TelemetrySequenceWatchdog w=new TelemetrySequenceWatchdog();
        PanelPreviewProtocol.Status active=status(1,61,0,0,0,0,0,0,0,0);
        w.observe(active);
        TelemetrySequenceWatchdog.Decision malformed=w.markMalformedTelemetry("truncated status");
        assertTrue(malformed.warning);assertTrue(malformed.hardFault);
        assertFalse(w.reconcile(active,true));
        PanelPreviewProtocol.Status fresh=status(0,0,0,0,0,0,0,0,0,0);
        w.observe(fresh);
        assertTrue(w.reconcile(fresh,true));
    }

    @Test public void renderRevisionMismatchPreventsTelemetryReconciliation() {
        TelemetrySequenceWatchdog w=new TelemetrySequenceWatchdog();
        PanelPreviewProtocol.Status active=status(1,63,0,0x10,7,7,7,0,0,0);
        w.observe(active);
        PanelPreviewProtocol.Status staleRender=status(1,63,0,0x10,7,7,6,0,0,0);
        assertFalse(w.reconcile(staleRender,true));
        assertTrue(w.snapshot().warning);
    }

    @Test public void committingRejectionWaitsForTerminalCommitBeforeClear() {
        TelemetrySequenceWatchdog w=new TelemetrySequenceWatchdog();
        w.observe(status(1,40,1,0,0,0,0,0,0,4));
        PanelPreviewProtocol.Status locked=status(2,40,1,0,0,0,0,1,5,5);
        assertTrue(w.observe(locked).warning);
        assertFalse(w.reconcile(locked,true));
        PanelPreviewProtocol.Status committed=status(3,40,1,0,0,0,0,1,5,5);
        assertTrue(w.reconcile(committed,true));
    }
}
