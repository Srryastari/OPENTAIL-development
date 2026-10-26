package com.opentail.android;

import android.Manifest;
import android.app.Activity;
import android.app.AlertDialog;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCallback;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.BluetoothManager;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanResult;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.widget.ArrayAdapter;
import android.widget.Toast;
import java.nio.charset.StandardCharsets;
import javax.crypto.Mac;
import javax.crypto.spec.SecretKeySpec;
import android.bluetooth.BluetoothStatusCodes;
import java.util.Arrays;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.UUID;
import java.util.function.Consumer;

/** BLE discovery, physical BOOT bench enrollment, and configuration. Trigger inputs remain Wi-Fi only. */
final class BleDiscovery {
    interface StatusListener { void onStatus(String message); }
    // ESP-IDF NimBLE BLE_UUID128_INIT uses little-endian byte order.
    private static final UUID SERVICE_UUID = UUID.fromString("55a56022-4915-7a92-cf44-b8b7102eaf21");
    private static final UUID INFO_UUID = UUID.fromString("55a56022-4915-7a92-cf44-b8b7102eaf22");
    private static final UUID COMMAND_UUID = UUID.fromString("55a56022-4915-7a92-cf44-b8b7102eaf23");
    private static final UUID CFG_CMD_UUID = UUID.fromString("55a56022-4915-7a92-cf44-b8b7102eaf25");
    private static final UUID CFG_RSP_UUID = UUID.fromString("55a56022-4915-7a92-cf44-b8b7102eaf26");
    private static final UUID ENROLL_UUID = UUID.fromString("55a56022-4915-7a92-cf44-b8b7102eaf27");
    private static final UUID CHALLENGE_UUID = UUID.fromString("55a56022-4915-7a92-cf44-b8b7102eaf28");
    private static final UUID REPLY_UUID = UUID.fromString("55a56022-4915-7a92-cf44-b8b7102eaf24");
    private static final int REQUEST_NEARBY = 137;
    private static final long SCAN_DURATION_MS = 10000L;

