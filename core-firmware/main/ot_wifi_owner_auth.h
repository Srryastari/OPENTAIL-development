#pragma once
/* Volatile, one-use authenticated Wi-Fi STOP admission; no dynamic allocation,
 * flash writes or access to effect/physical-output state. This is NOT a
 * general HTTP owner session. The nonce is consumed even after bad proof.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
enum { OT_WIFI_OWNER_NONCE_LEN=16, OT_WIFI_OWNER_MAC_LEN=32,
       OT_WIFI_OWNER_CHALLENGE_TTL_MS=8000 };
typedef struct {
    uint8_t nonce[OT_WIFI_OWNER_NONCE_LEN];
    uint32_t issued_ms;
    bool pending;
} ot_wifi_owner_challenge_t;
/* Implemented by the existing BLE owner-key store. Owner secret never leaves CORE. */
typedef bool (*ot_wifi_owner_hmac_t)(const uint8_t *data,size_t len,
                                    uint8_t out[OT_WIFI_OWNER_MAC_LEN]);
/* Existing BLE owner credential remains the one source of truth. */
bool ot_ble_owner_wifi_ready(void);
bool ot_ble_owner_wifi_hmac(const uint8_t *data,size_t len,
                            uint8_t out[OT_WIFI_OWNER_MAC_LEN]);
void ot_wifi_owner_issue(ot_wifi_owner_challenge_t *state,
                         const uint8_t fresh_nonce[OT_WIFI_OWNER_NONCE_LEN],
                         uint32_t now_ms);
bool ot_wifi_owner_verify_stop(ot_wifi_owner_challenge_t *state,
                               const uint8_t nonce[OT_WIFI_OWNER_NONCE_LEN],
                               const uint8_t mac[OT_WIFI_OWNER_MAC_LEN],
                               uint32_t now_ms,ot_wifi_owner_hmac_t hmac);
