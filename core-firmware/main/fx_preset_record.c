#include "fx_preset_record.h"
#include "effects_1d.h"
#include <string.h>
static uint16_t checksum(const ot_ble_fx_packet_t *fx){
 const uint8_t *data=(const uint8_t *)fx;
 uint16_t crc=0xffffu;
 for(size_t i=0;i<sizeof(*fx);i++){
   crc^=(uint16_t)data[i]<<8;
   for(unsigned b=0;b<8;b++)
     crc=crc&0x8000u?(uint16_t)((crc<<1)^0x1021u):(uint16_t)(crc<<1);
 }
 return crc;
}
static bool settings_ok(const ot_ble_fx_packet_t *fx){
 ot_wled_fx_metadata_t info;
 return fx&&ot_effects_1d_metadata(fx->effect_id,&info)&&
   fx->chain_mask>=1&&fx->chain_mask<=3&&fx->zone_mask>=1&&
   fx->brightness<=32&&fx->palette_id==0&&fx->ttl_sec>=1&&fx->ttl_sec<=60;
}
bool ot_fx_preset_pack(ot_fx_preset_record_t *rec,const ot_ble_fx_packet_t *fx){
 if(!rec||!settings_ok(fx))return false;
 memset(rec,0,sizeof(*rec));
 rec->magic=OT_FX_PRESET_MAGIC;rec->version=OT_FX_PRESET_VERSION;
 rec->fx=*fx;rec->crc=checksum(&rec->fx);
 return true;
}
bool ot_fx_preset_valid(const ot_fx_preset_record_t *rec){
 return rec&&rec->magic==OT_FX_PRESET_MAGIC&&
   rec->version==OT_FX_PRESET_VERSION&&
   settings_ok(&rec->fx)&&rec->crc==checksum(&rec->fx);
}
