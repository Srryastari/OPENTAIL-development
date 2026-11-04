package com.opentail.android;
import java.nio.charset.StandardCharsets;
import javax.crypto.Mac;
import javax.crypto.spec.SecretKeySpec;
/** Domain-scoped owner MAC for exactly POST /api/v1/owner/stop.
 * The BLE owner key never goes into an HTTP payload or log.
 */
final class WifiOwnerProof {
    private static final byte[] PREFIX =
        "OpenTail-WiFi-v1\nPOST\n/api/v1/owner/stop\n".getBytes(StandardCharsets.US_ASCII);
    static byte[] decodeNonce(String text) {
        if(text==null || !text.matches("(?i)[0-9a-f]{32}"))return null;
        byte[] nonce=new byte[16];
        for(int i=0;i<nonce.length;i++) {
            int hi=Character.digit(text.charAt(i*2),16);
            int lo=Character.digit(text.charAt(i*2+1),16);
            nonce[i]=(byte)((hi<<4)|lo);
        }
        return nonce;
    }
    static String stopProof(byte[] key,String nonceHex) throws Exception {
        byte[] nonce=decodeNonce(nonceHex);
        if(key==null || key.length!=16 || nonce==null)
            throw new IllegalArgumentException("Owner key or nonce invalid");
        byte[] message=new byte[PREFIX.length+nonce.length];
        System.arraycopy(PREFIX,0,message,0,PREFIX.length);
        System.arraycopy(nonce,0,message,PREFIX.length,nonce.length);
        Mac hmac=Mac.getInstance("HmacSHA256");
        hmac.init(new SecretKeySpec(key,"HmacSHA256"));
        byte[] digest=hmac.doFinal(message);
        final char[] hex="0123456789abcdef".toCharArray();
        char[] str=new char[digest.length*2];
        for(int i=0;i<digest.length;i++){
            str[2*i]=hex[(digest[i]&255)>>4];
            str[2*i+1]=hex[digest[i]&15];
        }
        java.util.Arrays.fill(message,(byte)0);
        java.util.Arrays.fill(digest,(byte)0);
        return new String(str);
    }
    private WifiOwnerProof(){}
}