    private final Activity activity;
    private final StatusListener listener;
    private final Handler ui = new Handler(Looper.getMainLooper());
    private final Map<String, BluetoothDevice> found = new LinkedHashMap<>();
    private final ArrayList<String> labels = new ArrayList<>();
    private final ArrayAdapter<String> entries;
    private BluetoothLeScanner scanner;
    private BluetoothGatt gatt;
    private AlertDialog scanDialog;
    private volatile boolean scanning;
    private boolean stopped;
    private volatile boolean connected = false;
    private volatile boolean connecting = false;
    private volatile BluetoothGattCharacteristic command;
    private BluetoothGattCharacteristic reply;
    private BluetoothGattCharacteristic enroll,challenge;
    private volatile boolean pairPending=false,challengePending=false;
    private String activeAddress="";
    private volatile boolean authorized = false;
    private volatile BluetoothGattCharacteristic cfgCommand,cfgReply;
    private java.util.function.Consumer<String> configListener;
    private volatile boolean cfgBusy=false;
    private volatile boolean cfgSaving=false,cfgWaitingWrite=false,cfgWaitingRead=false;
    private byte[] cfgBytes=new byte[BleConfigCodec.LENGTH],cfgExpected;
    private int cfgIndex=0,cfgOffset=0,cfgCrc=0,cfgRevision=0,cfgPhase=0;
    private final int[] cfgKnownRevision=new int[]{-1,-1};
    private final byte[][] cfgLatest=new byte[2][];
    private volatile int cfgWatchdog=0;
    void setConfigListener(java.util.function.Consumer<String> listener){configListener=listener;}
    boolean canConfigure(){return canControl() && cfgCommand!=null && cfgReply!=null;}
    void readOutput(int index){
        if(!canConfigure()){cfgNotify(false,"Authorize BLE over CORE Wi-Fi before editing outputs",null,index);return;}
        if(index<0||index>1||cfgBusy||writePending||readReplyPending){cfgNotify(false,"BLE is busy; retry after current transfer",null,index);return;}
        cfgBusy=true;cfgSaving=false;cfgIndex=index;cfgPhase=1;cfgOffset=0;
        cfgSend(new byte[]{2,1,(byte)index});
    }
    void saveOutput(int index,String json){
        if(!canConfigure()){cfgNotify(false,"Authorize BLE over CORE Wi-Fi before saving outputs",null,index);return;}
        if(index<0||index>1||cfgBusy||writePending||readReplyPending){cfgNotify(false,"BLE is busy; retry after current transfer",null,index);return;}
        if(cfgKnownRevision[index]<0){cfgNotify(false,"Load this output over BLE before saving",null,index);return;}
        try {cfgBytes=BleConfigCodec.encode(new org.json.JSONObject(json));}
        catch(Exception e){cfgNotify(false,"Invalid draft: "+e.getMessage(),null,index);return;}
        cfgExpected=Arrays.copyOf(cfgBytes,cfgBytes.length);
        cfgBusy=true;cfgSaving=true;cfgIndex=index;cfgPhase=3;cfgOffset=0;
        cfgCrc=BleConfigCodec.crc16(cfgBytes);cfgRevision=cfgKnownRevision[index];
        cfgSend(new byte[]{2,3,(byte)index,(byte)cfgBytes.length,(byte)(cfgBytes.length>>8),
                (byte)cfgCrc,(byte)(cfgCrc>>8),(byte)cfgRevision,(byte)(cfgRevision>>8)});
    }
    private void cfgNotify(boolean ok,String message,org.json.JSONObject output,int index){
        if(configListener==null)return;
        try{
            org.json.JSONObject response=new org.json.JSONObject();
            response.put("ok",ok);response.put("message",message);response.put("index",index);
            if(output!=null)response.put("output",output);
            String content=response.toString();ui.post(()->{if(!stopped&&configListener!=null)configListener.accept(content);});
        }catch(Exception ignored){}
    }
    private void cfgFail(String message){
        int index=cfgIndex;cfgBusy=false;cfgWaitingWrite=false;cfgWaitingRead=false;
        cfgWatchdog++;cfgNotify(false,message,null,index);pumpQueue();
    }
    @SuppressWarnings({"MissingPermission","deprecation"})
    private void cfgSend(byte[] packet){
        if(!cfgBusy||gatt==null||cfgCommand==null){cfgFail("Bluetooth disconnected during configuration");return;}
        cfgWaitingWrite=true;cfgWaitingRead=false;
        int watchdog=++cfgWatchdog;
        ui.postDelayed(()->{if(cfgBusy&&cfgWatchdog==watchdog)cfgFail("BLE operation timed out; reload output before retrying");},8000L);
        try{
            boolean ok;
            if(Build.VERSION.SDK_INT>=33)ok=gatt.writeCharacteristic(cfgCommand,packet,BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT)==BluetoothStatusCodes.SUCCESS;
            else{cfgCommand.setWriteType(BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT);cfgCommand.setValue(packet);ok=gatt.writeCharacteristic(cfgCommand);}
            if(!ok)cfgFail("BLE configuration write did not start");
        }catch(SecurityException ex){cfgFail("Bluetooth permission missing");}
    }
    @SuppressWarnings("MissingPermission")
    private void cfgWritten(BluetoothGatt connection,int status){
        cfgWaitingWrite=false;
        if(status!=BluetoothGatt.GATT_SUCCESS){cfgFail("BLE config write failed: "+status);return;}
        cfgWaitingRead=true;
        try{if(!connection.readCharacteristic(cfgReply))cfgFail("BLE config response read did not start");}
        catch(SecurityException e){cfgFail("BLE config read permission missing");}
    }
    private void cfgResponse(byte[] packet,int status){
        cfgWaitingRead=false;
        if(!cfgBusy||status!=BluetoothGatt.GATT_SUCCESS||packet==null||packet.length<4||
                packet[0]!=2||(packet[2]&255)!=cfgIndex){cfgFail("Invalid BLE configuration response");return;}
        final int opcode=packet[1]&255, code=packet[3]&255;
        if(code!=0){
            String error=code==2?"Stop all shows and release physical inputs before saving":
                    code==3?"Output changed elsewhere; reload it before saving":
                    code==4?"CORE failed to save configuration to flash":
                    code==5?"BLE authorization expired":
                    code==6?"Transfer checksum mismatch":"Invalid BLE configuration command/data";
            cfgFail(error);return;
        }
        if(cfgPhase==1 && opcode==0x81 && packet.length>=10){
            int size=(packet[4]&255)|((packet[5]&255)<<8);
            if(size!=BleConfigCodec.LENGTH){cfgFail("Unexpected firmware output configuration size");return;}
            cfgCrc=(packet[6]&255)|((packet[7]&255)<<8);
            cfgRevision=(packet[8]&255)|((packet[9]&255)<<8);
            cfgBytes=new byte[size];cfgOffset=0;cfgPhase=2;
            cfgReadNext();return;
        }
        if(cfgPhase==2 && opcode==0x82 && packet.length>=7){
            int offset=(packet[4]&255)|((packet[5]&255)<<8),count=packet.length-6;
            if(offset!=cfgOffset||count<1||count>14||offset+count>cfgBytes.length){cfgFail("BLE output chunk missing or out of order");return;}
            System.arraycopy(packet,6,cfgBytes,offset,count);cfgOffset+=count;
            if(cfgOffset<cfgBytes.length){cfgReadNext();return;}
            if(BleConfigCodec.crc16(cfgBytes)!=cfgCrc){cfgFail("BLE output read checksum mismatch");return;}
            try{
                org.json.JSONObject output=BleConfigCodec.decode(cfgBytes,cfgIndex);
                if(cfgSaving && (cfgExpected==null||!Arrays.equals(cfgBytes,cfgExpected))){cfgFail("CORE readback differs from saved output");return;}
                cfgKnownRevision[cfgIndex]=cfgRevision;cfgLatest[cfgIndex]=Arrays.copyOf(cfgBytes,cfgBytes.length);
                cfgBusy=false;cfgWatchdog++;
                cfgNotify(true,cfgSaving?"Saved to CORE flash and verified over BLE":"Loaded from CORE over BLE",output,cfgIndex);
                cfgSaving=false;pumpQueue();return;
            }catch(Exception e){cfgFail("CORE returned invalid output: "+e.getMessage());return;}
        }
        if(cfgPhase==3 && opcode==0x83){cfgPhase=4;cfgOffset=0;cfgWriteNext();return;}
        if(cfgPhase==4 && opcode==0x84){
            if(cfgOffset<cfgBytes.length){cfgWriteNext();return;}
            cfgPhase=5;cfgSend(new byte[]{2,5,(byte)cfgIndex});return;
        }
        if(cfgPhase==5 && opcode==0x85){
            cfgPhase=1;cfgSend(new byte[]{2,1,(byte)cfgIndex});return;
        }
        cfgFail("Unexpected BLE configuration state");
    }
    private void cfgReadNext(){cfgSend(new byte[]{2,2,(byte)cfgIndex,(byte)cfgOffset,(byte)(cfgOffset>>8)});}
    private void cfgWriteNext(){
        int n=Math.min(15,cfgBytes.length-cfgOffset);
        byte[] packet=new byte[n+5];packet[0]=2;packet[1]=4;packet[2]=(byte)cfgIndex;
        packet[3]=(byte)cfgOffset;packet[4]=(byte)(cfgOffset>>8);
        System.arraycopy(cfgBytes,cfgOffset,packet,5,n);cfgOffset+=n;cfgSend(packet);
    }

