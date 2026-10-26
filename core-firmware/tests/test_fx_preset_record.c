#include <assert.h>
#include "fx_preset_record.h"
int main(void){
 ot_ble_fx_packet_t input={0};
 input.effect_id=32768;input.chain_mask=3;input.zone_mask=255;
 input.r=255;input.g=40;input.b=5;
 input.brightness=20;input.speed=100;input.intensity=220;
 input.palette_id=0;input.ttl_sec=60;
 ot_fx_preset_record_t record;
 assert(ot_fx_preset_pack(&record,&input));
 assert(ot_fx_preset_valid(&record));
 assert(record.fx.effect_id==32768);
 input.palette_id=4;assert(ot_fx_preset_pack(&record,&input));
 assert(ot_fx_preset_valid(&record));
 record.fx.intensity^=1;
 assert(!ot_fx_preset_valid(&record));
 record.fx.intensity^=1;
 record.version++;
 assert(!ot_fx_preset_valid(&record));
 record.version--;
 input.palette_id=10;
 assert(!ot_fx_preset_pack(&record,&input));
 input.palette_id=0;input.effect_id=999;
 assert(!ot_fx_preset_pack(&record,&input));
 return 0;
}
