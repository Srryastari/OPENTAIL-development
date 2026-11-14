package com.opentail.android;

/**
 * One owner-requested diagnostic per connection. GATT admission is controlled
 * exclusively by BleDiscovery.pumpQueue(); this class never starts an ATT write.
 * No probe may be synthesized by connection, status, or readback initialization.
 */
final class PixelProbeRequestQueue {
    private int queuedLane=-1;

    synchronized boolean offer(int lane) {
        if(lane<0||lane>1||queuedLane!=-1)return false;
        queuedLane=lane;
        return true;
    }

    synchronized boolean pending() { return queuedLane>=0; }

    synchronized int take() {
        int lane=queuedLane;
        queuedLane=-1;
        return lane;
    }

    synchronized void clear() { queuedLane=-1; }
}
