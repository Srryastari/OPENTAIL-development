#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ble_config.h"
int main(void){
    ot_om_output_t a[OT_OM_OUTPUTS],out={0};ot_om_defaults(a);
    uint8_t frame[OT_CFG_WIRE_BYTES];
    ot_cfg_encode(&a[0],frame);
    assert(frame[0]==1 && frame[1]==1 && frame[2]==0);
    assert(ot_cfg_decode(frame,&out));
    /* Fresh universal profile uses the GRB default; BRG remains a valid user selection. */
    assert(out.active_pixels==1 && out.color_order==OT_OM_GRB);
    assert(!strcmp(out.name,a[0].name));
    assert(out.zone_count==a[0].zone_count);
    for(int z=0;z<out.zone_count;z++){
        assert(out.zones[z].pixels==a[0].zones[z].pixels);
        assert(out.zones[z].role==a[0].zones[z].role);
    }
    /* Arbitrary user-created profile with unequal zones; >255 tests 16-bit counts. */
    a[0].active_pixels=417;a[0].zone_count=5;
    const unsigned lengths[]={77,51,209,80,0};
    const uint8_t roles[]={OT_OM_DECORATIVE,OT_OM_AUX2,OT_OM_REVERSE,OT_OM_PARK_BRAKE,OT_OM_OFF};
    const char *names[]={"Accent","Aux Segment","Zone C","Zone D","Spare"};
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
    assert(out.active_pixels==417 && out.zones[3].pixels==80);
    frame[0]=5;assert(!ot_cfg_decode(frame,&out));frame[0]=1;
    frame[1]=0;frame[2]=0;assert(!ot_cfg_decode(frame,&out));
    frame[1]=417 & 255;frame[2]=417>>8;
    frame[5]=0;assert(!ot_cfg_decode(frame,&out));
    ot_cfg_encode(&a[0],frame);
    frame[37+28*1+25]=52; /* zone total now invalid */
    assert(!ot_cfg_decode(frame,&out));
    printf("BLE config codec validation passed (261-byte payload, CRC, invalid fields, generic 417-pixel profile).\\n");
    return 0;
}
