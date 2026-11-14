package com.opentail.android;
import java.nio.file.*;
import java.util.Arrays;
/** Real CORE encoded bytes, consumed by the exact Android source codec. */
public class WireInterop {
    static void check(boolean ok) { if(!ok) throw new AssertionError(); }
    static void put16(byte[] b,int at,int v) { b[at]=(byte)v;b[at+1]=(byte)(v>>>8); }
    public static void main(String[] args) throws Exception {
        for(int lane=0;lane<2;lane++) {
            byte[] wire=Files.readAllBytes(Path.of(args[0],"lane"+lane+".bin"));
            byte[] h=new byte[14];h[0]=3;h[1]=(byte)0x81;h[2]=(byte)lane;
            put16(h,4,667);put16(h,6,PanelPreviewProtocol.crc16(wire));put16(h,8,2);h[13]=1;
            PanelPreviewProtocol.SnapshotHeader header=PanelPreviewProtocol.SnapshotHeader.parse(h,lane);
            header.verify(wire,lane);
            PanelPreviewProtocol.MapIdentity map=PanelPreviewProtocol.MapIdentity.parse(new PanelPreviewProtocol.LaneSnapshot(header,wire));
            check(map.activePixels==(lane==0?37:144));check(map.zones.size()==1);
            check(map.zones.get(0).ranges.get(0).count==map.activePixels);
            if(lane==0) {check(map.zones.get(0).name.equals("Renamed accent"));check((wire[30]&255)==123);check((wire[35]&255)==201);check(wire[36]==1);}
            byte[] rebuilt=new byte[667];
            PanelV4LayoutProtocol.beginSave(lane,2,wire);
            for(int offset=0;offset<667;) {
                byte[] chunk=PanelV4LayoutProtocol.saveChunk(lane,wire,offset);
                System.arraycopy(chunk,5,rebuilt,offset,chunk.length-5);offset+=chunk.length-5;
            }
            check(Arrays.equals(wire,rebuilt));
            for(int i=0;i<wire.length;i++) {
                byte[] bad=wire.clone();bad[i]^=1;
                try {header.verify(bad,lane);throw new AssertionError("CRC accepted corruption at "+i);}
                catch(IllegalArgumentException expected) {}
            }
            try {header.verify(wire,1-lane);throw new AssertionError("cross lane");}
            catch(IllegalArgumentException expected) {}
            try {header.verify(Arrays.copyOf(wire,666),lane);throw new AssertionError("truncated");}
            catch(IllegalArgumentException expected) {}
        }
        System.out.println("PASS CORE -> Android Panel wire metadata, 37/144, upload chunks, 1334 corruptions, wrong lane and truncation");
    }
}
