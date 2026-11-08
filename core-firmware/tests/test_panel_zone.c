#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "panel_zone.h"
#include "ble_config.h"

static void make_output(ot_pz_output_t *o,unsigned lane,uint16_t pixels){
    memset(o,0,sizeof(*o));
    (void)snprintf(o->name,sizeof(o->name),"Lane %u",lane+1u);
    o->active_pixels=pixels;o->color_order=OT_OM_GRB;
    for(unsigned i=0;i<3;i++)o->rgb_gain[i]=o->white_gain[i]=255u;
}
static void make_panel(ot_pz_output_t *o,unsigned lane,uint16_t id,const char *name){
    ot_pz_panel_t *p=&o->panels[o->panel_count++];
    p->id=id;p->lane=(uint8_t)lane;
    (void)snprintf(p->name,sizeof(p->name),"%s",name);
}
static void make_zone(ot_pz_output_t *o,unsigned index,uint16_t id,uint16_t panel,
                      uint8_t role,const char *name,
                      uint16_t start,uint16_t count){
    ot_pz_zone_t *z=&o->zones[index];
    z->id=id;z->panel_id=panel;z->role=role;
    (void)snprintf(z->name,sizeof(z->name),"%s",name);
    z->range_count=1u;z->ranges[0]=(ot_pz_range_t){start,count};
    o->zone_count++;
    for(unsigned p=0;p<o->panel_count;p++)
        if(o->panels[p].id==panel)o->panels[p].zone_count++;
}
static ot_rgb_t expected_fallback(unsigned lane,uint8_t mask){
    if(mask&OT_SIG_BRAKE)return (ot_rgb_t){32,0,0};
    if(mask&OT_SIG_REVERSE)return (ot_rgb_t){32,32,32};
    const bool left=(mask&OT_SIG_LEFT)!=0u,right=(mask&OT_SIG_RIGHT)!=0u;
    if((left&&right)||(lane==0u&&left)||(lane==1u&&right))
        return (ot_rgb_t){32,19,0};
    if(mask&OT_SIG_PARK)return (ot_rgb_t){7,0,0};
    return (ot_rgb_t){0,0,0};
}
static void defaults_and_fallback(void){
    ot_pz_controller_t cfg;ot_pz_defaults(&cfg);
    assert(ot_pz_valid(&cfg));
    for(unsigned lane=0;lane<2;lane++){
        const ot_pz_output_t *o=&cfg.outputs[lane];
        assert(o->active_pixels==0u&&o->panel_count==0u&&o->zone_count==0u);
        uint8_t zones[OT_PZ_MAX_PIXELS];uint16_t logical[OT_PZ_MAX_PIXELS];
        ot_pz_build_pixel_map(o,zones,logical);
        for(unsigned px=0;px<OT_PZ_MAX_PIXELS;px++)assert(zones[px]==0xffu);
        for(unsigned mask=0;mask<256;mask++){
            const ot_rgb_t got=ot_pz_fallback_color(lane,(uint8_t)mask,32u);
            const ot_rgb_t want=expected_fallback(lane,(uint8_t)mask);
            assert(memcmp(&got,&want,sizeof(got))==0);
        }
    }
    const ot_rgb_t black={0,0,0},red={32,0,0},cyan={0,32,32};
    ot_rgb_t got=ot_pz_priority_color(true,true,red,true,cyan,red);
    assert(memcmp(&got,&black,3)==0);
    got=ot_pz_priority_color(false,true,red,true,cyan,black);
    assert(memcmp(&got,&red,3)==0);
    got=ot_pz_priority_color(false,false,black,true,cyan,red);
    assert(memcmp(&got,&cyan,3)==0);
}
static void hierarchy_and_ranges(void){
    ot_pz_controller_t cfg;ot_pz_defaults(&cfg);
    ot_pz_output_t *o=&cfg.outputs[0];make_output(o,0,100);
    make_panel(o,0,100,"Left housing");
    make_panel(o,0,101,"Trunk section");
    make_zone(o,0,1,100,OT_OM_BRAKE,"Brake",0,20);
    make_zone(o,1,2,100,OT_OM_LEFT,"Turn",20,30);
    make_zone(o,2,3,101,OT_OM_REVERSE,"Reverse",50,50);
    assert(ot_pz_valid(&cfg)); /* one lane, multiple panels, contiguous zones */
    uint8_t zones[OT_PZ_MAX_PIXELS];uint16_t logical[OT_PZ_MAX_PIXELS];
    ot_pz_build_pixel_map(o,zones,logical);
    assert(zones[0]==0u&&logical[0]==0u);
    assert(zones[19]==0u&&logical[19]==19u);
    assert(zones[20]==1u&&logical[20]==0u);
    assert(zones[49]==1u&&logical[49]==29u);
    assert(zones[50]==2u&&logical[50]==0u);
    assert(zones[99]==2u&&logical[99]==49u);
    assert(zones[100]==0xffu);

    /* A single logical zone spans two physical segments; its logical pixel
     * sequence continues across the discontinuity without reordering the lane. */
    ot_pz_defaults(&cfg);o=&cfg.outputs[0];make_output(o,0,100);
    make_panel(o,0,100,"One housing");
    make_zone(o,0,1,100,OT_OM_DECORATIVE,"Accent",0,10);
    o->zones[0].range_count=2;o->zones[0].ranges[1]=(ot_pz_range_t){30,10};
    assert(ot_pz_valid(&cfg));
    ot_pz_build_pixel_map(o,zones,logical);
    assert(zones[9]==0u&&logical[9]==9u);
    assert(zones[10]==0xffu);
    assert(zones[30]==0u&&logical[30]==10u);
    assert(zones[39]==0u&&logical[39]==19u);

    /* UNASSIGNED is not OFF; explicit OFF remains a distinct stored role. */
    ot_pz_defaults(&cfg);o=&cfg.outputs[0];make_output(o,0,20);
    make_panel(o,0,100,"Housing");
    make_zone(o,0,1,100,OT_PZ_ROLE_UNASSIGNED,"Unassigned",0,10);
    make_zone(o,1,2,100,OT_OM_OFF,"Disabled",10,10);
    assert(ot_pz_valid(&cfg));
    assert(o->zones[0].role==OT_PZ_ROLE_UNASSIGNED);
    assert(o->zones[1].role==OT_OM_OFF);
}
static void invalid_maps(void){
    ot_pz_controller_t cfg;ot_pz_defaults(&cfg);
    ot_pz_output_t *o=&cfg.outputs[0];make_output(o,0,20);
    make_panel(o,0,100,"Housing");
    make_zone(o,0,1,100,OT_OM_BRAKE,"Brake",0,10);
    make_zone(o,1,2,100,OT_OM_LEFT,"Turn",10,10);
    assert(ot_pz_valid(&cfg));
    ot_pz_controller_t bad=cfg;bad.outputs[0].zones[1].ranges[0].start=9;
    assert(!ot_pz_valid(&bad)); /* overlap */
    bad=cfg;bad.outputs[0].zones[1].ranges[0].start=20;
    assert(!ot_pz_valid(&bad)); /* out of bounds */
    bad=cfg;bad.outputs[0].zones[1].ranges[0].count=0;
    assert(!ot_pz_valid(&bad)); /* zero count */
    bad=cfg;bad.outputs[0].zones[1].id=1;
    assert(!ot_pz_valid(&bad)); /* duplicate zone ID */
    bad=cfg;bad.outputs[0].panels[0].id=1;
    assert(!ot_pz_valid(&bad)); /* panel/zone ID namespace collision */
    bad=cfg;bad.outputs[0].zones[1].panel_id=999;
    assert(!ot_pz_valid(&bad)); /* invalid panel reference */
    bad=cfg;bad.outputs[0].panels[0].lane=1;
    assert(!ot_pz_valid(&bad)); /* wrong output lane */
    bad=cfg;bad.outputs[0].panels[0].zone_count=1;
    assert(!ot_pz_valid(&bad)); /* malformed panel child count */
    bad=cfg;bad.outputs[0].zones[1].role=0xfeu;
    assert(!ot_pz_valid(&bad)); /* invalid role */
    bad=cfg;bad.outputs[0].zones[0].ranges[0].start=UINT16_MAX;
    assert(!ot_pz_valid(&bad)); /* uint16 end overflow/out of bounds */
    bad=cfg;bad.outputs[0].active_pixels=OT_PZ_MAX_PIXELS+1u;
    assert(!ot_pz_valid(&bad)); /* capacity rejection */
    bad=cfg;bad.outputs[0].zone_count=OT_PZ_MAX_ZONES_PER_OUTPUT+1u;
    assert(!ot_pz_valid(&bad));
    bad=cfg;bad.outputs[0].panel_count=OT_PZ_MAX_PANELS_PER_OUTPUT+1u;
    assert(!ot_pz_valid(&bad));
    bad=cfg;bad.outputs[0].zones[0].range_count=OT_PZ_MAX_RANGES_PER_ZONE+1u;
    assert(!ot_pz_valid(&bad));
}
static void legacy_migration(void){
    ot_om_output_t legacy[OT_OM_OUTPUTS];ot_om_defaults(legacy);
    legacy[0].active_pixels=417;legacy[0].zone_count=5;
    const uint16_t lengths[]={77,51,209,80,0};
    const uint8_t roles[]={OT_OM_DECORATIVE,OT_OM_AUX2,OT_OM_REVERSE,OT_OM_PARK_BRAKE,OT_OM_OFF};
    const char *names[]={"Accent","Aux Segment","Zone C","Zone D","Spare"};
    legacy[0].rgb_gain[0]=201;legacy[0].white_gain[2]=177;legacy[0].gamma_enabled=true;
    for(unsigned z=0;z<5;z++){
        memset(&legacy[0].zones[z],0,sizeof(legacy[0].zones[z]));
        (void)snprintf(legacy[0].zones[z].name,OT_OM_NAME_LEN,"%s",names[z]);
        legacy[0].zones[z].pixels=lengths[z];legacy[0].zones[z].role=roles[z];
    }
    assert(ot_om_valid(&legacy[0]));
    ot_pz_controller_t migrated;
    assert(ot_pz_migrate_legacy(legacy,true,&migrated));
    assert(ot_pz_valid(&migrated));
    const ot_pz_output_t *o=&migrated.outputs[0];
    assert(o->panel_count==1u&&o->zone_count==5u&&o->active_pixels==417u);
    assert(!strcmp(o->name,legacy[0].name));
    assert(o->color_order==legacy[0].color_order);
    assert(o->rgb_gain[0]==201u&&o->white_gain[2]==177u&&o->gamma_enabled);
    uint32_t offset=0u;
    for(unsigned z=0;z<5;z++){
        assert(!strcmp(o->zones[z].name,names[z]));
        assert(o->zones[z].role==roles[z]);
        if(lengths[z]){
            assert(o->zones[z].range_count==1u);
            assert(o->zones[z].ranges[0].start==offset);
            assert(o->zones[z].ranges[0].count==lengths[z]);
        }else{
            assert(o->zones[z].range_count==0u);
            assert(o->zones[z].flags==OT_PZ_ZONE_FLAG_LEGACY_EMPTY);
        }
        offset+=lengths[z];
    }
    ot_om_output_t restored;
    assert(ot_pz_to_legacy(o,0,&restored));
    assert(memcmp(&restored,&legacy[0],sizeof(restored))==0);
    assert(ot_pz_to_fx_view(o,0,&restored));
    assert(restored.active_pixels==417u&&restored.zone_count==5u);
    assert(restored.zones[0].pixels==77u);
    assert(!ot_pz_migrate_legacy(legacy,false,NULL));
    ot_pz_controller_t fresh;
    assert(ot_pz_migrate_legacy(NULL,false,&fresh));
    assert(ot_pz_valid(&fresh)&&fresh.outputs[0].panel_count==0u);
}
static void capacity_cases(void){
    ot_pz_controller_t cfg;ot_pz_defaults(&cfg);
    for(unsigned lane=0;lane<2;lane++){
        ot_pz_output_t *o=&cfg.outputs[lane];make_output(o,lane,2048);
        make_panel(o,lane,(uint16_t)(100u+lane),"Max panel");
        for(unsigned z=0;z<8;z++){
            const uint16_t count=z==7?256u:256u;
            make_zone(o,z,(uint16_t)(200u+lane*8u+z),
                      (uint16_t)(100u+lane),OT_OM_DECORATIVE,"Segment",
                      (uint16_t)(z*256u),count);
        }
    }
    assert(ot_pz_valid(&cfg)); /* 2,048 pixels per output */
    cfg.outputs[0].active_pixels=2048u;cfg.outputs[1].active_pixels=2048u;
    assert(ot_pz_valid(&cfg));
    cfg.outputs[0].zones[7].ranges[0].start=1791u;
    cfg.outputs[0].zones[7].ranges[0].count=1u;
    assert(!ot_pz_valid(&cfg)); /* overlaps zone 6 at physical pixel 1791 */
    cfg.outputs[0].zones[7].ranges[0].start=2047u;
    cfg.outputs[0].zones[7].ranges[0].count=2u;
    assert(!ot_pz_valid(&cfg)); /* exclusive end 2049 exceeds the 2,048-pixel lane */
    ot_pz_defaults(&cfg);
    for(unsigned lane=0;lane<2;lane++){
        ot_pz_output_t *o=&cfg.outputs[lane];make_output(o,lane,623);
        make_panel(o,lane,(uint16_t)(100u+lane),"GS400 lane");
        for(unsigned z=0;z<8;z++){
            const uint16_t start=(uint16_t)(z*77u);
            const uint16_t count=(uint16_t)(z==7?84u:77u);
            make_zone(o,z,(uint16_t)(200u+lane*8u+z),
                      (uint16_t)(100u+lane),OT_OM_DECORATIVE,"Zone",
                      start,count);
        }
    }
    assert(ot_pz_valid(&cfg)); /* 1,246 total pixels, 623 per output */
}
static void wire_roundtrip(void){
    ot_pz_controller_t cfg;ot_pz_defaults(&cfg);
    ot_pz_output_t *o=&cfg.outputs[1];make_output(o,1,100);
    make_panel(o,1,101,"Right panel");
    make_zone(o,0,201,101,OT_PZ_ROLE_UNASSIGNED,"Segment A",0,25);
    o->zones[0].range_count=2;o->zones[0].ranges[1]=(ot_pz_range_t){50,25};
    make_zone(o,1,202,101,OT_OM_OFF,"OFF",25,25);
    assert(ot_pz_valid(&cfg));
    uint8_t wire[OT_PZ_MAX_WIRE_BYTES];ot_pz_output_t decoded={0};
    assert(ot_pz_wire_encode(o,1,wire));
    assert(wire[0]==OT_PZ_WIRE_VERSION&&wire[1]==1u);
    assert(ot_pz_wire_decode(wire,1,&decoded));
    assert(memcmp(o,&decoded,sizeof(*o))==0);
    wire[0]=1;assert(!ot_pz_wire_decode(wire,1,&decoded));wire[0]=OT_PZ_WIRE_VERSION;
    assert(!ot_pz_wire_decode(wire,0,&decoded));
    wire[40]=0xff;assert(!ot_pz_wire_decode(wire,1,&decoded)); /* malformed panel ID/structure */
}
int main(void){
    defaults_and_fallback();hierarchy_and_ranges();invalid_maps();
    legacy_migration();capacity_cases();wire_roundtrip();
    puts("PASS Issue126 Panel/Zone model: migration, ranges, fallback, 256 masks, 1,246 pixels, 2,048/lane and v3 codec");
    return 0;
}
