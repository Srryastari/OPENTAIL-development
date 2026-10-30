package com.opentail.android;

import java.util.Arrays;

/** Exact wire contract from CORE #38 docs/CORE-PALETTE-BLE-V3.md.
 * Pure codec: no Bluetooth, renderer or persistence side effects.
 */
final class NativePaletteV3 {
    static final int VERSION=3, SLOTS=128, STOPS_MAX=18, CHUNK_MAX=15, READ_MAX=6;
    static final int FIRST_ID=200, LAST_ID=73;
    static final long MAX_REVISION=0xffffffffL;

    static final class Capabilities {
        final int slots,stops,chunk,firstId,lastId;
        Capabilities(int slots,int stops,int chunk,int first,int last) {
            this.slots=slots;this.stops=stops;this.chunk=chunk;this.firstId=first;this.lastId=last;
        }
    }
    static final class Reply {
        final int opcode,index,status,count,crc,offset;
        final long revision;
        final boolean present;
        final byte[] data;
        Reply(int opcode,int index,int status,long revision,int count,int crc,
              boolean present,int offset,byte[] data) {
            this.opcode=opcode;this.index=index;this.status=status;this.revision=revision;
            this.count=count;this.crc=crc;this.present=present;this.offset=offset;this.data=data;
        }
    }
    private static int u8(byte b){return b&255;}
    static long u32(byte[] p,int offset){
        return (p[offset]&255L)|((p[offset+1]&255L)<<8)|
                ((p[offset+2]&255L)<<16)|((p[offset+3]&255L)<<24);
    }
    private static byte[] withRev(int op,int index,long revision){
        slot(index);revision(revision);
        return new byte[]{VERSION,(byte)op,(byte)index,(byte)revision,
                (byte)(revision>>8),(byte)(revision>>16),(byte)(revision>>24)};
    }
    static void slot(int index){
        if(index<0||index>=SLOTS)throw new IllegalArgumentException("Palette slot must be 0–127");
    }
    static void revision(long value){
        if(value<0||value>MAX_REVISION)throw new IllegalArgumentException("Invalid CORE palette revision");
    }
    static int id(int slot){slot(slot);return FIRST_ID-slot;}
    static Capabilities capabilities(byte[] packet){
        if(packet==null||packet.length!=6||u8(packet[0])!=VERSION||
           u8(packet[1])!=SLOTS||u8(packet[2])!=STOPS_MAX||
           u8(packet[3])!=CHUNK_MAX||u8(packet[4])!=FIRST_ID||
           u8(packet[5])!=LAST_ID)
            throw new IllegalArgumentException("Unsupported CORE custom-palette capabilities");
        return new Capabilities(SLOTS,STOPS_MAX,CHUNK_MAX,FIRST_ID,LAST_ID);
    }
    static byte[] stops(String text){
        if(text==null)throw new IllegalArgumentException("Missing gradient stops");
        String[] entries=text.trim().split(",",-1);
        if(entries.length<2||entries.length>STOPS_MAX)
            throw new IllegalArgumentException("Use 2–18 position:#RRGGBB stops");
        byte[] out=new byte[entries.length*4];
        int previous=-1;
        for(int i=0;i<entries.length;i++){
            String[] parts=entries[i].trim().split(":",-1);
            if(parts.length!=2||!parts[0].trim().matches("[0-9]{1,3}")||
               !parts[1].trim().matches("#[A-Fa-f0-9]{6}"))
                throw new IllegalArgumentException("Use position:#RRGGBB for every stop");
            int position=Integer.parseInt(parts[0].trim());
            if(position>255||position<=previous)
                throw new IllegalArgumentException("Positions must strictly increase from 0 to 255");
            int rgb=Integer.parseInt(parts[1].trim().substring(1),16);
            out[4*i]=(byte)position;out[4*i+1]=(byte)(rgb>>16);
            out[4*i+2]=(byte)(rgb>>8);out[4*i+3]=(byte)rgb;
            previous=position;
        }
        validateStops(out);
        return out;
    }
    static void validateStops(byte[] data){
        if(data==null||data.length<8||data.length>72||data.length%4!=0)
            throw new IllegalArgumentException("CORE palette requires 2–18 RGBA-position stop bytes");
        int prev=-1;
        for(int i=0;i<data.length;i+=4){
            int pos=u8(data[i]);
            if(pos<=prev)throw new IllegalArgumentException("Unordered or duplicate stop positions");
            prev=pos;
        }
        if(u8(data[0])!=0||prev!=255)
            throw new IllegalArgumentException("Gradient endpoints must be 0 and 255");
    }
    static String formatStops(byte[] data){
        validateStops(data);
        StringBuilder sb=new StringBuilder();
        for(int i=0;i<data.length;i+=4){
            if(i!=0)sb.append(',');
            sb.append(u8(data[i])).append(":#");
            for(int j=1;j<=3;j++){
                int n=u8(data[i+j]);if(n<16)sb.append('0');
                sb.append(Integer.toHexString(n).toUpperCase(java.util.Locale.ROOT));
            }
        }
        return sb.toString();
    }
    static int crc(byte[] stops){validateStops(stops);return BleConfigCodec.crc16(stops);}
    static byte[] query(int index){slot(index);return new byte[]{VERSION,5,(byte)index};}
    static byte[] read(int index,int offset){
        slot(index);
        if(offset<0||offset>=72)throw new IllegalArgumentException("Invalid stop byte offset");
        return new byte[]{VERSION,7,(byte)index,(byte)offset};
    }
    static byte[] begin(int index,long base,byte[] stops){
        slot(index);revision(base);validateStops(stops);
        if(base==MAX_REVISION)throw new IllegalArgumentException("CORE palette revision exhausted");
        int crc=crc(stops);
        return new byte[]{VERSION,1,(byte)index,(byte)(stops.length/4),
                (byte)base,(byte)(base>>8),(byte)(base>>16),(byte)(base>>24),
                (byte)crc,(byte)(crc>>8)};
    }
    static byte[] data(int offset,byte[] stops){
        validateStops(stops);
        if(offset<0||offset>=stops.length)throw new IllegalArgumentException("Invalid stop transfer offset");
        int n=Math.min(CHUNK_MAX,stops.length-offset);
        byte[] b=new byte[4+n];b[0]=VERSION;b[1]=2;b[2]=(byte)offset;b[3]=(byte)(offset>>8);
        System.arraycopy(stops,offset,b,4,n);return b;
    }
    static byte[] commit(int index){slot(index);return new byte[]{VERSION,3,(byte)index};}
    static byte[] delete(int index,long base){return withRev(4,index,base);}
    static byte[] abort(){return new byte[]{VERSION,6};}
    static Reply reply(byte[] wire,int expectedOp,int expectedSlot){
        if(wire==null||wire.length!=12||u8(wire[0])!=VERSION||
           u8(wire[1])!=(expectedOp|0x80)||
           (expectedOp!=2&&expectedOp!=6&&u8(wire[2])!=expectedSlot))
            throw new IllegalArgumentException("Mismatched CORE palette response");
        int status=u8(wire[3]);
        if(status>4)throw new IllegalArgumentException("Unknown CORE palette status");
        int index=u8(wire[2]);
        if(expectedOp==2 && index!=expectedSlot)
            throw new IllegalArgumentException("CORE palette transfer slot mismatch");
        if(expectedOp==6 && index!=255)
            throw new IllegalArgumentException("CORE palette abort mismatch");
        if(expectedOp==7 && status==0){
            int offset=u8(wire[4]),n=u8(wire[5]);
            if(n<1||n>READ_MAX||offset+n>72)
                throw new IllegalArgumentException("Malformed CORE palette stop chunk");
            return new Reply(expectedOp,index,status,0,0,0,true,offset,Arrays.copyOfRange(wire,6,6+n));
        }
        return new Reply(expectedOp,index,status,u32(wire,4),u8(wire[8]),
                u8(wire[9])|(u8(wire[10])<<8),u8(wire[11])==1,0,null);
    }
    private NativePaletteV3(){}
}
