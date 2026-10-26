/* Bench-only BLE FX wire adapter: single WLED-compatible effect registry.
 * This parses parameters, but never renders or writes to GPIO itself. */
#include "ble_fx_command.h"
ot_ble_fx_result_t ot_ble_fx_parse(const uint8_t *p,size_t n,bool authorized,
                                   ot_ble_fx_packet_t *out){
  if(!p||!out||n!=OT_BLE_FX_WIRE_BYTES||p[0]!=1u||p[1]!=OT_BLE_FX_OPCODE)
    return OT_BLE_FX_BAD_WIRE;
  if(!authorized)return OT_BLE_FX_UNAUTHORIZED;
  uint16_t id=(uint16_t)p[2]|((uint16_t)p[3]<<8);
  ot_wled_fx_metadata_t meta;
  if(!ot_wled_fx_metadata(id,&meta))return OT_BLE_FX_UNSUPPORTED;
  if(p[4]<1||p[4]>3||p[5]<1||p[12]>32||p[15]!=0||p[16]<1||p[16]>60)
    return OT_BLE_FX_RANGE;
  *out=(ot_ble_fx_packet_t){id,p[4],p[5],p[6],p[7],p[8],p[9],p[10],p[11],
                              p[12],p[13],p[14],p[15],p[16]};
  return OT_BLE_FX_OK;
}
