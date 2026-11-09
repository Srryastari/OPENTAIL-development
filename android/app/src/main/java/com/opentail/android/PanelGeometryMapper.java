package com.opentail.android;

import org.json.JSONObject;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Deterministic bridge from the existing local Layout/physical-section links
 * to the explicit CORE Panel/Zone/range identity model.
 *
 * A local Studio zone is never assumed to be a CORE Panel. Every assigned
 * section must carry an explicit physicalPanelId/name before it can be mapped.
 * This class is pure translation: it does not start BLE or persist controller
 * configuration. The current Layout editor models one contiguous physical
 * section per design zone; multi-range zones require explicit future authoring.
 */
final class PanelGeometryMapper {
    static final int MAX_PIXELS = 2000;
    static final int MAX_PANELS_PER_LANE = 4;
    static final int MAX_ZONES_PER_LANE = 8;
    static final int MAX_PANEL_ID = 127;
    static final int ZONE_ID_BASE = 128;

    static final class Zone {
        final int id,panelId,section,start,count,roleFlags;
        final String name;
        Zone(int id,int panelId,int section,int start,int count,String name) {
            this.id=id;this.panelId=panelId;this.section=section;
            this.start=start;this.count=count;this.name=name;
            // CORE role 0x0f is UNASSIGNED; commissioning fallback remains authoritative.
            this.roleFlags=0x0f;
        }
        PanelPreviewProtocol.UpdateDescriptor descriptor() {
            return new PanelPreviewProtocol.UpdateDescriptor(panelId,id,0,start,count,roleFlags);
        }
    }

    static final class Panel {
        final int id,lane,zoneCount;
        final String name;
        Panel(int id,int lane,String name,int zoneCount) {
            this.id=id;this.lane=lane;this.name=name;this.zoneCount=zoneCount;
        }
    }

    static final class Lane {
        final int id,activePixels;
        final List<Panel> panels;
        final List<Zone> zones;
        Lane(int id,int activePixels,List<Panel> panels,List<Zone> zones) {
            this.id=id;this.activePixels=activePixels;
            this.panels=java.util.Collections.unmodifiableList(new ArrayList<>(panels));
            this.zones=java.util.Collections.unmodifiableList(new ArrayList<>(zones));
        }
        PanelPreviewProtocol.UpdateDescriptor[] descriptors() {
            PanelPreviewProtocol.UpdateDescriptor[] out=
                new PanelPreviewProtocol.UpdateDescriptor[zones.size()];
            for(int i=0;i<zones.size();i++)out[i]=zones.get(i).descriptor();
            return out;
        }
    }

    static final class Mapping {
        final Lane[] lanes;
        Mapping(Lane[] lanes) { this.lanes=lanes; }
        Lane lane(int lane) {
            if(lane<0||lane>=lanes.length)throw new IllegalArgumentException("Invalid output lane");
            return lanes[lane];
        }
    }

    private PanelGeometryMapper() {}

    static Mapping build(NativeProject project) {
        if(project==null)throw new IllegalArgumentException("Layout project is required");
        @SuppressWarnings("unchecked")
        List<JSONObject>[] assigned=new List[]{new ArrayList<>(),new ArrayList<>()};
        for(int i=0;i<project.zoneCount();i++) {
            JSONObject zone=project.zone(i);
            int lane=zone.optInt("physicalOutput",-1);
            if(lane<0)continue; // Unassigned design zones are not controller geometry.
            if(lane>1)throw new IllegalArgumentException("Physical output is outside the two-lane contract");
            if(!zone.has("physicalPanelId")||!zone.has("physicalPanelName"))
                throw new IllegalStateException("Section "+zone.optString("name")+
                    " has no explicit CORE Panel identity");
            assigned[lane].add(zone);
        }

        Lane[] lanes=new Lane[2];
        Map<Integer,Integer> globalPanelLanes=new HashMap<>();
        for(int lane=0;lane<2;lane++) {
            List<JSONObject> sections=assigned[lane];
            sections.sort(Comparator.comparingInt(z->z.optInt("physicalSection",-1)));
            if(sections.size()>MAX_ZONES_PER_LANE)
                throw new IllegalArgumentException("Output "+(lane+1)+" exceeds CORE's eight-zone limit");
            Map<Integer,String> panelNames=new HashMap<>();
            Map<Integer,Integer> panelCounts=new HashMap<>();
            List<Zone> zones=new ArrayList<>();
            int cursor=0;
            for(JSONObject source:sections) {
                int section=source.optInt("physicalSection",-1);
                int panelId=source.optInt("physicalPanelId",-1);
                String panelName=source.optString("physicalPanelName","").trim();
                int count=source.optInt("length",-1);
                if(section<0||section>7||panelId<1||panelId>MAX_PANEL_ID||
                   panelName.isEmpty()||count<1)
                    throw new IllegalArgumentException("Invalid physical section or Panel identity");
                Integer priorLane=globalPanelLanes.putIfAbsent(panelId,lane);
                if(priorLane!=null&&priorLane!=lane)
                    throw new IllegalArgumentException("CORE Panel IDs are controller-wide and cannot cross outputs");
                String priorName=panelNames.putIfAbsent(panelId,panelName);
                if(priorName!=null&&!priorName.equals(panelName))
                    throw new IllegalArgumentException("One Panel ID has conflicting names");
                panelCounts.put(panelId,panelCounts.getOrDefault(panelId,0)+1);
                long end=(long)cursor+count;
                if(end>MAX_PIXELS)
                    throw new IllegalArgumentException("Output "+(lane+1)+" exceeds the 2,000-pixel Android limit");
                int zoneId=ZONE_ID_BASE+lane*8+section+1;
                zones.add(new Zone(zoneId,panelId,section,cursor,count,source.optString("name")));
                cursor=(int)end;
            }
            if(panelNames.size()>MAX_PANELS_PER_LANE)
                throw new IllegalArgumentException("Output "+(lane+1)+" exceeds CORE's four-Panel limit");
            List<Panel> panels=new ArrayList<>();
            for(Map.Entry<Integer,String> e:panelNames.entrySet())
                panels.add(new Panel(e.getKey(),lane,e.getValue(),panelCounts.get(e.getKey())));
            panels.sort(Comparator.comparingInt(p->p.id));
            lanes[lane]=new Lane(lane,cursor,panels,zones);
        }
        return new Mapping(lanes);
    }
}
