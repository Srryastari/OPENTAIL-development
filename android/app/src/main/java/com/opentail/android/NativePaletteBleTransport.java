package com.opentail.android;

import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothStatusCodes;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import java.util.Arrays;

/** One serialized, authenticated BLE v3 palette transaction at a time.
 * Called only by BleDiscovery's existing owner and single-ATT admission gate.
 */
final class NativePaletteBleTransport {
    interface Listener {void onPalette(Result result);}
    static final class Result {
        final boolean ok,capable,present;
        final int index;
        final long revision;
        final byte[] stops;
        final String message;
        Result(boolean ok,boolean capable,int index,long revision,boolean present,byte[] stops,String message){
            this.ok=ok;this.capable=capable;this.index=index;this.revision=revision;
            this.present=present;this.stops=stops==null?null:Arrays.copyOf(stops,stops.length);
            this.message=message;
        }
    }
    private static final int NONE=0,CAPS=1,LOAD=2,UPLOAD=3,DELETE=4;
    private final Handler handler=new Handler(Looper.getMainLooper());
    private final Listener listener;
    private final Runnable idle;
    private BluetoothGatt gatt;
    private BluetoothGattCharacteristic command,reply,caps;
    private boolean busy=false,supported=false,abort=false,operationPending=false;
    private int kind=NONE,op,index,offset,total,crc,generation;
    private long baseRevision,verifiedRevision;
    private byte[] expected,received;
    NativePaletteBleTransport(Listener listener,Runnable idle){this.listener=listener;this.idle=idle;}
    boolean busy(){return busy;}
    boolean supported(){return supported && available();}
    boolean available(){return gatt!=null && command!=null && reply!=null && caps!=null;}
    void configure(BluetoothGatt connection,BluetoothGattCharacteristic cmd,
                   BluetoothGattCharacteristic rsp,BluetoothGattCharacteristic cap) {
        reset();gatt=connection;command=cmd;reply=rsp;caps=cap;
    }
    void reset(){
        busy=false;supported=false;abort=false;operationPending=false;
        generation++;kind=NONE;expected=null;received=null;
        gatt=null;command=null;reply=null;caps=null;
    }
    private void finish(boolean ok,boolean present,byte[] stops,String message){
        int doneIndex=index;long doneRevision=verifiedRevision;
        busy=false;abort=false;operationPending=false;generation++;kind=NONE;
        expected=null;received=null;
        listener.onPalette(new Result(ok,supported(),doneIndex,doneRevision,present,stops,message));
        handler.post(idle);
    }
    private boolean start(int mode,int slot){
        if(busy||!supported()||slot<0||slot>=NativePaletteV3.SLOTS)return false;
        busy=true;abort=false;operationPending=false;kind=mode;
        index=slot;offset=0;total=0;crc=0;verifiedRevision=-1;expected=null;received=null;
        return true;
    }
    boolean negotiate(){
        if(busy||!available())return false;
        busy=true;kind=CAPS;index=-1;supported=false;abort=false;
        operationPending=true;lease();
        try {
            if(gatt.readCharacteristic(caps))return true;
        }catch(SecurityException ignored){}
        finish(false,false,null,"CORE palette v3 capability read could not start");return false;
    }
    boolean load(int slot){
        if(!start(LOAD,slot))return false;
        send(NativePaletteV3.query(slot),5);return true;
    }
    boolean upload(int slot,long revision,byte[] stops){
        NativePaletteV3.begin(slot,revision,stops); // validate before entering ATT
        if(!start(UPLOAD,slot))return false;
        baseRevision=revision;expected=Arrays.copyOf(stops,stops.length);
        total=stops.length;crc=NativePaletteV3.crc(stops);
        send(NativePaletteV3.begin(slot,revision,stops),1);return true;
    }
    boolean delete(int slot,long revision){
        NativePaletteV3.delete(slot,revision);
        if(revision==NativePaletteV3.MAX_REVISION)
            throw new IllegalArgumentException("CORE palette revision exhausted");
        if(!start(DELETE,slot))return false;
        baseRevision=revision;
        send(NativePaletteV3.delete(slot,revision),4);return true;
    }
    void abortForStop(){
        if(!busy)return;
        abort=true;
        if(!operationPending)finish(false,false,null,"Palette transfer canceled for STOP");
    }
    private void lease(){
        int thisGeneration=++generation;
        handler.postDelayed(()->{
            if(busy && generation==thisGeneration)
                finish(false,false,null,"CORE palette BLE operation timed out; reconnect before retry");
        },8000);
    }
    @SuppressWarnings({"MissingPermission","deprecation"})
    private void send(byte[] bytes,int opcode){
        if(!busy||gatt==null||command==null){finish(false,false,null,"Palette transport disconnected");return;}
        op=opcode;operationPending=true;lease();
        try{
            boolean ok;
            if(Build.VERSION.SDK_INT>=33)
                ok=gatt.writeCharacteristic(command,bytes,BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT)
                   ==BluetoothStatusCodes.SUCCESS;
            else{
                command.setWriteType(BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT);
                command.setValue(bytes);ok=gatt.writeCharacteristic(command);
            }
            if(!ok)finish(false,false,null,"Palette GATT write could not start");
        }catch(SecurityException ex){finish(false,false,null,"Palette Bluetooth permission denied");}
    }
    @SuppressWarnings("MissingPermission")
    void written(BluetoothGatt owner,int status){
        if(!busy||kind==CAPS||owner!=gatt)return;
        operationPending=false;
        if(abort){finish(false,false,null,"Palette transfer canceled for STOP");return;}
        if(status!=BluetoothGatt.GATT_SUCCESS){
            finish(false,false,null,"Palette BLE write rejected ("+status+")");return;
        }
        try{
            operationPending=true;lease();
            if(!gatt.readCharacteristic(reply))
                finish(false,false,null,"Palette reply read could not start");
        }catch(SecurityException ex){finish(false,false,null,"Palette reply permission denied");}
    }
    void capability(BluetoothGatt owner,byte[] bytes,int status){
        if(!busy||kind!=CAPS||owner!=gatt)return;
        operationPending=false;
        if(abort){finish(false,false,null,"Palette negotiation canceled for STOP");return;}
        try {
            if(status!=BluetoothGatt.GATT_SUCCESS)throw new IllegalArgumentException("Capability ATT read failed");
            NativePaletteV3.capabilities(bytes);
            supported=true;
            finish(true,false,null,"CORE authenticated BLE v3 palette transport available");
        }catch(IllegalArgumentException ex){
            supported=false;finish(false,false,null,ex.getMessage());
        }
    }
    private void rejected(int status){
        String text=status==2?"CORE busy or palette referenced (release inputs/show and retry)":
                status==3?"Stale palette revision; reload this slot before retrying":
                status==4?"CORE could not persist palette to NVS":"Invalid CORE palette transaction";
        finish(false,false,null,text);
    }
    void read(BluetoothGatt owner,byte[] bytes,int status){
        if(!busy||kind==CAPS||owner!=gatt)return;
        operationPending=false;
        if(abort){finish(false,false,null,"Palette transfer canceled for STOP");return;}
        try{
            if(status!=BluetoothGatt.GATT_SUCCESS)
                throw new IllegalArgumentException("Palette reply ATT read failed");
            NativePaletteV3.Reply r=NativePaletteV3.reply(bytes,op,index);
            if(r.status!=0){rejected(r.status);return;}
            if(op==5) {
                verifiedRevision=r.revision;
                if(kind==DELETE){
                    if(r.present || r.revision!=baseRevision+1)
                        throw new IllegalArgumentException("Palette deletion not verified by CORE");
                    finish(true,false,null,"CORE palette deleted and tombstone verified");return;
                }
                if(kind==UPLOAD && (r.revision!=baseRevision+1 || !r.present))
                    throw new IllegalArgumentException("CORE palette revision/presence mismatch after commit");
                if(!r.present){
                    if(kind==UPLOAD)throw new IllegalArgumentException("CORE palette not persisted");
                    finish(true,false,null,"CORE palette slot is empty (revision "+r.revision+")");return;
                }
                if(r.count<2||r.count>NativePaletteV3.STOPS_MAX)
                    throw new IllegalArgumentException("Invalid CORE palette stop count");
                total=r.count*4;crc=r.crc;offset=0;received=new byte[total];
                if(kind==UPLOAD && (total!=expected.length||crc!=NativePaletteV3.crc(expected)))
                    throw new IllegalArgumentException("CORE saved palette CRC/count mismatch");
                send(NativePaletteV3.read(index,offset),7);return;
            }
            if(op==7){
                if(r.offset!=offset||r.data.length!=Math.min(NativePaletteV3.READ_MAX,total-offset))
                    throw new IllegalArgumentException("Palette readback chunk offset/length mismatch");
                System.arraycopy(r.data,0,received,offset,r.data.length);
                offset+=r.data.length;
                if(offset<total){send(NativePaletteV3.read(index,offset),7);return;}
                NativePaletteV3.validateStops(received);
                if(NativePaletteV3.crc(received)!=crc ||
                   (kind==UPLOAD&&!Arrays.equals(received,expected)))
                    throw new IllegalArgumentException("CORE palette readback CRC/data mismatch");
                finish(true,true,received,kind==UPLOAD?
                       "CORE palette NVS write and exact stop readback verified":
                       "CORE palette downloaded with valid CRC");return;
            }
            if(kind!=UPLOAD)
                throw new IllegalArgumentException("Unexpected palette write acknowledgement");
            if(op==1 || op==2){
                if(offset<expected.length){
                    byte[] chunk=NativePaletteV3.data(offset,expected);
                    offset+=chunk.length-4;
                    send(chunk,2);
                }else send(NativePaletteV3.commit(index),3);
                return;
            }
            if(op==3){send(NativePaletteV3.query(index),5);return;}
            if(kind==DELETE && op==4){send(NativePaletteV3.query(index),5);return;}
            throw new IllegalArgumentException("Unexpected CORE palette response opcode");
        }catch(IllegalArgumentException ex){finish(false,false,null,ex.getMessage());}
    }
}
