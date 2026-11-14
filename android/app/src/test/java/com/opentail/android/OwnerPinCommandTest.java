package com.opentail.android;
import org.junit.Test;
import static org.junit.Assert.*;
public class OwnerPinCommandTest {
    @Test public void skipAndInvalidInputNeverProduceDisableOrWrite() {
        for(String pin:new String[]{null,"","12345","1234567","-12345","abcdef","１２３４５６"})
            assertNull(OwnerPinCommand.encode(pin));
    }
    @Test public void sixDigitsPreserveLeadingZerosAndWireEndian() {
        assertArrayEquals(new byte[]{3,1,0,0,0,0},OwnerPinCommand.encode("000000"));
        assertArrayEquals(new byte[]{3,1,0x40,(byte)0xe2,1,0},OwnerPinCommand.encode("123456"));
        assertArrayEquals(new byte[]{3,1,0x3f,0x42,0x0f,0},OwnerPinCommand.encode("999999"));
    }
    @Test public void laterPinChangeUsesIndependentPacket() {
        byte[] first=OwnerPinCommand.encode("123456");
        byte[] second=OwnerPinCommand.encode("000001");
        assertNotSame(first,second);
        assertEquals(1,second[2]);assertEquals(0x40,first[2]);
    }
}
