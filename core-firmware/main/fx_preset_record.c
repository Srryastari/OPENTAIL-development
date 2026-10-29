#include "fx_preset_record.h"
#include "board_config.h"
#include "effects_1d.h"
#include "palette_1d.h"
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
   fx->chain_mask!=0&&
   (fx->chain_mask&~((1u<<OT_OM_OUTPUTS)-1u))==0&&fx->zone_mask>=1&&
   fx->brightness<=OT_MAX_BRIGHTNESS&&ot_palette_1d_supported(fx->effect_id,fx->palette_id)&&fx->ttl_sec>=1&&fx->ttl_sec<=60;
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

/* V2 record serializes each field for checksum stability across alignment
 * changes. Every active zone has an independent FX ID, palette and color. */
static uint16_t scene_crc_byte(uint16_t crc,uint8_t byte){
 crc^=(uint16_t)byte<<8;
 for(unsigned b=0;b<8;b++)
   crc=(crc&0x8000u)?(uint16_t)((crc<<1)^0x1021u):(uint16_t)(crc<<1);
 return crc;
}
static uint16_t scene_checksum(const ot_fx_scene_record_t *r){
 uint16_t crc=0xffffu;
 crc=scene_crc_byte(crc,(uint8_t)r->active_mask);
 crc=scene_crc_byte(crc,(uint8_t)(r->active_mask>>8));
 for(unsigned o=0;o<OT_OM_OUTPUTS;o++)for(unsigned z=0;z<OT_OM_ZONES;z++){
  const ot_ble_fx_packet_t *p=&r->effect[o][z];
  const uint8_t bytes[]={(uint8_t)p->effect_id,(uint8_t)(p->effect_id>>8),
   p->chain_mask,p->zone_mask,p->r,p->g,p->b,p->bg_r,p->bg_g,
   p->bg_b,p->brightness,p->speed,p->intensity,p->palette_id,p->ttl_sec};
  for(size_t i=0;i<sizeof(bytes);i++)crc=scene_crc_byte(crc,bytes[i]);
 }
 return crc;
}
bool ot_fx_scene_pack(ot_fx_scene_record_t *rec,
                      const ot_fx_zone_bank_t *bank,uint32_t now_ms){
 if(!rec||!bank)return false;
 memset(rec,0,sizeof(*rec));
 rec->magic=OT_FX_PRESET_MAGIC;rec->version=OT_FX_SCENE_VERSION;
 for(unsigned o=0;o<OT_OM_OUTPUTS;o++)for(unsigned z=0;z<OT_OM_ZONES;z++){
  const ot_fx_zone_slot_t *slot=&bank->zones[o][z];
  if(!ot_fx_slot_active(slot,now_ms))continue;
  ot_ble_fx_packet_t p=slot->request;
  p.chain_mask=(uint8_t)(1u<<o);p.zone_mask=(uint8_t)(1u<<z);
  if(!settings_ok(&p))return false;
  rec->active_mask=(uint16_t)(rec->active_mask | (uint16_t)(1u<<(o*OT_OM_ZONES+z)));
  rec->effect[o][z]=p;
 }
 if(!rec->active_mask)return false;
 rec->crc=scene_checksum(rec);
 return true;
}
bool ot_fx_scene_valid(const ot_fx_scene_record_t *rec){
 if(!rec||rec->magic!=OT_FX_PRESET_MAGIC||
    rec->version!=OT_FX_SCENE_VERSION||!rec->active_mask)return false;
 for(unsigned o=0;o<OT_OM_OUTPUTS;o++)for(unsigned z=0;z<OT_OM_ZONES;z++){
  if(!(rec->active_mask & (uint16_t)(1u<<(o*OT_OM_ZONES+z))))continue;
  const ot_ble_fx_packet_t *p=&rec->effect[o][z];
  if(!settings_ok(p)||p->chain_mask!=(uint8_t)(1u<<o)||
     p->zone_mask!=(uint8_t)(1u<<z))return false;
 }
 return rec->crc==scene_checksum(rec);
}
