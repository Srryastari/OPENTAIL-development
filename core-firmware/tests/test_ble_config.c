#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ble_config.h"
int main(void){
    ot_om_output_t a[OT_OM_OUTPUTS],out={0};ot_om_defaults(a);
    uint8_t frame[OT_CFG_WIRE_BYTES];
    ot_cfg_encode(&a[0],frame);
    assert(frame[0]==1 && frame[1]==30 && frame[2]==0);
    assert(ot_cfg_decode(frame,&out));
    assert(out.active_pixels==30 && out.color_order==OT_OM_BRG);
    assert(!strcmp(out.name,a[0].name));
    assert(out.zone_count==a[0].zone_count);
    for(int z=0;z<out.zone_count;z++){
        assert(out.zones[z].pixels==a[0].zones[z].pixels);
        assert(out.zones[z].role==a[0].zones[z].role);
    }
    /* 522-pixel main housing, four sections plus optional zero-length trunk. */
    a[0].active_pixels=522;a[0].zone_count=5;
    const unsigned lengths[]={118,109,42,253,0};
    const uint8_t roles[]={OT_OM_PARK_BRAKE,OT_OM_LEFT,OT_OM_REVERSE,OT_OM_PARK_BRAKE,OT_OM_OFF};
    const char *names[]={"Top Tail","Turn","Reverse","Bottom Tail","Trunk"};
    for(int i=0;i<5;i++){
        memset(a[0].zones[i].name,0,OT_OM_NAME_LEN);
        strcpy(a[0].zones[i].name,names[i]);
        a[0].zones[i].pixels=lengths[i];a[0].zones[i].role=roles[i];
    }
    assert(ot_om_valid(&a[0]));
    ot_cfg_encode(&a[0],frame);
    uint16_t crc=ot_cfg_crc16(frame,sizeof(frame));
    assert(crc==ot_cfg_crc16(frame,sizeof(frame)));
    assert(ot_cfg_decode(frame,&out));
    assert(out.active_pixels==522 && out.zones[3].pixels==253);
    frame[0]=5;assert(!ot_cfg_decode(frame,&out));frame[0]=1;
    frame[1]=0;frame[2]=0;assert(!ot_cfg_decode(frame,&out));
    frame[1]=522 & 255;frame[2]=522>>8;
    frame[5]=0;assert(!ot_cfg_decode(frame,&out));
    ot_cfg_encode(&a[0],frame);
    frame[37+28*1+25]=110; /* zone total now invalid */
    assert(!ot_cfg_decode(frame,&out));
    printf("BLE config codec validation passed (261-byte payload, CRC, invalid fields, 522 zones).\\n");
    return 0;
}
