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
import java.util.Arrays;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/** Single bounded, owner-signed secondary STOP. No URL/body from UI input.
 * We NEVER retry after a POST attempt: an HTTP timeout does not prove failure.
 * Does not make legacy bench HTTP commands authenticated.
 */
final class WifiOwnerStopClient {
    interface Listener { void result(boolean accepted,String message); }
    private static final String BASE="http://192.168.4.1";
    private final Context context;
    private final ConnectivityManager connectivity;
    private final ExecutorService queue=Executors.newSingleThreadExecutor();
    private final Handler main=new Handler(Looper.getMainLooper());
    private volatile boolean closed,active;
    WifiOwnerStopClient(Context context){
        this.context=context.getApplicationContext();
        connectivity=(ConnectivityManager)this.context.getSystemService(Context.CONNECTIVITY_SERVICE);
    }
    synchronized boolean stop(String expectedId,String previouslyPairedBleAddress,Listener callback){
        String id=ControllerLinkPolicy.normalizedId(expectedId);
        if(closed||active||id==null||previouslyPairedBleAddress==null||connectivity==null)
            return false;
        active=true;
        queue.execute(()->{
            boolean accepted=false;
            String message="Wi-Fi STOP unavailable; use authenticated BLE";
            byte[] ownerKey=BleOwnerVault.load(context,previouslyPairedBleAddress);
            try {
                if(ownerKey==null || ownerKey.length!=16)
                    throw new IllegalStateException("Owner credential unavailable");
                for(Network network:connectivity.getAllNetworks()){
                    NetworkCapabilities caps=connectivity.getNetworkCapabilities(network);
                    if(caps==null || !caps.hasTransport(NetworkCapabilities.TRANSPORT_WIFI))
                        continue;
                    JSONObject status=getJson(network,"/api/v1/status");
                    if(!id.equals(ControllerLinkPolicy.normalizedId(
                           status.optString("controller_id"))))continue;
                    JSONObject challenge=getJson(network,"/api/v1/owner/challenge");
                    String nonce=challenge.optString("nonce");
                    if(challenge.optInt("version")!=1 ||
                       !"POST /api/v1/owner/stop".equals(challenge.optString("scope")) ||
                       WifiOwnerProof.decodeNonce(nonce)==null)
                        throw new IllegalStateException("CORE did not support signed STOP");
                    String proof=WifiOwnerProof.stopProof(ownerKey,nonce);
                    // From here a timeout is AMBIGUOUS. Never replay STOP.
                    HttpURLConnection http=null;
                    try{
                        http=(HttpURLConnection)network.openConnection(
                            new URL(BASE+"/api/v1/owner/stop"));
                        http.setConnectTimeout(1200);
                        http.setReadTimeout(1200);
                        http.setInstanceFollowRedirects(false);
                        http.setRequestMethod("POST");
                        http.setRequestProperty("X-OpenTail-Nonce",nonce);
                        http.setRequestProperty("X-OpenTail-Proof",proof);
                        if(http.getResponseCode()==200){
                            JSONObject ack=readJson(http);
                            accepted=ack.optBoolean("ok",false);
                        }
                        message=accepted?
                           "Authenticated Wi-Fi STOP accepted; verify CORE output":
                           "Wi-Fi STOP not acknowledged; verify state, do not replay";
                    } finally {
                        if(http!=null)http.disconnect();
                    }
                    break;
                }
            }catch(Exception e){
                // Do not report success after an ambiguous request.
                message="Wi-Fi STOP unconfirmed; verify CORE state before retry";
            }finally{
                if(ownerKey!=null)Arrays.fill(ownerKey,(byte)0);
            }
            boolean finished=accepted;
            String report=message;
            main.post(()->{
                synchronized(WifiOwnerStopClient.this){active=false;if(closed)return;}
                callback.result(finished,report);
            });
        });
        return true;
    }
    private static JSONObject getJson(Network network,String path) throws Exception {
        HttpURLConnection connection=null;
        try{
            connection=(HttpURLConnection)network.openConnection(new URL(BASE+path));
            connection.setConnectTimeout(1200);
            connection.setReadTimeout(1200);
            connection.setInstanceFollowRedirects(false);
            connection.setRequestMethod("GET");
            if(connection.getResponseCode()!=200)throw new IllegalStateException("CORE GET failed");
            return readJson(connection);
        }finally{if(connection!=null)connection.disconnect();}
    }
    private static JSONObject readJson(HttpURLConnection connection)throws Exception {
        StringBuilder response=new StringBuilder();
        try(BufferedReader reader=new BufferedReader(
            new InputStreamReader(connection.getInputStream(),StandardCharsets.UTF_8))){
            char[] buf=new char[512];int n;
            while((n=reader.read(buf))!=-1){
                if(response.length()+n>8192)throw new IllegalStateException("Oversized CORE reply");
                response.append(buf,0,n);
            }
        }
        return new JSONObject(response.toString());
    }
    synchronized void close(){closed=true;queue.shutdownNow();}
}