    private volatile boolean authPending = false;
    private boolean writePending = false;
    private boolean readReplyPending = false;
    private byte[] pendingPacket;
    private byte[] nextPacket;

    /** Explicit state for status labels; never infer a live connection from a prior success message. */
    String connectionPhase() {
        if (scanning) return "scanning";
        if (connecting) return "connecting";
        if (!connected) return "disconnected";
        if (pairPending) return "pairing";
        if (challengePending || authPending) return "authorizing";
        if (cfgBusy) return cfgSaving ? "saving" : "reading";
        return authorized ? "authorized" : "connected";
    }
    /** Called when Android disables Bluetooth; clears stale GATT/auth/config UI immediately. */
    void onAdapterDisabled() {
        stopScan();
        if (scanDialog != null) { scanDialog.dismiss(); scanDialog = null; }
        connected = false; connecting = false; authorized = false; scanning = false;
        command = null; reply = null; enroll = null; challenge = null; cfgCommand = null; cfgReply = null;
        pairPending = false; challengePending = false; authPending = false;
        cfgBusy = false; cfgSaving = false; cfgWatchdog++;
        writePending = false; readReplyPending = false; nextPacket = null;
        Arrays.fill(cfgKnownRevision, -1);
        if (gatt != null) { try { gatt.close(); } catch (Exception ignored) {} gatt = null; }
        notifyStatus("Bluetooth off · connection closed");
    }
    void onAdapterEnabled() { notifyStatus("Bluetooth on · ready to scan or reconnect"); }
    boolean isConnected() { return connected; }
    boolean canControl() { return connected && authorized && command != null; }
    /** Physical BOOT/GPIO0 button must be held after CORE starts, until read completes. */
    @SuppressWarnings("MissingPermission")
    void pairWithBoot(){
        if(!connected||enroll==null){notifyStatus("Connect CORE v0.1.24 via BLE first");return;}
        if(cfgBusy||writePending||readReplyPending){notifyStatus("BLE is busy; retry pairing");return;}
        pairPending=true;challengePending=false;
        notifyStatus("Hold CORE BOOT button while the owner key is read…");
        try{if(!gatt.readCharacteristic(enroll)){pairPending=false;notifyStatus("Unable to request pairing");}}
        catch(SecurityException e){pairPending=false;notifyStatus("Bluetooth permission missing");}
    }
    /** Prefer owner-key authentication, with Wi-Fi token retained only as manual fallback. */
    @SuppressWarnings("MissingPermission")
    void authorizeStored(){
        if(!connected||challenge==null){notifyStatus("Connect CORE v0.1.24 first, or use legacy Wi-Fi authorization");return;}
        if(BleOwnerVault.load(activity,activeAddress)==null){notifyStatus("Not paired yet: hold BOOT and tap Pair with BOOT");return;}
        if(cfgBusy||writePending||readReplyPending){notifyStatus("BLE busy; retry authorization");return;}
        challengePending=true;
        try{if(!gatt.readCharacteristic(challenge)){challengePending=false;notifyStatus("Challenge read could not start");}}
        catch(SecurityException e){challengePending=false;notifyStatus("Bluetooth permission missing");}
    }
    private void ownerRead(BluetoothGatt connection,byte[] value,int status){
        pairPending=false;
        if(status!=BluetoothGatt.GATT_SUCCESS||value==null||value.length!=16){
            notifyStatus("Enrollment refused: hold BOOT on powered-on CORE during Pair (GATT "+status+")");return;
        }
        try{BleOwnerVault.save(activity,activeAddress,value);notifyStatus("Bench owner key saved to Android Keystore; authorizing…");ui.post(()->authorizeStored());}
        catch(Exception e){notifyStatus("Could not securely store owner key; pairing not complete");}
    }
    private void ownerChallenge(byte[] value,int status){
        challengePending=false;
        byte[] key=BleOwnerVault.load(activity,activeAddress);
        if(status!=BluetoothGatt.GATT_SUCCESS||value==null||value.length!=16||key==null){
            notifyStatus("BLE challenge unavailable or pairing key missing (GATT "+status+")");return;
        }
        try{
            Mac hmac=Mac.getInstance("HmacSHA256");hmac.init(new SecretKeySpec(key,"HmacSHA256"));
            hmac.update("OpenTail-BLE-v2".getBytes(StandardCharsets.US_ASCII));
            byte[] hash=hmac.doFinal(value);
            byte[] packet=new byte[18];packet[0]=1;packet[1]=4;System.arraycopy(hash,0,packet,2,16);
            authorized=false;authPending=true;enqueueWrite(packet);
        }catch(Exception e){notifyStatus("BLE owner authentication could not be prepared");}
    }

