/* Bench-only BLE FX wire adapter: single WLED-compatible effect registry.
 * This parses parameters, but never renders or writes to GPIO itself. */
#include "ble_fx_command.h"
#include "board_config.h"
#include "effects_1d.h"
#include "palette_1d.h"
ot_ble_fx_result_t ot_ble_fx_parse(const uint8_t *p,size_t n,bool authorized,
                                   ot_ble_fx_packet_t *out){
  if(!p||!out||n!=OT_BLE_FX_WIRE_BYTES||p[0]!=1u||(p[1]!=OT_BLE_FX_OPCODE && p[1]!=OT_BLE_FX_CONTINUOUS_OPCODE))
    return OT_BLE_FX_BAD_WIRE;
  if(!authorized)return OT_BLE_FX_UNAUTHORIZED;
  const bool continuous=p[1]==OT_BLE_FX_CONTINUOUS_OPCODE;
  uint16_t id=(uint16_t)p[2]|((uint16_t)p[3]<<8);
  ot_wled_fx_metadata_t meta;
  if(!ot_effects_1d_metadata(id,&meta))return OT_BLE_FX_UNSUPPORTED;
  if(p[4]==0||(p[4]&~((1u<<OT_OM_OUTPUTS)-1u))!=0||p[5]<1||p[12]>OT_MAX_BRIGHTNESS||!ot_palette_1d_supported(id,p[15])||(continuous?p[16]!=0u:(p[16]<1||p[16]>60)))
    return OT_BLE_FX_RANGE;
  *out=(ot_ble_fx_packet_t){
    .effect_id=id,.chain_mask=p[4],.zone_mask=p[5],
    .r=p[6],.g=p[7],.b=p[8],.bg_r=p[9],.bg_g=p[10],.bg_b=p[11],
    .brightness=p[12],.speed=p[13],.intensity=p[14],
    .palette_id=p[15],.ttl_sec=continuous?1u:p[16]
  };
  return OT_BLE_FX_OK;
}

/* Effects #110 is the source authority for the 13 proven third-color
 * consumers. Aurora is admitted only with its source-authentic RGB3 palette 4. */
static bool third_color_source_verified(uint16_t id){
    return ot_wled_fx_tertiary_supported(id);
}
ot_ble_fx_result_t ot_ble_fx_parse_tertiary(const uint8_t *p,size_t n,
    bool authorized,ot_ble_fx_packet_t *out,ot_ble_fx_tertiary_t *third){
    if(!p||!out||!third||n!=OT_BLE_FX_TERTIARY_WIRE_BYTES||
       p[0]!=1u||p[1]!=OT_BLE_FX_TERTIARY_CONTINUOUS_OPCODE||
       p[16]!=1u)return OT_BLE_FX_BAD_WIRE;
    if(!authorized)return OT_BLE_FX_UNAUTHORIZED;
    /* Reuse the established 17-byte admission, including palette,
     * output-zone masks, brightness and continuous lifetime rules.
     * Failed parsing never mutates either caller-owned output. */
    uint8_t legacy[OT_BLE_FX_WIRE_BYTES]={0};
    for(unsigned i=0;i<16u;i++)legacy[i]=p[i];
    legacy[1]=OT_BLE_FX_CONTINUOUS_OPCODE;legacy[16]=0u;
    ot_ble_fx_packet_t candidate={0};
    const ot_ble_fx_result_t rc=ot_ble_fx_parse(legacy,sizeof(legacy),true,&candidate);
    if(rc!=OT_BLE_FX_OK)return rc;
    ot_wled_fx_color_role_t roles[3]={0};
    if(!third_color_source_verified(candidate.effect_id)||
       (candidate.effect_id==38u && candidate.palette_id!=4u)||
       !ot_effects_1d_color_roles(candidate.effect_id,roles)||
       !roles[2].enabled)return OT_BLE_FX_UNSUPPORTED;
    *out=candidate;
    *third=(ot_ble_fx_tertiary_t){.r=p[17],.g=p[18],.b=p[19],.set=true};
    return OT_BLE_FX_OK;
}
