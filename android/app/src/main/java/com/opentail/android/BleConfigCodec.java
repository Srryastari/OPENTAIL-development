package com.opentail.android;

import java.nio.charset.StandardCharsets;
import java.util.Arrays;
import org.json.JSONObject;
import org.json.JSONArray;

/** Cross-platform fixed-size, independently validated BLE output configuration encoding. */
final class BleConfigCodec {
    static final int LENGTH = 261;
    static final String[] ORDERS = {"RGB", "RBG", "GRB", "GBR", "BRG", "BGR"};
    static final String[] ROLES = {"off", "park_brake", "parking", "brake", "left_turn", "right_turn", "reverse", "aux1", "aux2", "aux3", "decorative"};
    private static void name(byte[] buffer, int at, String s) throws Exception {
        if (s == null || s.isEmpty() || s.length() > 24 || !s.matches("[A-Za-z0-9 _./()\\-]+"))
            throw new IllegalArgumentException("Names must use 1–24 allowed ASCII characters");
        byte[] encoded = s.getBytes(StandardCharsets.US_ASCII);
        System.arraycopy(encoded, 0, buffer, at, encoded.length);
    }
    private static String readName(byte[] buffer, int at) throws Exception {
        int n=0;
        while(n<25 && buffer[at+n]!=0)n++;
        if(n==0||n==25)throw new Exception("Malformed BLE zone name");
        String result=new String(buffer,at,n,StandardCharsets.US_ASCII);
        if(!result.matches("[A-Za-z0-9 _./()\\-]+"))throw new Exception("Malformed BLE zone name");
        return result;
    }
    private static int index(String[] choices, String value) {
        for(int i=0;i<choices.length;i++)if(choices[i].equals(value))return i;
        return -1;
    }
    static int crc16(byte[] b) {
        int crc=0xffff;
        for(byte x:b){crc^=(x&255)<<8;for(int i=0;i<8;i++)crc=((crc&0x8000)!=0)?((crc<<1)^0x1021)&0xffff:(crc<<1)&0xffff;}
        return crc;
    }
    static byte[] encode(JSONObject json) throws Exception {
        byte[] bytes=new byte[LENGTH];
        int pixelCount=json.getInt("pixels");
        int order=index(ORDERS,json.getString("color_order"));
        JSONArray zones=json.getJSONArray("zones");
        if(pixelCount<1||pixelCount>522||order<0||zones.length()<1||zones.length()>8)
            throw new IllegalArgumentException("Invalid output pixel count / color order / zone count");
        bytes[0]=1;bytes[1]=(byte)pixelCount;bytes[2]=(byte)(pixelCount>>8);
        bytes[3]=(byte)order;bytes[4]=(byte)zones.length();
        name(bytes,5,json.getString("name"));
        JSONObject cal=json.optJSONObject("calibration");
        for(int a=0;a<2;a++){
            JSONArray gain=cal==null?null:cal.optJSONArray(a==0?"rgb_gain":"white_gain");
            for(int i=0;i<3;i++){
                int g=gain==null?255:gain.getInt(i);
                if(g<0||g>255)throw new IllegalArgumentException("Calibration must be 0–255");
                bytes[30+a*3+i]=(byte)g;
            }
        }
        bytes[36]=(byte)(cal!=null&&cal.optBoolean("gamma_enabled",false)?1:0);
        int sum=0;
        for(int z=0;z<zones.length();z++){
            JSONObject zone=zones.getJSONObject(z);int length=zone.getInt("pixels");
            int role=index(ROLES,zone.getString("role"));
            if(length<0||length>522||role<0)throw new IllegalArgumentException("Invalid zone length or role");
            int start=37+z*28;
            name(bytes,start,zone.getString("name"));
            bytes[start+25]=(byte)length;bytes[start+26]=(byte)(length>>8);
            bytes[start+27]=(byte)role;sum+=length;
        }
        if(sum!=pixelCount)throw new IllegalArgumentException("Zone pixel counts must equal active output pixels");
        return bytes;
    }
    static JSONObject decode(byte[] bytes,int index) throws Exception {
        if(bytes.length!=LENGTH||bytes[0]!=1)throw new Exception("Unsupported BLE output format");
        int pixels=(bytes[1]&255)|((bytes[2]&255)<<8),order=bytes[3]&255,count=bytes[4]&255;
        if(pixels<1||pixels>522||order>=ORDERS.length||count<1||count>8)throw new Exception("Invalid BLE output configuration");
        JSONObject result=new JSONObject();
        result.put("index",index);result.put("pin",index==0?16:17);
        result.put("pixels",pixels);result.put("name",readName(bytes,5));
        result.put("color_order",ORDERS[order]);result.put("zone_count",count);
        JSONObject cal=new JSONObject();JSONArray rgb=new JSONArray(),white=new JSONArray();
        for(int i=0;i<3;i++){rgb.put(bytes[30+i]&255);white.put(bytes[33+i]&255);}
        if(bytes[36]!=0&&bytes[36]!=1)throw new Exception("Invalid gamma option");
        cal.put("rgb_gain",rgb);cal.put("white_gain",white);
        cal.put("gamma_enabled",bytes[36]==1);result.put("calibration",cal);
        JSONArray zones=new JSONArray();int sum=0;
        for(int z=0;z<count;z++){
            int at=37+z*28, size=(bytes[at+25]&255)|((bytes[at+26]&255)<<8),role=bytes[at+27]&255;
            if(role>=ROLES.length||size>522)throw new Exception("Invalid zone data");
            JSONObject zone=new JSONObject();zone.put("name",readName(bytes,at));
            zone.put("pixels",size);zone.put("role",ROLES[role]);zones.put(zone);sum+=size;
        }
        if(sum!=pixels)throw new Exception("BLE zone total mismatch");
        result.put("zones",zones);return result;
    }
}