    /** Token must come from the paired phone's Wi-Fi connection to CORE; never persist it. */
    void authorize(String tokenHex) {
        if (!connected || command == null || reply == null) {
            notifyStatus("Connect OpenTail BLE before authorizing lighting controls"); return;
        }
        if (tokenHex == null || !tokenHex.matches("[a-fA-F0-9]{32}")) {
            notifyStatus("Invalid CORE BLE authorization token");return;
        }
        byte[] packet = new byte[18];packet[0]=1;packet[1]=1;
        for(int i=0;i<16;i++)packet[i+2]=(byte)Integer.parseInt(tokenHex.substring(2*i,2*i+2),16);
        authorized=false;
        authPending=true;
        enqueueWrite(packet);
    }

    /** Only native bench effects. TTL is 1..60 seconds; trigger priority remains in CORE. */
    void show(int effect,int chainMask,int zoneMask,int r,int g,int b,int brightness,int speed,int ttl) {
        if (!canControl()){notifyStatus("Authorize BLE using BOOT pairing or legacy Wi-Fi");return;}
        if (effect<1||effect>5||chainMask<1||chainMask>3||zoneMask<1||zoneMask>255||
                brightness<0||brightness>32||speed<0||speed>100||ttl<1||ttl>60) {
            notifyStatus("Invalid BLE show settings");return;
        }
        enqueueWrite(new byte[]{1,2,(byte)effect,(byte)chainMask,(byte)zoneMask,
            (byte)r,(byte)g,(byte)b,(byte)brightness,(byte)speed,(byte)ttl});
    }
    void stopShow() {
        if (!canControl()) {notifyStatus("BLE show control not authorized");return;}
        enqueueWrite(new byte[]{1,3});
    }

