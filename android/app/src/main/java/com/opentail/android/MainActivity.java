package com.opentail.android;

import android.annotation.SuppressLint;
import android.Manifest;
import android.content.pm.PackageManager;
import android.app.Activity;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothManager;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.IntentFilter;
import android.app.AlertDialog;
import android.content.SharedPreferences;
import android.content.Intent;
import android.graphics.Color;
import android.net.Uri;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.NetworkRequest;
import android.os.Build;
import android.os.Bundle;
import android.provider.Settings;
import android.view.Gravity;
import android.view.View;
import android.webkit.JavascriptInterface;
import android.webkit.WebChromeClient;
import android.webkit.WebResourceRequest;
import android.webkit.WebResourceResponse;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.webkit.WebSettings;
import android.webkit.ValueCallback;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.Spinner;
import android.widget.ArrayAdapter;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;
import android.widget.EditText;
import android.widget.SeekBar;
import androidx.webkit.WebViewAssetLoader;
import java.io.OutputStream;
import java.io.InputStream;
import java.io.ByteArrayOutputStream;
import org.json.JSONObject;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/** Native OpenTail Android shell. Wi-Fi controls use the device's unchanged Studio web app. */
public final class MainActivity extends Activity {
    private static final int PICK_PROJECT = 201;
    private static final int SAVE_PROJECT = 202;
    private static final String OFFLINE_URL = "https://appassets.androidplatform.net/assets/opentail.htm";
    private static final int MAX_EXPORT_CHARS = 2_000_000;
    private final ExecutorService io = Executors.newSingleThreadExecutor();

    private WebView web;
    private WebViewAssetLoader assets;
    private TextView connectionLabel;
    private TextView bleLabel;
    private ValueCallback<Uri[]> uploadCallback;
    private String pendingExport;
    private BleDiscovery ble;
    private volatile String lastBleMessage = "BLE not connected";
    private boolean coreMode = false;
    private boolean wifiConnectInProgress = false;
    private boolean appReady = false;
    private boolean bluetoothReceiverRegistered = false;
    private ConnectivityManager wifiMonitor;
    private ConnectivityManager.NetworkCallback wifiNetworkCallback;
    /** Event-driven transport refresh; no timer, no stale cached green status. */
    private void registerWifiStatusMonitor() {
        wifiMonitor = (ConnectivityManager) getSystemService(CONNECTIVITY_SERVICE);
        if (wifiMonitor == null) return;
        wifiNetworkCallback = new ConnectivityManager.NetworkCallback() {
            @Override public void onAvailable(Network network) { refreshTransportFromCallback(); }
            @Override public void onLost(Network network) { refreshTransportFromCallback(); }
            @Override public void onCapabilitiesChanged(Network network, NetworkCapabilities caps) { refreshTransportFromCallback(); }
        };
        try {
            wifiMonitor.registerNetworkCallback(new NetworkRequest.Builder()
                .addTransportType(NetworkCapabilities.TRANSPORT_WIFI).build(), wifiNetworkCallback);
        } catch (RuntimeException ex) {
            wifiNetworkCallback = null;
            android.util.Log.w("OpenTail", "Wi-Fi status callback unavailable", ex);
        }
    }
    private void refreshTransportFromCallback() {
        runOnUiThread(this::updateConnectionSurfaces);
    }
    /** System Bluetooth switch state must override any cached GATT "connected" state. */
    private final BroadcastReceiver bluetoothReceiver = new BroadcastReceiver() {
        @Override public void onReceive(Context context, Intent intent) {
            if (!BluetoothAdapter.ACTION_STATE_CHANGED.equals(intent.getAction())) return;
            final int adapterState = intent.getIntExtra(BluetoothAdapter.EXTRA_STATE, BluetoothAdapter.ERROR);
            if (adapterState == BluetoothAdapter.STATE_OFF || adapterState == BluetoothAdapter.STATE_TURNING_OFF) {
                if (ble != null) ble.onAdapterDisabled();
            } else if (adapterState == BluetoothAdapter.STATE_ON) {
                if (ble != null) ble.onAdapterEnabled();
            }
            updateConnectionSurfaces();
        }
    };
    private boolean bluetoothPermissionGranted() {
        return Build.VERSION.SDK_INT < 31 ||
                checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED;
    }
    private boolean bluetoothRadioOn() {
        if (!bluetoothPermissionGranted()) return false;
        BluetoothManager manager = (BluetoothManager) getSystemService(BLUETOOTH_SERVICE);
        BluetoothAdapter adapter = manager == null ? null : manager.getAdapter();
        try { return adapter != null && adapter.isEnabled(); }
        catch (SecurityException ex) { return false; }
    }
    private void updateConnectionSurfaces() {
        if (web != null && bridgePageAllowed()) web.evaluateJavascript(
                "window.OpenTailShell&&window.OpenTailShell.refresh();"+
                "window.OpenTailBLEOutputs&&window.OpenTailBLEOutputs.refresh();", null);
    }
    private boolean pageHadError = false;
    private boolean offlineSelected = false;
    private HapticController haptics;
    private SharedPreferences preferences;
    private static final String AUTO_WIFI = "auto_wifi";
    private static final String AUTO_BLE = "auto_ble";
    private static final String USE_CORE = "prefer_core_wifi";

