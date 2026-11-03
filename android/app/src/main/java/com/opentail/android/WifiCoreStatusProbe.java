package com.opentail.android;

import android.content.Context;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.os.Handler;
import android.os.Looper;
import org.json.JSONObject;
import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/** Read-only status from the already-joined CORE Wi-Fi AP.
 * Never joins a network, drives LEDs, transfers credentials or authenticates writes.
 * Uses the Wi-Fi Network explicitly, even when mobile data is Android's default.
 */
final class WifiCoreStatusProbe {
    interface Listener { void result(JSONObject verifiedStatus); }
    private static final URL STATUS_URL = url();
    private static URL url() {
        try { return new URL("http://192.168.4.1/api/v1/status"); }
        catch (java.net.MalformedURLException e) { throw new AssertionError(e); }
    }
    private final ConnectivityManager connectivity;
    private final ExecutorService worker = Executors.newSingleThreadExecutor();
    private final Handler main = new Handler(Looper.getMainLooper());
    private boolean running;
    private boolean closed;

    WifiCoreStatusProbe(Context context) {
        connectivity = (ConnectivityManager)context.getSystemService(Context.CONNECTIVITY_SERVICE);
    }

    synchronized void probe(String expectedId, Listener listener) {
        final String expected = ControllerLinkPolicy.normalizedId(expectedId);
        if (expected == null || closed || running || connectivity == null) return;
        running = true;
        worker.execute(() -> {
            JSONObject verified = null;
            try {
                for (Network network : connectivity.getAllNetworks()) {
                    NetworkCapabilities caps = connectivity.getNetworkCapabilities(network);
                    if (caps == null || !caps.hasTransport(NetworkCapabilities.TRANSPORT_WIFI)) continue;
                    HttpURLConnection connection = null;
                    try {
                        connection = (HttpURLConnection)network.openConnection(STATUS_URL);
                        connection.setConnectTimeout(1000);
                        connection.setReadTimeout(1000);
                        connection.setInstanceFollowRedirects(false);
                        connection.setRequestMethod("GET");
                        if (connection.getResponseCode() != 200) continue;
                        StringBuilder response = new StringBuilder();
                        try (BufferedReader reader = new BufferedReader(
                                new InputStreamReader(connection.getInputStream(),StandardCharsets.UTF_8))) {
                            char[] buf = new char[1024];
                            int n;
                            while ((n=reader.read(buf))!=-1) {
                                if (response.length()+n>8192) throw new java.io.IOException("oversized CORE status");
                                response.append(buf,0,n);
                            }
                        }
                        JSONObject data = new JSONObject(response.toString());
                        if (!data.optBoolean("bench_only",false) ||
                            !expected.equals(ControllerLinkPolicy.normalizedId(data.optString("controller_id"))))
                            continue;
                        verified=data;
                        break;
                    } catch (Exception ignored) {
                        // AP not joined or unavailable: BLE continues independently.
                    } finally {
                        if(connection!=null)connection.disconnect();
                    }
                }
            } catch (SecurityException ignored) {
                // Permission unavailable: never infer connectivity from a cached value.
            }
            final JSONObject response=verified;
            main.post(() -> {
                synchronized (WifiCoreStatusProbe.this) {
                    running=false;
                    if (closed) return;
                }
                listener.result(response);
            });
        });
    }

    synchronized void close() {
        closed=true;
        worker.shutdownNow();
    }
}
