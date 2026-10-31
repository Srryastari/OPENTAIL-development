package com.opentail.android;

import org.json.JSONArray;
import org.json.JSONObject;

/** Preflight only verified CORE readback. Local Layout assignments never
 * activate hardware; no persistence, output enabling or GPIO operations. */
final class CoreBenchTargets {
    private CoreBenchTargets() {}
    static int enabledMask(JSONObject[] outputs,int requestedChains,int requestedZones) {
        int mask=0;
        if(outputs==null || requestedChains<1 || requestedChains>3 ||
           requestedZones<1 || requestedZones>255)return 0;
        for(int o=0;o<Math.min(2,outputs.length);o++) {
            if((requestedChains&(1<<o))==0 || outputs[o]==null)continue;
            JSONObject out=outputs[o];
            if(out.optInt("pixels",0)<1)continue;
            JSONArray zones=out.optJSONArray("zones");
            if(zones==null)continue;
            for(int z=0;z<Math.min(8,zones.length());z++) {
                JSONObject zone=zones.optJSONObject(z);
                if(zone==null || (requestedZones&(1<<z))==0)continue;
                String role=zone.optString("role","off");
                boolean supported=role.equals("park_brake")||role.equals("parking")||
                    role.equals("brake")||role.equals("left_turn")||role.equals("right_turn")||
                    role.equals("reverse")||role.equals("aux1")||role.equals("aux2")||
                    role.equals("aux3")||role.equals("decorative");
                if(zone.optInt("pixels",0)>0 && supported) {mask|=1<<o;break;}
            }
        }
        return mask;
    }
}