    /** Runs only on allowlisted local Studio pages. No network or controller commands. */
    private static final String STUDIO_HAPTIC_JS = "(function(){"
        + "if(window.__otNativeHaptics)return;window.__otNativeHaptics=true;"
        + "var pendingSave='';var since=0;var scheduled=false;"
        + "document.addEventListener('click',function(e){"
        + "var t=e.target.closest('button,[role=button],input[type=button],input[type=submit]');"
        + "if(t&&!t.disabled){var title=t.textContent||'';"
        + "if(title.includes('Save to CORE')){"
        + "pendingSave=t.closest('.ot-om-card')?'output':(t.closest('.ot-tm-card')?'trigger':'');"
        + "since=Date.now();}window.OpenTailHaptics.feedback('tap');}},true);"
        + "document.addEventListener('change',function(e){"
        + "var t=e.target;if(t.matches('input[type=checkbox],input[type=radio],select'))"
        + "window.OpenTailHaptics.feedback('toggle');else if(t.matches('input[type=range]'))"
        + "window.OpenTailHaptics.feedback('slider');},true);"
        + "new MutationObserver(function(){if(!pendingSave||scheduled)return;scheduled=true;"
        + "setTimeout(function(){scheduled=false;"
        + "if(!pendingSave)return;if(Date.now()-since>15000){pendingSave='';return;}"
        + "var sel=pendingSave==='output'?'#ot-om-message':'.ot-tm-card [role=status]';"
        + "var node=document.querySelector(sel);var x=node?node.textContent||'':'';"
        + "var ok=x.includes('Saved to CORE and verified by device readback.')"
        + "||x.includes('Saved and verified by CORE readback.');"
        + "var fail=x.includes('Save not confirmed:');"
        + "if(ok||fail){window.OpenTailHaptics.feedback(fail?'error':'saved');pendingSave='';}"
        + "},120);"
        + "}).observe(document.body,{childList:true,subtree:true,characterData:true});"
        + "})();";

    @SuppressLint("SetJavaScriptEnabled")
    @Override public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        preferences = getSharedPreferences(HapticController.PREFS, MODE_PRIVATE);
        IntentFilter bluetoothFilter = new IntentFilter(BluetoothAdapter.ACTION_STATE_CHANGED);
        // Bluetooth adapter state is a protected system broadcast sent by the privileged Bluetooth service.
        if (Build.VERSION.SDK_INT >= 33) registerReceiver(bluetoothReceiver, bluetoothFilter, Context.RECEIVER_EXPORTED);
        else registerReceiver(bluetoothReceiver, bluetoothFilter);
        bluetoothReceiverRegistered = true;
        registerWifiStatusMonitor();
        haptics = new HapticController(this);
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setBackgroundColor(Color.rgb(12, 19, 16));
        root.setPadding(0, dp(4), 0, 0);

