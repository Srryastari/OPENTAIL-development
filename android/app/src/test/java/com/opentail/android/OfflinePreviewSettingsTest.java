package com.opentail.android;
import java.util.*;
import org.junit.Test;
import static org.junit.Assert.*;
public class OfflinePreviewSettingsTest {
    @Test public void anotherEffectCannotInheritActiveColorsOrParameters(){
        Map<String,Integer> values=new HashMap<>();
        values.put("effectId",9);
        values.put("background",0xffff0000);
        values.put("foreground",0xff00ff00);
        values.put("speed",255);
        values.put("paletteId",13);
        OfflinePreviewSettings.Reader read=(key,fallback)->values.getOrDefault(key,fallback);
        assertArrayEquals(new int[]{0xffffffff,0xff000000,128,128,160,0},
            OfflinePreviewSettings.read(1,read));
        values.put("wled.1.color1",0xff123456);
        assertEquals(0xff123456,OfflinePreviewSettings.read(1,read)[1]);
        assertEquals(0xffff0000,OfflinePreviewSettings.read(9,read)[1]);
    }
    @Test public void editsReachPreviewImmediatelyWithoutChangingEffect(){
        Map<String,Integer> values=new HashMap<>();
        OfflinePreviewSettings.Reader read=(key,fallback)->values.getOrDefault(key,fallback);
        values.put("speed",5);values.put("wled.9.speed",20);
        assertEquals(20,OfflinePreviewSettings.read(9,read)[2]);
        values.put("wled.9.speed",240);
        assertEquals(240,OfflinePreviewSettings.read(9,read)[2]);
        assertEquals(5,OfflinePreviewSettings.read(1,read)[2]);
        values.put("wled.9.intensity",255);values.put("wled.9.brightness",24);
        values.put("wled.9.palette",13);values.put("wled.9.color0",0xff123456);
        values.put("wled.9.color1",0xffabcdef);
        assertArrayEquals(new int[]{0xff123456,0xffabcdef,240,255,24,13},OfflinePreviewSettings.read(9,read));
    }
}
