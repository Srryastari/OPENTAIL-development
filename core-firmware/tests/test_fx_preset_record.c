#include <assert.h>
#include "fx_preset_record.h"
#include "board_config.h"
int main(void){
 /* Prior saved v1 FX blobs and v2 scenes must stay binary compatible.
  * Spark accent fields live in volatile per-zone state, not these records. */
 assert(sizeof(ot_ble_fx_packet_t)==16u);
 assert(sizeof(ot_fx_preset_record_t)==24u);
 assert(sizeof(ot_fx_scene_record_t)==268u);
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
 input.chain_mask=(uint8_t)(1u<<OT_OM_OUTPUTS);
 assert(!ot_fx_preset_pack(&record,&input));
 input.chain_mask=3;
 input.brightness=255u;
 assert(ot_fx_preset_pack(&record,&input));
 assert(ot_fx_preset_valid(&record));
 input.brightness=20;
 input.palette_id=10;
 assert(!ot_fx_preset_pack(&record,&input));
 input.palette_id=0;input.effect_id=999;
 assert(!ot_fx_preset_pack(&record,&input));
 /* A scene snapshot captures two distinct active effects without mixing
  * left/right zone mapping. Standalone scenes do not require a phone. */
 ot_fx_zone_bank_t bank;
 ot_fx_bank_reset(&bank);
 ot_fx_scene_output_identity_t panel_outputs[OT_OM_OUTPUTS]={0};
 panel_outputs[0].zone_count=1u;
 panel_outputs[1].zone_count=3u;
 ot_ble_fx_packet_t left={0},right={0};
 left.effect_id=3;left.chain_mask=1;left.zone_mask=1;
 left.r=255;left.brightness=20;left.ttl_sec=60;
 right.effect_id=32768;right.chain_mask=2;right.zone_mask=4;
 right.g=40;right.b=3;right.palette_id=4;right.brightness=16;right.ttl_sec=30;
 assert(ot_fx_bank_apply(&bank,&left,100,false));
 assert(ot_fx_bank_apply(&bank,&right,200,true));
 ot_fx_scene_record_t scene;
 assert(ot_fx_scene_pack(&scene,&bank,panel_outputs,300));
 assert(ot_fx_scene_valid(&scene));
 assert(scene.version==OT_FX_SCENE_VERSION);
 assert(scene.active_mask==((uint16_t)1u|((uint16_t)1u<<10)));
 assert(scene.effect[0][0].effect_id==3);
 assert(scene.effect[0][0].chain_mask==1&&scene.effect[0][0].zone_mask==1);
 assert(scene.effect[1][2].effect_id==32768);
 assert(scene.effect[1][2].chain_mask==2&&scene.effect[1][2].zone_mask==4);
 assert(scene.effect[1][2].palette_id==4);
 scene.effect[1][2].speed^=1;
 assert(!ot_fx_scene_valid(&scene));
 scene.effect[1][2].speed^=1;
 assert(ot_fx_scene_valid(&scene));
 scene.effect[0][0].chain_mask=2;
 assert(!ot_fx_scene_valid(&scene));
 scene.effect[0][0].chain_mask=1;
 assert(ot_fx_scene_valid(&scene));
 scene.version=1;assert(!ot_fx_scene_valid(&scene));
 scene.version=OT_FX_SCENE_VERSION;
 assert(ot_fx_scene_valid(&scene));
 ot_fx_bank_stop_temporary(&bank);
 assert(ot_fx_scene_pack(&scene,&bank,panel_outputs,500));
 assert(scene.active_mask==((uint16_t)1u<<10));
 assert(ot_fx_scene_valid(&scene));
 ot_fx_bank_stop_all(&bank);
 assert(!ot_fx_scene_pack(&scene,&bank,panel_outputs,500));

 /* Identity-owned bank slots can be allocated in a different order from the
  * current logical zones. Saving must project owners back to current zone
  * indices, not serialize transient bank-slot indices. */
 ot_om_output_t outputs[OT_OM_OUTPUTS]={0};
 ot_pz_output_t live_panels[OT_OM_OUTPUTS]={0};
 ot_fx_scene_output_identity_t scene_identities[OT_OM_OUTPUTS]={0};
 outputs[0].zone_count=2u;outputs[0].active_pixels=2u;
 outputs[0].zones[0].pixels=1u;outputs[0].zones[0].role=OT_OM_LEFT;
 outputs[0].zones[1].pixels=1u;outputs[0].zones[1].role=OT_OM_BRAKE;
 live_panels[0].zone_count=2u;
 live_panels[0].zones[0].panel_id=100u;live_panels[0].zones[0].id=200u;
 live_panels[0].zones[1].panel_id=100u;live_panels[0].zones[1].id=201u;
 scene_identities[0].zone_count=2u;
 scene_identities[0].zones[0].panel_id=100u;scene_identities[0].zones[0].zone_id=200u;
 scene_identities[0].zones[1].panel_id=100u;scene_identities[0].zones[1].zone_id=201u;
 ot_fx_zone_bank_t identity_bank;
 ot_fx_bank_reset(&identity_bank);
 ot_ble_fx_packet_t second_zone={0};
 second_zone.effect_id=32768u;second_zone.chain_mask=1u;second_zone.zone_mask=2u;
 second_zone.g=40u;second_zone.b=3u;second_zone.palette_id=4u;
 second_zone.brightness=255u;second_zone.ttl_sec=60u;
 assert(ot_fx_bank_apply_configured_identity(&identity_bank,&second_zone,
     outputs,live_panels,100u,false));
 assert(identity_bank.zones[0][0].owner_zone_id==201u);
 ot_ble_fx_packet_t first_zone={0};
 first_zone.effect_id=3u;first_zone.chain_mask=1u;first_zone.zone_mask=1u;
 first_zone.r=255u;first_zone.brightness=20u;first_zone.ttl_sec=60u;
 assert(ot_fx_bank_apply_configured_identity(&identity_bank,&first_zone,
     outputs,live_panels,200u,false));
 assert(identity_bank.zones[0][0].owner_zone_id==201u);
 assert(identity_bank.zones[0][1].owner_zone_id==200u);
 assert(ot_fx_scene_pack(&scene,&identity_bank,scene_identities,300u));
 assert(ot_fx_scene_valid(&scene));
 assert(scene.active_mask==3u);
 assert(scene.effect[0][0].effect_id==3u);
 assert(scene.effect[0][1].effect_id==32768u);
 assert(scene.effect[0][0].zone_mask==1u);
 assert(scene.effect[0][1].zone_mask==2u);
 assert(scene.effect[0][1].brightness==255u);
 return 0;
}