        // Unified WebView shell owns the header, connection cards and bottom navigation.
        // Keep labels for existing state transitions, but do not add duplicate native controls.
        connectionLabel = label("Offline Studio · Device not connected");
        bleLabel = label("BLE · Not connected");

        assets = new WebViewAssetLoader.Builder()
                .addPathHandler("/assets/", new WebViewAssetLoader.AssetsPathHandler(this))
                .build();
        web = new WebView(this);
        web.setBackgroundColor(Color.rgb(12, 19, 16));
        web.getSettings().setJavaScriptEnabled(true);
        web.getSettings().setDomStorageEnabled(true);
        web.getSettings().setAllowFileAccess(false);
        web.getSettings().setAllowContentAccess(true);  // Android document picker import.
        web.getSettings().setAllowFileAccessFromFileURLs(false);
        web.getSettings().setAllowUniversalAccessFromFileURLs(false);
        web.getSettings().setMixedContentMode(WebSettings.MIXED_CONTENT_NEVER_ALLOW);
        web.getSettings().setJavaScriptCanOpenWindowsAutomatically(false);
        web.getSettings().setSupportMultipleWindows(false);
        web.getSettings().setMediaPlaybackRequiresUserGesture(true);
        web.addJavascriptInterface(new ProjectExportBridge(), "OpenTailApp");
        web.addJavascriptInterface(new HapticBridge(), "OpenTailHaptics");
        web.addJavascriptInterface(new NativeBleBridge(), "OpenTailNativeBLE");
        web.addJavascriptInterface(new NativeAppBridge(), "OpenTailNativeApp");
        web.setWebViewClient(new WebViewClient() {
            @Override public WebResourceResponse shouldInterceptRequest(WebView view, WebResourceRequest request) {
                Uri url = request.getUrl();
                if (OFFLINE_URL.equals(url.toString())) return assets.shouldInterceptRequest(url);
                if ("192.168.4.1".equals(url.getHost()) && "http".equals(url.getScheme())) return null;
                // No external scripts, sites, analytics or CDN assets are needed by Studio.
                return new WebResourceResponse("text/plain", "UTF-8", new java.io.ByteArrayInputStream(new byte[0]));
            }
            @Override public boolean shouldOverrideUrlLoading(WebView view, WebResourceRequest request) {
                if (allowedPage(request.getUrl().toString())) return false;
                show("External navigation blocked. Only local OpenTail Studio and CORE are allowed.");
                return true;
            }
            @Override public void onPageFinished(WebView view, String url) {
                if (OFFLINE_URL.equals(url) && offlineSelected) {
                    coreMode = false;
                    CoreConnection.releaseWifi(MainActivity.this);
                    connectionLabel.setText("Offline Studio v0.19.47 · Local design only, no device sync");
                }
                if (allowedPage(url)) {
                    // Intercept Studio's project export to use Android's user-chosen document picker.
                    view.evaluateJavascript("window.download=function(name,text){window.OpenTailApp.exportJson(String(name),String(text));};", null);
                    view.evaluateJavascript(STUDIO_HAPTIC_JS, null);
                    injectInlineBleControls(view);
                    injectBleOutputControls(view);
                    injectUnifiedShell(view);
                    injectDirectBleDashboard(view);
                }
            }
            @Override public void onReceivedError(WebView view, WebResourceRequest request,
                                                  android.webkit.WebResourceError error) {
                if (request.isForMainFrame()) {
                    pageHadError = true;
                    connectionLabel.setText("Page unavailable · Check controller Wi-Fi and reconnect");
                }
            }
        });
        web.setWebChromeClient(new WebChromeClient() {
            @Override public boolean onShowFileChooser(WebView view, ValueCallback<Uri[]> callback, FileChooserParams params) {
                if (uploadCallback != null) uploadCallback.onReceiveValue(null);
                uploadCallback = callback;
                try {
                    Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
                    intent.setType("application/json");
                    intent.addCategory(Intent.CATEGORY_OPENABLE);
                    startActivityForResult(intent, PICK_PROJECT);
                    return true;
                } catch (Exception ex) {
                    uploadCallback = null;
                    show("File picker unavailable: " + ex.getMessage());
                    return false;
                }
            }
        });
        root.addView(web, new LinearLayout.LayoutParams(-1, 0, 1));
        setContentView(root);
        ble = new BleDiscovery(this, message -> {
            lastBleMessage = message;
            bleLabel.setText("BLE · " + message);
            if (web != null && bridgePageAllowed()) {
                web.evaluateJavascript("window.OpenTailBLEPanel&&window.OpenTailBLEPanel.statusChanged(" + JSONObject.quote(message) + ");", null);
                updateConnectionSurfaces();
                web.evaluateJavascript("window.OpenTailBLEOutputs&&window.OpenTailBLEOutputs.connectionChanged(" + JSONObject.quote(message) + ");",null);
            }
        });
        ble.setPreferredListener(address -> preferences.edit().putString("preferred_ble_address", address).apply());
        ble.setConfigListener(result -> {
            if(web!=null&&bridgePageAllowed())web.evaluateJavascript(
                "window.OpenTailBLEOutputs&&window.OpenTailBLEOutputs.onResult(" + JSONObject.quote(result) + ");",null);
        });
        appReady = true;
        openOffline(false);
        if (preferences.getBoolean(USE_CORE, false) && preferences.getBoolean(AUTO_WIFI, false)) {
            // The user must already have joined the CORE Wi-Fi AP using Android settings.
            connectCore(true);
        }
        if (preferences.getBoolean(AUTO_BLE, true)) {
            ble.tryReconnect(preferences.getString("preferred_ble_address", null));
        }
    }

    private TextView label(String text) {
        TextView v = new TextView(this);
        v.setText(text);
        v.setTextSize(12);
        v.setTextColor(Color.rgb(200, 211, 204));
        v.setPadding(0, dp(4), 0, dp(4));
        return v;
    }

    private LinearLayout row() {
        LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setGravity(Gravity.CENTER_VERTICAL);
        return row;
    }

    private void addButton(LinearLayout row, String title, Runnable action) {
        Button button = new Button(this);
        button.setAllCaps(false);
        button.setText(title);
        button.setTextSize(12);
        button.setTextColor(Color.rgb(229, 250, 233));
        button.setBackgroundTintList(android.content.res.ColorStateList.valueOf(Color.rgb(26, 69, 43)));
        button.setOnClickListener(v -> { haptics.tap(button, "tap"); action.run(); });
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(0, dp(48), 1);
        params.setMargins(dp(2), 0, dp(2), dp(2));
        row.addView(button, params);
    }

    private void openWifiSettings() {
        try {
            if (Build.VERSION.SDK_INT >= 29) startActivity(new Intent(Settings.Panel.ACTION_WIFI));
            else startActivity(new Intent(Settings.ACTION_WIFI_SETTINGS));
        } catch (Exception ex) { show("Open Android Wi-Fi settings and join OpenTail-CORE-Alpha"); }
    }

    private void connectCore() { connectCore(false); }

    private void connectCore(boolean automatic) {
        if (wifiConnectInProgress) return;
        wifiConnectInProgress = true;
        if (!automatic) offlineSelected = false;
        if (!automatic) connectionLabel.setText("Checking OpenTail CORE on 192.168.4.1…");
        io.execute(() -> {
            boolean bound = CoreConnection.bindToWifi(this);
            try {
                if (!bound) throw new Exception("Wi-Fi is not connected; join OpenTail-CORE-Alpha first");
                CoreConnection.verify();
                runOnUiThread(() -> {
                    wifiConnectInProgress = false;
                    if (offlineSelected) { CoreConnection.releaseWifi(this); return; }
                    coreMode = true;
                    preferences.edit().putBoolean(USE_CORE, true).apply();
                    connectionLabel.setText("CORE verified · Wi-Fi Studio · 192.168.4.1");
                    if (pageHadError || !CoreConnection.STUDIO.equals(web.getUrl())) {
                        pageHadError = false;
                        web.loadUrl(CoreConnection.STUDIO);
                    }
                });
            } catch (Exception ex) {
                CoreConnection.releaseWifi(this);
                runOnUiThread(() -> {
                    wifiConnectInProgress = false;
                    coreMode = false;
                    if (!automatic) {
                        connectionLabel.setText("CORE not reachable · Check Wi-Fi settings");
                        show(ex.getMessage());
                    } else if (!offlineSelected && !OFFLINE_URL.equals(web.getUrl())) {
                        // Keep native BLE Show Modes reachable when CORE Wi-Fi disappears.
                        web.loadUrl(OFFLINE_URL);
                    }
                });
            }
        });
    }

    private void openOffline() { openOffline(true); }
    private void openOffline(boolean selectedByUser) {
        if (selectedByUser) {
            offlineSelected = true;
            preferences.edit().putBoolean(USE_CORE, false).apply();
        }
        coreMode = false;
        CoreConnection.releaseWifi(this);
        connectionLabel.setText("Offline Studio v0.19.47 · Local design only, no device sync");
        if (!OFFLINE_URL.equals(web.getUrl())) web.loadUrl(OFFLINE_URL);
    }

    /** Prefer BLE owner HMAC auth; user can separately request legacy Wi-Fi token. */
    private void enableBleControls(){
        if(ble!=null)ble.authorizeStored();
    }

    /** Optional backward-compatible Wi-Fi token used on pre-v0.1.24 devices. */
    private void enableBleWifiFallback(){
        if (ble == null) return;
        bleLabel.setText("BLE · Obtaining temporary bench authorization from CORE Wi-Fi…");
        io.execute(() -> {
            try {
                if(!CoreConnection.bindToWifi(this))throw new Exception("Join OpenTail-CORE-Alpha Wi-Fi first");
                CoreConnection.verify();
                String token=CoreConnection.bleSessionToken();
                runOnUiThread(() -> ble.authorize(token));
            }catch(Exception ex){
                runOnUiThread(() -> {
                    bleLabel.setText("BLE · Authorization unavailable");
                    show(ex.getMessage());
                });
            }
        });
    }

    /** Install the same Show Modes BLE controls into both offline and CORE-hosted Studio.
     *  Repeating Studio rendering does not destroy them: the script mounts on each tab render.
     */
    private void injectInlineBleControls(WebView view) {
        try (InputStream in = getAssets().open("ble-show-panel.js");
             ByteArrayOutputStream out = new ByteArrayOutputStream()) {
            byte[] buf = new byte[8192];
            int n;
            while ((n = in.read(buf)) != -1) out.write(buf, 0, n);
            view.evaluateJavascript(out.toString("UTF-8"), null);
        } catch (Exception ex) {
            android.util.Log.e("OpenTail", "Failed to inject native BLE Show Modes", ex);
        }
    }

    private void injectBleOutputControls(WebView view) {
        try (InputStream in = getAssets().open("ble-output-panel.js");
             ByteArrayOutputStream out = new ByteArrayOutputStream()) {
            byte[] buf = new byte[8192];int n;
            while ((n = in.read(buf)) != -1) out.write(buf, 0, n);
            view.evaluateJavascript(out.toString("UTF-8"), null);
        } catch(Exception e){android.util.Log.e("OpenTail", "BLE Output Manager injection failed",e);}
    }

    /** Direct BLE dashboard actions; additive to existing BLE output and CORE editors. */
    private void injectDirectBleDashboard(WebView view) {
        try (InputStream in = getAssets().open("direct-ble-dashboard.js");
             ByteArrayOutputStream out = new ByteArrayOutputStream()) {
            byte[] buffer = new byte[8192];
            int n;
            while ((n = in.read(buffer)) != -1) out.write(buffer, 0, n);
            view.evaluateJavascript(out.toString("UTF-8"), null);
        } catch (Exception ex) {
            android.util.Log.e("OpenTail", "Direct BLE dashboard injection failed", ex);
        }
    }

    private void injectUnifiedShell(WebView view) {
        try (InputStream in = getAssets().open("unified-shell.js");
             ByteArrayOutputStream out = new ByteArrayOutputStream()) {
            byte[] buffer = new byte[8192];
            int read;
            while ((read = in.read(buffer)) != -1) out.write(buffer, 0, read);
            view.evaluateJavascript(out.toString("UTF-8"), null);
        } catch (Exception ex) {
            android.util.Log.e("OpenTail", "Unified interface failed to load", ex);
        }
    }

    /** Native actions are restricted to the controller and packaged Studio origins. */
    private final class NativeAppBridge {
        @JavascriptInterface public String getState() {
            try {
                JSONObject obj = new JSONObject();
                boolean radioOn = bluetoothRadioOn();
                obj.put("wifi", coreMode && !pageHadError && CoreConnection.hasWifiTransport(MainActivity.this));
                obj.put("bleRadioOn", radioOn);
                obj.put("ble", radioOn && ble != null && ble.isConnected());
                obj.put("bleAuthorized", radioOn && ble != null && ble.canControl());
                obj.put("blePhase", !bluetoothPermissionGranted() ? "permission" : !radioOn ? "off" : (ble == null ? "disconnected" : ble.connectionPhase()));
                obj.put("wifiMessage", connectionLabel.getText().toString());
                obj.put("bleMessage", lastBleMessage);
                obj.put("offline", offlineSelected || !coreMode);
                return obj.toString();
            } catch (Exception ex) { return "{}"; }
        }
        @JavascriptInterface public void connectWifi() {
            runOnUiThread(() -> { if (bridgePageAllowed()) connectCore(); });
        }
        @JavascriptInterface public void wifiSettings() {
            runOnUiThread(() -> { if (bridgePageAllowed()) openWifiSettings(); });
        }
        @JavascriptInterface public void offlineStudio() {
            runOnUiThread(() -> { if (bridgePageAllowed()) openOffline(); });
        }
        @JavascriptInterface public void scanBle() {
            runOnUiThread(() -> { if (bridgePageAllowed() && ble != null) ble.scan(); });
        }
        @JavascriptInterface public void authorizeBle() {
            runOnUiThread(() -> { if (bridgePageAllowed()) enableBleControls(); });
        }
        @JavascriptInterface public void appSettings() {
            runOnUiThread(() -> { if (bridgePageAllowed()) showSettings(); });
        }
    }

    private boolean bridgePageAllowed() {
        return web != null && (OFFLINE_URL.equals(web.getUrl()) || CoreConnection.STUDIO.equals(web.getUrl()));
    }

    /** Native BLE GATT bridge: only temporary bench show commands, never saved mapping or triggers. */
    private final class NativeBleBridge {
        @JavascriptInterface public String getState() {
            try {
                JSONObject j = new JSONObject();
                boolean radioOn = bluetoothRadioOn();
                j.put("radioOn", radioOn);
                j.put("connected", radioOn && ble != null && ble.isConnected());
                j.put("authorized", radioOn && ble != null && ble.canControl());
                j.put("phase", !bluetoothPermissionGranted() ? "permission" : !radioOn ? "off" : (ble == null ? "disconnected" : ble.connectionPhase()));
                j.put("message", lastBleMessage);
                return j.toString();
            } catch (Exception ignored) { return "{}"; }
        }
        @JavascriptInterface public void readOutput(int index) {
            runOnUiThread(() -> { if(bridgePageAllowed()&&ble!=null)ble.readOutput(index); });
        }
        @JavascriptInterface public void saveOutput(int index,String json) {
            runOnUiThread(() -> { if(bridgePageAllowed()&&ble!=null)ble.saveOutput(index,json); });
        }
        @JavascriptInterface public void scan() {
            runOnUiThread(() -> { if (bridgePageAllowed() && ble != null) ble.scan(); });
        }
        @JavascriptInterface public void authorize() {
            runOnUiThread(() -> { if (bridgePageAllowed()) enableBleControls(); });
        }
        @JavascriptInterface public void pairWithBoot(){
            runOnUiThread(() -> { if(bridgePageAllowed()&&ble!=null)ble.pairWithBoot(); });
        }
        @JavascriptInterface public void authorizeWifiLegacy(){
            runOnUiThread(() -> { if(bridgePageAllowed()) enableBleWifiFallback(); });
        }
        @JavascriptInterface public void stop() {
            runOnUiThread(() -> { if (bridgePageAllowed() && ble != null) ble.stopShow(); });
        }
        @JavascriptInterface public void start(int effect, int output, int zone,
                int r, int g, int b, int brightness, int speed, int ttl) {
            runOnUiThread(() -> {
                if (!bridgePageAllowed() || ble == null) return;
                if (!ble.canControl()) {
                    lastBleMessage = "BLE lighting not authorized. Pair using BOOT, or use legacy Wi-Fi.";
                    if (web != null) web.evaluateJavascript(
                        "window.OpenTailBLEPanel&&window.OpenTailBLEPanel.statusChanged(" + JSONObject.quote(lastBleMessage) + ");", null);
                    return;
                }
                ble.show(effect, output, zone, r, g, b, brightness, speed, ttl);
            });
        }
    }

    private void showSettings() {
        LinearLayout content = new LinearLayout(this);
        content.setOrientation(LinearLayout.VERTICAL);
        content.setPadding(dp(22), dp(10), dp(22), dp(4));
        CheckBox hapticToggle = new CheckBox(this);
        hapticToggle.setText("Enable haptic feedback");
        hapticToggle.setChecked(haptics.enabled());
        content.addView(hapticToggle);
        TextView title = label("Haptic intensity");
        content.addView(title);
        Spinner intensity = new Spinner(this);
        intensity.setAdapter(new ArrayAdapter<>(this, android.R.layout.simple_spinner_dropdown_item,
                new String[]{"Light", "Medium", "Strong"}));
        intensity.setSelection(haptics.strength());
        content.addView(intensity);
        CheckBox slider = new CheckBox(this);
        slider.setText("Slider feedback (on release)");
        slider.setChecked(haptics.sliderEnabled());
        content.addView(slider);
        CheckBox wifi = new CheckBox(this);
        wifi.setText("Reconnect Wi-Fi Studio when available");
        wifi.setChecked(preferences.getBoolean(AUTO_WIFI, false));
        content.addView(wifi);
        CheckBox bt = new CheckBox(this);
        bt.setText("Reconnect preferred BLE controller");
        bt.setChecked(preferences.getBoolean(AUTO_BLE, true));
        content.addView(bt);
        TextView note = label("BLE show and output configuration require one Wi-Fi authorization per BLE connection. Trigger-input mapping is still Wi-Fi-only.");
        content.addView(note);
        new AlertDialog.Builder(this).setTitle("OpenTail Android settings")
                .setView(content).setNegativeButton("Cancel", null)
                .setPositiveButton("Save", (dialog, which) -> {
                    haptics.configure(hapticToggle.isChecked(), intensity.getSelectedItemPosition(), slider.isChecked());
                    preferences.edit().putBoolean(AUTO_WIFI, wifi.isChecked())
                            .putBoolean(AUTO_BLE, bt.isChecked()).apply();
                    haptics.tap(web, "saved");
                }).show();
    }

    private final class HapticBridge {
        @JavascriptInterface public void feedback(String kind) {
            runOnUiThread(() -> {
                if (web != null && allowedPage(web.getUrl())) {
                    if ("tap".equals(kind) || "toggle".equals(kind) || "slider".equals(kind)
                            || "saved".equals(kind) || "error".equals(kind)) {
                        haptics.tap(web, kind);
                    }
                }
            });
        }
    }

    private static boolean allowedPage(String url) {
        return OFFLINE_URL.equals(url)
                || CoreConnection.STUDIO.equals(url)
                || (CoreConnection.ORIGIN + "/").equals(url)
                || "about:blank".equals(url);
    }

    /** Export bridge is only enabled on the two allowlisted Studio origins. */
    private final class ProjectExportBridge {
        @JavascriptInterface public void exportJson(String proposedName, String json) {
            runOnUiThread(() -> {
                if (web == null || !allowedPage(web.getUrl())) return;
                if (json == null || json.length() > MAX_EXPORT_CHARS) {
                    show("Project too large to export");
                    return;
                }
                pendingExport = json;
                String safe = proposedName == null ? "opentail-project.json"
                        : proposedName.replaceAll("[^A-Za-z0-9_.-]", "_");
                if (!safe.endsWith(".json")) safe += ".json";
                Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
                intent.setType("application/json");
                intent.addCategory(Intent.CATEGORY_OPENABLE);
                intent.putExtra(Intent.EXTRA_TITLE, safe);
                try { startActivityForResult(intent, SAVE_PROJECT); }
                catch (Exception ex) { show("Save picker unavailable"); pendingExport = null; }
            });
        }
    }

    @Override protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == PICK_PROJECT) {
            ValueCallback<Uri[]> callback = uploadCallback;
            uploadCallback = null;
            if (callback != null) callback.onReceiveValue(resultCode == RESULT_OK && data != null && data.getData() != null
                    ? new Uri[]{data.getData()} : null);
        } else if (requestCode == SAVE_PROJECT) {
            if (resultCode == RESULT_OK && data != null && data.getData() != null && pendingExport != null) {
                try (OutputStream out = getContentResolver().openOutputStream(data.getData(), "wt")) {
                    if (out == null) throw new Exception("Destination not writable");
                    out.write(pendingExport.getBytes(StandardCharsets.UTF_8));
                    show("Project exported to your chosen location");
                } catch (Exception ex) { show("Export failed: " + ex.getMessage()); }
            }
            pendingExport = null;
        }
    }

    @Override public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] results) {
        super.onRequestPermissionsResult(requestCode, permissions, results);
        if (ble != null) ble.handlePermissions(requestCode, results);
    }

    @Override protected void onResume() {
        super.onResume();
        if (!appReady) return;
        updateConnectionSurfaces();
        if (preferences.getBoolean(AUTO_WIFI, false) && preferences.getBoolean(USE_CORE, false)
                && !offlineSelected) connectCore(true);
        if (preferences.getBoolean(AUTO_BLE, true) && ble != null) {
            ble.tryReconnect(preferences.getString("preferred_ble_address", null));
        }
    }

    @Override protected void onStop() {
        super.onStop();
        if (ble != null) ble.stopScan();
    }

    @Override protected void onDestroy() {
        if (bluetoothReceiverRegistered) { unregisterReceiver(bluetoothReceiver); bluetoothReceiverRegistered=false; }
        if (wifiMonitor != null && wifiNetworkCallback != null) {
            try { wifiMonitor.unregisterNetworkCallback(wifiNetworkCallback); } catch (RuntimeException ignored) { }
            wifiNetworkCallback = null;
        }
        if (uploadCallback != null) uploadCallback.onReceiveValue(null);
        if (ble != null) ble.shutdown();
        io.shutdownNow();
        CoreConnection.releaseWifi(this);
        if (web != null) { web.removeJavascriptInterface("OpenTailApp"); web.removeJavascriptInterface("OpenTailHaptics"); web.removeJavascriptInterface("OpenTailNativeBLE"); web.removeJavascriptInterface("OpenTailNativeApp"); web.destroy(); }
        super.onDestroy();
    }

    @Override public void onBackPressed() {
        if (web != null && web.canGoBack()) web.goBack();
        else super.onBackPressed();
    }

    private void show(String message) { Toast.makeText(this, message, Toast.LENGTH_LONG).show(); }
    private int dp(int px) { return Math.round(px * getResources().getDisplayMetrics().density); }
}
