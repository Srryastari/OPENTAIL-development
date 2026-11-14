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
    @Test public void reconnectIdleProbeOwnsGattUntilCallbackAndSaveAlsoBlocksPolling() {
        PixelProbeRequestQueue queue=new PixelProbeRequestQueue();
        assertFalse(queue.blocksBackgroundTransfer(false,false));
        assertTrue(queue.offer(0));
        assertTrue(queue.blocksBackgroundTransfer(false,false));
        assertEquals(0,queue.take());
        // Dispatch consumes the queue, but ATT remains owned until the callback.
        assertTrue(queue.blocksBackgroundTransfer(true,false));
        assertFalse(queue.blocksBackgroundTransfer(false,false));
        assertTrue(queue.blocksBackgroundTransfer(false,true));
        assertTrue(queue.blocksBackgroundTransfer(true,true));
        queue.clear(); // reconnect creates no synthetic diagnostic
        assertFalse(queue.blocksBackgroundTransfer(false,false));
    }
}
