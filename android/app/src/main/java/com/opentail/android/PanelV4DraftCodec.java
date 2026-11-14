package com.opentail.android;

import java.nio.charset.StandardCharsets;
import java.util.Arrays;
import org.json.JSONArray;
import org.json.JSONObject;

/**
 * Strict Panel-v4 editor bridge. Never loads Output-v1 records, never invents
 * a one-pixel Off zone, and refuses to flatten multi-panel/range layouts.
 *
 * Metadata IDs in drafts are full 16-bit controller IDs. They are not the
 * truncated low-byte aliases used by the volatile preview protocol.
 */
final class PanelV4DraftCodec {
    static final int MAX_LEDS = 2000;
    static final String[] ORDERS = {"RGB","RBG","GRB","GBR","BRG","BGR"};
    static final String[] ROLES = {"off","park_brake","parking","brake","left_turn",
        "right_turn","reverse","aux1","aux2","aux3","decorative"};

    private PanelV4DraftCodec() {}
    private static int u8(byte b){ return b&255; }
    private static int u16(byte[] b,int offset){ return u8(b[offset])|(u8(b[offset+1])<<8); }
    private static void put16(byte[] b,int offset,int number) {
        b[offset]=(byte)number;b[offset+1]=(byte)(number>>>8);
    }
    private static String readName(byte[] b,int offset) {
        int end=offset;
        while(end<offset+25&&b[end]!=0)end++;
        if(end==offset||end==offset+25)
            throw new IllegalArgumentException("Invalid Panel v4 name");
        for(int i=end+1;i<offset+25;i++)
            if(b[i]!=0)throw new IllegalArgumentException("Invalid Panel name padding");
        return new String(b,offset,end-offset,StandardCharsets.US_ASCII);
    }
    private static void putName(byte[] b,int offset,String name) {
        if(name==null||name.length()<1||name.length()>24||
           !name.matches("[A-Za-z0-9 _./()\\-]+"))
            throw new IllegalArgumentException("Names must contain 1-24 supported ASCII characters");
        byte[] ascii=name.getBytes(StandardCharsets.US_ASCII);
        System.arraycopy(ascii,0,b,offset,ascii.length);
    }
    private static int index(String[] options,String value) {
        for(int i=0;i<options.length;i++)if(options[i].equals(value))return i;
        return -1;
    }
    private static int exactCount(JSONObject json,String key) throws Exception {
        Object value=json.get(key);
        if(!(value instanceof Number))
            throw new IllegalArgumentException("LED counts must be integers");
        try {
            return new java.math.BigDecimal(value.toString()).intValueExact();
        }catch(NumberFormatException|ArithmeticException ex){
            throw new IllegalArgumentException("LED counts must be integers",ex);
        }
    }
    static JSONObject fromSnapshot(PanelPreviewProtocol.LaneSnapshot snapshot) throws Exception {
        PanelPreviewProtocol.MapIdentity identity=PanelPreviewProtocol.MapIdentity.parse(snapshot);
        byte[] b=snapshot.wire;
        int lane=identity.lane,pixels=identity.activePixels;
        if(pixels>MAX_LEDS)
            throw new IllegalArgumentException("Configured controller output exceeds Android's 2000-LED editor limit");
        JSONObject draft=new JSONObject();
        draft.put("name",readName(b,5)).put("pixels",pixels)
            .put("color_order",ORDERS[u8(b[4])]);
        JSONObject calibration=new JSONObject();
        JSONArray rgb=new JSONArray(),white=new JSONArray();
        for(int i=0;i<3;i++){rgb.put(u8(b[30+i]));white.put(u8(b[33+i]));}
        calibration.put("rgb_gain",rgb).put("white_gain",white)
            .put("gamma_enabled",u8(b[36])!=0);
        draft.put("calibration",calibration);
        JSONArray zones=new JSONArray();
        // A complex existing controller layout is read-only in the simple editor.
        // The bridge must not destructively flatten it into one panel.
        boolean simple=identity.panels.size()<=1;
        int nextStart=0;
        for(PanelPreviewProtocol.MapZone z:identity.zones) {
            if(z.ranges.size()!=1||z.ranges.get(0).start!=nextStart||
               identity.panels.isEmpty()||z.panelId!=identity.panels.get(0).id)
                simple=false;
            if(!z.ranges.isEmpty())nextStart=z.ranges.get(0).start+z.ranges.get(0).count;
            JSONObject zone=new JSONObject().put("name",z.name)
                .put("id",z.id).put("panel_id",z.panelId)
                .put("role",z.role==255?"unassigned":ROLES[z.role])
                .put("pixels",z.ranges.size()==1?z.ranges.get(0).count:0);
            zones.put(zone);
        }
        draft.put("zones",zones).put("v4_editable",simple);
        if(identity.panels.size()==1)
            draft.put("panel_id",identity.panels.get(0).id)
                .put("panel_name",identity.panels.get(0).name);
        return draft;
    }