    /** Serialize Android GATT operations; BLE GATT only allows one pending request. */
    private void enqueueWrite(byte[] data) {
        if (writePending || readReplyPending || cfgBusy) {
            // One pending latest action; explicit STOP replaces queued starts.
            nextPacket=Arrays.copyOf(data,data.length);return;
        }
        pendingPacket=Arrays.copyOf(data,data.length);
        writeNow();
    }
    @SuppressWarnings({"MissingPermission","deprecation"})
    private void writeNow(){
        if(gatt==null||command==null||pendingPacket==null)return;
        byte[] packet=pendingPacket;
        writePending=true;
        try {
            boolean ok;
            if(Build.VERSION.SDK_INT>=33)
                ok=gatt.writeCharacteristic(command,packet,BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT)==BluetoothStatusCodes.SUCCESS;
            else {
                command.setWriteType(BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT);
                command.setValue(packet);
                ok=gatt.writeCharacteristic(command);
            }
            if(!ok){writePending=false;authPending=false;notifyStatus("BLE write could not start");}
        }catch(SecurityException ex){writePending=false;authPending=false;notifyStatus("BLE permission missing");}
    }
    private void pumpQueue(){
        if (!writePending&&!readReplyPending&&!cfgBusy&&nextPacket!=null){
            pendingPacket=nextPacket;nextPacket=null;writeNow();
        }
    }
    private String preferredPending = null;
    private Consumer<String> preferredListener;
    private static final long RECONNECT_DELAY_MS = 10000L;
    private long lastReconnectAttempt = 0L;

    void setPreferredListener(Consumer<String> listener) { this.preferredListener = listener; }

