package com.opentail.android;

import java.util.Locale;

/** Correlate BLE and Wi-Fi to the SAME physical CORE, without modifying lighting.
 * BLE remains the only authenticated command transport until Wi-Fi has its own
 * owner proof. A matching Wi-Fi status response is READ-ONLY, never a write grant.
 */
final class ControllerLinkPolicy {
    enum Transport { BLE, WIFI, NONE }
    static final long WIFI_STATUS_MAX_AGE_MS = 12_000L;
    private String controllerId;
    private boolean bleAuthorized;
    private long wifiSeenAtMs = -1L;
    private long lease;

    static String normalizedId(String value) {
        if (value == null || !value.matches("(?i)[0-9a-f]{12}")) return null;
        return value.toUpperCase(Locale.ROOT);
    }
    static String idFromBleInfo(String info) {
        if (info == null) return null;
        int i = info.indexOf(" | id=");
        if (i < 0) return null;
        String value = info.substring(i + 6).split("[ |]", 2)[0];
        return normalizedId(value);
    }
    synchronized long lease() { return lease; }
    synchronized String controllerId() { return controllerId; }

    /** Call ONLY after successful BLE owner-HMAC authorization. */
    synchronized boolean bleAuthorized(String authenticatedControllerId) {
        String id = normalizedId(authenticatedControllerId);
        if (id == null) return false;
        if (!id.equals(controllerId)) {
            controllerId = id;
            wifiSeenAtMs = -1L;
            lease++;
        }
        bleAuthorized = true;
        return true;
    }
    /** Explicit controller switch invalidates previous Wi-Fi identity. */
    synchronized void switchController() {
        controllerId = null;
        bleAuthorized = false;
        wifiSeenAtMs = -1L;
        lease++;
    }
    synchronized void bleUnavailable() { bleAuthorized = false; }
    /** Late async probes cannot select an old controller after a switch. */
    synchronized boolean wifiObserved(String observedId,long nowMs,long probeLease) {
        if (probeLease != lease || controllerId == null ||
            !controllerId.equals(normalizedId(observedId)) || nowMs < 0) return false;
        wifiSeenAtMs = nowMs;
        return true;
    }
    synchronized Transport diagnosticTransport(long nowMs) {
        if (bleAuthorized) return Transport.BLE;
        if (controllerId != null && wifiSeenAtMs >= 0 && nowMs >= wifiSeenAtMs &&
            nowMs - wifiSeenAtMs <= WIFI_STATUS_MAX_AGE_MS) return Transport.WIFI;
        return Transport.NONE;
    }
    synchronized Transport writeTransport() {
        return bleAuthorized ? Transport.BLE : Transport.NONE;
    }
}
