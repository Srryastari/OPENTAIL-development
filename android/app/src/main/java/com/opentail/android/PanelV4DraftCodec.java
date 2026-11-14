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

    /* Output picker indicates SAVED Panel v4 state, not the independently
     * editable, potentially stale/unsaved local output draft. Until an
     * authoritative lane snapshot arrives, the universal count is 0. */
    static int savedSelectorPixels(JSONObject confirmedOutput) {
        return confirmedOutput==null?0:Math.max(0,confirmedOutput.optInt("pixels",0));
    }
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

    /**
     * Preserve every controller Panel, Zone and ordered range in the editor's
     * JSON. "pixels" is the sum of a Zone's ranges; "ranges" holds the true
     * physical lane offsets. The legacy single-panel contiguous editor may
     * reflow ranges, but an advanced configuration never does so implicitly.
     */
    static JSONObject fromSnapshot(PanelPreviewProtocol.LaneSnapshot snapshot) throws Exception {
        PanelPreviewProtocol.MapIdentity map=PanelPreviewProtocol.MapIdentity.parse(snapshot);
        byte[] b=snapshot.wire;
        if(map.activePixels>MAX_LEDS)
            throw new IllegalArgumentException("Configured output exceeds the Android 2000-LED limit");
        JSONObject draft=new JSONObject()
            .put("name",readName(b,5)).put("pixels",map.activePixels)
            .put("color_order",ORDERS[u8(b[4])]);
        JSONArray rgb=new JSONArray(),white=new JSONArray();
        for(int i=0;i<3;i++){rgb.put(u8(b[30+i]));white.put(u8(b[33+i]));}
        draft.put("calibration",new JSONObject().put("rgb_gain",rgb)
            .put("white_gain",white).put("gamma_enabled",u8(b[36])!=0));
        JSONArray panels=new JSONArray(),zones=new JSONArray();
        for(PanelPreviewProtocol.MapPanel p:map.panels)
            panels.put(new JSONObject().put("id",p.id).put("name",p.name)
                .put("lane",p.lane).put("zone_count",p.zoneCount));
        boolean advanced=map.panels.size()>1;
        int cursor=0;
        for(int i=0;i<map.zones.size();i++){
            PanelPreviewProtocol.MapZone z=map.zones.get(i);
            JSONArray ranges=new JSONArray();
            int total=0;
            for(PanelPreviewProtocol.MapRange range:z.ranges){
                ranges.put(new JSONObject().put("start",range.start).put("count",range.count));
                total+=range.count;
            }
            if(z.ranges.size()!=1||z.ranges.get(0).start!=cursor||z.flags!=0)
                advanced=true;
            if(z.ranges.size()==1)cursor=z.ranges.get(0).start+z.ranges.get(0).count;
            JSONObject zone=new JSONObject().put("name",z.name).put("id",z.id)
                .put("panel_id",z.panelId).put("role",z.role==255?"unassigned":ROLES[z.role])
                .put("flags",z.flags).put("pixels",total).put("ranges",ranges);
            zones.put(zone);
        }
        draft.put("panels",panels).put("zones",zones)
            .put("v4_editable",true).put("v4_advanced",advanced);
        if(panels.length()==1){
            draft.put("panel_id",panels.getJSONObject(0).getInt("id"));
            draft.put("panel_name",panels.getJSONObject(0).getString("name"));
        }
        return draft;
    }

    private static int checkedId(JSONObject source,String key) throws Exception {
        int id=exactCount(source,key);
        if(id<1||id>65535||(id&255)==0)
            throw new IllegalArgumentException("Invalid full 16-bit Panel/Zone identity");
        return id;
    }
    private static final class Range {
        final int start,count;
        Range(int start,int count){this.start=start;this.count=count;}
    }

    /**
     * Encode all supported v4 shapes, including 4 Panels, 8 Zones and 8
     * discontiguous ranges per Zone. No automatic migration or flattening.
     * All unused slots remain zero and each full 16-bit identity is kept.
     */
    static byte[] encode(int lane,JSONObject draft,PanelPreviewProtocol.LaneSnapshot confirmed)
            throws Exception {
        if(lane<0||lane>1||draft==null||confirmed==null||confirmed.lane!=lane)
            throw new IllegalArgumentException("Fresh matching Panel v4 readback required");
        JSONObject baseline=fromSnapshot(confirmed);
        int pixels=exactCount(draft,"pixels");
        if(pixels<0||pixels>MAX_LEDS)throw new IllegalArgumentException("Output exceeds 2000 LEDs");
        JSONArray zones=draft.getJSONArray("zones");
        if(zones.length()>8||(pixels==0&&zones.length()!=0))
            throw new IllegalArgumentException("Invalid Panel v4 zone count");
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
            int red=rgb.getInt(i),w=white.getInt(i);
            if(red<0||red>255||w<0||w>255)
                throw new IllegalArgumentException("Invalid calibration gain");
            result[30+i]=(byte)red;result[33+i]=(byte)w;
        }
        result[36]=(byte)(cal.optBoolean("gamma_enabled",false)?1:0);
        // A genuinely unassigned lane may have a capacity and no zones;
        // do not fabricate one to fill the hardware buffer.
        if(zones.length()==0)return result;

        JSONArray requestedPanels=draft.optJSONArray("panels");
        if(requestedPanels==null)requestedPanels=baseline.optJSONArray("panels");
        java.util.LinkedHashMap<Integer,String> panelNames=new java.util.LinkedHashMap<>();
        if(requestedPanels!=null)for(int i=0;i<requestedPanels.length();i++){
            JSONObject panel=requestedPanels.getJSONObject(i);
            int id=checkedId(panel,"id");
            if(panelNames.put(id,panel.getString("name"))!=null)
                throw new IllegalArgumentException("Duplicate full Panel ID");
        }
        java.util.LinkedHashMap<Integer,Integer> owners=new java.util.LinkedHashMap<>();
        java.util.HashSet<Integer> zoneIds=new java.util.HashSet<>();
        java.util.HashSet<Integer> zoneAliases=new java.util.HashSet<>();
        java.util.HashSet<Integer> panelAliases=new java.util.HashSet<>();
        java.util.ArrayList<Range> occupied=new java.util.ArrayList<>();
        int linearCursor=0;
        boolean advanced=draft.optBoolean("v4_advanced",false);
        // Read old single-panel JSON as well; never substitute a fabricated
        // physical panel for a multi-panel controller readback.
        int fallbackPanel=draft.optInt("panel_id",baseline.optInt("panel_id",0x8001+lane));
        String fallbackName=draft.optString("panel_name",
            baseline.optString("panel_name","Panel O"+(lane+1)));
        for(int i=0;i<zones.length();i++){
            JSONObject zone=zones.getJSONObject(i);
            int id=zone.optInt("id",0);
            if(id==0){
                int section=zone.optInt("physical_section",i);
                if(section<0||section>=8)
                    throw new IllegalArgumentException("Invalid physical Zone section");
                id=0x4000|(129+lane*8+section);
            }
            if(id<1||id>65535||(id&255)==0||!zoneIds.add(id)||!zoneAliases.add(id&255))
                throw new IllegalArgumentException("Duplicate or ambiguous full Zone ID");
            int owner=zone.optInt("panel_id",fallbackPanel);
            if(owner<1||owner>65535||(owner&255)==0)
                throw new IllegalArgumentException("Missing full Panel owner identity");
            if(!panelNames.containsKey(owner)){
                if(panelNames.size()!=0 && owner!=fallbackPanel)
                    throw new IllegalArgumentException("Zone refers to an unknown Panel");
                panelNames.put(owner,fallbackName);
            }
            owners.put(owner,owners.getOrDefault(owner,0)+1);
            int role=index(ROLES,zone.optString("role","off"));
            if(role<0&&"unassigned".equals(zone.optString("role")))role=255;
            if(role<0)throw new IllegalArgumentException("Unknown Zone role");
            int at=155+i*64;
            put16(result,at,id);put16(result,at+2,owner);
            putName(result,at+4,zone.getString("name"));
            int flags=zone.optInt("flags",0);
            result[at+29]=(byte)role;
            JSONArray ranges=zone.optJSONArray("ranges");
            java.util.ArrayList<Range> items=new java.util.ArrayList<>();
            if(!advanced && flags==0){
                // Approved simple editor: contiguous single range and automatic
                // reflow after LED-count changes. No hidden gap elimination
                // is performed for advanced/imported maps.
                int count=exactCount(zone,"pixels");
                if(count<=0||linearCursor+count>pixels)
                    throw new IllegalArgumentException("Invalid contiguous Zone length");
                items.add(new Range(linearCursor,count));
                linearCursor+=count;
            } else if(ranges!=null) {
                if(ranges.length()>8)throw new IllegalArgumentException("Too many Zone ranges");
                int total=0;
                for(int n=0;n<ranges.length();n++){
                    JSONObject item=ranges.getJSONObject(n);
                    int start=exactCount(item,"start"),count=exactCount(item,"count");
                    if(start<0||count<=0||(long)start+count>pixels)
                        throw new IllegalArgumentException("Range outside configured LED count");
                    items.add(new Range(start,count));
                    total+=count;
                }
                if(total!=exactCount(zone,"pixels"))
                    throw new IllegalArgumentException("Zone total differs from its explicit ranges");
            } else {
                throw new IllegalArgumentException("Advanced Zone needs explicit ranges");
            }
            if(items.isEmpty()?(flags!=1):(flags!=0))
                throw new IllegalArgumentException("Invalid legacy-empty Zone marker");
            result[at+30]=(byte)flags;
            result[at+31]=(byte)items.size();
            for(int n=0;n<items.size();n++){
                Range item=items.get(n);
                for(Range prior:occupied)
                    if(item.start<prior.start+prior.count&&prior.start<item.start+item.count)
                        throw new IllegalArgumentException("Physical pixel ranges overlap");
                occupied.add(item);
                put16(result,at+32+4*n,item.start);
                put16(result,at+34+4*n,item.count);
            }
        }
        if(!advanced && linearCursor!=pixels)
            throw new IllegalArgumentException("Zone lengths must equal configured LEDs");
        // A panel without Zones is not persisted: explicitly removing the last
        // Zone also removes its Panel, not another output's saved ownership.
        java.util.ArrayList<Integer> used=new java.util.ArrayList<>();
        for(int id:panelNames.keySet())if(owners.containsKey(id))used.add(id);
        if(used.size()>4)throw new IllegalArgumentException("Output exceeds four Panels");
        for(int i=0;i<used.size();i++){
            int id=used.get(i);
            if(!panelAliases.add(id&255))
                throw new IllegalArgumentException("Ambiguous full Panel alias");
            int at=39+i*29;
            put16(result,at,id);result[at+2]=(byte)lane;
            putName(result,at+3,panelNames.get(id));
            result[at+28]=(byte)(int)owners.get(id);
        }
        result[37]=(byte)used.size();
        result[38]=(byte)zones.length();
        return result;
    }

}
