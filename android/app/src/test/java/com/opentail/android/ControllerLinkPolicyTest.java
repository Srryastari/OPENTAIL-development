package com.opentail.android;
import org.junit.Test;
import static org.junit.Assert.*;
public class ControllerLinkPolicyTest {
    @Test public void blePreferredThenWifiReadOnlyThenNoConnection() {
        ControllerLinkPolicy p=new ControllerLinkPolicy();
        assertEquals(ControllerLinkPolicy.Transport.NONE,p.diagnosticTransport(1));
        assertTrue(p.bleAuthorized("84c7bb5a6649"));
        long lease=p.lease();
        assertTrue(p.wifiObserved("84C7BB5A6649",1000,lease));
        assertEquals(ControllerLinkPolicy.Transport.BLE,p.diagnosticTransport(1001));
        assertEquals(ControllerLinkPolicy.Transport.BLE,p.writeTransport());
        p.bleUnavailable();
        assertEquals(ControllerLinkPolicy.Transport.WIFI,p.diagnosticTransport(1002));
        assertEquals(ControllerLinkPolicy.Transport.NONE,p.writeTransport());
        assertEquals(ControllerLinkPolicy.Transport.NONE,p.diagnosticTransport(15001));
    }
    @Test public void rejectsMismatchedAndStaleController() {
        ControllerLinkPolicy p=new ControllerLinkPolicy();
        assertTrue(p.bleAuthorized("001122334455"));
        long oldLease=p.lease();
        assertFalse(p.wifiObserved("001122334456",1,oldLease));
        p.switchController();
        assertFalse(p.wifiObserved("001122334455",2,oldLease));
        assertEquals(ControllerLinkPolicy.Transport.NONE,p.diagnosticTransport(2));
        assertFalse(p.bleAuthorized("bad"));
    }
    @Test public void bleInfoParsingIsBackwardCompatible() {
        String info="OpenTail CORE 0.1.31-alpha | BLE HMAC enrollment bench v2 | fx-tertiary13-v1 | id=84c7bb5a6649";
        assertEquals("84C7BB5A6649",ControllerLinkPolicy.idFromBleInfo(info));
        assertNull(ControllerLinkPolicy.idFromBleInfo("OpenTail CORE old | BLE HMAC"));
        assertNull(ControllerLinkPolicy.idFromBleInfo("x | id=invalid"));
    }
}
