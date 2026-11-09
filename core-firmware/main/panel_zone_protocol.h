#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "panel_zone.h"

#define OT_PZ_TX_MAGIC 0xfeu
#define OT_PZ_TX_OP_BEGIN 0x01u
#define OT_PZ_TX_OP_UPDATE 0x02u
#define OT_PZ_TX_OP_COMMIT 0x03u
#define OT_PZ_TX_OP_CANCEL 0x04u
#define OT_PZ_TX_OP_HEARTBEAT 0x05u
#define OT_PZ_TX_UPDATE_HEADER_BYTES 11u
#define OT_PZ_TX_DESCRIPTOR_BYTES 8u
#define OT_PZ_TX_MAX_DESCRIPTORS 4u
#define OT_PZ_TX_MIN_ATT_MTU 23u

typedef struct {
    uint8_t lane;
    uint32_t session_id;
    uint32_t sequence;
    uint32_t packet_crc32;
    size_t descriptor_count;
    ot_pz_range_update_t descriptors[OT_PZ_TX_MAX_DESCRIPTORS];
} ot_pz_tx_update_packet_t;
typedef struct {
    uint8_t opcode;
    uint8_t lane;
    uint8_t schema_version;
    uint16_t base_revision;
    uint16_t pixel_capacity;
    uint32_t session_id;
    uint32_t candidate_crc32;
} ot_pz_tx_control_packet_t;

typedef enum {
    OT_PZ_TX_SEQUENCE_NEW=0,
    OT_PZ_TX_SEQUENCE_DUPLICATE,
    OT_PZ_TX_SEQUENCE_STALE,
    OT_PZ_TX_SEQUENCE_CONFLICT
} ot_pz_tx_sequence_result_t;

/* Control packets: BEGIN is 8 bytes, COMMIT 11, CANCEL/HEARTBEAT 7.
 * UPDATE is intentionally parsed separately on the no-response characteristic. */
bool ot_pz_tx_parse_control(const uint8_t *packet,size_t length,
                            ot_pz_tx_control_packet_t *out);

/* UPDATE is [FE,02,lane,session_id:u32LE,sequence:u32LE,1..4 descriptors].
 * Descriptor = panel:u8,zone:u8,range_slot:u8,start:u16LE,count:u16LE,role_flags:u8.
 * Payload must fit one ATT write command (ATT_MTU - 3). */
bool ot_pz_tx_parse_update(const uint8_t *packet,size_t length,uint16_t att_mtu,
                           ot_pz_tx_update_packet_t *out);
ot_pz_tx_sequence_result_t ot_pz_tx_sequence_check(bool has_last,
    uint32_t last_sequence,uint32_t last_packet_crc32,
    uint32_t sequence,uint32_t packet_crc32);
