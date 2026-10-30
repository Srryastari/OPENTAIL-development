package com.opentail.android;

import org.junit.Test;
import java.util.Arrays;
import static org.junit.Assert.*;

/** Pinned to CORE #38 docs/CORE-PALETTE-BLE-V3.md at f9614060. */
public final class NativePaletteV3Test {
    private static void rejects(Runnable r) {
        try {r.run();fail("Expected an invalid BLE palette packet");}
        catch(IllegalArgumentException expected) {}
    }
    @Test public void capabilityNegotiationFailsClosed(){
        NativePaletteV3.Capabilities c=NativePaletteV3.capabilities(
                new byte[]{3,(byte)128,18,15,(byte)200,73});
        assertEquals(128,c.slots);
        assertEquals(15,c.chunk);
        assertEquals(73,c.lastId);
        assertEquals(200,NativePaletteV3.id(0));
        assertEquals(73,NativePaletteV3.id(127));
        rejects(()->NativePaletteV3.capabilities(new byte[]{2,(byte)128,18,15,(byte)200,73}));
        rejects(()->NativePaletteV3.capabilities(new byte[]{3,(byte)127,18,15,(byte)200,73}));
        rejects(()->NativePaletteV3.capabilities(new byte[]{3,(byte)128,18,16,(byte)200,73}));
        rejects(()->NativePaletteV3.capabilities(new byte[0]));
        rejects(()->NativePaletteV3.id(128));
    }
    @Test public void exactStopsAndCrc(){
        byte[] s=NativePaletteV3.stops("0:#ff0000,128:#00ff00,255:#0000ff");
        assertArrayEquals(new byte[]{0,(byte)255,0,0,(byte)128,0,(byte)255,0,
                (byte)255,0,0,(byte)255},s);
        assertEquals("0:#FF0000,128:#00FF00,255:#0000FF",NativePaletteV3.formatStops(s));
        assertEquals(BleConfigCodec.crc16(s),NativePaletteV3.crc(s));
        rejects(()->NativePaletteV3.stops("1:#FF0000,255:#0000FF"));
        rejects(()->NativePaletteV3.stops("0:#FF0000,254:#0000FF"));
        rejects(()->NativePaletteV3.stops("0:#FF0000,0:#0000FF,255:#FFFFFF"));
        rejects(()->NativePaletteV3.stops("0:#FF0000,128:#0000FF,100:#FFFFFF,255:#00FF00"));
        rejects(()->NativePaletteV3.stops("0:FF0000,255:#0000FF"));
        rejects(()->NativePaletteV3.stops("0:#FF0000,255:#FFFFGG"));
        rejects(()->NativePaletteV3.stops("0:#FF0000"));
        byte[] bad=s.clone();bad[4]=0;
        rejects(()->NativePaletteV3.validateStops(bad));
        byte[] tooMany=new byte[19*4];
        rejects(()->NativePaletteV3.validateStops(tooMany));
    }
    @Test public void uploadPacketsRespectTwentyByteAttBoundary(){
        byte[] s=NativePaletteV3.stops("0:#112233,128:#445566,255:#778899");
        byte[] begin=NativePaletteV3.begin(12,0x12345678L,s);
        assertArrayEquals(new byte[]{3,1,12,3,0x78,0x56,0x34,0x12,
                (byte)NativePaletteV3.crc(s),(byte)(NativePaletteV3.crc(s)>>8)},begin);
        assertArrayEquals(new byte[]{3,5,12},NativePaletteV3.query(12));
        assertArrayEquals(new byte[]{3,7,12,6},NativePaletteV3.read(12,6));
        assertArrayEquals(new byte[]{3,3,12},NativePaletteV3.commit(12));
        assertArrayEquals(new byte[]{3,6},NativePaletteV3.abort());
        assertArrayEquals(new byte[]{3,4,12,0x78,0x56,0x34,0x12},
                NativePaletteV3.delete(12,0x12345678L));
        assertEquals(16,NativePaletteV3.data(0,s).length);
        byte[] full=new byte[18*4];
        for(int i=0;i<18;i++)full[4*i]=(byte)(i==17?255:i*15);
        NativePaletteV3.validateStops(full);
        for(int off=0;off<full.length;off+=15)
            assertTrue(NativePaletteV3.data(off,full).length<=19);
        rejects(()->NativePaletteV3.begin(0,0xffffffffL,s));
        rejects(()->NativePaletteV3.delete(0,-1));
    }
    @Test public void authenticatedRepliesValidateVersionOpIndexAndStatus(){
        byte[] meta={3,(byte)0x85,7,0,5,4,3,2,3,0x34,0x12,1};
        NativePaletteV3.Reply parsed=NativePaletteV3.reply(meta,5,7);
        assertEquals(0x02030405L,parsed.revision);
        assertEquals(3,parsed.count);
        assertEquals(0x1234,parsed.crc);
        assertTrue(parsed.present);
        meta[3]=3;
        assertEquals(3,NativePaletteV3.reply(meta,5,7).status);
        meta[3]=5;
        rejects(()->NativePaletteV3.reply(meta,5,7));
        meta[3]=0;
        rejects(()->NativePaletteV3.reply(meta,5,8));
        rejects(()->NativePaletteV3.reply(Arrays.copyOf(meta,11),5,7));
    }
    @Test public void readChunkRequiresExactOffsetAndBounds(){
        byte[] read={3,(byte)0x87,3,0,6,6,0,1,2,3,4,5};
        NativePaletteV3.Reply reply=NativePaletteV3.reply(read,7,3);
        assertEquals(6,reply.offset);
        assertArrayEquals(new byte[]{0,1,2,3,4,5},reply.data);
        read[5]=7;
        rejects(()->NativePaletteV3.reply(read,7,3));
        read[5]=6;read[4]=69;
        rejects(()->NativePaletteV3.reply(read,7,3));
        byte[] abort={3,(byte)0x86,(byte)255,0,0,0,0,0,0,0,0,0};
        assertEquals(6,NativePaletteV3.reply(abort,6,255).opcode);
    }
}
