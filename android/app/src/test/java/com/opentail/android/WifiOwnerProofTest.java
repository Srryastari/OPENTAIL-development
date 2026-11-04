package com.opentail.android;
import org.junit.Test;
import static org.junit.Assert.*;
public class WifiOwnerProofTest {
    @Test public void fixedOwnerStopHmacVector() throws Exception {
        byte[] key=new byte[16];for(int i=0;i<16;i++)key[i]=(byte)i;
        String nonce="000102030405060708090a0b0c0d0e0f";
        assertEquals("28640a710b25a8cb1d37f8f3d9c61547730509ad6395488d433ebe11fba1f466",
                WifiOwnerProof.stopProof(key,nonce));
        assertNotEquals(WifiOwnerProof.stopProof(key,nonce),
                WifiOwnerProof.stopProof(key,"000102030405060708090a0b0c0d0e0e"));
    }
    @Test public void rejectsInvalidAndUnenrolledKey() throws Exception {
        assertNull(WifiOwnerProof.decodeNonce("not-a-challenge"));
        try {WifiOwnerProof.stopProof(null,"000102030405060708090a0b0c0d0e0f");fail();}
        catch(IllegalArgumentException expected){}
        try {WifiOwnerProof.stopProof(new byte[16],"bad");fail();}
        catch(IllegalArgumentException expected){}
    }
}
