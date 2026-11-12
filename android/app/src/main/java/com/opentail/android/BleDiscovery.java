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
    // Issue #126 authenticated Panel/Zone protocol. Optional on older CORE builds.
    private static final UUID PANEL_CAP_UUID = UUID.fromString("55a56022-4915-7a92-cf44-b8b7102eaf30");
    private static final UUID PANEL_CFG_CMD_UUID = UUID.fromString("55a56022-4915-7a92-cf44-b8b7102eaf31");
    private static final UUID PANEL_CFG_RSP_UUID = UUID.fromString("55a56022-4915-7a92-cf44-b8b7102eaf32");
    // Firmware #127 Panel v4's authenticated two-second white/cyan Pixel Probe.
    // This diagnostic does not alter zones, ownership, flash or Show Modes.
    private static final UUID PIXEL_PROBE_UUID = UUID.fromString("55a56022-4915-7a92-cf44-b8b7102eaf33");
    private static final UUID PANEL_PREVIEW_CONTROL_UUID = UUID.fromString("55a56022-4915-7a92-cf44-b8b7102eaf35");
    private static final UUID PANEL_PREVIEW_UPDATE_UUID = UUID.fromString("55a56022-4915-7a92-cf44-b8b7102eaf36");
    private static final UUID PANEL_PREVIEW_STATUS_UUID = UUID.fromString("55a56022-4915-7a92-cf44-b8b7102eaf37");
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

    /**
     * The white Connect action must never require Android Settings > Unpair.
     * Prefer the last connected CORE address, then an existing Android bond,
     * then the regular BLE discovery picker. Bonding and HMAC ownership remain
     * separate; this method never removes a bond or bypasses authorization.
     */
    @SuppressWarnings("MissingPermission")
    void connectFromApp(String preferredAddress) {
        if(stopped)return;
        if(connected) {
            if(canControl())notifyStatus("OpenTail CORE already connected and authorized");
            else {
                notifyStatus("OpenTail CORE connected; owner authorization required");
                if(BleOwnerVault.load(activity,activeAddress)!=null)authorizeStored();
            }
            return;
        }
        if(connecting) {
            // A second explicit Connect tap is a user-requested retry, not an
            // instruction to unpair in Android Settings. Retire this pending
            // app-owned GATT registration and immediately try again.
            BluetoothGatt pending=gatt;
            if(pending!=null){
                scanFallbackAllowed=false;
                recoverStalledGatt(pending,"Retrying OpenTail CORE connection");
                ui.postDelayed(()->{
                    if(!stopped && !connecting && !connected)
                        connectFromApp(preferredAddress);
                },500L);
            }else {
                connecting=false;
                ui.post(()->connectFromApp(preferredAddress));
            }
            return;
        }
        BluetoothManager manager=(BluetoothManager)activity.getSystemService(Activity.BLUETOOTH_SERVICE);
        BluetoothAdapter adapter=manager==null?null:manager.getAdapter();
        if(adapter==null||!adapter.isEnabled()){notifyStatus("Enable Bluetooth, then tap Connect");return;}
        if(Build.VERSION.SDK_INT>=31 &&
           activity.checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT)!=PackageManager.PERMISSION_GRANTED) {
            scanHeadless();return; // regular Nearby Devices permission request
        }
        if(scanning)stopScan();
        if(preferredAddress!=null && BluetoothAdapter.checkBluetoothAddress(preferredAddress)){
            try {
                connectReadOnly(adapter.getRemoteDevice(preferredAddress),true);
                return;
            }catch(IllegalArgumentException | SecurityException ex){
                notifyStatus("Saved CORE address unavailable; searching for controller");
            }
        }
        try {
            BluetoothDevice match=null;
            boolean multiple=false;
            for(BluetoothDevice device:adapter.getBondedDevices()) {
                String name=device.getName();
                if(name!=null && name.startsWith("OpenTail CORE")) {
                    if(match!=null){multiple=true;break;}
                    match=device;
                }
            }
            if(match!=null && !multiple){
                connectReadOnly(match,true);
                return;
            }
        }catch(SecurityException ex){notifyStatus("Bluetooth permission needed for remembered controllers");}
        scanHeadless();
    }
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
    // A user-requested read-only output reload outranks the continuously
    // sampled LED Preview. Never overlap an active ATT transaction.
    private volatile int queuedOutputRead=-1;
    private volatile BluetoothGatt queuedOutputOwner;
    private volatile int queuedOutputLease=0;
    private void clearQueuedOutputRead(){
        queuedOutputRead=-1;queuedOutputOwner=null;queuedOutputLease++;
    }
    // One bounded user action may wait for the current diagnostic ATT callback.
    // Re-enter its normal validation on the same GATT session after capture yields.
    private volatile Runnable previewCommand;
    private volatile BluetoothGatt previewCommandOwner;
    private boolean deferBehindFrame(Runnable action) {
        if(!frameCapture.busy() || previewCommand!=null)return false;
        previewCommandOwner=gatt;
        previewCommand=action;
        frameCapture.abortForStop();
        return true;
    }
    private void clearPreviewCommand(){previewCommand=null;previewCommandOwner=null;}
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
        if(cfgBusy||writePending||readReplyPending||fxCapsBusy||infoReadPending||triggerMapBusy||
           panelReadPending||panelSnapshotBusy||panelControlWritePending||panelUpdateWritePending||panelControlQueued||panelUpdateQueued||panelReadQueued!=0||
           panelSnapshotQueuedLane>=0||panelCancelQueued||
           frameCapture.busy()||palette.busy()||nextPacket!=null||
           authPending||pairPending||challengePending){
            notifyStatus("BLE busy; retry palette action after STOP/transfer");return false;
        }
        return true;
    }
    boolean negotiatePaletteV3(){
        if(canControl() && palette.available() && deferBehindFrame(this::negotiatePaletteV3))return true;
        if(!canControl()||!palette.available()){notifyStatus("CORE palette v3 unavailable or unauthorized");return false;}
        return paletteGate() && palette.negotiate();
    }
    boolean downloadPalette(int slot){
        if(canTransferPalettes() && deferBehindFrame(()->downloadPalette(slot)))return true;
        return canTransferPalettes() && paletteGate() && palette.load(slot);}
    boolean uploadPalette(int slot,long revision,String stops){
        if(canTransferPalettes() && deferBehindFrame(()->uploadPalette(slot,revision,stops)))return true;
        if(!canTransferPalettes()||!paletteGate())return false;
        try{return palette.upload(slot,revision,NativePaletteV3.stops(stops));}
        catch(IllegalArgumentException ex){notifyStatus(ex.getMessage());return false;}
    }
    boolean deletePalette(int slot,long revision){
        if(canTransferPalettes() && deferBehindFrame(()->deletePalette(slot,revision)))return true;
        if(!canTransferPalettes()||!paletteGate())return false;
        try{return palette.delete(slot,revision);}
        catch(IllegalArgumentException ex){notifyStatus(ex.getMessage());return false;}
    }
    private final CoreFrameCapture frameCapture=new CoreFrameCapture((output,sequence,rgb,error)->{
        CoreFrameCapture.Listener cb=frameListener;
        final BluetoothGatt owner=gatt;
        if(cb!=null) {
            frameCallbackPending=true;
            ui.post(()->{
                try {
                    if(!stopped && owner!=null && owner==gatt && canCaptureFrame())
                        cb.done(output,sequence,rgb,error);
                } finally {frameCallbackPending=false;}
            });
        }
        // An ATT watchdog may fire while Android still owns that operation.
        // Reconnect rather than starting a queued config request concurrently.
        if("CORE snapshot BLE operation timed out".equals(error)){
            if(queuedOutputRead>=0)
                cfgNotify(false,"CORE snapshot ATT stalled; reconnect before reloading output",null,queuedOutputRead);
            // Recovery closes GATT on the UI queue. Revoke admission now so
            // no UI/callback can start another ATT operation in that gap.
            connected=false;authorized=false;
            this.frameCapture.cancel();
            clearQueuedOutputRead();
            clearPreviewCommand();
            recoverStalledGatt(owner,"CORE snapshot ATT timed out; reconnect required");
            return;
        }
        pumpQueue();
    });
    void setFrameListener(CoreFrameCapture.Listener cb) { frameListener=cb; }
    boolean canCaptureFrame() {return canControl() && frameCapture.supported();}
    void cancelFrameCapture() { frameCapture.abortForStop(); }
    boolean captureFrame(int output) {
        if(!canCaptureFrame())return false;
        // UI preview probes at display cadence. A refused opportunistic capture
        // is normal backpressure, not a connection failure or user-facing BLE
        // error. Do not publish status on every busy tick (or restart Compose's
        // effect coroutine). Crucially, never overlap any pending ATT action.
        if(previewCommand!=null)return false;
        if(frameCapture.busy() || palette.busy() || frameCallbackPending || cfgBusy || queuedOutputRead>=0 || infoReadPending || infoReadQueued || writePending || nextPacket!=null ||
           readReplyPending || fxCapsBusy || infoReadPending || triggerMapBusy || authPending || pairPending || challengePending ||
           panelReadPending || panelSnapshotBusy || panelControlWritePending || panelUpdateWritePending || panelControlQueued || panelUpdateQueued || panelReadQueued!=0 ||
           panelSnapshotQueuedLane>=0 || panelCancelQueued)return false;
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
    // A long ATT Read Blob may fail to complete without an Android callback.
    // Never strand the shared GATT transaction queue indefinitely.
    private volatile int fxCapsWatchLease=0;
    private static final long FX_CAPABILITIES_TIMEOUT_MS=12000L;
    // After a catalog timeout, allow other commands on the recovered link.
    // The owner can explicitly retry, but auto-auth must not loop on the same stalled read.
    private volatile boolean fxCapsAutoReadSuppressed=false;
    private BluetoothGattCharacteristic triggerMapping;
    private BluetoothGattCharacteristic panelCapabilitiesCharacteristic;
    private BluetoothGattCharacteristic panelConfigCommand,panelConfigReply;
    private BluetoothGattCharacteristic panelPreviewControl,panelPreviewUpdate,panelPreviewStatus;
    private BluetoothGattCharacteristic pixelProbeCharacteristic;
    private volatile boolean pixelProbeWritePending=false;
    private volatile int pixelProbeWatchLease=0,pixelProbeLane=-1;
    private Consumer<String> pixelProbeListener;
    private PanelPreviewProtocol.Capabilities panelCapabilities;
    private final TelemetrySequenceWatchdog panelWatchdog=new TelemetrySequenceWatchdog();
    private PanelPreviewProtocol.Status panelLastStatus;
    private final PanelPreviewProtocol.Status[] panelStatuses=new PanelPreviewProtocol.Status[2];
    private final boolean[] panelStatusFresh=new boolean[2];
    private final PanelPreviewProtocol.LaneSnapshot[] panelConfirmedSnapshots=
        new PanelPreviewProtocol.LaneSnapshot[2];
    private final GeometryTransactionManager[] panelGeometryManagers=
        new GeometryTransactionManager[2];
    private final boolean[] panelSnapshotFresh=new boolean[2];
    private Consumer<PanelPreviewProtocol.Status> panelStatusListener;
    private Consumer<TelemetrySequenceWatchdog.Decision> panelSyncListener;
    private Consumer<PanelPreviewProtocol.LaneSnapshot> panelSnapshotListener;
    // Durable v4 writes use the same Panel CFG characteristic as snapshots.
    // ATT acknowledgement never establishes persistence; verify active readback.
    private boolean panelSaveBusy, panelSaveWaitingWrite, panelSaveWaitingRead;
    private int panelSaveLane=-1,panelSavePhase,panelSaveOffset,panelSaveLastCount;
    private int panelSaveRevision,panelSaveLease;
    private byte[] panelSaveWire;

    private volatile int panelReadQueued=0; // 1=capabilities, 2=status
    private volatile boolean panelReadPending=false;
    private volatile int panelReadKind=0,panelReadWatchLease=0;
    private volatile int panelStatusExpectedLane=-1;
    private int panelStatusReadRetries=0,panelStatusRetryLease=0;
    private volatile int panelSnapshotQueuedLane=-1;
    private volatile boolean panelSnapshotBusy=false,panelSnapshotWaitingWrite=false,
                             panelSnapshotWaitingRead=false,panelSnapshotAbortForStop=false;
    private volatile int panelSnapshotPhase=0,panelSnapshotOffset=0,panelSnapshotWatchLease=0;
    private int panelSnapshotLane=-1;
    private byte[] panelSnapshotWire;
    private PanelPreviewProtocol.SnapshotHeader panelSnapshotHeader;
    private static final int PANEL_CONTROL_BEGIN=1,PANEL_CONTROL_COMMIT=2,PANEL_CONTROL_HEARTBEAT=3;
    private volatile boolean panelCancelQueued=false,panelCancelWaitingWrite=false;
    private volatile boolean panelControlQueued=false,panelControlWaitingWrite=false;
    private volatile boolean panelUpdateQueued=false,panelUpdateWritePending=false;
    private volatile boolean panelControlWritePending=false;
    private byte[] panelCancelPacket,panelControlPacket,panelUpdatePacket;
    private int panelControlAction=0,panelControlLane=-1,panelUpdateLane=-1;
    private int panelControlWriteLease=0,panelUpdateWriteLease=0,negotiatedMtu=23;
    private long panelCancelAttemptedSession=-1L;
    private volatile int panelPollLease=0;
    private final int[] panelTransactionPollAttempts={0,0};
    private final boolean[] panelTransactionTimeoutLatched={false,false};
    private boolean panelReconciliationPending=false,panelReconciliationSnapshotVerified=false;
    private int panelReconciliationAttempts=0;
    private PanelPreviewProtocol.LaneSnapshot panelReconciliationSnapshot;
    private volatile boolean triggerMapBusy=false;
    private boolean triggerMapSavePending=false;
    private int[] triggerMapExpected;
    private int triggerMapBaseRevision;
    private Consumer<NativeTriggerMappingCodec.Snapshot> triggerMapListener;
    private volatile boolean mtuNegotiating=false;
    private volatile boolean infoReadPending=false;
    private BluetoothGattCharacteristic coreInfo;
    private volatile String coreInfoText;
    private boolean infoReadQueued=false;
    private Consumer<String> infoListener;
    private volatile boolean continuousFx=false;
    private volatile boolean tertiaryFx=false;
    boolean canContinuousFx(){return canControl() && continuousFx;}
    boolean canTertiaryFx(){return canContinuousFx() && tertiaryFx;}
    void setInfoListener(Consumer<String> listener){infoListener=listener;}
    private void clearInfo(){
        continuousFx=false;tertiaryFx=false;
        coreInfo=null;coreInfoText=null;infoReadQueued=false;
        final Consumer<String> receiver=infoListener;
        if(receiver!=null)ui.post(()->receiver.accept(null));
    }
    private Consumer<String> fxCapabilitiesListener;
    void setFxCapabilitiesListener(Consumer<String> listener){fxCapabilitiesListener=listener;}
    void setTriggerMapListener(Consumer<NativeTriggerMappingCodec.Snapshot> listener){
        triggerMapListener=listener;
    }
    void setPanelStatusListener(Consumer<PanelPreviewProtocol.Status> status,
                                Consumer<TelemetrySequenceWatchdog.Decision> sync) {
        panelStatusListener=status;panelSyncListener=sync;
    }
    void setPanelSnapshotListener(Consumer<PanelPreviewProtocol.LaneSnapshot> listener) {
        panelSnapshotListener=listener;
    }
    void setPixelProbeListener(Consumer<String> listener) { pixelProbeListener=listener; }
    private void probeNotify(String message) {
        final BluetoothGatt owner=gatt;
        notifyStatus(message);
        if(pixelProbeListener!=null)ui.post(()->{
            if(!stopped && owner==gatt && pixelProbeListener!=null)
                pixelProbeListener.accept(message);
        });
    }
    boolean canProbeFirstPixel() {
        return canControl() && panelCapabilities!=null &&
               (panelCapabilities.flags&1)!=0 && pixelProbeCharacteristic!=null;
    }
    /** White fixed pixel zero for two seconds, unlike a role-gated Solid FX.
     * This is firmware's existing 0xAA five-byte diagnostic, not a new opcode,
     * not a moving sweep, and never a durable config write.
     */
    @SuppressWarnings({"MissingPermission","deprecation"})
    boolean probeFirstPixel(int lane) {
        if(lane<0||lane>1||!canProbeFirstPixel()) {
            probeNotify("Pixel probe unavailable: authorize BLE and read Panel capabilities");
            return false;
        }
        // A probe is optional. Never interrupt or overlap another ATT action;
        // STOP remains queued ahead of all subsequent diagnostics.
        if(panelSaveBusy||pixelProbeWritePending||cfgBusy||queuedOutputRead>=0||
           writePending||readReplyPending||fxCapsBusy||infoReadPending||infoReadQueued||
           triggerMapBusy||panelReadPending||panelSnapshotBusy||
           panelControlWritePending||panelUpdateWritePending||
           panelControlQueued||panelUpdateQueued||panelReadQueued!=0||
           panelSnapshotQueuedLane>=0||panelCancelQueued||
           frameCapture.busy()||palette.busy()||nextPacket!=null||
           authPending||pairPending||challengePending||gatt==null) {
            probeNotify("Pixel probe busy: finish the current BLE transfer and retry");
            return false;
        }
        // [0xAA, output (one-based), index high, index low, white].
        // Only pixel zero is probed; all other LEDs retain their normal state.
        final byte[] packet=new byte[]{(byte)0xaa,(byte)(lane+1),0,0,1};
        final BluetoothGatt owner=gatt;
        pixelProbeWritePending=true;
        pixelProbeLane=lane;
        final int lease=++pixelProbeWatchLease;
        boolean started=false;
        try {
            if(Build.VERSION.SDK_INT>=33)
                started=owner.writeCharacteristic(pixelProbeCharacteristic,packet,
                    BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT)==BluetoothStatusCodes.SUCCESS;
            else {
                pixelProbeCharacteristic.setWriteType(BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT);
                pixelProbeCharacteristic.setValue(packet);
                started=owner.writeCharacteristic(pixelProbeCharacteristic);
            }
        }catch(SecurityException ex) {
            pixelProbeWritePending=false;
            pixelProbeLane=-1;
            pixelProbeWatchLease++;
            probeNotify("Pixel probe failed: Bluetooth write permission missing");
            return false;
        }
        if(!started) {
            pixelProbeWritePending=false;
            pixelProbeLane=-1;
            pixelProbeWatchLease++;
            probeNotify("Pixel probe failed: GATT write did not start");
            return false;
        }
        ui.postDelayed(()->{
            if(!stopped && owner==gatt && pixelProbeWritePending &&
               lease==pixelProbeWatchLease) {
                pixelProbeWritePending=false;pixelProbeLane=-1;
                pixelProbeWatchLease++;
                probeNotify("Pixel probe timed out; BLE state uncertain, reconnect before retry");
                recoverStalledGatt(owner,"Pixel probe ATT write timeout");
            }
        },8000L);
        return true;
    }
    boolean canReadPanelPreview() {
        return canControl() && panelCapabilities!=null && panelCapabilities.supportsPreviewV2() &&
               panelPreviewStatus!=null && panelConfigCommand!=null && panelConfigReply!=null;
    }
    /** Both authoritative lane snapshots and status-v2 reads are fresh and idle. */
    boolean panelGeometryReady() {
        if(!canReadPanelPreview())return false;
        for(int lane=0;lane<2;lane++) {
            PanelPreviewProtocol.Status status=panelStatuses[lane];
            GeometryTransactionManager manager=panelGeometryManagers[lane];
            if(!panelSnapshotFresh[lane]||!panelStatusFresh[lane]||
               panelConfirmedSnapshots[lane]==null||status==null||manager==null||
               status.state!=PanelPreviewProtocol.STATE_IDLE||status.sessionId!=0||
               manager.state()!=GeometryTransactionManager.State.READY||manager.syncUncertain())
                return false;
        }
        return true;
    }
    /** Current negotiated ATT MTU used by both the batch planner and the writer. */
    int panelUpdateMtu(){return Math.max(23,negotiatedMtu);}
    boolean readPanelStatus() {
        if(!canControl() || panelPreviewStatus==null || panelCapabilities==null ||
           !panelCapabilities.supportsPreviewV2())return false;
        panelStatusReadRetries=0;panelStatusRetryLease++;
        panelReadQueued=2;pumpQueue();return true;
    }
    boolean readPanelLaneSnapshot(int lane) {
        if(lane<0||lane>1||!canReadPanelPreview())return false;
        panelSnapshotFresh[lane]=false;panelStatusFresh[lane]=false;
        panelSnapshotQueuedLane=lane;pumpQueue();return true;
    }
    /** Begin only from two fresh, stable, authoritative lane maps/statuses. */
    boolean beginPanelPreview(int lane,GeometryTransactionManager.LayoutMapping mapping) {
        if(lane<0||lane>1||!canReadPanelPreview()||panelPreviewControl==null||
           mapping==null||panelControlQueued||panelControlWritePending||panelUpdateQueued||
           panelUpdateWritePending||panelCancelQueued)return false;
        for(int i=0;i<2;i++)
            if(!panelSnapshotFresh[i]||!panelStatusFresh[i]||
               panelConfirmedSnapshots[i]==null||panelStatuses[i]==null)return false;
        PanelPreviewProtocol.LaneSnapshot[] snapshots=panelConfirmedSnapshots.clone();
        PanelPreviewProtocol.Status[] statuses=panelStatuses.clone();
        try {GeometryTransactionManager.validateMappingAgainstSnapshots(mapping,snapshots,statuses);}
        catch(IllegalArgumentException ex){notifyStatus("Panel geometry preflight rejected: "+ex.getMessage());return false;}
        PanelPreviewProtocol.Status status=statuses[lane];
        GeometryTransactionManager manager=panelGeometryManagers[lane];
        if(manager==null||manager.state()!=GeometryTransactionManager.State.READY||
           status.state!=PanelPreviewProtocol.STATE_IDLE||status.sessionId!=0)return false;
        GeometryTransactionManager.Snapshot confirmed=manager.lastConfirmed();
        if(confirmed==null||confirmed.revision!=snapshots[lane].activeRevision||
           !Arrays.equals(confirmed.wire,snapshots[lane].wire))return false;
        if(!manager.requestBegin())return false;
        byte[] packet;
        // activeRevision is decoded as uint32 for shared status telemetry;
        // BEGIN carries the authoritative uint16 active revision.
        try {packet=PanelPreviewProtocol.begin(lane,(int)status.activeRevision,
            status.pixelCapacity,panelCapabilities.schemaVersion);}
        catch(IllegalArgumentException ex) {
            manager.markSynchronizationUncertain("Panel BEGIN values changed after preflight");
            panelStatusExpectedLane=lane;panelReadQueued=2;pumpQueue();return false;
        }
        if(queuePanelControl(lane,PANEL_CONTROL_BEGIN,packet))return true;
        manager.markSynchronizationUncertain("Panel BEGIN could not enter the serialized GATT queue");
        panelStatusExpectedLane=lane;panelReadQueued=2;pumpQueue();return false;
    }

    /** One acknowledged UPDATE at a time; callers send the next MTU batch only after status-v2 ACK. */
    boolean updatePanelPreview(int lane,byte[] candidateWire,
            PanelPreviewProtocol.UpdateDescriptor[] descriptors) {
        if(lane<0||lane>1||!canReadPanelPreview()||panelPreviewUpdate==null||
           panelControlQueued||panelControlWritePending||panelUpdateQueued||
           panelUpdateWritePending||panelCancelQueued)return false;
        GeometryTransactionManager manager=panelGeometryManagers[lane];
        PanelPreviewProtocol.Status status=panelStatuses[lane];
        if(manager==null||status==null||!panelStatusFresh[lane]||
           manager.state()!=GeometryTransactionManager.State.ACTIVE||
           status.state!=PanelPreviewProtocol.STATE_ACTIVE||
           status.sessionId!=manager.sessionId())return false;
        GeometryTransactionManager.Update update;
        try {update=manager.queueUpdate(candidateWire,descriptors,negotiatedMtu);}
        catch(IllegalArgumentException|IllegalStateException ex) {
            notifyStatus("Panel UPDATE rejected before BLE write: "+ex.getMessage());return false;
        }
        if(update==null)return false;
        panelUpdatePacket=update.packet;panelUpdateLane=lane;panelUpdateQueued=true;
        pumpQueue();return true;
    }

    boolean commitPanelPreview(int lane) {
        if(lane<0||lane>1||!canReadPanelPreview()||panelPreviewControl==null||
           panelControlQueued||panelControlWritePending||panelUpdateQueued||
           panelUpdateWritePending||panelCancelQueued)return false;
        GeometryTransactionManager manager=panelGeometryManagers[lane];
        PanelPreviewProtocol.Status status=panelStatuses[lane];
        if(manager==null||status==null||!panelStatusFresh[lane]||
           manager.state()!=GeometryTransactionManager.State.ACTIVE||
           status.state!=PanelPreviewProtocol.STATE_ACTIVE||
           status.sessionId!=manager.sessionId())return false;
        byte[] packet=manager.requestCommit(status);
        if(packet==null)return false;
        if(queuePanelControl(lane,PANEL_CONTROL_COMMIT,packet))return true;
        manager.markSynchronizationUncertain("Panel COMMIT could not enter the serialized GATT queue");
        panelStatusExpectedLane=lane;panelReadQueued=2;pumpQueue();return false;
    }

    boolean heartbeatPanelPreview(int lane) {
        if(lane<0||lane>1||!canReadPanelPreview()||panelPreviewControl==null||
           panelControlQueued||panelControlWritePending||panelUpdateQueued||
           panelUpdateWritePending||panelCancelQueued)return false;
        GeometryTransactionManager manager=panelGeometryManagers[lane];
        PanelPreviewProtocol.Status status=panelStatuses[lane];
        if(manager==null||status==null||!panelStatusFresh[lane]||
           manager.state()!=GeometryTransactionManager.State.ACTIVE||
           status.state!=PanelPreviewProtocol.STATE_ACTIVE||
           status.sessionId!=manager.sessionId())return false;
        return queuePanelControl(lane,PANEL_CONTROL_HEARTBEAT,
            PanelPreviewProtocol.heartbeat(lane,manager.sessionId()));
    }

    boolean cancelPanelPreview(int lane) {
        if(lane<0||lane>1||!canReadPanelPreview()||panelPreviewControl==null||
           panelCancelQueued||panelControlQueued||panelControlWritePending||
           panelUpdateQueued||panelUpdateWritePending)return false;
        GeometryTransactionManager manager=panelGeometryManagers[lane];
        PanelPreviewProtocol.Status status=panelStatuses[lane];
        if(manager==null||status==null||!panelStatusFresh[lane]||
           manager.state()!=GeometryTransactionManager.State.ACTIVE||
           status.state!=PanelPreviewProtocol.STATE_ACTIVE||
           status.sessionId!=manager.sessionId())return false;
        manager.markSynchronizationUncertain("Preview cancelled by owner");
        panelCancelPacket=PanelPreviewProtocol.cancel(lane,manager.sessionId());
        panelCancelQueued=true;pumpQueue();return true;
    }

    private boolean queuePanelControl(int lane,int action,byte[] packet) {
        if(packet==null||lane<0||lane>1||gatt==null||!canControl()||
           panelControlQueued||panelControlWritePending||panelUpdateQueued||
           panelUpdateWritePending||panelCancelQueued)return false;
        panelControlPacket=packet;panelControlLane=lane;panelControlAction=action;
        panelControlQueued=true;pumpQueue();return true;
    }

    boolean savePanelV4(int lane,String json) {
        if(lane<0||lane>1||!canReadPanelPreview()||panelSaveBusy||
           panelSnapshotBusy||panelSnapshotQueuedLane>=0||panelReadPending||
           panelReadQueued!=0||panelControlQueued||panelUpdateQueued||
           panelControlWritePending||panelUpdateWritePending||
           panelCancelQueued||cfgBusy||writePending||readReplyPending||
           pixelProbeWritePending||frameCapture.busy()||palette.busy()||
           nextPacket!=null||triggerMapBusy||fxCapsBusy||authPending||
           pairPending||challengePending)return false;
        PanelPreviewProtocol.LaneSnapshot base=lastConfirmedPanelSnapshot(lane);
        if(base==null||base.pending||!base.persistent||base.activeRevision>=65535)
            return false;
        try {
            byte[] candidate=PanelV4DraftCodec.encode(
                lane,new org.json.JSONObject(json),base);
            panelSaveBusy=true;panelSaveLane=lane;panelSaveWire=candidate;
            panelSaveRevision=base.activeRevision;panelSavePhase=0;
            panelSaveOffset=0;
            panelSaveWrite(PanelV4LayoutProtocol.beginSave(lane,panelSaveRevision,candidate));
            return true;
        }catch(Exception ex){
            notifyStatus("Panel v4 Save validation failed: "+ex.getMessage());
            return false;
        }
    }
    @SuppressWarnings({"MissingPermission","deprecation"})
    private void panelSaveWrite(byte[] packet) {
        if(gatt==null||panelConfigCommand==null) {
            panelSaveFinish(false,"Panel Save connection unavailable; reconcile before retry",null);
            return;
        }
        BluetoothGatt owner=gatt;
        int lease=++panelSaveLease;
        panelSaveWaitingWrite=true;
        boolean ok=false;
        try {
            if(Build.VERSION.SDK_INT>=33)
                ok=owner.writeCharacteristic(panelConfigCommand,packet,
                    BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT)==BluetoothStatusCodes.SUCCESS;
            else {
                panelConfigCommand.setWriteType(BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT);
                panelConfigCommand.setValue(packet);
                ok=owner.writeCharacteristic(panelConfigCommand);
            }
        }catch(SecurityException ex){notifyStatus("Panel Save Bluetooth permission denied");}
        if(!ok) {
            panelSaveFinish(false,"Panel Save write unavailable; outcome uncertain",null);
            return;
        }
        ui.postDelayed(()->{
            if(!stopped&&owner==gatt&&panelSaveBusy&&panelSaveLease==lease)
                recoverStalledGatt(owner,"Panel v4 Save timed out; reconnect and reconcile");
        },8000L);
    }
    private void panelSaveWritten(BluetoothGatt owner,int status) {
        if(owner!=gatt||!panelSaveBusy||!panelSaveWaitingWrite)return;
        panelSaveWaitingWrite=false;
        if(status!=BluetoothGatt.GATT_SUCCESS) {
            panelSaveFinish(false,"Panel v4 Save GATT "+status+"; outcome uncertain",null);
            return;
        }
        panelSaveWaitingRead=true;
        boolean ok=false;
        try{ok=owner.readCharacteristic(panelConfigReply);}
        catch(SecurityException ex){notifyStatus("Panel Save ACK permission denied");}
        if(!ok)panelSaveFinish(false,"Panel Save ACK unavailable; outcome uncertain",null);
    }
    private void panelSaveRead(BluetoothGatt owner,byte[] bytes,int status) {
        if(owner!=gatt||!panelSaveBusy||!panelSaveWaitingRead)return;
        panelSaveWaitingRead=false;panelSaveLease++;
        if(status!=BluetoothGatt.GATT_SUCCESS) {
            panelSaveFinish(false,"Panel Save ACK GATT "+status+"; outcome uncertain",null);
            return;
        }
        int opcode=panelSavePhase==0?3:panelSavePhase==1?4:
            panelSavePhase==2?5:6;
        int result;
        try{result=PanelV4LayoutProtocol.parseSaveAck(panelSaveLane,opcode,bytes);}
        catch(IllegalArgumentException ex){
            panelSaveFinish(false,"Panel Save ACK mismatch; reconcile before retry",null);
            return;
        }
        if(result!=0) {
            panelSaveFinish(false,"Controller rejected Panel v4 command ("+result+")",null);
            return;
        }
        if(panelSavePhase==3) {
            panelSaveFinish(false,"Save interrupted by STOP before finalization",null);
            return;
        }
        if(panelSavePhase==2) {
            panelSavePhase=4;
            // COMMIT ACK is only admission. Read active v4 journal confirmation.
            startPanelSnapshot(panelSaveLane);
            return;
        }
        if(panelSavePhase==1)panelSaveOffset+=panelSaveLastCount;
        else panelSavePhase=1;
        if(isStopPacket(nextPacket)) {
            panelSavePhase=3;
            panelSaveWrite(PanelV4LayoutProtocol.cancelUncommittedSave(panelSaveLane));
        }else if(panelSaveOffset>=panelSaveWire.length) {
            panelSavePhase=2;
            panelSaveWrite(PanelV4LayoutProtocol.completeSave(panelSaveLane));
        }else {
            byte[] chunk=PanelV4LayoutProtocol.saveChunk(
                panelSaveLane,panelSaveWire,panelSaveOffset);
            panelSaveLastCount=chunk.length-5;
            panelSaveWrite(chunk);
        }
    }
    private void panelSaveFinish(boolean success,String message,org.json.JSONObject confirmed) {
        int lane=panelSaveLane;
        panelSaveBusy=false;panelSaveWaitingWrite=false;panelSaveWaitingRead=false;
        panelSaveLane=-1;panelSaveWire=null;panelSavePhase=0;panelSaveOffset=0;
        panelSaveLease++;
        if(lane>=0&&lane<2)cfgNotify(success,message,confirmed,lane);
        if(!success)notifyStatus(message);
        pumpQueue();
    }

    PanelPreviewProtocol.LaneSnapshot lastConfirmedPanelSnapshot(int lane) {
        if(lane<0||lane>1||!panelSnapshotFresh[lane])return null;
        return panelConfirmedSnapshots[lane];
    }
    GeometryTransactionManager panelGeometryManager(int lane) {
        if(lane<0||lane>1)return null;
        return panelGeometryManagers[lane];
    }
    private void queuePanelCapabilitiesRead() {
        if(canControl() && panelCapabilitiesCharacteristic!=null) {
            panelReadQueued=1;pumpQueue();
        }
    }
    private void panelNotify(PanelPreviewProtocol.Status status,
                             TelemetrySequenceWatchdog.Decision decision) {
        final BluetoothGatt owner=gatt;
        if(panelStatusListener!=null)ui.post(()->{
            if(!stopped && owner==gatt && panelStatusListener!=null)
                panelStatusListener.accept(status);
        });
        if(panelSyncListener!=null && decision!=null)ui.post(()->{
            if(!stopped && (owner==gatt || status==null) && panelSyncListener!=null)
                panelSyncListener.accept(decision);
        });
    }
    private void panelDisconnected() {
        if(panelWatchdog.hasSession())panelNotify(null,panelWatchdog.markDisconnected());
        // The old BLE session is no longer authoritative. A reconnect may
        // discover another controller or a newer committed map, even if the
        // previous manager was READY. Never reuse that manager's revision or
        // its Panel/Zone aliases to authorize a new BEGIN.
        for(int lane=0;lane<2;lane++) {
            if(panelGeometryManagers[lane]!=null)
                panelGeometryManagers[lane].connectionLost();
            panelGeometryManagers[lane]=null;
            panelConfirmedSnapshots[lane]=null;
        }
        panelCapabilities=null;panelLastStatus=null;
        panelStatuses[0]=null;panelStatuses[1]=null;
        panelStatusFresh[0]=false;panelStatusFresh[1]=false;
        panelSnapshotFresh[0]=false;panelSnapshotFresh[1]=false;
        panelCapabilitiesCharacteristic=null;panelConfigCommand=null;panelConfigReply=null;
        panelPreviewControl=null;panelPreviewUpdate=null;panelPreviewStatus=null;
        pixelProbeCharacteristic=null;pixelProbeWritePending=false;
        pixelProbeLane=-1;pixelProbeWatchLease++;
        panelReadQueued=0;panelReadPending=false;panelReadKind=0;panelReadWatchLease++;
        panelStatusExpectedLane=-1;panelStatusReadRetries=0;panelStatusRetryLease++;
        panelSnapshotQueuedLane=-1;panelSnapshotBusy=false;panelSnapshotWaitingWrite=false;
        panelSnapshotWaitingRead=false;panelSnapshotAbortForStop=false;
        if(panelSaveBusy)panelSaveFinish(false,"Panel v4 Save interrupted; outcome uncertain",null);
        panelSnapshotWatchLease++;panelCancelQueued=false;
        panelCancelWaitingWrite=false;panelControlWritePending=false;panelCancelPacket=null;
        panelControlQueued=false;panelControlWaitingWrite=false;panelControlPacket=null;
        panelControlAction=0;panelControlLane=-1;panelControlWriteLease++;
        panelUpdateQueued=false;panelUpdateWritePending=false;panelUpdatePacket=null;
        panelUpdateLane=-1;panelUpdateWriteLease++;negotiatedMtu=23;
        panelPollLease++;panelReconciliationPending=false;
        Arrays.fill(panelTransactionPollAttempts,0);
        Arrays.fill(panelTransactionTimeoutLatched,false);
        panelReconciliationSnapshotVerified=false;panelReconciliationSnapshot=null;
    }
    boolean canMapTriggers(){return canControl() && triggerMapping!=null;}
    private boolean triggerMapAvailable(){
        if(!canMapTriggers()){notifyStatus("CORE trigger mapping BLE v1 unavailable or unauthorized");return false;}
        if(gatt==null||cfgBusy||writePending||readReplyPending||fxCapsBusy||infoReadPending||triggerMapBusy||
           panelReadPending||panelSnapshotBusy||panelControlWritePending||panelUpdateWritePending||panelControlQueued||panelUpdateQueued||panelReadQueued!=0||
           panelSnapshotQueuedLane>=0||panelCancelQueued||
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
        if(canMapTriggers() && deferBehindFrame(this::readTriggerMap))return true;
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
        if(canMapTriggers() && mapping!=null) {
            int[] requested=Arrays.copyOf(mapping,mapping.length);
            if(deferBehindFrame(()->saveTriggerMap(expectedRevision,requested)))return true;
        }
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

    /** Initial capability discovery is optional, never allowed to block authorized controls. */
    void readFxCapabilitiesAfterAuth() {
        if (fxCapsAutoReadSuppressed) {
            notifyStatus("BLE effects catalog auto-read paused after timeout; use manual retry");
            return;
        }
        readFxCapabilitiesInternal();
    }
    /** Manual read explicitly retries a catalog previously timed out. */
    void readFxCapabilities() {
        readFxCapabilitiesInternal();
    }
    @SuppressWarnings("MissingPermission")
    private void readFxCapabilitiesInternal() {
        final BluetoothGatt owner=gatt;
        if(!connected||owner==null){
            notifyStatus("BLE effects catalog unavailable: CORE disconnected");
            fxNotify("[]");
            return;
        }
        if(fxCaps==null){
            notifyStatus("BLE effects catalog characteristic 0x29 missing on connected CORE");
            fxNotify("[]");
            return;
        }
        if(deferBehindFrame(this::readFxCapabilitiesInternal))return;
        if(cfgBusy||writePending||readReplyPending||fxCapsBusy||infoReadPending||triggerMapBusy||frameCapture.busy()||palette.busy()||nextPacket!=null||authPending||pairPending||challengePending){
            notifyStatus("BLE busy; retry supported effects");return;
        }
        fxCapsBusy=true;
        final int lease=++fxCapsWatchLease;
        try {
            if(!owner.readCharacteristic(fxCaps)){
                fxCapsBusy=false;
                ++fxCapsWatchLease;
                notifyStatus("BLE effects catalog ATT read could not start");
                fxNotify("[]");pumpQueue();
                return;
            }
        } catch(SecurityException ex){
            fxCapsBusy=false;
            ++fxCapsWatchLease;
            notifyStatus("Bluetooth permission denied for BLE effects catalog");
            fxNotify("[]");pumpQueue();
            return;
        }
        ui.postDelayed(()->{
            if(stopped || owner!=gatt || lease!=fxCapsWatchLease || !fxCapsBusy)return;
            fxCapsAutoReadSuppressed=true;
            // The outstanding GATT operation may still own ATT. Do not just
            // clear fxCapsBusy and start other reads on a wedged connection.
            recoverStalledGatt(owner,"CORE effects catalog ATT read timed out");
        },FX_CAPABILITIES_TIMEOUT_MS);
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
        if(!fxCapsBusy)return; // ignore unsolicited/late callback
        fxCapsBusy=false;
        ++fxCapsWatchLease;
        if(status!=BluetoothGatt.GATT_SUCCESS){
            notifyStatus("CORE effects catalog ATT read failed (GATT "+status+")");
            fxNotify("[]");pumpQueue();return;
        }
        if(raw==null||raw.length<2){
            notifyStatus("CORE effects catalog was missing or truncated");
            fxNotify("[]");pumpQueue();return;
        }
        try {
            String ids=BleFxCapabilities.decode(raw);
            fxCapsAutoReadSuppressed=false;
            notifyStatus("CORE effects catalog read: "+(raw[1]&255)+" effects");
            fxNotify(ids);
        } catch(IllegalArgumentException ex){
            notifyStatus("CORE effect catalog invalid ("+raw.length+" bytes): "+ex.getMessage());
            fxNotify("[]");
        }
        pumpQueue();
    }
    private BluetoothGattCharacteristic enroll,challenge;
    private volatile boolean pairPending=false,challengePending=false;
    private String activeAddress="";
    private String connectingAddress="";
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
    // Monotonic physical BLE timing. These are measurements, not fabricated
    // guarantees of flash persistence or on-device light output.
    private long cfgRequestedMs=0L,cfgTransferMs=0L,cfgReadbackMs=0L;
    private int cfgReadFragments=0;
    private String cfgTimingDetail() {
        long now=android.os.SystemClock.elapsedRealtime();
        if(cfgRequestedMs<=0L)return "";
        long elapsed=Math.max(0L,now-cfgRequestedMs);
        long queue=cfgTransferMs>0L?Math.max(0L,cfgTransferMs-cfgRequestedMs):0L;
        long readback=cfgReadbackMs>0L?Math.max(0L,now-cfgReadbackMs):0L;
        return " · BLE "+elapsed+" ms (queue "+queue+" ms, readback "+
            readback+" ms, "+cfgReadFragments+" fragments)";
    }
    void setConfigListener(java.util.function.Consumer<String> listener){configListener=listener;}
    boolean canConfigure(){return canControl() && cfgCommand!=null && cfgReply!=null;}
    /** Non-destructive automatic read after authorization. Busy GATT
     * operations are not interrupted or duplicated; UI may try later. */
    boolean readOutputIfReady(int index) {
        if(index<0||index>1||!canConfigure()||cfgBusy||queuedOutputRead>=0||
           writePending||readReplyPending||fxCapsBusy||infoReadPending||triggerMapBusy||
           panelReadPending||panelSnapshotBusy||panelControlWritePending||
           panelUpdateWritePending||panelControlQueued||panelUpdateQueued||
           panelReadQueued!=0||panelSnapshotQueuedLane>=0||panelCancelQueued||
           palette.busy()||nextPacket!=null||authPending||pairPending||challengePending)
            return false;
        readOutput(index);
        return true;
    }
    void readOutput(int index){
        if(!canConfigure()){cfgNotify(false,"Pair and authorize BLE before editing CORE outputs",null,index);return;}
        if(index<0||index>1){cfgNotify(false,"Invalid CORE output index",null,index);return;}
        if(cfgBusy || queuedOutputRead>=0){
            cfgNotify(false,"Output configuration read/save already in progress",null,index);return;
        }
        cfgRequestedMs=android.os.SystemClock.elapsedRealtime();
        cfgTransferMs=0L;cfgReadbackMs=0L;cfgReadFragments=0;
        // Preview frames are optional telemetry. Unlike a write or STOP, a
        // read-only Output Manager reload should queue rather than fail while
        // the Layout's auto-refreshing LED Preview owns the GATT operation.
        if(frameCapture.busy()){
            queuedOutputOwner=gatt;
            queuedOutputRead=index;
            final int lease=++queuedOutputLease;
            notifyStatus("Output "+(index+1)+" read queued behind LED Preview");
            ui.postDelayed(()->{
                if(stopped || queuedOutputRead!=index || lease!=queuedOutputLease)return;
                // Do not launch a second ATT request after a stalled callback.
                clearQueuedOutputRead();
                cfgNotify(false,"CORE snapshot still busy; GATT recovery required",null,index);
                recoverStalledGatt(queuedOutputOwner!=null?queuedOutputOwner:gatt,
                    "CORE preview did not release ATT for output reload");
            },12000L);
            frameCapture.abortForStop(); // waits for in-flight ATT callback
            pumpQueue();                 // runs only after GATT drains
            return;
        }
        if(writePending||readReplyPending||fxCapsBusy||infoReadPending||triggerMapBusy||palette.busy()||
           nextPacket!=null||authPending||pairPending||challengePending){
            cfgNotify(false,"BLE is busy; "+configReadBlocker()+"; retry after current transfer",null,index);return;
        }
        beginOutputRead(index);
    }
    private void beginOutputRead(int index){
        cfgBusy=true;cfgSaving=false;cfgIndex=index;cfgPhase=1;cfgOffset=0;
        cfgTransferMs=android.os.SystemClock.elapsedRealtime();
        cfgReadbackMs=cfgTransferMs;
        cfgSend(new byte[]{2,1,(byte)index});
    }
    private String configReadBlocker(){
        if(writePending || readReplyPending)return "command acknowledgement in flight";
        if(fxCapsBusy)return "effects catalog read in flight";
        if(triggerMapBusy)return "trigger mapping in flight";
        if(palette.busy())return "palette transfer in flight";
        if(nextPacket!=null)return "queued STOP or effect command";
        if(authPending || pairPending || challengePending)return "authorization in flight";
        return "unknown pending GATT operation";
    }
    /** True means admitted (possibly waiting for one preview ATT callback).
     * Only revision-checked readback proves application. */
    boolean saveOutput(int index,String json){
        if(!canConfigure()){cfgNotify(false,"Pair and authorize BLE before saving CORE outputs",null,index);return false;}
        if(index>=0 && index<=1 && deferBehindFrame(()->saveOutput(index,json)))return true;
        if(index<0||index>1||cfgBusy||writePending||readReplyPending||fxCapsBusy||infoReadPending||triggerMapBusy||
           panelReadPending||panelSnapshotBusy||panelControlWritePending||panelUpdateWritePending||panelControlQueued||panelUpdateQueued||panelReadQueued!=0||
           panelSnapshotQueuedLane>=0||panelCancelQueued||
           frameCapture.busy()||palette.busy()||nextPacket!=null||authPending||pairPending||challengePending){cfgNotify(false,"BLE is busy; retry after current transfer",null,index);return false;}
        if(cfgKnownRevision[index]<0){cfgNotify(false,"Load this output over BLE before saving",null,index);return false;}
        if(cfgKnownRevision[index]>=65535){cfgNotify(false,"CORE output revision exhausted; no wrap or additional save permitted",null,index);return false;}
        try {cfgBytes=BleConfigCodec.encode(new org.json.JSONObject(json));}
        catch(Exception e){cfgNotify(false,"Invalid draft: "+e.getMessage(),null,index);return false;}
        cfgExpected=Arrays.copyOf(cfgBytes,cfgBytes.length);
        cfgRequestedMs=android.os.SystemClock.elapsedRealtime();
        cfgTransferMs=cfgRequestedMs;cfgReadbackMs=0L;cfgReadFragments=0;
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
        boolean wasSaving=cfgSaving;
        cfgBusy=false;cfgSaving=false;cfgWaitingWrite=false;cfgWaitingRead=false;
        cfgWatchdog++;
        // A GATT 133 or lost ACK can arrive after flash commit. Treat the
        // result as unknown; never issue an automatic second write.
        cfgNotify(false,wasSaving?
            message+" · Save unverified; reconnect and Reload before retrying.":
            message,null,index);
        pumpQueue();
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
            cfgReadFragments++;
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
                cfgNotify(true,(cfgSaving?
                    (cfgRevision==cfgSaveBase?
                        "CORE active output already matched; no new revision committed":
                        "Saved to CORE and active output verified over BLE"):
                    "Loaded active output from CORE over BLE")+cfgTimingDetail(),output,cfgIndex);
                cfgSaving=false;pumpQueue();return;
            }catch(Exception e){cfgFail("CORE returned invalid output: "+e.getMessage());return;}
        }
        if(cfgPhase==3 && opcode==0x83){cfgPhase=4;cfgOffset=0;cfgWriteNext();return;}
        if(cfgPhase==4 && opcode==0x84){
            if(cfgOffset<cfgBytes.length){cfgWriteNext();return;}
            cfgPhase=5;cfgSend(new byte[]{2,5,(byte)cfgIndex});return;
        }
        if(cfgPhase==5 && opcode==0x85){
            cfgReadbackMs=android.os.SystemClock.elapsedRealtime();
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
    private volatile int ownerWatchLease=0;
    private static final long OWNER_TRANSACTION_TIMEOUT_MS=8000L;
    private void watchOwnerTransaction(BluetoothGatt owner) {
        final int lease=++ownerWatchLease;
        ui.postDelayed(()->{
            if(!stopped && owner==gatt && lease==ownerWatchLease &&
                (pairPending || challengePending || authPending))
                recoverStalledGatt(owner,"CORE owner authorization timed out");
        },OWNER_TRANSACTION_TIMEOUT_MS);
    }
    private volatile int commandWatchLease=0;
    private static final long COMMAND_TRANSACTION_TIMEOUT_MS=8000L;
    private void watchCommandTransaction(BluetoothGatt owner) {
        final int lease=++commandWatchLease;
        ui.postDelayed(()->{
            if(!stopped && owner==gatt && lease==commandWatchLease &&
                (writePending || readReplyPending))
                recoverStalledGatt(owner,"CORE command acknowledgement timed out");
        },COMMAND_TRANSACTION_TIMEOUT_MS);
    }
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
        autoReconnectLease++;autoReconnectBudget=1;infoReadPending=false;
        stopScan();
        frameCapture.cancel();clearPreviewCommand();palette.reset(); // GATT close may never produce a disconnect callback
        panelDisconnected(); // also clear stale Panel/Probe capability and in-flight diagnostic lease
        clearQueuedOutputRead();
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
    void onAdapterEnabled() {
        notifyStatus("Bluetooth on · ready to scan or reconnect");
        scheduleReconnect(activeAddress,false);
    }
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
        if(deferBehindFrame(this::pairWithBoot))return;
        if(cfgBusy||writePending||readReplyPending||fxCapsBusy||infoReadPending||triggerMapBusy||frameCapture.busy()||palette.busy()||nextPacket!=null||pairPending||challengePending||authPending){notifyStatus("BLE is busy; retry pairing");return;}
        pairPending=true;challengePending=false;
        notifyStatus("Hold CORE BOOT button while the owner key is read…");
        try{if(!gatt.readCharacteristic(enroll)){pairPending=false;notifyStatus("Unable to request pairing");}
            else watchOwnerTransaction(gatt);}
        catch(SecurityException e){pairPending=false;notifyStatus("Bluetooth permission missing");}
    }
    /** Prefer owner-key authentication, with Wi-Fi token retained only as manual fallback. */
    @SuppressWarnings("MissingPermission")
    void authorizeStored(){
        if(!connected||challenge==null){notifyStatus("Connect CORE v0.1.24 first, or use legacy Wi-Fi authorization");return;}
        if(BleOwnerVault.load(activity,activeAddress)==null){notifyStatus("Not paired yet: hold BOOT and tap Pair with BOOT");return;}
        if(deferBehindFrame(this::authorizeStored))return;
        if(cfgBusy||writePending||readReplyPending||fxCapsBusy||infoReadPending||triggerMapBusy||frameCapture.busy()||palette.busy()||nextPacket!=null||pairPending||challengePending||authPending){notifyStatus("BLE busy; retry authorization");return;}
        challengePending=true;
        try{if(!gatt.readCharacteristic(challenge)){challengePending=false;notifyStatus("Challenge read could not start");}
            else watchOwnerTransaction(gatt);}
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
        if(deferBehindFrame(()->authorize(tokenHex)))return;
        // Legacy-token authorization must not mark authPending for an older
        // command's reply or overlap another ATT operation or queued STOP.
        if (cfgBusy || writePending || readReplyPending || fxCapsBusy || infoReadPending || triggerMapBusy ||
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
            speed<0||speed>255||intensity<0||intensity>255||!BleFxPalettePolicy.isWireCandidate(effectId,paletteId)||
            ttl<1||ttl>60||r<0||r>255||g<0||g>255||b<0||b>255||
            bgR<0||bgR>255||bgG<0||bgG>255||bgB<0||bgB>255) {
            notifyStatus("Unsupported or invalid BLE FX settings");return;
        }
        enqueueWrite(new byte[]{1,5,(byte)effectId,(byte)(effectId>>8),
          (byte)chainMask,(byte)zoneMask,(byte)r,(byte)g,(byte)b,
          (byte)bgR,(byte)bgG,(byte)bgB,(byte)brightness,(byte)speed,
          (byte)intensity,(byte)paletteId,(byte)ttl});
    }
    void showContinuousTertiaryFx(int id,int chains,int zones,int r,int g,int b,
          int br,int bg,int bb,int brightness,int speed,int intensity,int palette,int third) {
        if(!canTertiaryFx()){
            notifyStatus("CORE third-color capability unavailable or unauthorized; third color is local only");
            return;
        }
        if(!TertiaryFxCommand.supportedForFirmware(id,palette,coreInfoText)){
            notifyStatus("CORE advertised capability does not admit effect "+id+
                " palette "+palette+"; third-color command rejected without legacy fallback");
            return;
        }
        try {enqueueWrite(TertiaryFxCommand.encode(id,chains,zones,r,g,b,br,bg,bb,
            brightness,speed,intensity,palette,third));}
        catch(IllegalArgumentException ex){notifyStatus(ex.getMessage());}
    }
    void showContinuousFx(int id,int chains,int zones,int r,int g,int b,
          int br,int bg,int bb,int brightness,int speed,int intensity,int paletteId) {
        if(!canContinuousFx()){
            notifyStatus("Continuous capability not available; read CORE info or update firmware");
            return;
        }
        try {enqueueWrite(ContinuousFxCommand.encode(id,chains,zones,r,g,b,br,bg,bb,
                brightness,speed,intensity,paletteId));}
        catch(IllegalArgumentException ex){notifyStatus(ex.getMessage());}
    }
    /** Explicit NVS preset actions: 8 slots, owner authorized; no auto-start on boot. */
    void fxPreset(int action,int slot) {
        if (!canControl()) {notifyStatus("Authorize BLE before preset changes");return;}
        if (action<6||action>8||slot<0||slot>=8){
            notifyStatus("Invalid CORE preset action or slot");return;
        }
        // PLAY belongs in the existing STOP-aware command queue, so a later
        // STOP supersedes it even if it arrived during diagnostic capture.
        if(action==7 && frameCapture.busy()) {
            enqueueWrite(new byte[]{1,(byte)action,(byte)slot});return;
        }
        if(deferBehindFrame(()->fxPreset(action,slot)))return;
        // Explicit preset NVS actions cannot be overwritten by live FX coalescing.
        if(cfgBusy||writePending||readReplyPending||fxCapsBusy||infoReadPending||triggerMapBusy||
           panelReadPending||panelSnapshotBusy||panelControlWritePending||panelUpdateWritePending||panelControlQueued||panelUpdateQueued||panelReadQueued!=0||
           panelSnapshotQueuedLane>=0||panelCancelQueued||
           frameCapture.busy()||palette.busy()||nextPacket!=null||
           authPending||pairPending||challengePending){
            notifyStatus("BLE busy; retry preset action");return;
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
        if (panelSaveBusy || pixelProbeWritePending || writePending || readReplyPending || cfgBusy || fxCapsBusy || infoReadPending || triggerMapBusy ||
            panelReadPending || panelSnapshotBusy || panelControlWritePending || panelUpdateWritePending || panelControlQueued || panelUpdateQueued || panelReadQueued!=0 ||
            panelSnapshotQueuedLane>=0 || panelCancelQueued ||
            frameCapture.busy() || palette.busy() || nextPacket!=null) {
            if (stop) {
                nextPacket=Arrays.copyOf(data,data.length);
                afterStopPacket=null;
                if(panelSnapshotBusy) {
                    panelSnapshotAbortForStop=true;
                    if(!panelSnapshotWaitingWrite&&!panelSnapshotWaitingRead)
                        panelSnapshotFail("Panel snapshot aborted to prioritize STOP");
                }
            } else if (isStopPacket(nextPacket)) {
                // A later FX edit must not erase a STOP waiting behind ATT.
                // Coalesce newer edits in one successor slot after STOP.
                afterStopPacket=Arrays.copyOf(data,data.length);
            } else {
                nextPacket=Arrays.copyOf(data,data.length);
            }
            if(frameCapture.busy())frameCapture.abortForStop();
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
            else watchCommandTransaction(gatt);
        }catch(SecurityException ex){commandWriteStartFailed("BLE permission missing");}
    }
    private void pumpQueue(){
        if(panelSaveBusy||pixelProbeWritePending||writePending||readReplyPending||cfgBusy||fxCapsBusy||infoReadPending||triggerMapBusy||
           panelReadPending||panelSnapshotBusy||panelControlWritePending||panelUpdateWritePending||
           frameCapture.busy()||palette.busy()||authPending||pairPending||challengePending)return;
        // Preserve existing STOP-before-FX order. Never prioritize a read
        // over a previously queued safety STOP.
        if(nextPacket!=null){
            pendingPacket=nextPacket;
            nextPacket=isStopPacket(pendingPacket)?afterStopPacket:null;
            afterStopPacket=null;
            writeNow();
            return;
        }
        if(previewCommand!=null) {
            Runnable action=previewCommand;
            BluetoothGatt owner=previewCommandOwner;
            clearPreviewCommand();
            if(!stopped && owner!=null && owner==gatt && connected)action.run();
            // An action may be declined by its normal validation. Continue to
            // drain queued STOP/FX only after rechecking all transaction flags.
            pumpQueue();
            return;
        }
        if(queuedOutputRead>=0){
            if(queuedOutputOwner!=gatt || !canConfigure()){
                clearQueuedOutputRead();return;
            }
            int index=queuedOutputRead;
            clearQueuedOutputRead();
            beginOutputRead(index);
            return;
        }
        if(panelCancelQueued) {
            startPanelCancel();
            return;
        }
        if(panelControlQueued) {
            startPanelControl();
            return;
        }
        if(panelUpdateQueued) {
            startPanelUpdate();
            return;
        }
        if(panelSnapshotQueuedLane>=0) {
            int lane=panelSnapshotQueuedLane;panelSnapshotQueuedLane=-1;
            startPanelSnapshot(lane);
            return;
        }
        if(panelReadQueued!=0) {
            startPanelRead();
            return;
        }
        if(infoReadQueued && coreInfo!=null && gatt!=null){
            infoReadQueued=false;
            infoReadPending=true;
            boolean started=false;
            try{started=gatt.readCharacteristic(coreInfo);}
            catch(SecurityException ex){notifyStatus("CORE firmware information read permission denied");}
            if(!started)infoReadPending=false;
        }
    }
    private String preferredPending = null;
    private Consumer<String> preferredListener;
    private static final long RECONNECT_DELAY_MS = 10000L;
    private long lastReconnectAttempt = 0L;
    // One bounded automatic reconnect per established CORE session.
    private volatile int autoReconnectBudget=1;
    private volatile int autoReconnectLease=0;
    private void scheduleReconnect(String address, boolean afterScan) {
        if(address==null || address.isEmpty())return;
        ui.post(()->{
            if(stopped || connecting || connected || autoReconnectBudget<=0)return;
            autoReconnectBudget--;
            final int lease=++autoReconnectLease;
            // Retry promptly after a ready-session drop; allow the one-shot
            // discovery fallback its full window after an initial failure.
            final long delay=afterScan?SCAN_DURATION_MS+2000L:1800L;
            ui.postDelayed(()->{
                if(lease==autoReconnectLease && !stopped && !connected && !connecting && !scanning)
                    tryReconnect(address);
            },delay);
        });
    }

    void setPreferredListener(Consumer<String> listener) { this.preferredListener = listener; }

    /** One bounded direct GATT reconnect for the previously selected CORE address. */
    void tryReconnect(String address) {
        if (stopped || connected || connecting || scanning || address == null || address.isEmpty()) return;
        if (lastReconnectAttempt!=0L && android.os.SystemClock.elapsedRealtime() - lastReconnectAttempt < RECONNECT_DELAY_MS) return;
        if (Build.VERSION.SDK_INT >= 31 &&
                activity.checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) return;
        lastReconnectAttempt = android.os.SystemClock.elapsedRealtime();
        // A known device does not need advertising or a scan permission to
        // establish a GATT connection; Android OS bonding is retained.
        connectFromApp(address);
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
                    // Prefer a single-tap BLE connect on a bench with exactly one
                    // controller; retain manual selection for multiple devices.
                    if(headlessScan && preferredPending==null && found.size()==1){
                        ui.postDelayed(()->{
                            if(stopped || !scanning || scanGeneration!=scanLease ||
                               connecting || connected || found.size()!=1)return;
                            BluetoothDevice sole=found.values().iterator().next();
                            stopScan();
                            connectReadOnly(sole);
                        },1800L);
                    }
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
    private void connectReadOnly(BluetoothDevice device) {connectReadOnly(device,false);}

    private volatile int connectionAttempt=0;
    private volatile boolean scanFallbackAllowed=false;
    private static final long GATT_SETUP_TIMEOUT_MS=18000L;

    /** In-app recovery for a GATT client that never calls back or never discovers CORE. */
    @SuppressWarnings("MissingPermission")
    private void recoverStalledGatt(BluetoothGatt owner, String reason) {
        ui.post(()->{
            if(stopped || owner!=gatt)return;
            boolean fallback=scanFallbackAllowed;
            scanFallbackAllowed=false;
            connectionAttempt++;
            connected=false;connecting=false;authorized=false;mtuNegotiating=false;infoReadPending=false;
            panelDisconnected();
            frameCapture.cancel();clearPreviewCommand();palette.reset();clearQueuedOutputRead();fxCaps=null;fxCapsBusy=false;
            command=null;reply=null;enroll=null;challenge=null;triggerMapping=null;
            triggerMapBusy=false;triggerMapSavePending=false;triggerMapExpected=null;
            cfgCommand=null;cfgReply=null;cfgBusy=false;cfgSaving=false;cfgWatchdog++;
            pairPending=false;challengePending=false;authPending=false;
            writePending=false;readReplyPending=false;pendingPacket=null;nextPacket=null;afterStopPacket=null;
            Arrays.fill(cfgKnownRevision,-1);Arrays.fill(cfgLatest,null);
            if(gatt==owner)gatt=null;
            try{owner.disconnect();}catch(Exception ignored){}
            try{owner.close();}catch(Exception ignored){}
            notifyStatus(reason+(fallback?" · retrying BLE discovery":" · tap Connect to retry"));
            scheduleReconnect(connectingAddress,fallback);
            if(fallback)ui.postDelayed(()->{
                if(!stopped && !connected && !connecting && !scanning)scanHeadless();
            },350L);
        });
    }

    /** The GATT registration, MTU and service discovery each may fail silently. */
    private void watchGattReady(BluetoothGatt owner,int attempt){
        ui.postDelayed(()->{
            if(stopped || attempt!=connectionAttempt || gatt!=owner)return;
            if(!connected || command==null || reply==null || infoReadPending || challengePending || authPending)
                recoverStalledGatt(owner,"CORE BLE connection/setup timed out");
        },GATT_SETUP_TIMEOUT_MS);
    }

    @SuppressWarnings("MissingPermission")
    private void connectReadOnly(BluetoothDevice device,boolean knownDevice) {
        if (Build.VERSION.SDK_INT >= 31
                && activity.checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) {
            notifyStatus("Nearby-device permission required");
            return;
        }
        // A controller switch invalidates every old Panel session and readback.
        panelDisconnected();
        // Never overlap a stale client registration with a new GATT connection.
        if (gatt != null) {
            BluetoothGatt previous=gatt;gatt=null;
            try{previous.disconnect();}catch(Exception ignored){}
            try{previous.close();}catch(Exception ignored){}
        }
        scanFallbackAllowed=knownDevice;
        // A failed switch must never silently reconnect the previous vehicle.
        connectingAddress=device.getAddress();
        autoReconnectLease++;
        final int attempt=++connectionAttempt;
        frameCapture.cancel();clearPreviewCommand();palette.reset();clearQueuedOutputRead();Arrays.fill(cfgKnownRevision,-1);authorized=false;infoReadPending=false;command=null;reply=null;enroll=null;challenge=null;cfgCommand=null;cfgReply=null;fxCaps=null;fxCapsBusy=false;triggerMapping=null;triggerMapBusy=false;triggerMapSavePending=false;triggerMapExpected=null;pairPending=false;challengePending=false;authPending=false;cfgBusy=false;cfgWatchdog++;writePending=false;readReplyPending=false;pendingPacket=null;nextPacket=null;afterStopPacket=null;
        connecting = true;
        connected = false;
        notifyStatus("Connecting to OpenTail BLE…");
        try {
            gatt = device.connectGatt(activity, false, callback, BluetoothDevice.TRANSPORT_LE);
            if(gatt==null){
                connecting=false;
                scanFallbackAllowed=false;
                notifyStatus("BLE connection could not start; rescan and retry");
                if(knownDevice)ui.post(this::scanHeadless);
            }else watchGattReady(gatt,attempt);
        } catch (SecurityException ex) {
            connecting=false;scanFallbackAllowed=false;notifyStatus("BLE connection permission denied");
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
                boolean fallback=scanFallbackAllowed;
                scanFallbackAllowed=false;
                connectionAttempt++;
                infoReadPending=false;
                connecting = false;
                connected = false;
                frameCapture.cancel();clearPreviewCommand();palette.reset();clearQueuedOutputRead();panelDisconnected();Arrays.fill(cfgKnownRevision,-1);authorized = false;command=null;reply=null;fxCaps=null;fxCapsBusy=false;fxNotify("[]");triggerMapping=null;triggerMapBusy=false;triggerMapSavePending=false;triggerMapExpected=null;enroll=null;challenge=null;cfgCommand=null;cfgReply=null;pairPending=false;challengePending=false;authPending=false;cfgBusy=false;cfgWatchdog++;writePending=false;readReplyPending=false;pendingPacket=null;nextPacket=null;afterStopPacket=null;
                notifyStatus("BLE disconnected (GATT status " + status + ")");
                connection.close();
                if (gatt == connection) gatt = null;
                scheduleReconnect(connectingAddress,fallback);
                if(fallback)ui.postDelayed(()->{
                    if(!stopped && !connected && !connecting && !scanning)scanHeadless();
                },350L);
            }
        }

        @SuppressWarnings("MissingPermission")
        @Override public void onMtuChanged(BluetoothGatt connection, int mtu, int status) {
            if(stopped || connection!=gatt)return;
            if(mtuNegotiating){
                mtuNegotiating=false;
                negotiatedMtu=status==BluetoothGatt.GATT_SUCCESS?Math.max(23,mtu):23;
                notifyStatus("BLE MTU "+negotiatedMtu+"; discovering services…");
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
                if(!connection.discoverServices()){
                    notifyStatus("BLE service discovery could not start; reconnect CORE");
                    recoverStalledGatt(connection,"CORE service discovery rejected by Android");
                }
            } catch(SecurityException ex) {
                notifyStatus("Bluetooth permission missing for BLE service discovery");
                recoverStalledGatt(connection,"CORE service discovery permission lost");
            }
        }

        @SuppressWarnings("MissingPermission")
        @Override public void onServicesDiscovered(BluetoothGatt connection, int status) {
            if(stopped || connection!=gatt)return;
            if (status != BluetoothGatt.GATT_SUCCESS) {
                notifyStatus("BLE service discovery failed: " + status);
                recoverStalledGatt(connection,"CORE BLE service discovery failed");
                return;
            }
            BluetoothGattService service = connection.getService(SERVICE_UUID);
            BluetoothGattCharacteristic info = service == null ? null : service.getCharacteristic(INFO_UUID);
            clearInfo(); // never show a previous controller's firmware
            coreInfo=info;
            if (info == null) {
                notifyStatus("BLE connected; no supported OpenTail info characteristic");
                recoverStalledGatt(connection,"CORE GATT service missing or outdated");
                return;
            }
            // Existing read-only connection remains compatible with older CORE firmware.
            command = service.getCharacteristic(COMMAND_UUID);
            reply = service.getCharacteristic(REPLY_UUID);
            fxCaps = service.getCharacteristic(FX_CAPS_UUID);
            triggerMapping = service.getCharacteristic(TRIGGER_MAP_UUID);
            panelCapabilitiesCharacteristic=service.getCharacteristic(PANEL_CAP_UUID);
            panelConfigCommand=service.getCharacteristic(PANEL_CFG_CMD_UUID);
            panelConfigReply=service.getCharacteristic(PANEL_CFG_RSP_UUID);
            pixelProbeCharacteristic=service.getCharacteristic(PIXEL_PROBE_UUID);
            pixelProbeWritePending=false;pixelProbeLane=-1;pixelProbeWatchLease++;
            panelPreviewControl=service.getCharacteristic(PANEL_PREVIEW_CONTROL_UUID);
            panelPreviewUpdate=service.getCharacteristic(PANEL_PREVIEW_UPDATE_UUID);
            panelPreviewStatus=service.getCharacteristic(PANEL_PREVIEW_STATUS_UUID);
            panelCapabilities=null;
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
            autoReconnectBudget=1;autoReconnectLease++;lastReconnectAttempt=0L;
            scanFallbackAllowed=false; // service discovery succeeded; clear one-shot fallback
            if (preferredListener != null) preferredListener.accept(connection.getDevice().getAddress());
            // The info read is optional discovery metadata. If Android rejects
            // its ATT start, do not strand owner auto-authorization indefinitely.
            // Stored owner-key authentication must not block on optional INFO.
            if(challenge!=null && BleOwnerVault.load(activity,activeAddress)!=null) {
                ui.post(thisAutoAuth(connection));
                return;
            }
            boolean infoStarted=false;
            infoReadPending=true;
            try { infoStarted=connection.readCharacteristic(info); }
            catch (SecurityException ex) { notifyStatus("Bluetooth permission missing for CORE information read"); }
            if (!infoStarted) {
                infoReadPending=false;
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
            if (FRAME_RSP_UUID.equals(characteristic.getUuid())) { ui.post(()->frameCapture.read(connection,value,status));return; }
            if (REPLY_UUID.equals(characteristic.getUuid())) { acceptReply(value,status);return; }
            if (FX_CAPS_UUID.equals(characteristic.getUuid())) {fxRead(value,status);return;}
            if (TRIGGER_MAP_UUID.equals(characteristic.getUuid())) {triggerMapRead(connection,value,status);return;}
            if (PANEL_CAP_UUID.equals(characteristic.getUuid())) {panelCapabilitiesRead(connection,value,status);return;}
            if (PANEL_PREVIEW_STATUS_UUID.equals(characteristic.getUuid())) {panelStatusRead(connection,value,status);return;}
            if (PANEL_CFG_RSP_UUID.equals(characteristic.getUuid())) {if(panelSaveWaitingRead)panelSaveRead(connection,value,status);else panelSnapshotRead(connection,value,status);return;}
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
                if (FRAME_RSP_UUID.equals(characteristic.getUuid())) {byte[] frameValue=characteristic.getValue();ui.post(()->frameCapture.read(connection,frameValue,status));return;}
                if (REPLY_UUID.equals(characteristic.getUuid())) {
                    acceptReply(characteristic.getValue(),status);return;
                }
                if (FX_CAPS_UUID.equals(characteristic.getUuid())) {fxRead(characteristic.getValue(),status);return;}
                if (TRIGGER_MAP_UUID.equals(characteristic.getUuid())) {triggerMapRead(connection,characteristic.getValue(),status);return;}
                if (PANEL_CAP_UUID.equals(characteristic.getUuid())) {panelCapabilitiesRead(connection,characteristic.getValue(),status);return;}
                if (PANEL_PREVIEW_STATUS_UUID.equals(characteristic.getUuid())) {panelStatusRead(connection,characteristic.getValue(),status);return;}
                if (PANEL_CFG_RSP_UUID.equals(characteristic.getUuid())) {if(panelSaveWaitingRead)panelSaveRead(connection,characteristic.getValue(),status);else panelSnapshotRead(connection,characteristic.getValue(),status);return;}
                if (INFO_UUID.equals(characteristic.getUuid())) {
                    finishInfoRead(connection,characteristic.getValue(),status);
                }
            }
        }
        @Override public void onCharacteristicWrite(BluetoothGatt connection,BluetoothGattCharacteristic characteristic,int status){
            if(stopped || connection!=gatt)return;
            if (PIXEL_PROBE_UUID.equals(characteristic.getUuid())) {
                if(!pixelProbeWritePending)return;
                final int lane=pixelProbeLane;
                pixelProbeWritePending=false;pixelProbeLane=-1;pixelProbeWatchLease++;
                probeNotify(status==BluetoothGatt.GATT_SUCCESS?
                    "Pixel 0 white probe accepted by CORE on O"+(lane+1)+
                    "; 2-second lease, actual illumination still requires physical check":
                    "Pixel 0 probe rejected on O"+(lane+1)+" (GATT "+status+")");
                pumpQueue();
                return;
            }
            if (CFG_CMD_UUID.equals(characteristic.getUuid())) {cfgWritten(connection,status);return;}
            if (PALETTE_CMD_UUID.equals(characteristic.getUuid())) {palette.written(connection,status);return;}
            if (FRAME_CMD_UUID.equals(characteristic.getUuid())) {ui.post(()->frameCapture.written(connection,status));return;}
            if (TRIGGER_MAP_UUID.equals(characteristic.getUuid())) {triggerMapWritten(connection,status);return;}
            if (PANEL_CFG_CMD_UUID.equals(characteristic.getUuid())) {if(panelSaveWaitingWrite)panelSaveWritten(connection,status);else panelSnapshotWritten(connection,status);return;}
            if (PANEL_PREVIEW_CONTROL_UUID.equals(characteristic.getUuid())) {
                if(panelCancelWaitingWrite)panelCancelWritten(connection,status);
                else panelControlWritten(connection,status);
                return;
            }
            if (PANEL_PREVIEW_UPDATE_UUID.equals(characteristic.getUuid())) {
                panelUpdateWritten(connection,status);return;
            }
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

    @SuppressWarnings("MissingPermission")
    private void startPanelRead() {
        final int kind=panelReadQueued;panelReadQueued=0;
        final BluetoothGatt owner=gatt;
        BluetoothGattCharacteristic characteristic=kind==1?
            panelCapabilitiesCharacteristic:panelPreviewStatus;
        if(owner==null||characteristic==null||!canControl()) {
            panelReadPending=false;panelReadKind=0;pumpQueue();return;
        }
        panelReadPending=true;panelReadKind=kind;
        final int lease=++panelReadWatchLease;
        boolean started=false;
        try{started=owner.readCharacteristic(characteristic);}
        catch(SecurityException ex){notifyStatus("Panel telemetry read permission denied");}
        if(!started){
            panelReadPending=false;panelReadKind=0;
            notifyStatus("Panel telemetry read could not start");pumpQueue();return;
        }
        ui.postDelayed(()->{
            if(!stopped&&owner==gatt&&panelReadPending&&panelReadWatchLease==lease)
                recoverStalledGatt(owner,"Panel telemetry ATT read timed out; reconnect required");
        },10000L);
    }

    private void panelCapabilitiesRead(BluetoothGatt owner,byte[] value,int status) {
        if(owner!=gatt)return;
        if(panelReadPending&&panelReadKind==1){panelReadPending=false;panelReadKind=0;panelReadWatchLease++;}
        if(status!=BluetoothGatt.GATT_SUCCESS||value==null){
            panelCapabilities=null;notifyStatus("CORE Panel capability read failed: "+status);
            pumpQueue();return;
        }
        try {
            panelCapabilities=PanelPreviewProtocol.Capabilities.parse(value);
            if(panelCapabilities.supportsPreviewV2()) {
                // Android's GATT stack performs the characteristic long-read;
                // the app validates the complete 61-byte value and never
                // applies the ATT read offset a second time.
                panelReadQueued=2;
            } else notifyStatus("CORE Panel preview v2 is not fully advertised; geometry editing remains read-only");
        } catch(IllegalArgumentException ex) {
            panelCapabilities=null;notifyStatus("Invalid CORE Panel capabilities: "+ex.getMessage());
        }
        pumpQueue();
    }

    private void panelRetryStatusRead(BluetoothGatt owner) {
        if(panelStatusReadRetries>=3) {
            notifyStatus("CORE Panel status retries exhausted; reconnect or request a fresh status read");
            return;
        }
        panelStatusReadRetries++;
        final int lease=++panelStatusRetryLease;
        ui.postDelayed(()->{
            if(!stopped&&owner==gatt&&panelStatusRetryLease==lease&&
               !panelReadPending&&panelReadQueued==0) {
                panelReadQueued=2;pumpQueue();
            }
        },250L);
    }

    private void panelStatusRead(BluetoothGatt owner,byte[] value,int status) {
        if(owner!=gatt)return;
        if(panelReadPending&&panelReadKind==2){panelReadPending=false;panelReadKind=0;panelReadWatchLease++;}
        if(status!=BluetoothGatt.GATT_SUCCESS||value==null){
            if(panelStatusExpectedLane>=0)panelStatusFresh[panelStatusExpectedLane]=false;
            else if(panelLastStatus!=null)panelStatusFresh[panelLastStatus.lane]=false;
            panelStatusExpectedLane=-1;
            TelemetrySequenceWatchdog.Decision malformed=panelWatchdog.markMalformedTelemetry(
                "CORE Panel status read failed; transaction synchronization is uncertain");
            for(GeometryTransactionManager manager:panelGeometryManagers)
                if(manager!=null)manager.markSynchronizationUncertain(malformed.message);
            panelNotify(null,malformed);
            notifyStatus("CORE Panel status read failed: "+status);
            panelRetryStatusRead(owner);pumpQueue();return;
        }
        final PanelPreviewProtocol.Status current;
        try{current=PanelPreviewProtocol.Status.parse(value);}
        catch(IllegalArgumentException ex){
            if(panelStatusExpectedLane>=0)panelStatusFresh[panelStatusExpectedLane]=false;
            panelStatusExpectedLane=-1;
            TelemetrySequenceWatchdog.Decision malformed=panelWatchdog.markMalformedTelemetry(
                "Invalid or truncated 61-byte CORE Panel status; authoritative reconciliation required");
            for(GeometryTransactionManager manager:panelGeometryManagers)
                if(manager!=null)manager.markSynchronizationUncertain(malformed.message);
            panelNotify(null,malformed);
            notifyStatus("Invalid CORE Panel status v2: "+ex.getMessage());
            panelRetryStatusRead(owner);pumpQueue();return;
        }
        if(panelStatusExpectedLane>=0&&current.lane!=panelStatusExpectedLane) {
            panelStatusFresh[panelStatusExpectedLane]=false;
            panelStatusExpectedLane=-1;
            notifyStatus("CORE Panel status lane did not match the requested snapshot/transaction lane");
            pumpQueue();return;
        }
        panelStatusExpectedLane=-1;
        panelStatusReadRetries=0;panelStatusRetryLease++;
        panelLastStatus=current;
        panelStatuses[current.lane]=current;
        panelStatusFresh[current.lane]=true;
        GeometryTransactionManager manager=panelGeometryManagers[current.lane];
        if(manager!=null) {
            manager.observeStatus(current);
            if(manager.state()==GeometryTransactionManager.State.BEGINNING &&
               current.state==PanelPreviewProtocol.STATE_ACTIVE)manager.acceptBegin(current);
            else if(manager.state()==GeometryTransactionManager.State.UPDATE_PENDING &&
                    current.state==PanelPreviewProtocol.STATE_ACTIVE)manager.acknowledgeUpdate(current);
            if(current.lastRejectedReason==PanelPreviewProtocol.ERR_COMMITTING_LOCKED)
                manager.rejectUpdateDuringCommit(PanelPreviewProtocol.ERR_COMMITTING_LOCKED);
        }
        TelemetrySequenceWatchdog.Decision decision=panelWatchdog.observe(current);
        if(manager!=null&&decision.warning)manager.markSynchronizationUncertain(decision.message);
        panelNotify(current,decision);

        if(manager!=null&&!panelReconciliationPending&&
           manager.state()==GeometryTransactionManager.State.RECONCILING) {
            if(!decision.requiresReconciliation)
                decision=panelWatchdog.markReconciliationFailed(manager.lastError());
            panelNotify(current,decision);
            panelBeginReconciliation(owner,current,decision);
            pumpQueue();return;
        }

        if(panelReconciliationPending&&panelReconciliationSnapshotVerified) {
            if(current.state==PanelPreviewProtocol.STATE_COMMITTING) {
                panelReconciliationSnapshotVerified=false;
                if(panelTransactionPollAttempts[current.lane]>=40&&
                   !panelTransactionTimeoutLatched[current.lane]) {
                    panelTransactionTimeoutLatched[current.lane]=true;
                    TelemetrySequenceWatchdog.Decision timeout=panelWatchdog.markReconciliationFailed(
                        "CORE commit remains COMMITTING beyond the bounded acknowledgement window; observing same commit");
                    if(manager!=null)manager.markSynchronizationUncertain(timeout.message);
                    panelNotify(current,timeout);
                }
                panelScheduleCommitPoll(owner);
                pumpQueue();return;
            }
            PanelPreviewProtocol.LaneSnapshot snapshot=panelReconciliationSnapshot;
            boolean stable=snapshot!=null&&snapshot.lane==current.lane&&
                snapshot.activeRevision==current.activeRevision&&
                snapshot.activeRevision==current.renderRevision&&
                snapshot.stagedRevision==current.stagedRevision&&
                snapshot.pending==current.configPending;
            boolean managerReconciled=true;
            if(stable) {
                GeometryTransactionManager.Snapshot confirmed=new GeometryTransactionManager.Snapshot(
                    snapshot.lane,snapshot.activeRevision,snapshot.wire);
                if(manager==null) {
                    manager=new GeometryTransactionManager(confirmed);
                    panelGeometryManagers[current.lane]=manager;
                } else managerReconciled=manager.reconcile(current,confirmed);
            }
            boolean watchdogReconciled=stable&&managerReconciled&&panelWatchdog.reconcile(current,true);
            boolean reconciled=stable&&managerReconciled&&watchdogReconciled;
            TelemetrySequenceWatchdog.Decision reconciliationFailure=null;
            if(stable&&!reconciled) {
                String why=!managerReconciled
                    ?"CORE telemetry sequence/candidate CRC does not match the authoritative geometry snapshot"
                    :"CORE telemetry watchdog rejected the authoritative geometry reconciliation";
                reconciliationFailure=panelWatchdog.markReconciliationFailed(why);
                if(manager!=null)manager.markSynchronizationUncertain(why);
            }
            panelReconciliationPending=false;
            panelReconciliationSnapshotVerified=false;
            if(reconciled) {
                panelReconciliationAttempts=0;
                panelTransactionPollAttempts[current.lane]=0;
                panelTransactionTimeoutLatched[current.lane]=false;
                panelNotify(current,panelWatchdog.snapshot());
            } else if(!stable&&panelReconciliationAttempts<1) {
                panelReconciliationAttempts++;
                panelReconciliationPending=true;
                panelSnapshotQueuedLane=current.lane;
            } else if(reconciliationFailure!=null) {
                panelNotify(current,reconciliationFailure);
                if(reconciliationFailure.hardFault&&
                   !panelWatchdog.awaitingFreshSession(current))
                    panelBeginReconciliation(owner,current,reconciliationFailure);
            }
            pumpQueue();return;
        }
        if(manager!=null) {
            GeometryTransactionManager.State transactionState=manager.state();
            boolean awaitingAck=transactionState==GeometryTransactionManager.State.BEGINNING||
                transactionState==GeometryTransactionManager.State.UPDATE_PENDING||
                transactionState==GeometryTransactionManager.State.COMMIT_REQUESTED||
                transactionState==GeometryTransactionManager.State.COMMITTING;
            if(awaitingAck) {
                if(panelTransactionPollAttempts[current.lane]>=40&&
                   !panelTransactionTimeoutLatched[current.lane]) {
                    panelTransactionTimeoutLatched[current.lane]=true;
                    TelemetrySequenceWatchdog.Decision timeout=panelWatchdog.markReconciliationFailed(
                        "CORE Panel transaction did not acknowledge its latest phase within the bounded poll window");
                    manager.markSynchronizationUncertain(timeout.message);
                    panelNotify(current,timeout);
                    panelBeginReconciliation(owner,current,timeout);
                    pumpQueue();return;
                }
                panelScheduleTransactionPoll(owner,current.lane);
            } else {
                panelTransactionPollAttempts[current.lane]=0;
                panelTransactionTimeoutLatched[current.lane]=false;
            }
        }
        if(decision.requiresReconciliation)panelBeginReconciliation(owner,current,decision);
        pumpQueue();
    }

    private void panelBeginReconciliation(BluetoothGatt owner,
            PanelPreviewProtocol.Status current,TelemetrySequenceWatchdog.Decision decision) {
        if(owner!=gatt||!canControl())return;
        if(current.state==PanelPreviewProtocol.STATE_COMMITTING) {
            panelReconciliationPending=true;
            panelScheduleCommitPoll(owner);return;
        }
        if(!panelReconciliationPending) {
            panelReconciliationPending=true;panelReconciliationAttempts=0;
            panelReconciliationSnapshotVerified=false;panelReconciliationSnapshot=null;
        }
        if(panelWatchdog.requiresCancelFor(current)) {
            if(panelCancelAttemptedSession!=current.sessionId) {
                panelCancelPacket=PanelPreviewProtocol.cancel(current.lane,current.sessionId);
                panelCancelQueued=true;pumpQueue();return;
            }
            notifyStatus("CORE still reports the hard-fault geometry session as ACTIVE after CANCEL; reconnect before editing");
            panelReconciliationPending=false;panelReconciliationSnapshotVerified=false;
            panelReconciliationSnapshot=null;return;
        }
        panelSnapshotQueuedLane=current.lane;pumpQueue();
    }

    private void panelScheduleCommitPoll(BluetoothGatt owner) {
        int lane=panelLastStatus==null?-1:panelLastStatus.lane;
        if(lane>=0)panelScheduleTransactionPoll(owner,lane);
    }

    private void panelScheduleTransactionPoll(BluetoothGatt owner,int lane) {
        if(lane<0||lane>1)return;
        final int lease=++panelPollLease;
        final int attempt=++panelTransactionPollAttempts[lane];
        long delay=attempt<=40?200L:1000L;
        ui.postDelayed(()->{
            if(!stopped&&owner==gatt&&canControl()&&panelPollLease==lease&&
               !panelReadPending&&panelReadQueued==0)readPanelStatus();
        },delay);
    }

    @SuppressWarnings({"MissingPermission","deprecation"})
    private void startPanelCancel() {
        panelCancelQueued=false;
        if(gatt==null||panelPreviewControl==null||panelCancelPacket==null) {
            panelCancelPacket=null;panelSnapshotQueuedLane=panelLastStatus==null?-1:panelLastStatus.lane;
            pumpQueue();return;
        }
        panelControlWritePending=true;panelCancelWaitingWrite=true;
        final BluetoothGatt owner=gatt;final int lease=++panelReadWatchLease;
        boolean started=false;
        try {
            if(Build.VERSION.SDK_INT>=33)
                started=owner.writeCharacteristic(panelPreviewControl,panelCancelPacket,
                    BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT)==BluetoothStatusCodes.SUCCESS;
            else {
                panelPreviewControl.setWriteType(BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT);
                panelPreviewControl.setValue(panelCancelPacket);
                started=owner.writeCharacteristic(panelPreviewControl);
            }
        } catch(SecurityException ex){notifyStatus("Panel preview CANCEL permission denied");}
        if(!started) {
            panelCancelWaitingWrite=false;panelControlWritePending=false;
            if(panelLastStatus!=null)panelCancelAttemptedSession=panelLastStatus.sessionId;
            panelCancelPacket=null;
            if(panelLastStatus!=null)panelSnapshotQueuedLane=panelLastStatus.lane;
            pumpQueue();return;
        }
        ui.postDelayed(()->{
            if(!stopped&&owner==gatt&&panelControlWritePending&&
               panelCancelWaitingWrite&&panelReadWatchLease==lease)
                recoverStalledGatt(owner,"Panel preview CANCEL timed out; reconnect required");
        },8000L);
    }

    @SuppressWarnings({"MissingPermission","deprecation"})
    private void startPanelControl() {
        panelControlQueued=false;
        if(gatt==null||panelPreviewControl==null||panelControlPacket==null) {
            GeometryTransactionManager manager=panelControlLane>=0&&panelControlLane<2
                ?panelGeometryManagers[panelControlLane]:null;
            if(manager!=null)manager.markSynchronizationUncertain("Panel control characteristic unavailable");
            panelStatusExpectedLane=panelControlLane;
            panelControlPacket=null;panelControlAction=0;panelControlLane=-1;
            panelReadQueued=2;pumpQueue();return;
        }
        panelControlWritePending=true;panelControlWaitingWrite=true;
        final BluetoothGatt owner=gatt;final int lease=++panelControlWriteLease;
        boolean started=false;
        try {
            if(Build.VERSION.SDK_INT>=33)
                started=owner.writeCharacteristic(panelPreviewControl,panelControlPacket,
                    BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT)==BluetoothStatusCodes.SUCCESS;
            else {
                panelPreviewControl.setWriteType(BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT);
                panelPreviewControl.setValue(panelControlPacket);
                started=owner.writeCharacteristic(panelPreviewControl);
            }
        } catch(SecurityException ex){notifyStatus("Panel preview control permission denied");}
        if(!started) {
            panelControlWaitingWrite=false;panelControlWritePending=false;
            GeometryTransactionManager manager=panelControlLane>=0&&panelControlLane<2
                ?panelGeometryManagers[panelControlLane]:null;
            if(manager!=null)manager.markSynchronizationUncertain("Panel control ATT write could not start");
            panelStatusExpectedLane=panelControlLane;
            panelControlPacket=null;panelControlAction=0;panelControlLane=-1;
            panelReadQueued=2;pumpQueue();return;
        }
        ui.postDelayed(()->{
            if(!stopped&&owner==gatt&&panelControlWaitingWrite&&panelControlWriteLease==lease)
                recoverStalledGatt(owner,"Panel preview control write timed out; reconnect required");
        },8000L);
    }

    @SuppressWarnings({"MissingPermission","deprecation"})
    private void startPanelUpdate() {
        panelUpdateQueued=false;
        if(gatt==null||panelPreviewUpdate==null||panelUpdatePacket==null) {
            GeometryTransactionManager manager=panelUpdateLane>=0&&panelUpdateLane<2
                ?panelGeometryManagers[panelUpdateLane]:null;
            if(manager!=null)manager.markSynchronizationUncertain("Panel UPDATE characteristic unavailable");
            panelStatusExpectedLane=panelUpdateLane;
            panelUpdatePacket=null;panelUpdateLane=-1;panelReadQueued=2;pumpQueue();return;
        }
        panelUpdateWritePending=true;
        final BluetoothGatt owner=gatt;final int lease=++panelUpdateWriteLease;
        boolean started=false;
        try {
            if(Build.VERSION.SDK_INT>=33)
                started=owner.writeCharacteristic(panelPreviewUpdate,panelUpdatePacket,
                    BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE)==BluetoothStatusCodes.SUCCESS;
            else {
                panelPreviewUpdate.setWriteType(BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE);
                panelPreviewUpdate.setValue(panelUpdatePacket);
                started=owner.writeCharacteristic(panelPreviewUpdate);
            }
        } catch(SecurityException ex){notifyStatus("Panel UPDATE permission denied");}
        if(!started) {
            panelUpdateWritePending=false;
            GeometryTransactionManager manager=panelUpdateLane>=0&&panelUpdateLane<2
                ?panelGeometryManagers[panelUpdateLane]:null;
            if(manager!=null)manager.markSynchronizationUncertain("Panel UPDATE ATT write could not start");
            panelStatusExpectedLane=panelUpdateLane;
            panelUpdatePacket=null;panelUpdateLane=-1;panelReadQueued=2;pumpQueue();return;
        }
        ui.postDelayed(()->{
            if(!stopped&&owner==gatt&&panelUpdateWritePending&&panelUpdateWriteLease==lease)
                recoverStalledGatt(owner,"Panel UPDATE write completion timed out; reconnect required");
        },8000L);
    }

    private void panelControlWritten(BluetoothGatt owner,int status) {
        if(owner!=gatt||!panelControlWaitingWrite)return;
        int lane=panelControlLane,action=panelControlAction;
        panelControlWaitingWrite=false;panelControlWritePending=false;
        panelControlPacket=null;panelControlAction=0;panelControlLane=-1;
        if(lane>=0&&lane<2) {
            GeometryTransactionManager manager=panelGeometryManagers[lane];
            if(status==BluetoothGatt.GATT_SUCCESS) {
                if(manager!=null&&action==PANEL_CONTROL_COMMIT)manager.commitWriteCompleted();
            } else if(manager!=null) {
                manager.markSynchronizationUncertain("CORE Panel control ATT write failed ("+status+")");
            }
        }
        if(status!=BluetoothGatt.GATT_SUCCESS)
            notifyStatus("CORE Panel control write failed: "+status);
        if(lane>=0&&lane<2)panelStatusExpectedLane=lane;
        if(status==BluetoothGatt.GATT_SUCCESS&&action==PANEL_CONTROL_COMMIT) {
            panelSnapshotFresh[0]=false;panelSnapshotFresh[1]=false;
            panelStatusFresh[0]=false;panelStatusFresh[1]=false;
        }
        panelReadQueued=2;pumpQueue();
    }

    private void panelUpdateWritten(BluetoothGatt owner,int status) {
        if(owner!=gatt||!panelUpdateWritePending)return;
        int lane=panelUpdateLane;
        panelUpdateWritePending=false;panelUpdatePacket=null;panelUpdateLane=-1;
        if(status!=BluetoothGatt.GATT_SUCCESS) {
            GeometryTransactionManager manager=lane>=0&&lane<2?panelGeometryManagers[lane]:null;
            if(manager!=null)manager.markSynchronizationUncertain("CORE Panel UPDATE ATT write failed ("+status+")");
            notifyStatus("CORE Panel UPDATE write failed: "+status);
        }
        // ATT write completion is not an application ACK. Always read status-v2.
        if(lane>=0&&lane<2)panelStatusExpectedLane=lane;
        panelReadQueued=2;pumpQueue();
    }

    private void panelCancelWritten(BluetoothGatt owner,int status) {
        if(owner!=gatt||!panelCancelWaitingWrite)return;
        panelCancelWaitingWrite=false;panelControlWritePending=false;
        if(panelLastStatus!=null)panelCancelAttemptedSession=panelLastStatus.sessionId;
        panelCancelPacket=null;
        if(status!=BluetoothGatt.GATT_SUCCESS) {
            notifyStatus("CORE Panel preview CANCEL write failed: "+status);
            if(panelLastStatus!=null)panelSnapshotQueuedLane=panelLastStatus.lane;
        } else {
            if(panelLastStatus!=null)panelStatusExpectedLane=panelLastStatus.lane;
            panelReadQueued=2;
        }
        pumpQueue();
    }

    @SuppressWarnings({"MissingPermission","deprecation"})
    private void startPanelSnapshot(int lane) {
        if(!canReadPanelPreview()||gatt==null)return;
        panelSnapshotBusy=true;panelSnapshotLane=lane;panelSnapshotPhase=0;
        panelSnapshotOffset=0;panelSnapshotHeader=null;panelSnapshotAbortForStop=false;
        panelSnapshotFresh[lane]=false;panelStatusFresh[lane]=false;
        panelSnapshotWire=new byte[PanelPreviewProtocol.MAP_BYTES];
        panelSnapshotWrite(new byte[]{3,1,(byte)lane});
    }

    @SuppressWarnings({"MissingPermission","deprecation"})
    private void panelSnapshotWrite(byte[] packet) {
        if(!panelSnapshotBusy||gatt==null||panelConfigCommand==null) {
            panelSnapshotFail("Panel snapshot write lost its GATT session");return;
        }
        panelSnapshotWaitingWrite=true;panelSnapshotWaitingRead=false;
        final BluetoothGatt owner=gatt;final int lease=++panelSnapshotWatchLease;
        boolean started=false;
        try {
            if(Build.VERSION.SDK_INT>=33)
                started=owner.writeCharacteristic(panelConfigCommand,packet,
                    BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT)==BluetoothStatusCodes.SUCCESS;
            else {
                panelConfigCommand.setWriteType(BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT);
                panelConfigCommand.setValue(packet);
                started=owner.writeCharacteristic(panelConfigCommand);
            }
        } catch(SecurityException ex){notifyStatus("Panel snapshot write permission denied");}
        if(!started){panelSnapshotFail("CORE Panel snapshot command could not start");return;}
        ui.postDelayed(()->{
            if(!stopped&&owner==gatt&&panelSnapshotBusy&&panelSnapshotWatchLease==lease)
                recoverStalledGatt(owner,"Panel snapshot transfer timed out; reconnect required");
        },20000L);
    }

    @SuppressWarnings("MissingPermission")
    private void panelSnapshotWritten(BluetoothGatt owner,int status) {
        if(owner!=gatt||!panelSnapshotBusy||!panelSnapshotWaitingWrite)return;
        panelSnapshotWaitingWrite=false;
        if(status!=BluetoothGatt.GATT_SUCCESS) {
            panelSnapshotFail("CORE Panel snapshot write failed: "+status);return;
        }
        if(panelSnapshotAbortForStop) {
            panelSnapshotFail("Panel snapshot aborted to prioritize STOP");return;
        }
        panelSnapshotWaitingRead=true;
        try {
            if(!owner.readCharacteristic(panelConfigReply))
                panelSnapshotFail("CORE Panel snapshot read did not start");
        } catch(SecurityException ex){panelSnapshotFail("Panel snapshot read permission denied");}
    }

    private void panelSnapshotRead(BluetoothGatt owner,byte[] value,int status) {
        if(owner!=gatt||!panelSnapshotBusy||!panelSnapshotWaitingRead)return;
        panelSnapshotWaitingRead=false;
        if(panelSnapshotAbortForStop) {
            panelSnapshotFail("Panel snapshot aborted to prioritize STOP");return;
        }
        if(status!=BluetoothGatt.GATT_SUCCESS||value==null) {
            panelSnapshotFail("CORE Panel snapshot response failed: "+status);return;
        }
        try {
            if(panelSnapshotPhase==0) {
                panelSnapshotHeader=PanelPreviewProtocol.SnapshotHeader.parse(value,panelSnapshotLane);
                panelSnapshotPhase=1;panelSnapshotOffset=0;
                panelSnapshotReadNextChunk();return;
            }
            if(value.length<6||PanelPreviewProtocol.u8(value[0])!=PanelPreviewProtocol.WIRE_VERSION||
               PanelPreviewProtocol.u8(value[1])!=0x82||
               PanelPreviewProtocol.u8(value[2])!=panelSnapshotLane||
               PanelPreviewProtocol.u16(value,3)!=panelSnapshotOffset)
                throw new IllegalArgumentException("Panel snapshot chunk header/offset mismatch");
            int count=value.length-5;
            if(count<1||count>15||panelSnapshotOffset+count>panelSnapshotWire.length)
                throw new IllegalArgumentException("Panel snapshot chunk length out of range");
            System.arraycopy(value,5,panelSnapshotWire,panelSnapshotOffset,count);
            panelSnapshotOffset+=count;
            if(panelSnapshotOffset<panelSnapshotWire.length) {
                panelSnapshotReadNextChunk();return;
            }
            panelSnapshotHeader.verify(panelSnapshotWire,panelSnapshotLane);
            PanelPreviewProtocol.LaneSnapshot snapshot=
                new PanelPreviewProtocol.LaneSnapshot(panelSnapshotHeader,panelSnapshotWire);
            panelConfirmedSnapshots[panelSnapshotLane]=snapshot;
            panelSnapshotFresh[panelSnapshotLane]=true;
            if(panelSaveBusy&&panelSavePhase==4&&panelSaveLane==snapshot.lane) {
                boolean durable=Arrays.equals(panelSaveWire,snapshot.wire)&&
                    snapshot.activeRevision!=panelSaveRevision&&
                    !snapshot.pending&&snapshot.persistent;
                if(durable) {
                    try {
                        org.json.JSONObject result=PanelV4DraftCodec.fromSnapshot(snapshot);
                        panelGeometryManagers[snapshot.lane]=new GeometryTransactionManager(
                            new GeometryTransactionManager.Snapshot(snapshot.lane,
                                snapshot.activeRevision,snapshot.wire));
                        panelSaveFinish(true,"Panel v4 Saved · revision "+
                            snapshot.activeRevision+" confirmed",result);
                    }catch(Exception ex){
                        panelSaveFinish(false,"Panel v4 readback decode failed; outcome unknown",null);
                    }
                }else panelSaveFinish(false,
                    "Panel v4 Save not yet confirmed; reconnect and reconcile before retry",null);
            }
            if(panelGeometryManagers[panelSnapshotLane]==null) {
                panelGeometryManagers[panelSnapshotLane]=new GeometryTransactionManager(
                    new GeometryTransactionManager.Snapshot(snapshot.lane,snapshot.activeRevision,snapshot.wire));
            }
            panelSnapshotBusy=false;panelSnapshotWire=null;panelSnapshotHeader=null;
            if(panelSnapshotListener!=null) {
                final BluetoothGatt current=gatt;
                ui.post(()->{
                    if(!stopped&&current==gatt&&panelSnapshotListener!=null)
                        panelSnapshotListener.accept(snapshot);
                });
            }
            if(panelReconciliationPending) {
                panelReconciliationSnapshot=snapshot;
                panelReconciliationSnapshotVerified=true;
            }
            // Every fresh lane map is paired with a fresh status-v2 read before
            // it can authorize a new preview transaction.
            panelStatusExpectedLane=panelSnapshotLane;
            panelReadQueued=2;
            pumpQueue();
        } catch(IllegalArgumentException ex) {
            panelSnapshotFail("CORE Panel snapshot rejected: "+ex.getMessage());
        }
    }

    private void panelSnapshotReadNextChunk() {
        byte[] packet=new byte[]{3,2,(byte)panelSnapshotLane,
            (byte)panelSnapshotOffset,(byte)(panelSnapshotOffset>>>8)};
        panelSnapshotWrite(packet);
    }

    private void panelSnapshotFail(String message) {
        panelSnapshotBusy=false;panelSnapshotWaitingWrite=false;panelSnapshotWaitingRead=false;
        panelSnapshotAbortForStop=false;
        if(panelSnapshotLane>=0&&panelSnapshotLane<2) {
            panelSnapshotFresh[panelSnapshotLane]=false;
            panelStatusFresh[panelSnapshotLane]=false;
        }
        panelStatusExpectedLane=-1;
        panelSnapshotWire=null;panelSnapshotHeader=null;panelSnapshotWatchLease++;
        notifyStatus(message);
        if(panelReconciliationPending) {
            panelReconciliationPending=false;panelReconciliationSnapshotVerified=false;
        }
        pumpQueue();
    }

    // INFO is optional metadata, not a prerequisite for owner authentication.
    // Both API-level GATT read callbacks must continue the same auth flow,
    // including when a started read returns a failure status.
    private void finishInfoRead(BluetoothGatt connection,byte[] value,int status) {
        infoReadPending=false;
        continuousFx=false;tertiaryFx=false;coreInfoText=null;
        if(connection!=gatt)return;
        if(status==BluetoothGatt.GATT_SUCCESS && value!=null){
            String info=new String(value,StandardCharsets.UTF_8);
            coreInfoText=info;
            continuousFx=ContinuousFxCommand.advertised(info);
            tertiaryFx=TertiaryFxCommand.advertised(info);
            String version=info.startsWith("OpenTail CORE ")
                ? info.substring("OpenTail CORE ".length()).split(" \\|",2)[0].trim() : "";
            final String reported=version.isEmpty()?null:version;
            final Consumer<String> receiver=infoListener;
            if(receiver!=null)ui.post(()->{
                if(!stopped && connection==gatt)receiver.accept(reported);
            });
        }else notifyStatus("CORE information read failed: "+status);
        // INFO is optional. Never gate or revoke owner authorization on it.
        if(challenge!=null && BleOwnerVault.load(activity,activeAddress)!=null &&
           !canControl() && !challengePending && !authPending)
            ui.post(thisAutoAuth(connection));
        pumpQueue();
    }

    private Runnable thisAutoAuth(BluetoothGatt owner) {
        return ()->{
            if (!stopped && owner==gatt && connected && !canControl()) authorizeStored();
        };
    }

    /** Both FX command and output-config responses can revoke CORE ownership. */
    private void revokeOwnerSession(){
        clearQueuedOutputRead();
        authorized=false;
        panelDisconnected();
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
        if(authPending){
            authorized="OK AUTH".equals(response);authPending=false;
            if(authorized)queuePanelCapabilitiesRead();
            if(authorized && coreInfo!=null) {
                final BluetoothGatt owner=gatt;
                // Allow the existing authorized FX catalog read to start
                // before this optional metadata read enters the GATT queue.
                ui.postDelayed(()->{
                    if(!stopped && owner==gatt && connected && authorized){
                        infoReadQueued=true;pumpQueue();
                    }
                },500L);
            }
        }
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
        autoReconnectLease++;infoReadPending=false;
        frameCapture.cancel();clearPreviewCommand();palette.reset();clearQueuedOutputRead();
        connected = false;
        connecting = false;
        authorized = false;cfgBusy=false;fxCaps=null;fxCapsBusy=false;fxNotify("[]");triggerMapping=null;triggerMapBusy=false;triggerMapSavePending=false;triggerMapExpected=null;cfgWatchdog++;
        stopScan();
        if (scanDialog != null) { scanDialog.dismiss(); scanDialog = null; }
        panelDisconnected();
        if (gatt != null) { gatt.disconnect(); gatt.close(); gatt = null; }
    }

    private void notifyStatus(String message) {
        ui.post(() -> {
            if (!stopped) listener.onStatus(message);
        });
    }
}