    /**
     * Return an immutable full v4 candidate. Current simple editor supports a
     * single panel and contiguous sections only. All inactive slots are zero.
     * A complex original is rejected instead of losing its ranges on Save.
     */
    static byte[] encode(int lane,JSONObject draft,PanelPreviewProtocol.LaneSnapshot confirmed)
            throws Exception {
        if(lane<0||lane>1||draft==null||confirmed==null||confirmed.lane!=lane)
            throw new IllegalArgumentException("Fresh matching Panel v4 readback required");
        JSONObject baseline=fromSnapshot(confirmed);
        if(!baseline.getBoolean("v4_editable"))
            throw new IllegalArgumentException("Multi-panel or multi-range layout cannot be overwritten by simple editor");
        int pixels=exactCount(draft,"pixels");
        if(pixels<0||pixels>MAX_LEDS)throw new IllegalArgumentException("Output exceeds 2000 LEDs");
        JSONArray zones=draft.getJSONArray("zones");
        if(zones.length()>8||(pixels==0&&zones.length()!=0)||
           (pixels>0&&zones.length()==0))
            throw new IllegalArgumentException("Invalid Panel v4 zone count for configured LED length");
        byte[] result=new byte[PanelV4LayoutProtocol.MAP_BYTES];
        result[0]=3;result[1]=(byte)lane;put16(result,2,pixels);
        int order=index(ORDERS,draft.getString("color_order"));
        if(order<0)throw new IllegalArgumentException("Unsupported output color order");
        result[4]=(byte)order;
        putName(result,5,draft.getString("name"));
        JSONObject cal=draft.optJSONObject("calibration");
        if(cal==null)cal=baseline.getJSONObject("calibration");
        JSONArray rgb=cal.getJSONArray("rgb_gain"),white=cal.getJSONArray("white_gain");
        if(rgb.length()!=3||white.length()!=3)throw new IllegalArgumentException("Invalid calibration");
        for(int i=0;i<3;i++){
            int r=rgb.getInt(i),w=white.getInt(i);
            if(r<0||r>255||w<0||w>255)throw new IllegalArgumentException("Invalid calibration gain");
            result[30+i]=(byte)r;result[33+i]=(byte)w;
        }
        result[36]=(byte)(cal.optBoolean("gamma_enabled",false)?1:0);
        if(zones.length()==0)return result;
        int panelId=draft.optInt("panel_id",0);
        if(panelId==0)panelId=baseline.optInt("panel_id",0x8001+lane);
        if(panelId<1||panelId>65535)
            throw new IllegalArgumentException("Invalid full Panel ID");
        String panelName=draft.optString("panel_name",baseline.optString("panel_name","Panel 1"));
        result[37]=1;result[38]=(byte)zones.length();
        put16(result,39,panelId);result[41]=(byte)lane;
        putName(result,42,panelName);result[67]=(byte)zones.length();
        int cursor=0;
        boolean[] usedAlias=new boolean[256];
        for(int i=0;i<zones.length();i++){
            JSONObject zone=zones.getJSONObject(i);
            int count=exactCount(zone,"pixels");
            if(count<=0||count>MAX_LEDS||cursor+count>pixels)
                throw new IllegalArgumentException("Invalid contiguous zone pixel range");
            int id=zone.optInt("id",0);
            if(id==0){
                // Match GeometryTransactionManager.mapLayout() and the
                // firmware's 0x4000|low-byte identity: section slots 0..7
                // have aliases 129..136 on O1 and 137..144 on O2.
                // A different ID would make direct Save diverge from the
                // *already playing* unsaved geometry, correctly rejecting it.
                int section=zone.optInt("physical_section",i);
                if(section<0||section>=8)
                    throw new IllegalArgumentException("Invalid physical zone section");
                id=0x4000|(129+lane*8+section);
            }
            int alias=id&255;
            if(id<1||id>65535||alias==0||usedAlias[alias])
                throw new IllegalArgumentException("Duplicate or ambiguous full Zone ID");
            usedAlias[alias]=true;
            int owner=zone.optInt("panel_id",panelId);
            if(owner!=panelId)
                throw new IllegalArgumentException("Cross-panel zone edits require the advanced Panel editor");
            int role=index(ROLES,zone.optString("role","off"));
            if(role<0&&"unassigned".equals(zone.optString("role")))role=255;
            if(role<0)throw new IllegalArgumentException("Unknown zone role");
            int at=155+i*64;
            put16(result,at,id);put16(result,at+2,panelId);
            putName(result,at+4,zone.getString("name"));
            result[at+29]=(byte)role;result[at+31]=1;
            put16(result,at+32,cursor);put16(result,at+34,count);
            cursor+=count;
        }
        if(cursor!=pixels)
            throw new IllegalArgumentException("Zone lengths must equal configured LED count");
        return result;
    }
}
