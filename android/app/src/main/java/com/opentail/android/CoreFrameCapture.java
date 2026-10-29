package com.opentail.android;

import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothStatusCodes;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import java.util.Arrays;

/** Optional CORE snapshot v1 client; never guesses animation pixels or writes NVS.
 * Operates only when already authorized and when both v1 GATT chars are present.
 * GATT operations are serialized by owning BleDiscovery while busy().
 */
final class CoreFrameCapture {
    interface Listener { void done(int output, long sequence, byte[] rgb, String error); }
    private static final int CHUNK_PIXELS=56;
    private final Handler handler=new Handler(Looper.getMainLooper());
    private final Listener listener;
    private BluetoothGatt gatt;
    private BluetoothGattCharacteristic command,reply;
    private boolean busy=false,operationPending=false,abortRequested=false;
    private int output=0,count=0,offset=0,crc=0,polls=0,generation=0;
    private long sequence=0;
    private byte[] pixels;
    CoreFrameCapture(Listener listener) {this.listener=listener;}
    void configure(BluetoothGatt connection,BluetoothGattCharacteristic cmd,
                   BluetoothGattCharacteristic rsp) {
        cancel();gatt=connection;command=cmd;reply=rsp;
    }
    boolean supported() {return gatt!=null && command!=null && reply!=null;}
    boolean busy() {return busy;}
    void cancel() {
        busy=false;operationPending=false;abortRequested=false;generation++;pixels=null;
        gatt=null;command=null;reply=null;
    }
    /** STOP must not wait behind a long diagnostic transfer. Finish the
     * current GATT operation, then release the queue without requesting more.
     */
    void abortForStop() {
        if(!busy)return;
        abortRequested=true;
        if(!operationPending)fail("CORE snapshot canceled for STOP");
    }
    void request(int out) {
        if(busy || !supported() || out<0 || out>255) {
            listener.done(out,0,null,"CORE snapshot busy or unsupported");return;
        }
        output=out;count=0;offset=0;crc=0;sequence=0;polls=0;pixels=null;busy=true;
        abortRequested=false;operationPending=false;
        send(new byte[]{1,1,(byte)out});
    }
    private void fail(String message) {
        if(!busy)return;
        busy=false;operationPending=false;abortRequested=false;generation++;pixels=null;
        listener.done(output,sequence,null,message);
    }
    @SuppressWarnings({"MissingPermission","deprecation"})
    private void send(byte[] data) {
        if(!busy || !supported()){fail("CORE snapshot disconnected");return;}
        operationPending=true;
        int lease=++generation;
        handler.postDelayed(()->{if(busy && generation==lease)
            fail("CORE snapshot BLE operation timed out");},8000);
        try {
            boolean ok;
            if(Build.VERSION.SDK_INT>=33)
                ok=gatt.writeCharacteristic(command,data,BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT)
                    ==BluetoothStatusCodes.SUCCESS;
            else {
                command.setWriteType(BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT);
                command.setValue(data);
                ok=gatt.writeCharacteristic(command);
            }
            if(!ok)fail("CORE frame request could not start");
        }catch(SecurityException e){fail("Bluetooth permission missing");}
    }
    @SuppressWarnings("MissingPermission")
    void written(BluetoothGatt connection,int status) {
        if(!busy || connection!=gatt)return;
        operationPending=false;
        if(abortRequested){fail("CORE snapshot canceled for STOP");return;}
        if(status!=BluetoothGatt.GATT_SUCCESS){fail("CORE frame command rejected: "+status);return;}
        try{operationPending=true;if(!gatt.readCharacteristic(reply))fail("CORE frame response could not start");}
        catch(SecurityException e){fail("Bluetooth permission missing");}
    }
    private static int u16(byte[] a,int p) {return (a[p]&255)|((a[p+1]&255)<<8);}
    private static long u32(byte[] a,int p) {
        return (a[p]&255L)|((a[p+1]&255L)<<8)|((a[p+2]&255L)<<16)|((a[p+3]&255L)<<24);
    }
    @SuppressWarnings("MissingPermission")
    void read(BluetoothGatt connection,byte[] a,int status) {
        if(!busy || connection!=gatt)return;
        operationPending=false;
        if(abortRequested){fail("CORE snapshot canceled for STOP");return;}
        if(status!=BluetoothGatt.GATT_SUCCESS || a==null || a.length<11 || a[0]!=1) {
            fail("CORE returned an incomplete snapshot response");return;
        }
        if(a[1]==1 && a.length==12) {
            if((a[3]&255)!=output){fail("CORE snapshot channel mismatch");return;}
            if(a[2]==0 && polls++<30){
                final int lease=generation;
                handler.postDelayed(()->{
                    if(!busy || generation!=lease || connection!=gatt)return;
                    try {operationPending=true;if(!gatt.readCharacteristic(reply))fail("Snapshot polling failed");}
                    catch(SecurityException ex){fail("Bluetooth permission missing");}
                },50);
                return;
            }
            if(a[2]!=1){fail("CORE did not complete the requested frame");return;}
            count=u16(a,4);sequence=u32(a,6);crc=u16(a,10);
            if(count<1 || count>ExactLedFrame.MAX_PIXELS){
                fail("CORE returned an invalid pixel count");return;
            }
            pixels=new byte[count*3];offset=0;next();return;
        }
        if(a[1]!=2 || a[2]!=1 || (a[3]&255)!=output ||
            a.length<11 || u32(a,4)!=sequence || u16(a,8)!=offset){
            fail("CORE frame sequence or offset mismatch");return;
        }
        int n=a[10]&255;
        if(n==0 || n>CHUNK_PIXELS || n>count-offset || a.length!=11+3*n){
            fail("CORE RGB chunk missing or truncated");return;
        }
        System.arraycopy(a,11,pixels,offset*3,n*3);offset+=n;
        if(offset<count){next();return;}
        if(BleConfigCodec.crc16(pixels)!=crc){fail("CORE RGB888 checksum mismatch");return;}
        byte[] complete=Arrays.copyOf(pixels,pixels.length);
        busy=false;operationPending=false;abortRequested=false;pixels=null;generation++;
        listener.done(output,sequence,complete,null);
    }
    private void next(){
        final int n=Math.min(CHUNK_PIXELS,count-offset);
        send(new byte[]{1,2,(byte)output,(byte)sequence,(byte)(sequence>>8),
            (byte)(sequence>>16),(byte)(sequence>>24),(byte)offset,(byte)(offset>>8),(byte)n});
    }
}
