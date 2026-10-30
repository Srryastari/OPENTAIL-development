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
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.text.SpannableString;
import android.text.style.ForegroundColorSpan;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;
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
    private static final UUID FRAME_CMD_UUID=UUID.fromString("55a56022-4915-7a92-cf44-b8b7102eaf2a");
    private static final UUID FRAME_RSP_UUID=UUID.fromString("55a56022-4915-7a92-cf44-b8b7102eaf2b");
    private static final UUID FX_CAPS_UUID = UUID.fromString("55a56022-4915-7a92-cf44-b8b7102eaf29");
    // Optional trigger-team v1 contract (PR #24); absent on older CORE firmware.
    private static final UUID TRIGGER_MAP_UUID = UUID.fromString("55a56022-4915-7a92-cf44-b8b7102eaf2c");
    private static final UUID PALETTE_CMD_UUID = UUID.fromString("55a56022-4915-7a92-cf44-b8b7102eaf2d");
    private static final UUID PALETTE_RSP_UUID = UUID.fromString("55a56022-4915-7a92-cf44-b8b7102eaf2e");
    private static final UUID PALETTE_CAP_UUID = UUID.fromString("55a56022-4915-7a92-cf44-b8b7102eaf2f");
    private static final int REQUEST_NEARBY = 137;
    private static final long SCAN_DURATION_MS = 10000L;

    private final Activity activity;
    private final StatusListener listener;
    private final Handler ui = new Handler(Looper.getMainLooper());
    // Declared before the palette field initializer that captures them.
    private volatile BluetoothGatt gatt;
    private boolean stopped;
    private final Map<String, BluetoothDevice> found = new LinkedHashMap<>();
    private final ArrayList<String> labels = new ArrayList<>();
    // Headless discovery lets Jetpack Compose own the visible scan results.
    // Existing GATT authentication/configuration handling is unchanged.
    interface ScanResultListener { void onFound(String address, String label); }
    private ScanResultListener composeScanResults;
    private boolean headlessScan=false;
    void setScanResultListener(ScanResultListener listener) { composeScanResults=listener; }
    void scanHeadless() { headlessScan=true; scanImpl(); }
    @SuppressWarnings("MissingPermission")
    void connectFoundDevice(String address) {
        BluetoothDevice device=found.get(address);
        if(device==null){notifyStatus("Device no longer in scan results; scan again");return;}
        stopScan();
        connectReadOnly(device);
    }
    private CoreFrameCapture.Listener frameListener;
    // The transport can become idle before its completed frame reaches Compose.
    // Hold capture admission until the previous callback has been consumed.
    private volatile boolean frameCallbackPending=false;
    private BluetoothGattCharacteristic paletteCommand,paletteReply,paletteCapabilities;
    private java.util.function.Consumer<NativePaletteBleTransport.Result> paletteListener;
    private final NativePaletteBleTransport palette=new NativePaletteBleTransport(
        result->{
            final BluetoothGatt owner=gatt;
            ui.post(()->{
                if(!stopped && owner!=null && owner==gatt && paletteListener!=null)
                    paletteListener.accept(result);
            });
        },this::pumpQueue);
    void setPaletteListener(java.util.function.Consumer<NativePaletteBleTransport.Result> cb){paletteListener=cb;}
    boolean canTransferPalettes(){return canControl() && palette.supported();}
    void cancelPaletteTransfer(){palette.abortForStop();}
    private boolean paletteGate(){
        if(cfgBusy||writePending||readReplyPending||fxCapsBusy||triggerMapBusy||
           frameCapture.busy()||palette.busy()||nextPacket!=null||
           authPending||pairPending||challengePending){
            notifyStatus("BLE busy; retry palette action after STOP/transfer");return false;
        }
        return true;
    }
    boolean negotiatePaletteV3(){
        if(!canControl()||!palette.available()){notifyStatus("CORE palette v3 unavailable or unauthorized");return false;}
        return paletteGate() && palette.negotiate();
    }
    boolean downloadPalette(int slot){return canTransferPalettes() && paletteGate() && palette.load(slot);}
    boolean uploadPalette(int slot,long revision,String stops){
        if(!canTransferPalettes()||!paletteGate())return false;
        try{return palette.upload(slot,revision,NativePaletteV3.stops(stops));}
        catch(IllegalArgumentException ex){notifyStatus(ex.getMessage());return false;}
    }
    boolean deletePalette(int slot,long revision){
        if(!canTransferPalettes()||!paletteGate())return false;
        try{return palette.delete(slot,revision);}
        catch(IllegalArgumentException ex){notifyStatus(ex.getMessage());return false;}
    }
    private final CoreFrameCapture frameCapture=new CoreFrameCapture((output,sequence,rgb,error)->{
        CoreFrameCapture.Listener cb=frameListener;
        if(cb!=null) {
            frameCallbackPending=true;
            ui.post(()->{
                try {cb.done(output,sequence,rgb,error);}
                finally {frameCallbackPending=false;}
            });
        }
        pumpQueue();
    });
    void setFrameListener(CoreFrameCapture.Listener cb) { frameListener=cb; }
    boolean canCaptureFrame() {return canControl() && frameCapture.supported();}
    void cancelFrameCapture() { frameCapture.abortForStop(); }
    boolean captureFrame(int output) {
        if(!canCaptureFrame()){notifyStatus("CORE frame snapshots unavailable or unauthorized");return false;}
        if(frameCapture.busy() || palette.busy() || frameCallbackPending || cfgBusy || writePending || nextPacket!=null ||
           readReplyPending || fxCapsBusy || triggerMapBusy || authPending || pairPending || challengePending) {
            notifyStatus("BLE busy; retry frame capture");return false;
        }
        frameCapture.request(output);
        return true;
    }
    private final ArrayAdapter<String> entries;
    private BluetoothLeScanner scanner;
    // Updated by UI lifecycle, read by Android's asynchronous GATT callbacks.
    private AlertDialog scanDialog;
    private volatile boolean scanning;
    private volatile int scanGeneration=0;
    private volatile boolean connected = false;
    private volatile boolean connecting = false;
    private volatile BluetoothGattCharacteristic command;
    private BluetoothGattCharacteristic reply;
    private volatile BluetoothGattCharacteristic fxCaps;
    private volatile boolean fxCapsBusy=false;
    private BluetoothGattCharacteristic triggerMapping;
    private volatile boolean triggerMapBusy=false;
    private boolean triggerMapSavePending=false;
    private int[] triggerMapExpected;
    private int triggerMapBaseRevision;
    private Consumer<NativeTriggerMappingCodec.Snapshot> triggerMapListener;
    private volatile boolean mtuNegotiating=false;
    private Consumer<String> fxCapabilitiesListener;
    void setFxCapabilitiesListener(Consumer<String> listener){fxCapabilitiesListener=listener;}
    void setTriggerMapListener(Consumer<NativeTriggerMappingCodec.Snapshot> listener){
        triggerMapListener=listener;
    }
    boolean canMapTriggers(){return canControl() && triggerMapping!=null;}
    private boolean triggerMapAvailable(){
        if(!canMapTriggers()){notifyStatus("CORE trigger mapping BLE v1 unavailable or unauthorized");return false;}
        if(gatt==null||cfgBusy||writePending||readReplyPending||fxCapsBusy||triggerMapBusy||
           frameCapture.busy()||palette.busy()||nextPacket!=null||authPending||pairPending||challengePending){
            notifyStatus("BLE busy; retry trigger mapping");return false;
        }
        return true;
    }
    // A new mapping read/write invalidates the last verified snapshot.
    // An uncertain BLE outcome must never leave a stale mapping editable.
    private void invalidateTriggerReadback(BluetoothGatt owner){
        if(triggerMapListener!=null)ui.post(()->{
            if(!stopped && owner==gatt && triggerMapListener!=null)
                triggerMapListener.accept(null);
        });
    }
    @SuppressWarnings("MissingPermission")
    boolean readTriggerMap(){
        if(!triggerMapAvailable())return false;
        triggerMapBusy=true;triggerMapSavePending=false;triggerMapExpected=null;
        try {
            if(gatt.readCharacteristic(triggerMapping)){
                invalidateTriggerReadback(gatt);return true;
            }
        }catch(SecurityException ignored){}
        triggerMapBusy=false;notifyStatus("Trigger map read could not start");pumpQueue();return false;
    }
    @SuppressWarnings({"MissingPermission","deprecation"})
    boolean saveTriggerMap(int expectedRevision,int[] mapping){
        byte[] wire;
        try {wire=NativeTriggerMappingCodec.encode(expectedRevision,mapping);}
        catch(IllegalArgumentException ex){notifyStatus("Invalid trigger map draft");return false;}
        if(expectedRevision>=65535){notifyStatus("CORE trigger revision exhausted; cannot save or wrap");return false;}
        if(!triggerMapAvailable())return false;
        triggerMapBusy=true;triggerMapSavePending=true;
        triggerMapBaseRevision=expectedRevision;triggerMapExpected=mapping.clone();
        boolean started=false;
        try {
            if(Build.VERSION.SDK_INT>=33)
                started=gatt.writeCharacteristic(triggerMapping,wire,
                    BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT)==BluetoothStatusCodes.SUCCESS;
            else {
                triggerMapping.setWriteType(BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT);
                triggerMapping.setValue(wire);
                started=gatt.writeCharacteristic(triggerMapping);
            }
        }catch(SecurityException ignored){}
        if(started){invalidateTriggerReadback(gatt);return true;}
        triggerMapBusy=false;triggerMapSavePending=false;triggerMapExpected=null;
        notifyStatus("Trigger map write could not start");pumpQueue();return false;
    }
    private void triggerMapRead(BluetoothGatt connection,byte[] raw,int status){
        if(connection!=gatt || !triggerMapBusy || !canMapTriggers())return;
        boolean verifying=triggerMapSavePending;
        triggerMapBusy=false;triggerMapSavePending=false;
        NativeTriggerMappingCodec.Snapshot result=null;
        try {
            if(status!=BluetoothGatt.GATT_SUCCESS)throw new IllegalArgumentException("GATT "+status);
            result=NativeTriggerMappingCodec.decode(raw);
        }catch(IllegalArgumentException ex){
            notifyStatus("Invalid trigger map readback: "+ex.getMessage());
        }
        if(result!=null){
            if(verifying){
                if(result.status!=0)
                    notifyStatus("CORE rejected trigger mapping (status "+result.status+"); reload before retry");
                else if(!result.confirmsApplied(triggerMapBaseRevision,triggerMapExpected) &&
                        (triggerMapBaseRevision>=65535 || result.revision!=triggerMapBaseRevision+1))
                    notifyStatus("Trigger mapping revision mismatch; reload before retry");
                else if(!result.persisted)
                    notifyStatus("Trigger mapping committed but not yet active; reload after physical inputs release");
                else if(result.confirmsApplied(triggerMapBaseRevision,triggerMapExpected))
                    notifyStatus("CORE trigger mapping verified and active");
                else notifyStatus("Trigger mapping readback mismatch; reload before retry");
            } else notifyStatus("CORE trigger mapping read · rev "+result.revision);
            if(triggerMapListener!=null){
                NativeTriggerMappingCodec.Snapshot delivered=result;
                // Delivery is asynchronous. Never restore a stale physical
                // mapping after disconnect, adapter-off, or switching CORE.
                ui.post(()->{
                    if(connection==gatt && canMapTriggers() && triggerMapListener!=null)
                        triggerMapListener.accept(delivered);
                });
            }
        }
        triggerMapExpected=null;
        pumpQueue();
    }
    @SuppressWarnings("MissingPermission")
    private void triggerMapWritten(BluetoothGatt connection,int status){
        if(connection!=gatt || !triggerMapBusy || !canMapTriggers())return;
        if(status!=BluetoothGatt.GATT_SUCCESS){
            triggerMapBusy=false;triggerMapSavePending=false;triggerMapExpected=null;
            notifyStatus("CORE trigger mapping ATT write failed ("+status+")");pumpQueue();return;
        }
        try {if(connection.readCharacteristic(triggerMapping))return;}
        catch(SecurityException ignored){}
        triggerMapBusy=false;triggerMapSavePending=false;triggerMapExpected=null;
        notifyStatus("CORE trigger mapping readback could not start");pumpQueue();
    }

    @SuppressWarnings("MissingPermission")
    void readFxCapabilities(){
        if(!connected||gatt==null||fxCaps==null){fxNotify("[]");return;}
        if(cfgBusy||writePending||readReplyPending||fxCapsBusy||triggerMapBusy||frameCapture.busy()||palette.busy()||nextPacket!=null||authPending||pairPending||challengePending){
            notifyStatus("BLE busy; retry supported effects");return;
        }
        fxCapsBusy=true;
        try {
            if(!gatt.readCharacteristic(fxCaps)){
                fxCapsBusy=false;fxNotify("[]");pumpQueue();
            }
        } catch(SecurityException ex){
            fxCapsBusy=false;fxNotify("[]");pumpQueue();
        }
    }
    private void fxNotify(String json){
        // A capability reply may already be posted when disconnect or a
        // different CORE invalidates its GATT ownership. Never expose IDs
        // from the old controller as capabilities of the new connection.
        final BluetoothGatt owner=gatt;
        ui.post(()->{
            if(owner==gatt && fxCapabilitiesListener!=null)
                fxCapabilitiesListener.accept(connected && authorized ? json : "[]");
        });
    }
    private void fxRead(byte[] raw,int status){
        fxCapsBusy=false;
        if(status!=BluetoothGatt.GATT_SUCCESS||raw==null||raw.length<2||raw[0]!=1){fxNotify("[]");pumpQueue();return;}
        int n=raw[1]&255;
        try {fxNotify(BleFxCapabilities.decode(raw));}
        catch(IllegalArgumentException ex){
            notifyStatus("CORE effect catalog invalid: "+ex.getMessage());
            fxNotify("[]");
        }
        pumpQueue();
    }
    private BluetoothGattCharacteristic enroll,challenge;
    private volatile boolean pairPending=false,challengePending=false;
    private String activeAddress="";
    private volatile boolean authorized = false;
    private volatile BluetoothGattCharacteristic cfgCommand,cfgReply;
    private java.util.function.Consumer<String> configListener;
    private volatile boolean cfgBusy=false;
    private volatile boolean cfgSaving=false,cfgWaitingWrite=false,cfgWaitingRead=false;
    private byte[] cfgBytes=new byte[BleConfigCodec.LENGTH],cfgExpected;
    private int cfgIndex=0,cfgOffset=0,cfgCrc=0,cfgRevision=0,cfgPhase=0,cfgSaveBase=0;
    private final int[] cfgKnownRevision=new int[]{-1,-1};
    private final byte[][] cfgLatest=new byte[2][];
    private volatile int cfgWatchdog=0;
    void setConfigListener(java.util.function.Consumer<String> listener){configListener=listener;}
    boolean canConfigure(){return canControl() && cfgCommand!=null && cfgReply!=null;}
    void readOutput(int index){
        if(!canConfigure()){cfgNotify(false,"Pair and authorize BLE before editing CORE outputs",null,index);return;}
        if(index<0||index>1||cfgBusy||writePending||readReplyPending||fxCapsBusy||triggerMapBusy||frameCapture.busy()||palette.busy()||nextPacket!=null||authPending||pairPending||challengePending){cfgNotify(false,"BLE is busy; retry after current transfer",null,index);return;}
        cfgBusy=true;cfgSaving=false;cfgIndex=index;cfgPhase=1;cfgOffset=0;
        cfgSend(new byte[]{2,1,(byte)index});
    }
    /** Returns true when an output save enters the GATT transaction; only readback proves application. */
    boolean saveOutput(int index,String json){
        if(!canConfigure()){cfgNotify(false,"Pair and authorize BLE before saving CORE outputs",null,index);return false;}
        if(index<0||index>1||cfgBusy||writePending||readReplyPending||fxCapsBusy||triggerMapBusy||frameCapture.busy()||palette.busy()||nextPacket!=null||authPending||pairPending||challengePending){cfgNotify(false,"BLE is busy; retry after current transfer",null,index);return false;}
        if(cfgKnownRevision[index]<0){cfgNotify(false,"Load this output over BLE before saving",null,index);return false;}
        if(cfgKnownRevision[index]>=65535){cfgNotify(false,"CORE output revision exhausted; no wrap or additional save permitted",null,index);return false;}
        try {cfgBytes=BleConfigCodec.encode(new org.json.JSONObject(json));}
        catch(Exception e){cfgNotify(false,"Invalid draft: "+e.getMessage(),null,index);return false;}
        cfgExpected=Arrays.copyOf(cfgBytes,cfgBytes.length);
        cfgBusy=true;cfgSaving=true;cfgIndex=index;cfgPhase=3;cfgOffset=0;
        cfgCrc=BleConfigCodec.crc16(cfgBytes);cfgRevision=cfgKnownRevision[index];cfgSaveBase=cfgRevision;
        return cfgSend(new byte[]{2,3,(byte)index,(byte)cfgBytes.length,(byte)(cfgBytes.length>>8),
                (byte)cfgCrc,(byte)(cfgCrc>>8),(byte)cfgRevision,(byte)(cfgRevision>>8)});
    }
    private void cfgNotify(boolean ok,String message,org.json.JSONObject output,int index){
        if(configListener==null)return;
        // Output responses can be queued across a disconnect. They must not
        // repopulate new-device output state after an old CORE is discarded.
        final BluetoothGatt owner=gatt;
        try{
            org.json.JSONObject response=new org.json.JSONObject();
            response.put("ok",ok);response.put("message",message);response.put("index",index);
            if(output!=null)response.put("output",output);
            String content=response.toString();ui.post(()->{
                if(!stopped && owner==gatt && configListener!=null)
                    configListener.accept(content);
            });
        }catch(Exception ignored){}
    }
    private void cfgFail(String message){
        int index=cfgIndex;
        // A failed save may have reached flash before its confirmation failed.
        // Require a fresh authoritative read instead of reusing stale revision.
        if(cfgSaving && index>=0 && index<cfgKnownRevision.length) {
            cfgKnownRevision[index]=-1;
            cfgLatest[index]=null;
        }
        cfgBusy=false;cfgSaving=false;cfgWaitingWrite=false;cfgWaitingRead=false;
        cfgWatchdog++;cfgNotify(false,message,null,index);pumpQueue();
    }
    @SuppressWarnings({"MissingPermission","deprecation"})
    private boolean cfgSend(byte[] packet){
        if(!cfgBusy||gatt==null||cfgCommand==null){cfgFail("Bluetooth disconnected during configuration");return false;}
        cfgWaitingWrite=true;cfgWaitingRead=false;
        int watchdog=++cfgWatchdog;
        ui.postDelayed(()->{if(cfgBusy&&cfgWatchdog==watchdog)cfgFail("BLE operation timed out; reload output before retrying");},8000L);
        try{
            boolean ok;
            if(Build.VERSION.SDK_INT>=33)ok=gatt.writeCharacteristic(cfgCommand,packet,BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT)==BluetoothStatusCodes.SUCCESS;
            else{cfgCommand.setWriteType(BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT);cfgCommand.setValue(packet);ok=gatt.writeCharacteristic(cfgCommand);}
            if(!ok){cfgFail("BLE configuration write did not start");return false;}
            return true;
        }catch(SecurityException ex){cfgFail("Bluetooth permission missing");return false;}
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
            if(code==5) revokeOwnerSession();
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
                if(cfgSaving && (cfgRevision<cfgSaveBase || cfgRevision>cfgSaveBase+1)) {
                    cfgFail("CORE revision changed unexpectedly; reload output before another save");return;
                }
                if(cfgSaving && (cfgExpected==null||!Arrays.equals(cfgBytes,cfgExpected))){
                    // A commit may be staged while physical inputs remain
                    // asserted; old active geometry is not a verified apply.
                    cfgFail("CORE active output differs; may be pending trigger release. Reload after release; no active change verified");
                    return;
                }
                cfgKnownRevision[cfgIndex]=cfgRevision;cfgLatest[cfgIndex]=Arrays.copyOf(cfgBytes,cfgBytes.length);
                cfgBusy=false;cfgWatchdog++;
                cfgNotify(true,cfgSaving?
                    (cfgRevision==cfgSaveBase?
                        "CORE active output already matched; no new revision committed":
                        "Saved to CORE and active output verified over BLE"):
                    "Loaded active output from CORE over BLE",output,cfgIndex);
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
    // Preserve a queued STOP before the latest subsequent FX edit. Without
    // this slot, last-write-wins could replace STOP with a newer start.
    private byte[] afterStopPacket;

    /** Explicit state for status labels; never infer a live connection from a prior success message. */
    String connectionPhase() {
        if (scanning) return "scanning";
        if (connecting) return "connecting";
        if (!connected) return "disconnected";
        if (pairPending) return "pairing";
        if (challengePending || authPending) return "authorizing";
        if (cfgBusy) return cfgSaving ? "saving" : "reading";
        if (triggerMapBusy) return triggerMapSavePending ? "saving trigger map" : "reading trigger map";
        return authorized ? "authorized" : "connected";
    }
    /** Called when Android disables Bluetooth; clears stale GATT/auth/config UI immediately. */
    void onAdapterDisabled() {
        stopScan();
        frameCapture.cancel();palette.reset(); // GATT close may never produce a disconnect callback
        if (scanDialog != null) { scanDialog.dismiss(); scanDialog = null; }
        connected = false; connecting = false; authorized = false; scanning = false;
        command = null; reply = null; fxCaps=null; fxCapsBusy=false; fxNotify("[]"); triggerMapping=null; triggerMapBusy=false; triggerMapSavePending=false; triggerMapExpected=null; enroll = null; challenge = null; cfgCommand = null; cfgReply = null;
        pairPending = false; challengePending = false; authPending = false;
        cfgBusy = false; cfgSaving = false; cfgWatchdog++;
        writePending = false; readReplyPending = false; pendingPacket = null; nextPacket = null; afterStopPacket = null;
        Arrays.fill(cfgKnownRevision, -1);
        if (gatt != null) { try { gatt.close(); } catch (Exception ignored) {} gatt = null; }
        notifyStatus("Bluetooth off · connection closed");
    }
    void onAdapterEnabled() { notifyStatus("Bluetooth on · ready to scan or reconnect"); }
    boolean isConnected() { return connected; }
    boolean canControl() { return connected && authorized && command != null; }
    /** One UI action: use the stored owner key or enroll with physical BOOT once. */
    void authorizeOrPair() {
        // The combined Home/Settings action must not open a second GATT
        // challenge while this session is already authorized.
        if (canControl()) { notifyStatus("BLE already authorized"); return; }
        if (!connected) { notifyStatus("Connect to CORE over Bluetooth first"); return; }
        if (BleOwnerVault.load(activity,activeAddress)==null) pairWithBoot();
        else authorizeStored();
    }
    /** Physical BOOT/GPIO0 button must be held after CORE starts, until read completes. */
    @SuppressWarnings("MissingPermission")
    void pairWithBoot(){
        if(!connected||enroll==null){notifyStatus("Connect CORE v0.1.24 via BLE first");return;}
        if(cfgBusy||writePending||readReplyPending||fxCapsBusy||triggerMapBusy||frameCapture.busy()||palette.busy()||nextPacket!=null||pairPending||challengePending||authPending){notifyStatus("BLE is busy; retry pairing");return;}
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
        if(cfgBusy||writePending||readReplyPending||fxCapsBusy||triggerMapBusy||frameCapture.busy()||palette.busy()||nextPacket!=null||pairPending||challengePending||authPending){notifyStatus("BLE busy; retry authorization");return;}
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
        // Legacy-token authorization must not mark authPending for an older
        // command's reply or overlap another ATT operation or queued STOP.
        if (cfgBusy || writePending || readReplyPending || fxCapsBusy || triggerMapBusy ||
                frameCapture.busy() || palette.busy() || nextPacket != null || pairPending ||
                challengePending || authPending) {
            notifyStatus("BLE busy; retry authorization"); return;
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
    /** One shared WLED-compatible ID, not an Android-only or particle-only effect menu.
     * The CORE performs all frame generation; this sends a single 17-byte BLE control packet.
     * Only capabilities exposed by firmware can be selected in the unified Studio editor.
     */
    void showFx(int effectId,int chainMask,int zoneMask,
                int r,int g,int b,int bgR,int bgG,int bgB,
                int brightness,int speed,int intensity,int paletteId,int ttl) {
        if (!canControl()) {notifyStatus("BLE FX control not authorized");return;}
        if (effectId<0||effectId>65535||chainMask<1||chainMask>3||
            zoneMask<1||zoneMask>255||brightness<0||brightness>32||
            speed<0||speed>255||intensity<0||intensity>255||!(paletteId==0||(effectId==32768&&(paletteId==2||paletteId==3||paletteId==4)))||
            ttl<1||ttl>60||r<0||r>255||g<0||g>255||b<0||b>255||
            bgR<0||bgR>255||bgG<0||bgG>255||bgB<0||bgB>255) {
            notifyStatus("Unsupported or invalid BLE FX settings");return;
        }
        enqueueWrite(new byte[]{1,5,(byte)effectId,(byte)(effectId>>8),
          (byte)chainMask,(byte)zoneMask,(byte)r,(byte)g,(byte)b,
          (byte)bgR,(byte)bgG,(byte)bgB,(byte)brightness,(byte)speed,
          (byte)intensity,(byte)paletteId,(byte)ttl});
    }
    /** Explicit NVS preset actions: 8 slots, owner authorized; no auto-start on boot. */
    void fxPreset(int action,int slot) {
        if (!canControl()) {notifyStatus("Authorize BLE before preset changes");return;}
        if (action<6||action>8||slot<0||slot>=8){
            notifyStatus("Invalid CORE preset action or slot");return;
        }
        enqueueWrite(new byte[]{1,(byte)action,(byte)slot});
    }
    void stopShow() {
        if (!canControl()) {notifyStatus("BLE show control not authorized");return;}
        enqueueWrite(new byte[]{1,3});
    }

    /** Serialize Android GATT operations; BLE GATT only allows one pending request. */
    private static boolean isStopPacket(byte[] data) {
        return data != null && data.length == 2 && data[0] == 1 && data[1] == 3;
    }
    /** A failed STOP must never release a newer deferred effect START. */
    private void discardFollowingFxIfStopFailed() {
        if (isStopPacket(pendingPacket)) {
            nextPacket=null;
            afterStopPacket=null;
        }
    }
    private void commandWriteStartFailed(String message) {
        discardFollowingFxIfStopFailed();
        writePending=false;
        authPending=false;
        pendingPacket=null;
        notifyStatus(message);
        // Do not recurse into another writeCharacteristic on the failing stack.
        ui.post(this::pumpQueue);
    }
    private void enqueueWrite(byte[] data) {
        final boolean stop = isStopPacket(data);
        if(palette.busy() && stop) palette.abortForStop();
        if(frameCapture.busy() && stop) {
            // STOP supersedes diagnostic reads. Drain the one in-flight GATT
            // operation before issuing STOP; never interrupt an ATT transaction.
            nextPacket=Arrays.copyOf(data,data.length);
            afterStopPacket=null;
            frameCapture.abortForStop();
            return;
        }
        if (writePending || readReplyPending || cfgBusy || fxCapsBusy || triggerMapBusy || frameCapture.busy() || palette.busy() || nextPacket!=null) {
            if (stop) {
                nextPacket=Arrays.copyOf(data,data.length);
                afterStopPacket=null;
            } else if (isStopPacket(nextPacket)) {
                // A later FX edit must not erase a STOP waiting behind ATT.
                // Coalesce newer edits in one successor slot after STOP.
                afterStopPacket=Arrays.copyOf(data,data.length);
            } else {
                nextPacket=Arrays.copyOf(data,data.length);
            }
            return;
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
            if(!ok)commandWriteStartFailed("BLE write could not start");
        }catch(SecurityException ex){commandWriteStartFailed("BLE permission missing");}
    }
    private void pumpQueue(){
        if (!writePending&&!readReplyPending&&!cfgBusy&&!fxCapsBusy&&!triggerMapBusy&&!frameCapture.busy()&&!palette.busy()&&nextPacket!=null){
            pendingPacket=nextPacket;
            nextPacket=isStopPacket(pendingPacket)?afterStopPacket:null;
            afterStopPacket=null;
            writeNow();
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
        entries = new ArrayAdapter<String>(activity, android.R.layout.simple_list_item_1, labels) {
            @Override public View getView(int position, View convertView, ViewGroup parent) {
                TextView row = (TextView) super.getView(position, convertView, parent);
                row.setTextColor(Color.WHITE);
                row.setBackgroundColor(Color.rgb(9, 9, 9));
                row.setPadding(24, 20, 24, 20);
                row.setMinHeight(58);
                return row;
            }
        };
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

    void scan() { headlessScan=false; scanImpl(); }
    private void scanImpl() {
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
        final int scanLease=++scanGeneration;
        found.clear();
        labels.clear();
        entries.notifyDataSetChanged();
        if (!background && !headlessScan) {
            preferredPending = null;
            SpannableString title = new SpannableString("Find OpenTail CORE");
            title.setSpan(new ForegroundColorSpan(Color.WHITE), 0, title.length(), 0);
            scanDialog = new AlertDialog.Builder(activity)
                    .setTitle(title)
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
            if (scanDialog.getWindow() != null) {
                GradientDrawable panel = new GradientDrawable();
                panel.setColor(Color.rgb(9, 9, 9));
                panel.setCornerRadius(24f * activity.getResources().getDisplayMetrics().density);
                panel.setStroke(1, Color.rgb(48, 48, 48));
                scanDialog.getWindow().setBackgroundDrawable(panel);
            }
            if (scanDialog.getButton(AlertDialog.BUTTON_NEGATIVE) != null)
                scanDialog.getButton(AlertDialog.BUTTON_NEGATIVE).setTextColor(Color.WHITE);
        }
        try {
            scanning = true;
            scanner.startScan(scanCallback);
            if (!background) notifyStatus("Scanning for OpenTail CORE (10 seconds)…");
            ui.postDelayed(() -> {
                if (!scanning || scanGeneration!=scanLease || stopped) return;
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
            final int scanLease=scanGeneration;
            ui.post(() -> {
                if(stopped || !scanning || scanGeneration!=scanLease)return;
                String address = result.getDevice().getAddress();
                if (!found.containsKey(address)) {
                    found.put(address, result.getDevice());
                    labels.add(name + "  •  " + address);
                    entries.notifyDataSetChanged();
                    if(composeScanResults!=null)composeScanResults.onFound(address,name);
                }
                if (preferredPending != null && preferredPending.equalsIgnoreCase(address)) {
                    preferredPending = null;
                    stopScan();
                    connectReadOnly(result.getDevice());
                }
            });
        }
        @Override public void onScanFailed(int errorCode) {
            final int scanLease=scanGeneration;
            ui.post(() -> {
                if(stopped || !scanning || scanGeneration!=scanLease)return;
                stopScan();
                notifyStatus("BLE scan error " + errorCode);
            });
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
        frameCapture.cancel();palette.reset();Arrays.fill(cfgKnownRevision,-1);authorized=false;command=null;reply=null;enroll=null;challenge=null;cfgCommand=null;cfgReply=null;fxCaps=null;fxCapsBusy=false;triggerMapping=null;triggerMapBusy=false;triggerMapSavePending=false;triggerMapExpected=null;pairPending=false;challengePending=false;authPending=false;cfgBusy=false;cfgWatchdog++;writePending=false;readReplyPending=false;pendingPacket=null;nextPacket=null;afterStopPacket=null;
        connecting = true;
        connected = false;
        notifyStatus("Connecting to OpenTail BLE…");
        try {
            gatt = device.connectGatt(activity, false, callback, BluetoothDevice.TRANSPORT_LE);
            if(gatt==null){connecting=false;notifyStatus("BLE connection could not start; rescan and retry");}
        } catch (SecurityException ex) {
            connecting=false;notifyStatus("BLE connection permission denied");
        }
    }

    private final BluetoothGattCallback callback = new BluetoothGattCallback() {
        @SuppressWarnings("MissingPermission")
        @Override public void onConnectionStateChange(BluetoothGatt connection, int status, int newState) {
            if (stopped || connection != gatt) {
                connection.close();
                return; // late callback must not close the new CORE session
            }
            if (status == BluetoothGatt.GATT_SUCCESS && newState == BluetoothProfile.STATE_CONNECTED) {
                connecting = false;
                connected = true;
                notifyStatus("BLE connected · negotiating MTU…");
                try { mtuNegotiating=connection.requestMtu(247); }
                catch (SecurityException ex) { mtuNegotiating=false; }
                if(!mtuNegotiating) discoverCoreServices(connection);
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED || status != BluetoothGatt.GATT_SUCCESS) {
                connecting = false;
                connected = false;
                frameCapture.cancel();palette.reset();Arrays.fill(cfgKnownRevision,-1);authorized = false;command=null;reply=null;fxCaps=null;fxCapsBusy=false;fxNotify("[]");triggerMapping=null;triggerMapBusy=false;triggerMapSavePending=false;triggerMapExpected=null;enroll=null;challenge=null;cfgCommand=null;cfgReply=null;pairPending=false;challengePending=false;authPending=false;cfgBusy=false;cfgWatchdog++;writePending=false;readReplyPending=false;pendingPacket=null;nextPacket=null;afterStopPacket=null;
                notifyStatus("BLE disconnected (GATT status " + status + ")");
                connection.close();
                if (gatt == connection) gatt = null;
            }
        }

        @SuppressWarnings("MissingPermission")
        @Override public void onMtuChanged(BluetoothGatt connection, int mtu, int status) {
            if(stopped || connection!=gatt)return;
            if(mtuNegotiating){
                mtuNegotiating=false;
                notifyStatus("BLE MTU "+mtu+"; discovering services…");
                discoverCoreServices(connection);
            }
        }

        /** A rejected discovery call otherwise leaves the connection awaiting
         * an event Android will never deliver. Keep the existing user retry
         * path and never issue an overlapping second discovery.
         */
        @SuppressWarnings("MissingPermission")
        private void discoverCoreServices(BluetoothGatt connection) {
            if(stopped || connection!=gatt)return;
            try {
                if(!connection.discoverServices())
                    notifyStatus("BLE service discovery could not start; reconnect CORE");
            } catch(SecurityException ex) {
                notifyStatus("Bluetooth permission missing for BLE service discovery");
            }
        }

        @SuppressWarnings("MissingPermission")
        @Override public void onServicesDiscovered(BluetoothGatt connection, int status) {
            if(stopped || connection!=gatt)return;
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
            fxCaps = service.getCharacteristic(FX_CAPS_UUID);
            triggerMapping = service.getCharacteristic(TRIGGER_MAP_UUID);
            enroll = service.getCharacteristic(ENROLL_UUID);
            challenge = service.getCharacteristic(CHALLENGE_UUID);
            activeAddress=connection.getDevice().getAddress();
            cfgCommand = service.getCharacteristic(CFG_CMD_UUID);
            cfgReply = service.getCharacteristic(CFG_RSP_UUID);
            frameCapture.configure(connection,service.getCharacteristic(FRAME_CMD_UUID),service.getCharacteristic(FRAME_RSP_UUID));
            paletteCommand=service.getCharacteristic(PALETTE_CMD_UUID);
            paletteReply=service.getCharacteristic(PALETTE_RSP_UUID);
            paletteCapabilities=service.getCharacteristic(PALETTE_CAP_UUID);
            palette.configure(connection,paletteCommand,paletteReply,paletteCapabilities);
            authorized = false;
            if (preferredListener != null) preferredListener.accept(connection.getDevice().getAddress());
            // The info read is optional discovery metadata. If Android rejects
            // its ATT start, do not strand owner auto-authorization indefinitely.
            boolean infoStarted=false;
            try { infoStarted=connection.readCharacteristic(info); }
            catch (SecurityException ex) { notifyStatus("Bluetooth permission missing for CORE information read"); }
            if (!infoStarted) {
                notifyStatus("CORE information read unavailable; authorization can still proceed");
                if (challenge!=null && BleOwnerVault.load(activity,activeAddress)!=null)
                    ui.post(thisAutoAuth(connection));
            }
        }

        @Override public void onCharacteristicRead(BluetoothGatt connection, BluetoothGattCharacteristic characteristic,
                                                    byte[] value, int status) {
            if(stopped || connection!=gatt)return;
            if (ENROLL_UUID.equals(characteristic.getUuid())) {ownerRead(connection,value,status);return;}
            if (CHALLENGE_UUID.equals(characteristic.getUuid())) {ownerChallenge(value,status);return;}
            if (CFG_RSP_UUID.equals(characteristic.getUuid())) { cfgResponse(value,status);return; }
            if (PALETTE_RSP_UUID.equals(characteristic.getUuid())) {palette.read(connection,value,status);return;}
            if (PALETTE_CAP_UUID.equals(characteristic.getUuid())) {palette.capability(connection,value,status);return;}
            if (FRAME_RSP_UUID.equals(characteristic.getUuid())) { frameCapture.read(connection,value,status);return; }
            if (REPLY_UUID.equals(characteristic.getUuid())) { acceptReply(value,status);return; }
            if (FX_CAPS_UUID.equals(characteristic.getUuid())) {fxRead(value,status);return;}
            if (TRIGGER_MAP_UUID.equals(characteristic.getUuid())) {triggerMapRead(connection,value,status);return;}
            if (INFO_UUID.equals(characteristic.getUuid())) {
                finishInfoRead(connection,value,status);
            }
        }

        @SuppressWarnings("deprecation")
        @Override public void onCharacteristicRead(BluetoothGatt connection, BluetoothGattCharacteristic characteristic,
                                                    int status) {
            if(stopped || connection!=gatt)return;
            if (Build.VERSION.SDK_INT < 33) {
                if (ENROLL_UUID.equals(characteristic.getUuid())) {ownerRead(connection,characteristic.getValue(),status);return;}
                if (CHALLENGE_UUID.equals(characteristic.getUuid())) {ownerChallenge(characteristic.getValue(),status);return;}
                if (CFG_RSP_UUID.equals(characteristic.getUuid())) {cfgResponse(characteristic.getValue(),status);return;}
                if (PALETTE_RSP_UUID.equals(characteristic.getUuid())) {palette.read(connection,characteristic.getValue(),status);return;}
                if (PALETTE_CAP_UUID.equals(characteristic.getUuid())) {palette.capability(connection,characteristic.getValue(),status);return;}
                if (FRAME_RSP_UUID.equals(characteristic.getUuid())) {frameCapture.read(connection,characteristic.getValue(),status);return;}
                if (REPLY_UUID.equals(characteristic.getUuid())) {
                    acceptReply(characteristic.getValue(),status);return;
                }
                if (FX_CAPS_UUID.equals(characteristic.getUuid())) {fxRead(characteristic.getValue(),status);return;}
                if (TRIGGER_MAP_UUID.equals(characteristic.getUuid())) {triggerMapRead(connection,characteristic.getValue(),status);return;}
                if (INFO_UUID.equals(characteristic.getUuid())) {
                    finishInfoRead(connection,characteristic.getValue(),status);
                }
            }
        }
        @Override public void onCharacteristicWrite(BluetoothGatt connection,BluetoothGattCharacteristic characteristic,int status){
            if(stopped || connection!=gatt)return;
            if (CFG_CMD_UUID.equals(characteristic.getUuid())) {cfgWritten(connection,status);return;}
            if (PALETTE_CMD_UUID.equals(characteristic.getUuid())) {palette.written(connection,status);return;}
            if (FRAME_CMD_UUID.equals(characteristic.getUuid())) {frameCapture.written(connection,status);return;}
            if (TRIGGER_MAP_UUID.equals(characteristic.getUuid())) {triggerMapWritten(connection,status);return;}
            if (!COMMAND_UUID.equals(characteristic.getUuid())) return;
            writePending=false;
            if(status!=BluetoothGatt.GATT_SUCCESS){
                discardFollowingFxIfStopFailed();
                authPending=false;authorized=false;
                notifyStatus("BLE write failed (GATT " + status + ")");pumpQueue();return;
            }
            if(reply==null){discardFollowingFxIfStopFailed();authPending=false;notifyStatus("CORE missing BLE reply characteristic");pumpQueue();return;}
            readReplyPending=true;
            try {
                if (!connection.readCharacteristic(reply)){
                    readReplyPending=false;discardFollowingFxIfStopFailed();authPending=false;
                    notifyStatus("Could not read CORE BLE result");pumpQueue();
                }
            } catch(SecurityException ex){readReplyPending=false;discardFollowingFxIfStopFailed();authPending=false;notifyStatus("BLE read permission missing");pumpQueue();}
        }
    };

    // INFO is optional metadata, not a prerequisite for owner authentication.
    // Both API-level GATT read callbacks must continue the same auth flow,
    // including when a started read returns a failure status.
    private void finishInfoRead(BluetoothGatt connection,byte[] value,int status) {
        if (status==BluetoothGatt.GATT_SUCCESS && value!=null)
            notifyStatus("BLE connected · " + new String(value,StandardCharsets.UTF_8));
        else notifyStatus("BLE connected; information read failed: " + status);
        if (challenge!=null && BleOwnerVault.load(activity,activeAddress)!=null)
            ui.post(thisAutoAuth(connection));
    }

    private Runnable thisAutoAuth(BluetoothGatt owner) {
        return ()->{
            if (!stopped && owner==gatt && connected && !canControl()) authorizeStored();
        };
    }

    /** Both FX command and output-config responses can revoke CORE ownership. */
    private void revokeOwnerSession(){
        authorized=false;
        Arrays.fill(cfgKnownRevision,-1);
        Arrays.fill(cfgLatest,null);
        nextPacket=null;
        afterStopPacket=null;
        fxNotify("[]");
        palette.configure(gatt,paletteCommand,paletteReply,paletteCapabilities);
    }

    private void acceptReply(byte[] raw,int status){
        readReplyPending=false;
        String response = status==BluetoothGatt.GATT_SUCCESS && raw!=null
                ? new String(raw,StandardCharsets.US_ASCII) : "ERR READ";
        if (response.startsWith("ERR")) discardFollowingFxIfStopFailed();
        if(authPending){authorized="OK AUTH".equals(response);authPending=false;}
        if("ERR AUTH".equals(response)) revokeOwnerSession();
        if(response.startsWith("ERR"))notifyStatus("BLE · " + response + " (Wi-Fi control unaffected)");
        else notifyStatus("BLE · " + response + (authorized?" · lighting enabled":""));
        pumpQueue();
    }

    @SuppressWarnings("MissingPermission")
    void stopScan() {
        if (!scanning) return;
        scanning = false;
        scanGeneration++;
        try { if (scanner != null) scanner.stopScan(scanCallback); }
        catch (SecurityException | IllegalStateException ignored) { }
    }

    @SuppressWarnings("MissingPermission")
    void shutdown() {
        stopped = true;
        frameCapture.cancel();palette.reset();
        connected = false;
        connecting = false;
        authorized = false;cfgBusy=false;fxCaps=null;fxCapsBusy=false;fxNotify("[]");triggerMapping=null;triggerMapBusy=false;triggerMapSavePending=false;triggerMapExpected=null;cfgWatchdog++;
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