    /** One bounded background scan for the previously selected BLE address; no infinite reconnect loop. */
    void tryReconnect(String address) {
        if (stopped || connected || connecting || scanning || address == null || address.isEmpty()) return;
        if (android.os.SystemClock.elapsedRealtime() - lastReconnectAttempt < RECONNECT_DELAY_MS) return;
        if (Build.VERSION.SDK_INT >= 31 && (activity.checkSelfPermission(Manifest.permission.BLUETOOTH_SCAN) != PackageManager.PERMISSION_GRANTED
                || activity.checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED)) return;
        if (Build.VERSION.SDK_INT < 31 && activity.checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION) != PackageManager.PERMISSION_GRANTED) return;
        lastReconnectAttempt = android.os.SystemClock.elapsedRealtime();
        preferredPending = address;
        beginScan(true);
    }

    BleDiscovery(Activity activity, StatusListener listener) {
        this.activity = activity;
        this.listener = listener;
        entries = new ArrayAdapter<>(activity, android.R.layout.simple_list_item_1, labels);
    }

    boolean handlePermissions(int requestCode, int[] results) {
        if (requestCode != REQUEST_NEARBY) return false;
        for (int status : results) {
            if (status != PackageManager.PERMISSION_GRANTED) {
                notifyStatus("Bluetooth permission denied; Wi-Fi Studio still works");
                return true;
            }
        }
        beginScan(false);
        return true;
    }

    void scan() {
        if (Build.VERSION.SDK_INT >= 31) {
            if (activity.checkSelfPermission(Manifest.permission.BLUETOOTH_SCAN) != PackageManager.PERMISSION_GRANTED
                    || activity.checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) {
                activity.requestPermissions(new String[] {
                        Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT
                }, REQUEST_NEARBY);
                return;
            }
        } else if (activity.checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION) != PackageManager.PERMISSION_GRANTED) {
            activity.requestPermissions(new String[]{Manifest.permission.ACCESS_FINE_LOCATION}, REQUEST_NEARBY);
            return;
        }
        beginScan(false);
    }

    @SuppressWarnings("MissingPermission")
    private void beginScan(boolean background) {
        BluetoothManager manager = (BluetoothManager) activity.getSystemService(Activity.BLUETOOTH_SERVICE);
        BluetoothAdapter adapter = manager == null ? null : manager.getAdapter();
        if (adapter == null) {
            notifyStatus("Bluetooth LE is unavailable on this phone");
            return;
        }
        if (!adapter.isEnabled()) {
            notifyStatus("Enable Bluetooth in Android settings, then scan again");
            return;
        }
        scanner = adapter.getBluetoothLeScanner();
        if (scanner == null) {
            notifyStatus("Bluetooth LE scanner unavailable");
            return;
        }
        stopScan();
        found.clear();
        labels.clear();
        entries.notifyDataSetChanged();
        if (!background) {
            preferredPending = null;
            scanDialog = new AlertDialog.Builder(activity)
                    .setTitle("Find OpenTail CORE")
                    .setAdapter(entries, (dialog, position) -> {
                        String address = new ArrayList<>(found.keySet()).get(position);
                        stopScan();
                        BluetoothDevice device = found.get(address);
                        if (device != null) connectReadOnly(device);
                    })
                    .setNegativeButton("Close", (dialog, which) -> stopScan())
                    .create();
            scanDialog.setOnDismissListener(dialog -> stopScan());
            scanDialog.show();
        }
        try {
            scanning = true;
            scanner.startScan(scanCallback);
            if (!background) notifyStatus("Scanning for OpenTail CORE (10 seconds)…");
            ui.postDelayed(() -> {
                if (!scanning) return;
                stopScan();
                if (!background) notifyStatus(found.isEmpty() ? "No OpenTail BLE advertisement detected" : "Scan complete; tap OpenTail CORE to connect");
                preferredPending = null;
            }, SCAN_DURATION_MS);
        } catch (SecurityException | IllegalStateException ex) {
            stopScan();
            notifyStatus("BLE scan couldn't start: " + ex.getMessage());
        }
    }

    private final ScanCallback scanCallback = new ScanCallback() {
        @Override public void onScanResult(int callbackType, ScanResult result) {
            final String name = result.getScanRecord() == null ? null : result.getScanRecord().getDeviceName();
            if (name == null || !name.startsWith("OpenTail CORE")) return;
            ui.post(() -> {
                String address = result.getDevice().getAddress();
                if (!found.containsKey(address)) {
                    found.put(address, result.getDevice());
                    labels.add(name + "  •  " + address);
                    entries.notifyDataSetChanged();
                }
                if (preferredPending != null && preferredPending.equalsIgnoreCase(address)) {
                    preferredPending = null;
                    stopScan();
                    connectReadOnly(result.getDevice());
                }
            });
        }
        @Override public void onScanFailed(int errorCode) {
            ui.post(() -> { stopScan(); notifyStatus("BLE scan error " + errorCode); });
        }
    };

    @SuppressWarnings("MissingPermission")
    private void connectReadOnly(BluetoothDevice device) {
        if (Build.VERSION.SDK_INT >= 31
                && activity.checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) {
            notifyStatus("Nearby-device permission required");
            return;
        }
        if (gatt != null) { gatt.close(); gatt = null; }
        Arrays.fill(cfgKnownRevision,-1);authorized=false;command=null;reply=null;enroll=null;challenge=null;cfgCommand=null;cfgReply=null;pairPending=false;challengePending=false;cfgBusy=false;cfgWatchdog++;writePending=false;readReplyPending=false;nextPacket=null;
        connecting = true;
        connected = false;
        notifyStatus("Connecting to OpenTail BLE…");
        try { gatt = device.connectGatt(activity, false, callback, BluetoothDevice.TRANSPORT_LE); }
        catch (SecurityException ex) { connecting = false; notifyStatus("BLE connection permission denied"); }
    }

    private final BluetoothGattCallback callback = new BluetoothGattCallback() {
        @SuppressWarnings("MissingPermission")
        @Override public void onConnectionStateChange(BluetoothGatt connection, int status, int newState) {
            if (stopped) { connection.close(); return; }
            if (status == BluetoothGatt.GATT_SUCCESS && newState == BluetoothProfile.STATE_CONNECTED) {
                connecting = false;
                connected = true;
                notifyStatus("BLE connected · reading status…");
                connection.discoverServices();
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED || status != BluetoothGatt.GATT_SUCCESS) {
                connecting = false;
                connected = false;
                Arrays.fill(cfgKnownRevision,-1);authorized = false;command=null;reply=null;enroll=null;challenge=null;cfgCommand=null;cfgReply=null;pairPending=false;challengePending=false;cfgBusy=false;cfgWatchdog++;writePending=false;readReplyPending=false;nextPacket=null;
                notifyStatus("BLE disconnected (GATT status " + status + ")");
                connection.close();
                if (gatt == connection) gatt = null;
            }
        }

        @SuppressWarnings("MissingPermission")
        @Override public void onServicesDiscovered(BluetoothGatt connection, int status) {
            if (status != BluetoothGatt.GATT_SUCCESS) {
                notifyStatus("BLE service discovery failed: " + status);
                return;
            }
            BluetoothGattService service = connection.getService(SERVICE_UUID);
            BluetoothGattCharacteristic info = service == null ? null : service.getCharacteristic(INFO_UUID);
            if (info == null) {
                notifyStatus("BLE connected; no supported OpenTail info characteristic");
                return;
            }
            // Existing read-only connection remains compatible with older CORE firmware.
            command = service.getCharacteristic(COMMAND_UUID);
            reply = service.getCharacteristic(REPLY_UUID);
            enroll = service.getCharacteristic(ENROLL_UUID);
            challenge = service.getCharacteristic(CHALLENGE_UUID);
            activeAddress=connection.getDevice().getAddress();
            cfgCommand = service.getCharacteristic(CFG_CMD_UUID);
            cfgReply = service.getCharacteristic(CFG_RSP_UUID);
            authorized = false;
            if (preferredListener != null) preferredListener.accept(connection.getDevice().getAddress());
            connection.readCharacteristic(info);
        }

        @Override public void onCharacteristicRead(BluetoothGatt connection, BluetoothGattCharacteristic characteristic,
                                                    byte[] value, int status) {
            if (ENROLL_UUID.equals(characteristic.getUuid())) {ownerRead(connection,value,status);return;}
            if (CHALLENGE_UUID.equals(characteristic.getUuid())) {ownerChallenge(value,status);return;}
            if (CFG_RSP_UUID.equals(characteristic.getUuid())) { cfgResponse(value,status);return; }
            if (REPLY_UUID.equals(characteristic.getUuid())) { acceptReply(value,status);return; }
            if (INFO_UUID.equals(characteristic.getUuid()) && status == BluetoothGatt.GATT_SUCCESS) {
                notifyStatus("BLE connected · " + new String(value, StandardCharsets.UTF_8));
                if(challenge!=null&&BleOwnerVault.load(activity,activeAddress)!=null)ui.post(thisAutoAuth());
            } else {
                notifyStatus("BLE connected; information read failed: " + status);
            }
        }

        @SuppressWarnings("deprecation")
        @Override public void onCharacteristicRead(BluetoothGatt connection, BluetoothGattCharacteristic characteristic,
                                                    int status) {
            if (Build.VERSION.SDK_INT < 33) {
                if (ENROLL_UUID.equals(characteristic.getUuid())) {ownerRead(connection,characteristic.getValue(),status);return;}
                if (CHALLENGE_UUID.equals(characteristic.getUuid())) {ownerChallenge(characteristic.getValue(),status);return;}
                if (CFG_RSP_UUID.equals(characteristic.getUuid())) {cfgResponse(characteristic.getValue(),status);return;}
                if (REPLY_UUID.equals(characteristic.getUuid())) {
                    acceptReply(characteristic.getValue(),status);return;
                }
                if (INFO_UUID.equals(characteristic.getUuid()) && status == BluetoothGatt.GATT_SUCCESS) {
                    byte[] value = characteristic.getValue();
                    notifyStatus("BLE connected · " + new String(value == null ? new byte[0] : value, StandardCharsets.UTF_8));
                    if(challenge!=null&&BleOwnerVault.load(activity,activeAddress)!=null)ui.post(thisAutoAuth());
                } else notifyStatus("BLE connected; information read failed: " + status);
            }
        }
        @Override public void onCharacteristicWrite(BluetoothGatt connection,BluetoothGattCharacteristic characteristic,int status){
            if (CFG_CMD_UUID.equals(characteristic.getUuid())) {cfgWritten(connection,status);return;}
            if (!COMMAND_UUID.equals(characteristic.getUuid())) return;
            writePending=false;
            if(status!=BluetoothGatt.GATT_SUCCESS){
                authPending=false;authorized=false;
                notifyStatus("BLE write failed (GATT " + status + ")");pumpQueue();return;
            }
            if(reply==null){authPending=false;notifyStatus("CORE missing BLE reply characteristic");pumpQueue();return;}
            readReplyPending=true;
            try {
                if (!connection.readCharacteristic(reply)){
                    readReplyPending=false;authPending=false;
                    notifyStatus("Could not read CORE BLE result");pumpQueue();
                }
            } catch(SecurityException ex){readReplyPending=false;authPending=false;notifyStatus("BLE read permission missing");pumpQueue();}
        }
    };

    private Runnable thisAutoAuth(){return ()->authorizeStored();}

    private void acceptReply(byte[] raw,int status){
        readReplyPending=false;
        String response = status==BluetoothGatt.GATT_SUCCESS && raw!=null
                ? new String(raw,StandardCharsets.US_ASCII) : "ERR READ";
        if(authPending){authorized="OK AUTH".equals(response);authPending=false;}
        if(response.startsWith("ERR"))notifyStatus("BLE · " + response + " (Wi-Fi control unaffected)");
        else notifyStatus("BLE · " + response + (authorized?" · lighting enabled":""));
        pumpQueue();
    }

    @SuppressWarnings("MissingPermission")
    void stopScan() {
        if (!scanning) return;
        scanning = false;
        try { if (scanner != null) scanner.stopScan(scanCallback); }
        catch (SecurityException | IllegalStateException ignored) { }
    }

    @SuppressWarnings("MissingPermission")
    void shutdown() {
        stopped = true;
        connected = false;
        connecting = false;
        authorized = false;cfgBusy=false;cfgWatchdog++;
        stopScan();
        if (scanDialog != null) { scanDialog.dismiss(); scanDialog = null; }
        if (gatt != null) { gatt.disconnect(); gatt.close(); gatt = null; }
    }

    private void notifyStatus(String message) {
        ui.post(() -> {
            if (!stopped) listener.onStatus(message);
        });
    }
}
