package com.opentail.android;

import org.junit.Test;
import static org.junit.Assert.*;

/** Reconnect -> BLE initialization -> idle -> one diagnostic, without Save. */
public class PixelProbeRequestQueueTest {
    @Test public void reconnectThenIdleOffersOneRealProbeOnO1() {
        PixelProbeRequestQueue queue=new PixelProbeRequestQueue();
        assertFalse(queue.pending());
        // Any old connection's unsent owner request is canceled on reconnect.
        assertTrue(queue.offer(1));
        queue.clear();
        assertFalse(queue.pending());
        // Normal snapshot/status initialization must not itself create a probe.
        assertEquals(-1,queue.take());
        assertTrue(queue.offer(0));
        assertTrue(queue.pending());
        // Owner's only tap is retained while other BLE operations drain.
        assertFalse(queue.offer(0));
        assertEquals(0,queue.take());
        assertEquals(-1,queue.take());
        assertFalse(queue.pending());
    }

    @Test public void onlyOneLaneAndNoFabricatedOrRepeatedProbe() {
        PixelProbeRequestQueue queue=new PixelProbeRequestQueue();
        assertFalse(queue.offer(-1));
        assertFalse(queue.offer(2));
        assertEquals(-1,queue.take());
        assertTrue(queue.offer(1));
        assertFalse(queue.offer(0)); // switching output never steals owner request
        assertEquals(1,queue.take());
        assertTrue(queue.offer(0));
        queue.clear();               // GATT disconnect never replays the old probe
        assertEquals(-1,queue.take());
    }
    @Test public void safetyStopCancelsQueuedProbeInsteadOfReplayingAfterAck() {
        PixelProbeRequestQueue queue=new PixelProbeRequestQueue();
        assertTrue(queue.offer(0));
        // BleDiscovery.enqueueWrite(STOP) clears this before any ATT START.
        queue.clear();
        assertEquals(-1,queue.take());
        assertFalse(queue.pending());
    }
}
