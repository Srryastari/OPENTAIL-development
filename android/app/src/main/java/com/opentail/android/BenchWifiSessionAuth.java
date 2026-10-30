package com.opentail.android;

import android.content.Context;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import org.json.JSONObject;
import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;

/**
 * Bench-only, button-free initial BLE authorization through the existing
 * password-protected OpenTail CORE Wi-Fi AP. No token is logged or persisted.
 * Uses a Wi-Fi Network explicitly, never cellular or unrestricted web access.
 * Off-phone API calls must run on a worker thread.
 */
final class BenchWifiSessionAuth {
    static final String HOST="192.168.4.1";
    static final String URL_TEXT="http://192.168.4.1/api/v1/ble/session";
    private BenchWifiSessionAuth() { }

    static String request(Context context) throws Exception {
        ConnectivityManager manager =
            (ConnectivityManager)context.getSystemService(Context.CONNECTIVITY_SERVICE);
        if(manager==null)throw new IllegalStateException("Wi-Fi network service unavailable");
        Network wifi=null;
        for(Network network:manager.getAllNetworks()){
            NetworkCapabilities caps=manager.getNetworkCapabilities(network);
            if(caps!=null && caps.hasTransport(NetworkCapabilities.TRANSPORT_WIFI)){
                wifi=network; break;
            }
        }
        if(wifi==null)throw new IllegalStateException("Connect phone to OpenTail-CORE-Alpha Wi-Fi for no-BOOT pairing");
        HttpURLConnection conn=(HttpURLConnection)wifi.openConnection(new URL(URL_TEXT));
        conn.setInstanceFollowRedirects(false);
        conn.setConnectTimeout(2500);
        conn.setReadTimeout(2500);
        conn.setRequestMethod("GET");
        conn.setUseCaches(false);
        try {
            if(conn.getResponseCode()!=200)
                throw new IllegalStateException("CORE bench Wi-Fi session unavailable");
            String contentType=conn.getContentType();
            if(contentType==null || !contentType.toLowerCase(java.util.Locale.ROOT).contains("application/json"))
                throw new IllegalArgumentException("Unexpected CORE session response type");
            ByteArrayOutputStream body=new ByteArrayOutputStream();
            try(InputStream in=conn.getInputStream()){
                byte[] chunk=new byte[256];
                int n;
                while((n=in.read(chunk))!=-1){
                    if(body.size()+n>512)throw new IllegalArgumentException("CORE session response too large");
                    body.write(chunk,0,n);
                }
            }
            return parseSession(new String(body.toByteArray(),StandardCharsets.UTF_8));
        }finally{conn.disconnect();}
    }
    static String parseSession(String text) throws Exception {
        JSONObject json=new JSONObject(text);
        if(json.optInt("protocol",-1)!=1 || !json.optBoolean("bench_only",false) ||
           !json.optBoolean("volatile",false))
            throw new IllegalArgumentException("Unsupported CORE bench authorization protocol");
        String token=json.optString("token","");
        if(!token.matches("[A-Fa-f0-9]{32}"))
            throw new IllegalArgumentException("Invalid CORE session token");
        return token;
    }
}
