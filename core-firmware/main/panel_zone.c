#include "panel_zone.h"
#include <string.h>
#include <stdio.h>

static bool pz_name_ok(const char *s){
    if(!s)return false;
    for(unsigned i=0;i<OT_PZ_NAME_LEN;i++){
        const unsigned char c=(unsigned char)s[i];
        if(c==0)return i>0u;
        if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||
             (c>='0'&&c<='9')||c==' '||c=='-'||c=='_'||
             c=='.'||c=='/'||c=='('||c==')'))return false;
    }
    return false;
}
static bool pz_role_ok(uint8_t role){
    return role<=OT_OM_DECORATIVE || role==OT_PZ_ROLE_UNASSIGNED;
}
static uint16_t read_le16(const uint8_t *p){
    return (uint16_t)((uint16_t)p[0]|((uint16_t)p[1]<<8));
}
static void write_le16(uint8_t *p,uint16_t value){
    p[0]=(uint8_t)value;p[1]=(uint8_t)(value>>8);
}
void ot_pz_defaults(ot_pz_controller_t *config){
    if(!config)return;
    memset(config,0,sizeof(*config));
    for(unsigned lane=0;lane<OT_PZ_OUTPUTS;lane++){
        ot_pz_output_t *out=&config->outputs[lane];
        (void)snprintf(out->name,sizeof(out->name),"Pixel OUT %u",lane+1u);
        out->active_pixels=0u; /* Unknown physical length; never persist capacity as length. */
        out->color_order=OT_OM_GRB;
        for(unsigned c=0;c<3u;c++)out->rgb_gain[c]=out->white_gain[c]=255u;
    }
}
uint16_t ot_pz_zone_pixel_count(const ot_pz_zone_t *zone){
    if(!zone)return 0u;
    uint32_t total=0u;
    for(unsigned i=0;i<zone->range_count&&i<OT_PZ_MAX_RANGES_PER_ZONE;i++)
        total+=(uint32_t)zone->ranges[i].count;
    return total>UINT16_MAX?UINT16_MAX:(uint16_t)total;
}
bool ot_pz_output_valid(const ot_pz_output_t *out,unsigned lane){
    if(!out||lane>=OT_PZ_OUTPUTS||!pz_name_ok(out->name)||
       out->active_pixels>OT_PZ_MAX_PIXELS||
       out->color_order>OT_OM_BGR||
       out->panel_count>OT_PZ_MAX_PANELS_PER_OUTPUT||
       out->zone_count>OT_PZ_MAX_ZONES_PER_OUTPUT)return false;
    if(out->active_pixels==0u && (out->panel_count!=0u||out->zone_count!=0u))
        return false;
    for(unsigned p=0;p<out->panel_count;p++){
        const ot_pz_panel_t *panel=&out->panels[p];
        if(!panel->id||panel->lane!=lane||!pz_name_ok(panel->name)||
           panel->zone_count>OT_PZ_MAX_ZONES_PER_OUTPUT)return false;
        for(unsigned q=0;q<p;q++)if(out->panels[q].id==panel->id)return false;
        unsigned actual=0u;
        for(unsigned z=0;z<out->zone_count;z++)
            if(out->zones[z].panel_id==panel->id)actual++;
        if(actual!=panel->zone_count)return false;
    }
    for(unsigned z=0;z<out->zone_count;z++){
        const ot_pz_zone_t *zone=&out->zones[z];
        if(!zone->id||!pz_name_ok(zone->name)||!pz_role_ok(zone->role)||
           (zone->flags&~OT_PZ_ZONE_FLAGS_MASK)!=0u||
           zone->range_count>OT_PZ_MAX_RANGES_PER_ZONE)return false;
        bool panel_found=false;
        for(unsigned p=0;p<out->panel_count;p++)
            if(out->panels[p].id==zone->panel_id)panel_found=true;
        if(!panel_found)return false;
        if(zone->range_count==0u){
            /* Only a read-only migration placeholder can have no pixels.
             * New v3 clients cannot create these records over the wire. */
            if(zone->flags!=OT_PZ_ZONE_FLAG_LEGACY_EMPTY)return false;
        }else if(zone->flags!=0u)return false;
        uint32_t zone_total=0u;
        for(unsigned r=0;r<zone->range_count;r++){
            const uint32_t start=zone->ranges[r].start;
            const uint32_t count=zone->ranges[r].count;
            const uint32_t end=start+count;
            if(count==0u||end>out->active_pixels||end>65536u)return false;
            zone_total+=count;
        }
        if(zone_total>out->active_pixels)return false;
        for(unsigned q=0;q<z;q++)if(out->zones[q].id==zone->id)return false;
    }
    /* Global lane coordinates: ranges may be adjacent or discontiguous but
     * never overlap, including ranges owned by different panels/zones. */
    for(unsigned za=0;za<out->zone_count;za++){
        const ot_pz_zone_t *a=&out->zones[za];
        for(unsigned ra=0;ra<a->range_count;ra++){
            const uint32_t a0=a->ranges[ra].start;
            const uint32_t a1=a0+a->ranges[ra].count;
            for(unsigned zb=za;zb<out->zone_count;zb++){
                const ot_pz_zone_t *b=&out->zones[zb];
                const unsigned first=zb==za?ra+1u:0u;
                for(unsigned rb=first;rb<b->range_count;rb++){
                    const uint32_t b0=b->ranges[rb].start;
                    const uint32_t b1=b0+b->ranges[rb].count;
                    if(a0<b1&&b0<a1)return false;
                }
            }
        }
    }
    return true;
}
bool ot_pz_valid(const ot_pz_controller_t *config){
    if(!config)return false;
    for(unsigned lane=0;lane<OT_PZ_OUTPUTS;lane++)
        if(!ot_pz_output_valid(&config->outputs[lane],lane))return false;
    /* Panel and Zone IDs share one controller-wide namespace. */
    for(unsigned a=0;a<OT_PZ_OUTPUTS;a++){
        const ot_pz_output_t *oa=&config->outputs[a];
        for(unsigned p=0;p<oa->panel_count;p++){
            const uint16_t id=oa->panels[p].id;
            for(unsigned b=0;b<OT_PZ_OUTPUTS;b++){
                const ot_pz_output_t *ob=&config->outputs[b];
                for(unsigned q=0;q<ob->panel_count;q++)
                    if((a!=b||p!=q)&&ob->panels[q].id==id)return false;
                for(unsigned z=0;z<ob->zone_count;z++)
                    if(ob->zones[z].id==id)return false;
            }
        }
        for(unsigned z=0;z<oa->zone_count;z++){
            const uint16_t id=oa->zones[z].id;
            for(unsigned b=a;b<OT_PZ_OUTPUTS;b++){
                const ot_pz_output_t *ob=&config->outputs[b];
                const unsigned first=b==a?z+1u:0u;
                for(unsigned q=first;q<ob->zone_count;q++)
                    if(ob->zones[q].id==id)return false;
            }
            for(unsigned b=0;b<OT_PZ_OUTPUTS;b++)
                for(unsigned p=0;p<config->outputs[b].panel_count;p++)
                    if(config->outputs[b].panels[p].id==id)return false;
        }
    }
    return true;
}
bool ot_pz_split(const ot_pz_controller_t *runtime,
                 ot_pz_lane_config_set_t *lanes,ot_pz_map_t *map){
    if(!runtime||!lanes||!map||!ot_pz_valid(runtime))return false;
    ot_pz_lane_config_set_t next_lanes={0};
    ot_pz_map_t next_map={0};
    for(unsigned lane=0;lane<OT_PZ_OUTPUTS;lane++){
        const ot_pz_output_t *source=&runtime->outputs[lane];
        ot_pz_lane_config_t *hardware=&next_lanes.lanes[lane];
        ot_pz_map_output_t *geometry=&next_map.outputs[lane];
        hardware->lane_id=(uint8_t)lane;
        memcpy(hardware->name,source->name,sizeof(hardware->name));
        hardware->active_pixels=source->active_pixels;
        hardware->color_order=source->color_order;
        memcpy(hardware->rgb_gain,source->rgb_gain,sizeof(hardware->rgb_gain));
        memcpy(hardware->white_gain,source->white_gain,sizeof(hardware->white_gain));
        hardware->gamma_enabled=source->gamma_enabled;
        geometry->lane_id=(uint8_t)lane;
        geometry->panel_count=source->panel_count;
        geometry->zone_count=source->zone_count;
        memcpy(geometry->panels,source->panels,
               (size_t)source->panel_count*sizeof(geometry->panels[0]));
        memcpy(geometry->zones,source->zones,
               (size_t)source->zone_count*sizeof(geometry->zones[0]));
    }
    *lanes=next_lanes;*map=next_map;
    return true;
}
bool ot_pz_join(const ot_pz_lane_config_set_t *lanes,const ot_pz_map_t *map,
                ot_pz_controller_t *runtime){
    if(!lanes||!map||!runtime)return false;
    ot_pz_controller_t next;ot_pz_defaults(&next);
    for(unsigned lane=0;lane<OT_PZ_OUTPUTS;lane++){
        const ot_pz_lane_config_t *hardware=&lanes->lanes[lane];
        const ot_pz_map_output_t *geometry=&map->outputs[lane];
        if(hardware->lane_id!=lane||geometry->lane_id!=lane||
           geometry->panel_count>OT_PZ_MAX_PANELS_PER_OUTPUT||
           geometry->zone_count>OT_PZ_MAX_ZONES_PER_OUTPUT)return false;
        ot_pz_output_t *out=&next.outputs[lane];
        memcpy(out->name,hardware->name,sizeof(out->name));
        out->active_pixels=hardware->active_pixels;
        out->color_order=hardware->color_order;
        memcpy(out->rgb_gain,hardware->rgb_gain,sizeof(out->rgb_gain));
        memcpy(out->white_gain,hardware->white_gain,sizeof(out->white_gain));
        out->gamma_enabled=hardware->gamma_enabled;
        out->panel_count=geometry->panel_count;
        out->zone_count=geometry->zone_count;
        memcpy(out->panels,geometry->panels,
               (size_t)geometry->panel_count*sizeof(out->panels[0]));
        memcpy(out->zones,geometry->zones,
               (size_t)geometry->zone_count*sizeof(out->zones[0]));
    }
    if(!ot_pz_valid(&next))return false;
    *runtime=next;
    return true;
}
bool ot_pz_lane_config_set_valid(const ot_pz_lane_config_set_t *lanes){
    if(!lanes)return false;
    ot_pz_map_t empty={0};
    for(unsigned lane=0;lane<OT_PZ_OUTPUTS;lane++)
        empty.outputs[lane].lane_id=(uint8_t)lane;
    ot_pz_controller_t joined;
    return ot_pz_join(lanes,&empty,&joined);
}
bool ot_pz_map_valid(const ot_pz_map_t *map){
    if(!map)return false;
    ot_pz_lane_config_set_t maximum={0};
    for(unsigned lane=0;lane<OT_PZ_OUTPUTS;lane++){
        ot_pz_lane_config_t *hardware=&maximum.lanes[lane];
        hardware->lane_id=(uint8_t)lane;
        (void)snprintf(hardware->name,sizeof(hardware->name),
                       "Pixel OUT %u",lane+1u);
        hardware->active_pixels=OT_PZ_MAX_PIXELS;
        hardware->color_order=OT_OM_GRB;
        for(unsigned c=0;c<3u;c++)
            hardware->rgb_gain[c]=hardware->white_gain[c]=255u;
    }
    ot_pz_controller_t joined;
    return ot_pz_join(&maximum,map,&joined);
}
bool ot_pz_activation_inputs_quiet(uint8_t instantaneous,uint8_t debounced,
                                   uint8_t quiet_samples,uint8_t required_samples,
                                   uint32_t current_epoch,uint32_t expected_epoch){
    return instantaneous==0u && debounced==0u &&
           quiet_samples>=required_samples && current_epoch==expected_epoch;
}
bool ot_pz_preview_expire(ot_pz_preview_t *state,uint32_t now_ms){
    if(!state||!state->active||state->committing)return false;
    if((int32_t)(now_ms-state->expires_ms)<0)return false;
    state->active=false;
    state->committing=false;
    return true;
}
void ot_pz_preview_cancel(ot_pz_preview_t *state){
    if(state){state->active=false;state->committing=false;}
}
bool ot_pz_preview_begin(ot_pz_preview_t *state,
                         const ot_pz_controller_t *committed,unsigned lane,
                         uint16_t active_revision,uint16_t base_revision,
                         bool pending,uint32_t now_ms){
    if(!state||!committed||lane>=OT_PZ_OUTPUTS||!active_revision||
       active_revision!=base_revision||pending||!ot_pz_valid(committed)||
       state->committing)return false;
    (void)ot_pz_preview_expire(state,now_ms);
    if(state->active)return false;
    state->candidate=*committed;
    state->lane=(uint8_t)lane;
    state->base_revision=base_revision;
    state->expires_ms=now_ms+OT_PZ_PREVIEW_LEASE_MS;
    state->active=true;
    return true;
}
bool ot_pz_preview_update(ot_pz_preview_t *state,unsigned lane,
                          const ot_pz_output_t *next,uint16_t base_revision,
                          uint16_t active_revision,bool pending,uint32_t now_ms){
    if(!state||!next||!state->active||state->committing||lane>=OT_PZ_OUTPUTS||
       state->lane!=lane||state->base_revision!=base_revision||
       active_revision!=base_revision||pending||
       (int32_t)(now_ms-state->expires_ms)>=0||
       !ot_pz_output_valid(next,lane)){
        if(state)(void)ot_pz_preview_expire(state,now_ms);
        return false;
    }
    /* Called under the owner mutex in CORE. Keep full-schema scratch out of
     * the constrained NimBLE task stack, including nested UPDATE calls. */
    static ot_pz_controller_t candidate;
    candidate=state->candidate;
    candidate.outputs[lane]=*next;
    if(!ot_pz_valid(&candidate))return false;
    state->candidate=candidate;
    return true;
}
static int pz_find_panel(const ot_pz_output_t *out,uint16_t panel_id){
    for(unsigned i=0;i<out->panel_count;i++)
        if(out->panels[i].id==panel_id)return (int)i;
    return -1;
}
/* UPDATE has one byte per ID. Resolve the low-byte alias to the full stable
 * ID; multiple matches are rejected rather than guessed. */
