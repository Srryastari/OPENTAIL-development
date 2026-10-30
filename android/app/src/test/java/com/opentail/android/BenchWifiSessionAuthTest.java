package com.opentail.android;

import org.junit.Test;
import static org.junit.Assert.*;

/** No Android networking or device needed; validates token admission logic. */
public class BenchWifiSessionAuthTest {
    @Test public void acceptsOnlyVolatileBenchToken() throws Exception {
        assertEquals("0123456789abcdef0123456789abcdef",
            BenchWifiSessionAuth.parseSession(
                "{\"protocol\":1,\"token\":\"0123456789abcdef0123456789abcdef\",\"bench_only\":true,\"volatile\":true}"));
    }
    @Test public void rejectsMissingFlagsAndUntrustedValues() throws Exception {
        String token="0123456789abcdef0123456789abcdef";
        for(String data:new String[]{
            "{\"protocol\":2,\"token\":\""+token+"\",\"bench_only\":true,\"volatile\":true}",
            "{\"protocol\":1,\"token\":\""+token+"\",\"bench_only\":false,\"volatile\":true}",
            "{\"protocol\":1,\"token\":\""+token+"\",\"bench_only\":true,\"volatile\":false}",
            "{\"protocol\":1,\"token\":\"hello\",\"bench_only\":true,\"volatile\":true}",
            "{\"protocol\":1,\"bench_only\":true,\"volatile\":true}"
        }) {
            try { BenchWifiSessionAuth.parseSession(data); fail("Accepted invalid bench response"); }
            catch(IllegalArgumentException expected) { }
        }
    }
}
