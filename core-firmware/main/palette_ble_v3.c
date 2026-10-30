#include "palette_ble_v3.h"
#include "palette_transfer.h"
#include "palette_runtime.h"
#include <string.h>
extern bool ot_ble_palette_in_use(uint8_t index);
static ot_palette_transfer_t upload;
static uint8_t response[OT_PALETTE_REPLY_BYTES]={OT_PALETTE_WIRE_VERSION};
static uint32_t read32(const uint8_t *p){
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|
           ((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
void ot_palette_ble_session_reset(void){
    ot_palette_transfer_reset(&upload);
    memset(response,0,sizeof(response));
    response[0]=OT_PALETTE_WIRE_VERSION;
}
void ot_palette_ble_reply(uint8_t out[OT_PALETTE_REPLY_BYTES]){
    if(out)memcpy(out,response,sizeof(response));
}
static void ack(uint8_t op,uint8_t index,uint8_t status){
    ot_palette_entry_t e={0};
    if(index<OT_PALETTE_SLOTS)(void)ot_palette_runtime_get(index,&e);
    memset(response,0,sizeof(response));
    response[0]=OT_PALETTE_WIRE_VERSION;
    response[1]=(uint8_t)(op|0x80u);
    response[2]=index;response[3]=status;
    for(unsigned i=0;i<4;i++)response[4+i]=(uint8_t)(e.revision>>(i*8u));
    response[8]=e.count;response[9]=(uint8_t)e.stops_crc;
    response[10]=(uint8_t)(e.stops_crc>>8);response[11]=e.present?1u:0u;
}
void ot_palette_ble_process(const uint8_t *p,size_t n,uint32_t now){
    if(!p||n<2u||n>OT_PALETTE_WIRE_MAX||p[0]!=OT_PALETTE_WIRE_VERSION){
        ot_palette_transfer_reset(&upload);ack(0x7fu,255u,1u);return;
    }
    const uint8_t op=p[1];uint8_t index=n>=3u?p[2]:255u,status=1u;
    switch(op){
      case 1: { /* BEGIN [v,1,index,count,base_revisionLE32,stops_crcLE16] */
        ot_palette_transfer_reset(&upload);
        if(n!=10u||index>=OT_PALETTE_SLOTS)break;
        ot_palette_entry_t e={0};
        if(!ot_palette_runtime_get(index,&e))break;
        const uint32_t revision=read32(p+4);
        if(revision!=e.revision){status=3u;break;}
        status=ot_palette_transfer_begin(&upload,index,p[3],revision,
               (uint16_t)p[8]|((uint16_t)p[9]<<8),now)?0u:1u;
        break;
      }
      case 2: { /* DATA [v,2,offsetLE16,data1..15] */
        index=upload.active?upload.index:255u;
        if(n<5u)break;
        const uint16_t offset=(uint16_t)p[2]|((uint16_t)p[3]<<8);
        status=ot_palette_transfer_chunk(&upload,offset,p+4,n-4u,now)?0u:1u;
        break;
      }
      case 3: { /* COMMIT [v,3,index], durable NVS first */
        if(n!=3u)break;
        ot_palette_record_t rec={0};
        if(!ot_palette_transfer_finish(&upload,index,now,&rec))break;
        status=ot_palette_runtime_put(&rec,rec.revision-1u)?0u:4u;
        break;
      }
      case 4: { /* DELETE [v,4,index,base_revisionLE32] */
        ot_palette_transfer_reset(&upload);
        if(n!=7u||index>=OT_PALETTE_SLOTS)break;
        if(ot_ble_palette_in_use(index)){status=2u;break;}
        ot_palette_entry_t e={0};
        if(!ot_palette_runtime_get(index,&e))break;
        if(e.revision!=read32(p+3)){status=3u;break;}
        status=ot_palette_runtime_delete(index,e.revision)?0u:4u;
        break;
      }
      case 5: { /* QUERY [v,5,index] */
        if(n==3u&&index<OT_PALETTE_SLOTS)status=0u;
        break;
      }
      case 6: { /* ABORT [v,6] */
        if(n!=2u)break;
        ot_palette_transfer_reset(&upload);index=255u;status=0u;
        break;
      }
      default:break;
    }
    ack(op,index,status);
}
