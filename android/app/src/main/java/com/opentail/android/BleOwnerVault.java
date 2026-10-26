package com.opentail.android;

import android.content.Context;
import android.content.SharedPreferences;
import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyProperties;
import android.util.Base64;
import java.security.KeyStore;
import java.security.SecureRandom;
import javax.crypto.Cipher;
import javax.crypto.KeyGenerator;
import javax.crypto.SecretKey;
import javax.crypto.spec.GCMParameterSpec;

/** Bench owner key stored encrypted under a hardware-backed key when Android supports it.
 * Android Keystore hardware protection is device dependent. Never log or export the key. */
final class BleOwnerVault {
    private static final String ALIAS="OpenTail.BLE.Owner.1";
    private static final String PREFS="opentail_ble_owner";
    private static SecretKey key() throws Exception {
        KeyStore store=KeyStore.getInstance("AndroidKeyStore");store.load(null);
        java.security.Key existing=store.getKey(ALIAS,null);
        if(existing instanceof SecretKey)return (SecretKey)existing;
        KeyGenerator generator=KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES,"AndroidKeyStore");
        generator.init(new KeyGenParameterSpec.Builder(ALIAS,KeyProperties.PURPOSE_ENCRYPT|KeyProperties.PURPOSE_DECRYPT)
            .setBlockModes(KeyProperties.BLOCK_MODE_GCM).setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
            .setRandomizedEncryptionRequired(true).build());
        return generator.generateKey();
    }
    static void save(Context c,String address,byte[] value) throws Exception {
        if(value==null||value.length!=16)throw new IllegalArgumentException("Owner key must have 16 bytes");
        Cipher cipher=Cipher.getInstance("AES/GCM/NoPadding");cipher.init(Cipher.ENCRYPT_MODE,key());
        byte[] iv=cipher.getIV(),encrypted=cipher.doFinal(value);
        byte[] packed=new byte[1+iv.length+encrypted.length];packed[0]=(byte)iv.length;
        System.arraycopy(iv,0,packed,1,iv.length);System.arraycopy(encrypted,0,packed,1+iv.length,encrypted.length);
        if(!c.getSharedPreferences(PREFS,Context.MODE_PRIVATE).edit().putString(address,Base64.encodeToString(packed,Base64.NO_WRAP)).commit())
            throw new IllegalStateException("Could not persist BLE owner key");
    }
    static byte[] load(Context c,String address){
        try{
            SharedPreferences prefs=c.getSharedPreferences(PREFS,Context.MODE_PRIVATE);
            String stored=prefs.getString(address,null);if(stored==null)return null;
            byte[] packed=Base64.decode(stored,Base64.DEFAULT);
            int n=packed[0]&255;if(n!=12||packed.length<1+n+16)return null;
            Cipher cipher=Cipher.getInstance("AES/GCM/NoPadding");
            cipher.init(Cipher.DECRYPT_MODE,key(),new GCMParameterSpec(128,packed,1,n));
            byte[] decoded=cipher.doFinal(packed,1+n,packed.length-1-n);
            return decoded.length==16?decoded:null;
        }catch(Exception ex){return null;}
    }
    private BleOwnerVault(){}
}