static int pz_find_panel_wire(const ot_pz_output_t *out,uint8_t wire_id){
    int found=-1;
    for(unsigned i=0;i<out->panel_count;i++){
        if((out->panels[i].id&OT_PZ_ID_WIRE_MASK)!=wire_id)continue;
        if(found>=0)return -2;
        found=(int)i;
    }
    return found;
}
static int pz_find_zone_wire(const ot_pz_output_t *out,uint8_t wire_id){
    int found=-1;
    for(unsigned i=0;i<out->zone_count;i++){
        if((out->zones[i].id&OT_PZ_ID_WIRE_MASK)!=wire_id)continue;
        if(found>=0)return -2;
        found=(int)i;
    }
    return found;
}
static void pz_recount_panel_zones(ot_pz_output_t *out){
    for(unsigned p=0;p<out->panel_count;p++){
        unsigned count=0;
        for(unsigned z=0;z<out->zone_count;z++)
            if(out->zones[z].panel_id==out->panels[p].id)count++;
        out->panels[p].zone_count=(uint8_t)count;
    }
}
static void pz_remove_zone(ot_pz_output_t *out,unsigned index){
    if(index>=out->zone_count)return;
    if(index+1u<out->zone_count)
        memmove(&out->zones[index],&out->zones[index+1u],
                (out->zone_count-index-1u)*sizeof(out->zones[0]));
    out->zone_count--;
    memset(&out->zones[out->zone_count],0,sizeof(out->zones[0]));
    pz_recount_panel_zones(out);
}
static void pz_remove_panel(ot_pz_output_t *out,unsigned index){
    if(index>=out->panel_count)return;
    const uint16_t id=out->panels[index].id;
    for(unsigned z=0;z<out->zone_count;){
        if(out->zones[z].panel_id==id)pz_remove_zone(out,z);
        else z++;
    }
    if(index+1u<out->panel_count)
        memmove(&out->panels[index],&out->panels[index+1u],
                (out->panel_count-index-1u)*sizeof(out->panels[0]));
    out->panel_count--;
    memset(&out->panels[out->panel_count],0,sizeof(out->panels[0]));
}
static bool pz_apply_one_update(ot_pz_output_t *out,unsigned lane,
                                const ot_pz_range_update_t *u){
    if(!out||!u||(u->role_flags&0xc0u)!=0u)return false;
    const uint8_t wire_role=(uint8_t)(u->role_flags&OT_PZ_TX_ROLE_MASK);
    /* The four-bit wire value 0x0f is the only representable UNASSIGNED
     * sentinel; normalize it before validation/storage. Role 0 remains OFF. */
    const uint8_t role=(wire_role==OT_PZ_TX_ROLE_MASK)?
        OT_PZ_ROLE_UNASSIGNED:wire_role;
    const uint8_t action=(uint8_t)((u->role_flags&OT_PZ_TX_ACTION_MASK)>>OT_PZ_TX_ACTION_SHIFT);
    if(u->panel_id==0u)return false;
    int panel_index=pz_find_panel_wire(out,u->panel_id);
    if(panel_index==-2)return false; /* ambiguous low-byte identity */
    const uint16_t panel_id=panel_index>=0?out->panels[panel_index].id:
        (uint16_t)(OT_PZ_PANEL_ID_PREFIX|u->panel_id);
    if(action==OT_PZ_TX_DELETE_PANEL){
        if(u->zone_id!=0u||u->range_id!=0u||u->start!=0u||u->count!=0u||role!=0u||
           panel_index<0)return false;
        pz_remove_panel(out,(unsigned)panel_index);
        return true;
    }
    if(u->zone_id==0u)return false;
    int zone_index=pz_find_zone_wire(out,u->zone_id);
    if(zone_index==-2)return false; /* ambiguous low-byte identity */
    const uint16_t zone_id=zone_index>=0?out->zones[zone_index].id:
        (uint16_t)(OT_PZ_ZONE_ID_PREFIX|u->zone_id);
    if(action==OT_PZ_TX_DELETE_ZONE){
        if(u->range_id!=0u||u->start!=0u||u->count!=0u||role!=0u||
           zone_index<0||out->zones[zone_index].panel_id!=panel_id)return false;
        pz_remove_zone(out,(unsigned)zone_index);
        panel_index=pz_find_panel(out,panel_id);
        if(panel_index>=0&&out->panels[panel_index].zone_count==0u)
            pz_remove_panel(out,(unsigned)panel_index);
        return true;
    }
    if(action==OT_PZ_TX_DELETE_RANGE){
        if(u->start!=0u||u->count!=0u||role!=0u||zone_index<0||
           out->zones[zone_index].panel_id!=panel_id||
           u->range_id>=out->zones[zone_index].range_count)return false;
        ot_pz_zone_t *zone=&out->zones[zone_index];
        if(u->range_id+1u<zone->range_count)
            memmove(&zone->ranges[u->range_id],&zone->ranges[u->range_id+1u],
                    (zone->range_count-u->range_id-1u)*sizeof(zone->ranges[0]));
        zone->range_count--;
        memset(&zone->ranges[zone->range_count],0,sizeof(zone->ranges[0]));
        if(zone->range_count==0u){
            pz_remove_zone(out,(unsigned)zone_index);
            panel_index=pz_find_panel(out,panel_id);
            if(panel_index>=0&&out->panels[panel_index].zone_count==0u)
                pz_remove_panel(out,(unsigned)panel_index);
        }
        return true;
    }
    if(action!=OT_PZ_TX_UPSERT||u->count==0u||
       !(role<=OT_OM_DECORATIVE||role==OT_PZ_ROLE_UNASSIGNED)||
       u->range_id>=OT_PZ_MAX_RANGES_PER_ZONE)return false;
    if(panel_index<0){
        if(out->panel_count>=OT_PZ_MAX_PANELS_PER_OUTPUT)return false;
        panel_index=(int)out->panel_count++;
        ot_pz_panel_t *panel=&out->panels[panel_index];
        memset(panel,0,sizeof(*panel));panel->id=panel_id;
        panel->lane=(uint8_t)lane;
        (void)snprintf(panel->name,sizeof(panel->name),"Panel %u",(unsigned)panel_id);
    }
    if(zone_index<0){
        if(out->zone_count>=OT_PZ_MAX_ZONES_PER_OUTPUT)return false;
        zone_index=(int)out->zone_count++;
        ot_pz_zone_t *zone=&out->zones[zone_index];
        memset(zone,0,sizeof(*zone));zone->id=zone_id;
        zone->panel_id=panel_id;zone->role=role;
        (void)snprintf(zone->name,sizeof(zone->name),"Zone %u",(unsigned)zone_id);
        pz_recount_panel_zones(out);
    }else if(out->zones[zone_index].panel_id!=panel_id)return false;
    ot_pz_zone_t *zone=&out->zones[zone_index];
    if(u->range_id>zone->range_count)return false;
    if(u->range_id==zone->range_count){
        if(zone->range_count>=OT_PZ_MAX_RANGES_PER_ZONE)return false;
        zone->range_count++;
    }
    zone->role=role;zone->flags=0u;
    zone->ranges[u->range_id]=(ot_pz_range_t){u->start,u->count};
    return true;
}
bool ot_pz_preview_apply_updates(ot_pz_preview_t *state,unsigned lane,
                          const ot_pz_range_update_t *updates,size_t update_count,
                          uint16_t base_revision,uint16_t active_revision,
                          bool pending,uint32_t now_ms){
    if(!state||!updates||update_count==0u||update_count>4u||
       !state->active||state->committing||lane>=OT_PZ_OUTPUTS||
       state->lane!=lane||state->base_revision!=base_revision||
       active_revision!=base_revision||pending||
       (int32_t)(now_ms-state->expires_ms)>=0){
        if(state)(void)ot_pz_preview_expire(state,now_ms);
        return false;
    }
    for(size_t i=0;i<update_count;i++)
        for(size_t j=0;j<i;j++)
            if(updates[i].panel_id==updates[j].panel_id&&
               updates[i].zone_id==updates[j].zone_id&&
               updates[i].range_id==updates[j].range_id)return false;
    /* The caller serializes previews; this bounded scratch is non-reentrant.
     * This keeps 5+ KiB off the BLE/GATT stack. */
    static ot_pz_preview_t candidate;
    candidate=*state;
    ot_pz_output_t *out=&candidate.candidate.outputs[lane];
    for(size_t i=0;i<update_count;i++)
        if(!pz_apply_one_update(out,lane,&updates[i]))return false;
    if(!ot_pz_valid(&candidate.candidate))return false;
    candidate.expires_ms=state->expires_ms; /* UPDATE does not extend the lease. */
    *state=candidate;
    return true;
}
bool ot_pz_preview_heartbeat(ot_pz_preview_t *state,unsigned lane,
                          uint16_t base_revision,uint32_t now_ms){
    if(!state||!state->active||state->committing||lane>=OT_PZ_OUTPUTS||
       state->lane!=lane||state->base_revision!=base_revision||
       (int32_t)(now_ms-state->expires_ms)>=0){
        if(state)(void)ot_pz_preview_expire(state,now_ms);
        return false;
    }
    state->expires_ms=now_ms+OT_PZ_PREVIEW_LEASE_MS;
    return true;
}
bool ot_pz_migrate_legacy(const ot_om_output_t legacy[OT_OM_OUTPUTS],
                          bool legacy_persisted,ot_pz_controller_t *out){
    if(!out)return false;
    ot_pz_defaults(out);
    if(!legacy_persisted)return true;
    if(!legacy)return false;
    for(unsigned lane=0;lane<OT_PZ_OUTPUTS;lane++){
        const ot_om_output_t *src=&legacy[lane];
        if(!ot_om_valid(src))return false;
        ot_pz_output_t *dst=&out->outputs[lane];
        memcpy(dst->name,src->name,OT_PZ_NAME_LEN);
        dst->active_pixels=src->active_pixels;
        dst->color_order=src->color_order;
        memcpy(dst->rgb_gain,src->rgb_gain,sizeof(dst->rgb_gain));
        memcpy(dst->white_gain,src->white_gain,sizeof(dst->white_gain));
        dst->gamma_enabled=src->gamma_enabled;
        dst->panel_count=1u;
        dst->zone_count=src->zone_count;
        ot_pz_panel_t *panel=&dst->panels[0];
        panel->id=(uint16_t)(OT_PZ_PANEL_ID_PREFIX+1u+lane);
        panel->lane=(uint8_t)lane;
        (void)snprintf(panel->name,sizeof(panel->name),"Panel %u",lane+1u);
        panel->zone_count=src->zone_count;
        uint32_t offset=0u;
        for(unsigned z=0;z<src->zone_count;z++){
            const ot_om_zone_t *old=&src->zones[z];
            ot_pz_zone_t *zone=&dst->zones[z];
            zone->id=(uint16_t)(OT_PZ_ZONE_ID_PREFIX+1u+lane*OT_PZ_MAX_ZONES_PER_OUTPUT+z);
            zone->panel_id=panel->id;
            memcpy(zone->name,old->name,OT_PZ_NAME_LEN);
            zone->role=old->role;
            if(old->pixels==0u){
                zone->flags=OT_PZ_ZONE_FLAG_LEGACY_EMPTY;
                zone->range_count=0u;
            }else{
                zone->range_count=1u;
                zone->ranges[0].start=(uint16_t)offset;
                zone->ranges[0].count=old->pixels;
            }
            offset+=old->pixels;
        }
        if(offset!=src->active_pixels)return false;
    }
    return ot_pz_valid(out);
}
static bool pz_id_used(const ot_pz_controller_t *cfg,unsigned skip_lane,uint16_t id){
    if(!cfg)return false;
    for(unsigned lane=0;lane<OT_PZ_OUTPUTS;lane++){
        if(lane==skip_lane)continue;
        const ot_pz_output_t *o=&cfg->outputs[lane];
        for(unsigned p=0;p<o->panel_count;p++)if(o->panels[p].id==id)return true;
        for(unsigned z=0;z<o->zone_count;z++)if(o->zones[z].id==id)return true;
    }
    return false;
}
static uint16_t pz_allocate_id(const ot_pz_controller_t *cfg,unsigned lane,
                               const ot_pz_output_t *built,uint16_t seed){
    for(uint32_t step=0;step<UINT16_MAX;step++){
        uint16_t id=(uint16_t)(seed+step);
        if(id==0u||(id&OT_PZ_ID_WIRE_MASK)==0u)continue;
        bool used=pz_id_used(cfg,lane,id);
        if(built){
            for(unsigned p=0;p<built->panel_count;p++)if(built->panels[p].id==id)used=true;
            for(unsigned z=0;z<built->zone_count;z++)if(built->zones[z].id==id)used=true;
        }
        if(!used)return id;
    }
    return 0u;
}
bool ot_pz_from_legacy_output(const ot_om_output_t *legacy,unsigned lane,
                              const ot_pz_controller_t *existing,
                              ot_pz_output_t *out){
    if(!legacy||!out||lane>=OT_PZ_OUTPUTS||!ot_om_valid(legacy))return false;
    ot_pz_output_t next={0};
    memcpy(next.name,legacy->name,OT_PZ_NAME_LEN);
    next.active_pixels=legacy->active_pixels;next.color_order=legacy->color_order;
    memcpy(next.rgb_gain,legacy->rgb_gain,sizeof(next.rgb_gain));
    memcpy(next.white_gain,legacy->white_gain,sizeof(next.white_gain));
    next.gamma_enabled=legacy->gamma_enabled;
    next.panel_count=1u;next.zone_count=legacy->zone_count;
    const uint16_t panel_id=pz_allocate_id(existing,lane,&next,
        (uint16_t)(OT_PZ_PANEL_ID_PREFIX+1u+lane));
    if(!panel_id)return false;
    next.panels[0].id=panel_id;next.panels[0].lane=(uint8_t)lane;
    (void)snprintf(next.panels[0].name,sizeof(next.panels[0].name),"Panel %u",lane+1u);
    next.panels[0].zone_count=legacy->zone_count;
    uint32_t offset=0u;
    for(unsigned z=0;z<legacy->zone_count;z++){
        ot_pz_zone_t *zone=&next.zones[z];
        zone->id=pz_allocate_id(existing,lane,&next,
            (uint16_t)(OT_PZ_ZONE_ID_PREFIX+1u+lane*OT_PZ_MAX_ZONES_PER_OUTPUT+z));
        if(!zone->id)return false;
        zone->panel_id=panel_id;zone->role=legacy->zones[z].role;
        memcpy(zone->name,legacy->zones[z].name,OT_PZ_NAME_LEN);
        if(legacy->zones[z].pixels==0u){
            zone->flags=OT_PZ_ZONE_FLAG_LEGACY_EMPTY;
        }else{
            zone->range_count=1u;
            zone->ranges[0]=(ot_pz_range_t){(uint16_t)offset,legacy->zones[z].pixels};
        }
        offset+=legacy->zones[z].pixels;
    }
    if(offset!=legacy->active_pixels||!ot_pz_output_valid(&next,lane))return false;
    /* BLE Output v1 Save runs on the NimBLE host stack. A prior final
     * validation copied the complete controller a second time onto that
     * stack, even after the Save path moved its candidate into mutex-owned
     * BSS. Validate the already-authoritative existing controller directly:
     * newly allocated panel/zone IDs are checked against every OTHER lane
     * and all previously assigned IDs in next, and next was individually
     * validated above. No full-controller stack copy is required. */
    if(existing && !ot_pz_valid(existing))return false;
    *out=next;return true;
}
bool ot_pz_to_legacy(const ot_pz_output_t *src,unsigned lane,
                     ot_om_output_t *dst){
    if(!src||!dst||lane>=OT_PZ_OUTPUTS||
       !ot_pz_output_valid(src,lane))return false;
    if(src->panel_count==0u&&src->zone_count==0u){
        ot_om_output_t defaults[OT_OM_OUTPUTS];ot_om_defaults(defaults);
        *dst=defaults[lane];return true;
    }
    if(src->panel_count!=1u||src->zone_count==0u||
       src->panels[0].zone_count!=src->zone_count)return false;
    ot_om_output_t next={0};
    memcpy(next.name,src->name,OT_OM_NAME_LEN);
    next.active_pixels=src->active_pixels;
    next.color_order=src->color_order;
    memcpy(next.rgb_gain,src->rgb_gain,sizeof(next.rgb_gain));
    memcpy(next.white_gain,src->white_gain,sizeof(next.white_gain));
    next.gamma_enabled=src->gamma_enabled;
    next.zone_count=src->zone_count;
    uint32_t offset=0u;
    for(unsigned z=0;z<src->zone_count;z++){
        const ot_pz_zone_t *zone=&src->zones[z];
        if(zone->panel_id!=src->panels[0].id||
           zone->role==OT_PZ_ROLE_UNASSIGNED||
           (zone->range_count!=1u&&zone->range_count!=0u))return false;
        if(zone->range_count==1u&&zone->ranges[0].start!=offset)return false;
        const uint16_t count=zone->range_count?zone->ranges[0].count:0u;
        memcpy(next.zones[z].name,zone->name,OT_OM_NAME_LEN);
        next.zones[z].pixels=count;
        next.zones[z].role=zone->role;
        offset+=count;
    }
    if(offset!=src->active_pixels||!ot_om_valid(&next))return false;
    *dst=next;return true;
}
bool ot_pz_to_fx_view(const ot_pz_output_t *src,unsigned lane,
                      ot_om_output_t *dst){
    if(!src||!dst||!ot_pz_output_valid(src,lane))return false;
    ot_om_output_t view={0};
    memcpy(view.name,src->name,OT_OM_NAME_LEN);
    view.color_order=src->color_order;
    memcpy(view.rgb_gain,src->rgb_gain,sizeof(view.rgb_gain));
    memcpy(view.white_gain,src->white_gain,sizeof(view.white_gain));
    view.gamma_enabled=src->gamma_enabled;
    uint32_t total=0u;
    for(unsigned z=0;z<src->zone_count;z++)
        total+=ot_pz_zone_pixel_count(&src->zones[z]);
    if(total==0u){
        view.active_pixels=1u;view.zone_count=1u;
        (void)snprintf(view.zones[0].name,sizeof(view.zones[0].name),"Unconfigured");
        view.zones[0].pixels=1u;view.zones[0].role=OT_OM_OFF;
        *dst=view;return true;
    }
    if(total>OT_PZ_MAX_PIXELS)return false;
    view.active_pixels=(uint16_t)total;
    view.zone_count=src->zone_count;
    for(unsigned z=0;z<src->zone_count;z++){
        const ot_pz_zone_t *zone=&src->zones[z];
        memcpy(view.zones[z].name,zone->name,OT_OM_NAME_LEN);
        view.zones[z].pixels=ot_pz_zone_pixel_count(zone);
        view.zones[z].role=zone->role==OT_PZ_ROLE_UNASSIGNED?
                           OT_OM_DECORATIVE:zone->role;
    }
    *dst=view;return true;
}
void ot_pz_build_pixel_map(const ot_pz_output_t *out,
                           uint8_t zone_index[OT_PZ_MAX_PIXELS],
                           uint16_t logical_index[OT_PZ_MAX_PIXELS]){
    if(!zone_index||!logical_index)return;
    memset(zone_index,0xff,OT_PZ_MAX_PIXELS*sizeof(zone_index[0]));
    memset(logical_index,0,OT_PZ_MAX_PIXELS*sizeof(logical_index[0]));
    if(!out)return;
    for(unsigned z=0;z<out->zone_count&&z<OT_PZ_MAX_ZONES_PER_OUTPUT;z++){
        const ot_pz_zone_t *zone=&out->zones[z];
        uint32_t logical=0u;
        for(unsigned r=0;r<zone->range_count&&r<OT_PZ_MAX_RANGES_PER_ZONE;r++){
            const uint32_t start=zone->ranges[r].start;
            const uint32_t end=start+zone->ranges[r].count;
            for(uint32_t px=start;px<end&&px<OT_PZ_MAX_PIXELS;px++){
                zone_index[px]=(uint8_t)z;
                logical_index[px]=(uint16_t)logical++;
            }
        }
    }
}
static ot_rgb_t scale_rgb(unsigned r,unsigned g,unsigned b,uint8_t brightness){
    return (ot_rgb_t){
        (uint8_t)((r*brightness+127u)/255u),
        (uint8_t)((g*brightness+127u)/255u),
        (uint8_t)((b*brightness+127u)/255u)
    };
}
ot_rgb_t ot_pz_fallback_color(unsigned lane,uint8_t raw,uint8_t brightness){
    if(lane>=OT_PZ_OUTPUTS)return (ot_rgb_t){0,0,0};
    if(raw&OT_SIG_BRAKE)return scale_rgb(255u,0u,0u,brightness);
    if(raw&OT_SIG_REVERSE)return scale_rgb(255u,255u,255u,brightness);
    const bool left=(raw&OT_SIG_LEFT)!=0u;
    const bool right=(raw&OT_SIG_RIGHT)!=0u;
    if((left&&right)||(lane==0u&&left)||(lane==1u&&right))
        return scale_rgb(255u,150u,0u,brightness);
    if(raw&OT_SIG_PARK)return scale_rgb(55u,0u,0u,brightness);
    return (ot_rgb_t){0,0,0};
}
ot_rgb_t ot_pz_priority_color(bool system_inhibit,bool physical_owned,
                              ot_rgb_t physical_color,bool diagnostic_owned,
                              ot_rgb_t diagnostic_color,ot_rgb_t effect_color){
    if(system_inhibit)return (ot_rgb_t){0,0,0};
    if(physical_owned)return physical_color;
    if(diagnostic_owned)return diagnostic_color;
    return effect_color;
}
bool ot_pz_wire_encode(const ot_pz_output_t *out,unsigned lane,
                       uint8_t wire[OT_PZ_MAX_WIRE_BYTES]){
    if(!wire||!out||!ot_pz_output_valid(out,lane))return false;
    memset(wire,0,OT_PZ_MAX_WIRE_BYTES);
    size_t k=0u;
    wire[k++]=OT_PZ_WIRE_VERSION;wire[k++]=(uint8_t)lane;
    write_le16(wire+k,out->active_pixels);k+=2u;
    wire[k++]=out->color_order;
    memcpy(wire+k,out->name,OT_PZ_NAME_LEN);k+=OT_PZ_NAME_LEN;
    memcpy(wire+k,out->rgb_gain,3u);k+=3u;
    memcpy(wire+k,out->white_gain,3u);k+=3u;
    wire[k++]=out->gamma_enabled?1u:0u;
    wire[k++]=out->panel_count;wire[k++]=out->zone_count;
    for(unsigned p=0;p<OT_PZ_MAX_PANELS_PER_OUTPUT;p++){
        const ot_pz_panel_t *panel=&out->panels[p];
        write_le16(wire+k,panel->id);k+=2u;
        wire[k++]=panel->lane;
        memcpy(wire+k,panel->name,OT_PZ_NAME_LEN);k+=OT_PZ_NAME_LEN;
        wire[k++]=panel->zone_count;
    }
    for(unsigned z=0;z<OT_PZ_MAX_ZONES_PER_OUTPUT;z++){
        const ot_pz_zone_t *zone=&out->zones[z];
        write_le16(wire+k,zone->id);k+=2u;
        write_le16(wire+k,zone->panel_id);k+=2u;
        memcpy(wire+k,zone->name,OT_PZ_NAME_LEN);k+=OT_PZ_NAME_LEN;
        wire[k++]=zone->role;wire[k++]=zone->flags;wire[k++]=zone->range_count;
        for(unsigned r=0;r<OT_PZ_MAX_RANGES_PER_ZONE;r++){
            write_le16(wire+k,zone->ranges[r].start);k+=2u;
            write_le16(wire+k,zone->ranges[r].count);k+=2u;
        }
    }
    return k==OT_PZ_MAX_WIRE_BYTES;
}
bool ot_pz_wire_decode(const uint8_t wire[OT_PZ_MAX_WIRE_BYTES],
                       unsigned expected_lane,ot_pz_output_t *out){
    if(!wire||!out||expected_lane>=OT_PZ_OUTPUTS||
       wire[0]!=OT_PZ_WIRE_VERSION||wire[1]!=expected_lane)return false;
    ot_pz_output_t next={0};size_t k=0u;
    if(wire[k++]!=OT_PZ_WIRE_VERSION||wire[k++]!=expected_lane)return false;
    next.active_pixels=read_le16(wire+k);k+=2u;
    next.color_order=wire[k++];
    memcpy(next.name,wire+k,OT_PZ_NAME_LEN);k+=OT_PZ_NAME_LEN;
    memcpy(next.rgb_gain,wire+k,3u);k+=3u;
    memcpy(next.white_gain,wire+k,3u);k+=3u;
    if(wire[k]>1u)return false;
    next.gamma_enabled=wire[k++]!=0u;
    next.panel_count=wire[k++];next.zone_count=wire[k++];
    if(next.panel_count>OT_PZ_MAX_PANELS_PER_OUTPUT||
       next.zone_count>OT_PZ_MAX_ZONES_PER_OUTPUT)return false;
    for(unsigned p=0;p<OT_PZ_MAX_PANELS_PER_OUTPUT;p++){
        ot_pz_panel_t *panel=&next.panels[p];
        panel->id=read_le16(wire+k);k+=2u;
        panel->lane=wire[k++];
        memcpy(panel->name,wire+k,OT_PZ_NAME_LEN);k+=OT_PZ_NAME_LEN;
        panel->zone_count=wire[k++];
        if(p>=next.panel_count){
            if(panel->id||panel->lane||panel->zone_count||
               memcmp(panel->name,(uint8_t[OT_PZ_NAME_LEN]){0},OT_PZ_NAME_LEN)!=0)
                return false;
        }
    }
    for(unsigned z=0;z<OT_PZ_MAX_ZONES_PER_OUTPUT;z++){
        ot_pz_zone_t *zone=&next.zones[z];
        zone->id=read_le16(wire+k);k+=2u;
        zone->panel_id=read_le16(wire+k);k+=2u;
        memcpy(zone->name,wire+k,OT_PZ_NAME_LEN);k+=OT_PZ_NAME_LEN;
        zone->role=wire[k++];zone->flags=wire[k++];zone->range_count=wire[k++];
        if(zone->range_count>OT_PZ_MAX_RANGES_PER_ZONE)return false;
        for(unsigned r=0;r<OT_PZ_MAX_RANGES_PER_ZONE;r++){
            zone->ranges[r].start=read_le16(wire+k);k+=2u;
            zone->ranges[r].count=read_le16(wire+k);k+=2u;
            if(r>=zone->range_count&&
               (zone->ranges[r].start||zone->ranges[r].count))return false;
        }
        if(z>=next.zone_count){
            if(zone->id||zone->panel_id||zone->role||zone->flags||
               zone->range_count||
               memcmp(zone->name,(uint8_t[OT_PZ_NAME_LEN]){0},OT_PZ_NAME_LEN)!=0)
                return false;
        }
    }
    if(k!=OT_PZ_MAX_WIRE_BYTES||!ot_pz_output_valid(&next,expected_lane))
        return false;
    *out=next;return true;
}
