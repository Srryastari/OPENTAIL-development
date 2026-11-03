#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "ble_fx_command.h"
#include "fx_zone_bank.h"
#include "board_config.h"
#include "effects_1d.h"

/* Tests the actual 20-byte BLE envelope and per-zone CORE dispatch.
 * RGB optical observation, GATT radio timing and phone support need bench proof.
 */
static void test_wire_and_auth(void){
    uint8_t cmd[OT_BLE_FX_TERTIARY_WIRE_BYTES]={
        1,OT_BLE_FX_TERTIARY_CONTINUOUS_OPCODE,54,0,1,1,
        255,0,0, 0,0,255, 32,128,128,0, 1, 0,255,0
    };
    ot_ble_fx_packet_t fx={.effect_id=500u};
    ot_ble_fx_tertiary_t third={.r=7,.g=8,.b=9,.set=false};
    assert(sizeof(cmd)==20u);
    assert(ot_ble_fx_parse_tertiary(cmd,20,false,&fx,&third)==OT_BLE_FX_UNAUTHORIZED);
    assert(fx.effect_id==500u && !third.set);
    assert(ot_ble_fx_parse_tertiary(cmd,20,true,&fx,&third)==OT_BLE_FX_OK);
    assert(fx.effect_id==54u&&fx.chain_mask==1u&&fx.zone_mask==1u);
    assert(fx.ttl_sec==1u&&third.set&&third.r==0&&third.g==255&&third.b==0);
    cmd[17]=0;cmd[18]=0;cmd[19]=0; /* explicit true black */
    assert(ot_ble_fx_parse_tertiary(cmd,20,true,&fx,&third)==OT_BLE_FX_OK);
    assert(third.set&&third.r==0&&third.g==0&&third.b==0);
    cmd[16]=0;
    assert(ot_ble_fx_parse_tertiary(cmd,20,true,&fx,&third)==OT_BLE_FX_BAD_WIRE);
    cmd[16]=2;
    assert(ot_ble_fx_parse_tertiary(cmd,20,true,&fx,&third)==OT_BLE_FX_BAD_WIRE);
    cmd[16]=1;
    assert(ot_ble_fx_parse_tertiary(cmd,19,true,&fx,&third)==OT_BLE_FX_BAD_WIRE);
    cmd[1]=10;
    assert(ot_ble_fx_parse_tertiary(cmd,20,true,&fx,&third)==OT_BLE_FX_BAD_WIRE);
    cmd[1]=11;
    cmd[12]=33;
    assert(ot_ble_fx_parse_tertiary(cmd,20,true,&fx,&third)==OT_BLE_FX_RANGE);
    cmd[12]=32;
    cmd[4]=4;
    assert(ot_ble_fx_parse_tertiary(cmd,20,true,&fx,&third)==OT_BLE_FX_RANGE);
    cmd[4]=1;
    cmd[5]=0;
    assert(ot_ble_fx_parse_tertiary(cmd,20,true,&fx,&third)==OT_BLE_FX_RANGE);
    cmd[5]=1;
    cmd[15]=1; /* unsupported unresolved palette for this source */
    assert(ot_ble_fx_parse_tertiary(cmd,20,true,&fx,&third)==OT_BLE_FX_RANGE);
    cmd[15]=0;
    /* Old command admission remains unchanged, with no third value serialized. */
    uint8_t legacy[OT_BLE_FX_WIRE_BYTES]={1,10,54,0,1,1,255,0,0,0,0,255,32,128,128,0,0};
    assert(ot_ble_fx_parse(legacy,17,true,&fx)==OT_BLE_FX_OK);
    assert(ot_ble_fx_parse(legacy,17,false,&fx)==OT_BLE_FX_UNAUTHORIZED);
    assert(ot_ble_fx_parse(legacy,20,true,&fx)==OT_BLE_FX_BAD_WIRE);
    /* Every Effects #110 source-proven ID is now BLE-authorized, but
     * Aurora ID38 only consumes third RGB with source palette ID4. */
    const uint8_t verified[]={28,29,34,38,50,52,54,55,56,84,87,95,103};
    for(size_t i=0;i<sizeof(verified);i++){
        cmd[2]=verified[i];
        cmd[15]=cmd[2]==38u?4u:0u;
        assert(ot_ble_fx_parse_tertiary(cmd,20,true,&fx,&third)==OT_BLE_FX_OK);
    }
    cmd[2]=38u;cmd[15]=0u;
    fx.effect_id=500u;third.set=false;
    assert(ot_ble_fx_parse_tertiary(cmd,20,true,&fx,&third)==OT_BLE_FX_UNSUPPORTED);
    assert(fx.effect_id==500u && !third.set); /* reject without mutation */
    cmd[15]=50u;
    assert(ot_ble_fx_parse_tertiary(cmd,20,true,&fx,&third)==OT_BLE_FX_UNSUPPORTED);
    cmd[15]=4u;
    assert(ot_ble_fx_parse_tertiary(cmd,20,true,&fx,&third)==OT_BLE_FX_OK);
    const uint8_t unsupported[]={10,11,40,48,60,91};
    for(size_t i=0;i<sizeof(unsupported);i++){
        cmd[2]=unsupported[i];cmd[15]=0u;
        fx.effect_id=500u;third.set=false;
        assert(ot_ble_fx_parse_tertiary(cmd,20,true,&fx,&third)==OT_BLE_FX_UNSUPPORTED);
        assert(fx.effect_id==500u && !third.set);
    }
    cmd[2]=0;cmd[15]=0;
    assert(ot_ble_fx_parse_tertiary(cmd,20,true,&fx,&third)==OT_BLE_FX_UNSUPPORTED);
}
static void test_zone_phase_and_legacy_reset(void){
    ot_fx_zone_bank_t bank={0};
    ot_om_output_t outs[OT_OM_OUTPUTS]={0};
    ot_om_defaults(outs);
    for(unsigned o=0;o<OT_OM_OUTPUTS;o++){
        outs[o].zone_count=1;
        outs[o].active_pixels=30;
        outs[o].zones[0].pixels=30;
        outs[o].zones[0].role=OT_OM_DECORATIVE;
    }
    ot_ble_fx_packet_t fx={.effect_id=54,.chain_mask=1,.zone_mask=1,
         .r=32,.g=0,.b=0,.bg_r=0,.bg_g=0,.bg_b=32,
         .brightness=32,.speed=80,.intensity=128,.palette_id=0,.ttl_sec=1};
    ot_ble_fx_tertiary_t third={.r=0,.g=32,.b=0,.set=true};
    assert(ot_fx_bank_apply_continuous_tertiary(&bank,&fx,outs,&third,100));
    ot_fx_zone_slot_t *a=&bank.zones[0][0];
    assert(a->continuous && a->tertiary.set && a->tertiary.g==32);
    const uint32_t original_gen=a->generation, original_start=a->started_ms;
    fx.speed=200;
    third=(ot_ble_fx_tertiary_t){.r=0,.g=0,.b=0,.set=true};
    assert(ot_fx_bank_apply_continuous_tertiary(&bank,&fx,outs,&third,120));
    assert(a->generation==original_gen && a->started_ms==original_start);
    assert(a->tertiary.set&&a->tertiary.r==0&&a->tertiary.g==0&&a->tertiary.b==0);
    assert(!bank.zones[1][0].configured);
    /* Same effect on the other output: independently owned third channel. */
    fx.chain_mask=2;
    third=(ot_ble_fx_tertiary_t){.r=20,.g=12,.b=9,.set=true};
    assert(ot_fx_bank_apply_continuous_tertiary(&bank,&fx,outs,&third,135));
    assert(bank.zones[1][0].tertiary.r==20);
    assert(!a->configured); /* global continuous target replacement */
    fx.chain_mask=3;
    assert(ot_fx_bank_apply_continuous_tertiary(&bank,&fx,outs,&third,145));
    assert(bank.zones[0][0].tertiary.r==20);
    assert(bank.zones[1][0].tertiary.r==20);
    const uint32_t g0=bank.zones[0][0].generation;
    /* Older 17-byte client must not inherit a previous explicit third slot. */
    assert(ot_fx_bank_apply_continuous(&bank,&fx,outs,150));
    assert(!bank.zones[0][0].tertiary.set);
    assert(!bank.zones[1][0].tertiary.set);
    assert(bank.zones[0][0].generation==g0); /* no phase reset */
    /* Reject phantom zero-length target BEFORE mutation. */
    fx.chain_mask=2;
    outs[1].zones[0].role=OT_OM_OFF;
    assert(!ot_fx_bank_apply_continuous_tertiary(&bank,&fx,outs,&third,180));
    assert(bank.zones[0][0].configured && bank.zones[0][0].generation==g0);
    ot_fx_bank_stop_all(&bank);
    assert(!ot_fx_bank_any_active(&bank,200));
}
int main(void){
    test_wire_and_auth();
    test_zone_phase_and_legacy_reset();
    printf("PASS: authenticated 20-byte tertiary, 17-byte compatibility, 13 proven IDs, 6 unsupported, Aurora palette4 only, true black, isolated zones, continuous phase and STOP\n");
    return 0;
}
