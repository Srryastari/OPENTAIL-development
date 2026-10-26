package com.opentail.android;

import android.content.Context;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import org.json.JSONObject;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;

/** Connects only to the known CORE access point and verifies its protocol before opening Studio. */
final class CoreConnection {
    static final String ORIGIN = "http://192.168.4.1";
    static final String STUDIO = ORIGIN + "/studio";
    static final String PROTOCOL = "opentail-core-alpha-v1";
    private CoreConnection() {}

    /** Wi-Fi transport detection for UI; CORE must also have passed verify() before coreMode becomes true. */
    static boolean hasWifiTransport(Context context) {
        ConnectivityManager cm = (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE);
        if (cm == null) return false;
        for (Network network : cm.getAllNetworks()) {
            NetworkCapabilities caps = cm.getNetworkCapabilities(network);
            if (caps != null && caps.hasTransport(NetworkCapabilities.TRANSPORT_WIFI)) return true;
        }
        return false;
    }

    static boolean bindToWifi(Context context) {
        ConnectivityManager cm = (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE);
        if (cm == null) return false;
        for (Network network : cm.getAllNetworks()) {
            NetworkCapabilities caps = cm.getNetworkCapabilities(network);
            if (caps != null && caps.hasTransport(NetworkCapabilities.TRANSPORT_WIFI)) {
                return cm.bindProcessToNetwork(network);
            }
        }
        return false;
    }

    static void releaseWifi(Context context) {
        ConnectivityManager cm = (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE);
        if (cm != null) cm.bindProcessToNetwork(null);
    }

    static String verify() throws Exception {
        HttpURLConnection conn = (HttpURLConnection) new URL(ORIGIN + "/api/v1/capabilities").openConnection();
        conn.setRequestMethod("GET");
        conn.setConnectTimeout(3000);
        conn.setReadTimeout(3000);
        conn.setUseCaches(false);
        try {
            if (conn.getResponseCode() != 200) throw new Exception("CORE did not return HTTP 200");
            try (InputStream stream = conn.getInputStream()) {
                byte[] payload = readLimited(stream, 32768);
                JSONObject doc = new JSONObject(new String(payload, StandardCharsets.UTF_8));
                if (!PROTOCOL.equals(doc.optString("protocol"))) {
                    throw new Exception("Device does not advertise the OpenTail CORE protocol");
                }
                if (!doc.optBoolean("bench_only", false)) {
                    throw new Exception("Unexpected device safety profile; refusing connection");
                }
                return PROTOCOL;
            }
        } finally {
            conn.disconnect();
        }
    }

    /** Volatile per-boot BLE authorization. Caller must already be on CORE Wi-Fi. */
    static String bleSessionToken() throws Exception {
        HttpURLConnection conn=(HttpURLConnection)new URL(ORIGIN + "/api/v1/ble/session").openConnection();
        conn.setRequestMethod("GET");conn.setConnectTimeout(3000);conn.setReadTimeout(3000);conn.setUseCaches(false);
        try {
            if(conn.getResponseCode()!=200)throw new Exception("CORE BLE authorization requires firmware v0.1.20+");
            try(InputStream stream=conn.getInputStream()){
                JSONObject v=new JSONObject(new String(readLimited(stream,1024),StandardCharsets.UTF_8));
                if(v.optInt("protocol")!=1||!v.optBoolean("bench_only"))throw new Exception("Unsupported BLE control profile");
                String token=v.optString("token");
                if(!token.matches("[a-f0-9]{32}"))throw new Exception("Invalid BLE session token from CORE");
                return token;
            }
        } finally {conn.disconnect();}
    }

    private static byte[] readLimited(InputStream stream, int limit) throws Exception {
        java.io.ByteArrayOutputStream out = new java.io.ByteArrayOutputStream();
        byte[] buf = new byte[4096];
        int total = 0;
        for (int n; (n = stream.read(buf)) != -1;) {
            total += n;
            if (total > limit) throw new Exception("CORE response is unexpectedly large");
            out.write(buf, 0, n);
        }
        return out.toByteArray();
    }
}
