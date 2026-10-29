/* Bench-only BLE FX wire adapter: single WLED-compatible effect registry.
 * This parses parameters, but never renders or writes to GPIO itself. */
#include "ble_fx_command.h"
#include "effects_1d.h"
#include "palette_1d.h"
ot_ble_fx_result_t ot_ble_fx_parse(const uint8_t *p,size_t n,bool authorized,
                                   ot_ble_fx_packet_t *out){
  if(!p||!out||n!=OT_BLE_FX_WIRE_BYTES||p[0]!=1u||p[1]!=OT_BLE_FX_OPCODE)
    return OT_BLE_FX_BAD_WIRE;
  if(!authorized)return OT_BLE_FX_UNAUTHORIZED;
  uint16_t id=(uint16_t)p[2]|((uint16_t)p[3]<<8);
  ot_wled_fx_metadata_t meta;
  if(!ot_effects_1d_metadata(id,&meta))return OT_BLE_FX_UNSUPPORTED;
  if(p[4]<1||p[4]>3||p[5]<1||p[12]>32||!ot_palette_1d_supported(id,p[15])||p[16]<1||p[16]>60)
    return OT_BLE_FX_RANGE;
  *out=(ot_ble_fx_packet_t){
    .effect_id=id,.chain_mask=p[4],.zone_mask=p[5],
    .r=p[6],.g=p[7],.b=p[8],.bg_r=p[9],.bg_g=p[10],.bg_b=p[11],
    .brightness=p[12],.speed=p[13],.intensity=p[14],
    .palette_id=p[15],.ttl_sec=p[16]
  };
  return OT_BLE_FX_OK;
}
