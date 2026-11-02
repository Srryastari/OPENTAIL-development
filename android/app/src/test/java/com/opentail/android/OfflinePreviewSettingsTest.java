package com.opentail.android;
import java.util.*;
import org.junit.Test;
import static org.junit.Assert.*;
public class OfflinePreviewSettingsTest {
    @Test public void legacySettingsSurviveSwitchAndDeselection(){
        Map<String,Integer> values=new HashMap<>();
        values.put("effectId",9);values.put("background",0xffff0000);
        values.put("accent",0xff123456);
        OfflinePreviewSettings.Reader read=(key,fallback)->values.getOrDefault(key,fallback);
        OfflinePreviewSettings.snapshot(9,read,values::put);
        values.put("effectId",-1);values.put("background",0xff000000);
        values.put("accent",0xffffffff);
        assertEquals(0xffff0000,OfflinePreviewSettings.read(9,read)[1]);
        assertEquals(Integer.valueOf(0xff123456),values.get("wled.9.accent"));
        assertEquals(0xff000000,OfflinePreviewSettings.read(1,read)[1]);
    }
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
        assertEquals(128,OfflinePreviewSettings.read(1,read)[2]);
        values.put("wled.9.intensity",255);values.put("wled.9.brightness",24);
        values.put("wled.9.palette",13);values.put("wled.9.color0",0xff123456);
        values.put("wled.9.color1",0xffabcdef);
        assertArrayEquals(new int[]{0xff123456,0xffabcdef,240,255,24,13},OfflinePreviewSettings.read(9,read));
    }

    @Test public void eachEffectScopeIgnoresOtherCurrentGlobalSettings(){
        Map<String,Integer> values=new HashMap<>();
        values.put("effectId",54);
        values.put("foreground",0xffaa5500);
        values.put("background",0xff0099aa);
        values.put("speed",255);
        values.put("intensity",0);
        values.put("paletteId",128);
        OfflinePreviewSettings.Reader read=(key,fb)->values.getOrDefault(key,fb);
        for(int other:new int[]{0,1,9,38,55,56,78,84,103,32768,32787}){
            assertArrayEquals("effect "+other,
                new int[]{0xffffffff,0xff000000,128,128,160,0},
                OfflinePreviewSettings.read(other,read));
        }
        values.put("wled.78.color0",0xffa1b2c3);
        values.put("wled.78.color1",0xffd4e5f6);
        values.put("wled.78.speed",0);
        assertEquals(0xffa1b2c3,OfflinePreviewSettings.read(78,read)[0]);
        assertEquals(0xffd4e5f6,OfflinePreviewSettings.read(78,read)[1]);
        assertEquals(0,OfflinePreviewSettings.read(78,read)[2]);
        // 0x000000 stays truly black; no nonzero minimum or brightness floor.
        values.put("wled.103.color2",0xff000000);
        OfflinePreviewSettings.snapshot(103,read,values::put);
        assertEquals(Integer.valueOf(0xff000000),values.get("wled.103.accent"));
        // Current effect's original legacy globals remain available.
        assertEquals(255,OfflinePreviewSettings.read(54,read)[2]);
    }
}
