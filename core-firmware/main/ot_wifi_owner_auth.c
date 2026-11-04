#include "ot_wifi_owner_auth.h"
#include <string.h>
/* Exact canonical HMAC: ASCII domain/method/path + raw 16-byte nonce.
 * Future endpoint types MUST have distinct domain/path and their own reviews.
 */
static const uint8_t prefix[]="OpenTail-WiFi-v1\nPOST\n/api/v1/owner/stop\n";
void ot_wifi_owner_issue(ot_wifi_owner_challenge_t *state,
                         const uint8_t fresh_nonce[OT_WIFI_OWNER_NONCE_LEN],
                         uint32_t now_ms){
    if(!state||!fresh_nonce)return;
    memcpy(state->nonce,fresh_nonce,OT_WIFI_OWNER_NONCE_LEN);
    state->issued_ms=now_ms;
    state->pending=true;
}
bool ot_wifi_owner_verify_stop(ot_wifi_owner_challenge_t *state,
                               const uint8_t nonce[OT_WIFI_OWNER_NONCE_LEN],
                               const uint8_t mac[OT_WIFI_OWNER_MAC_LEN],
                               uint32_t now_ms,ot_wifi_owner_hmac_t hmac){
    if(!state)return false;
    const bool was_pending=state->pending;
    /* One-use even on failure: no retry of an ambiguous STOP. */
    state->pending=false;
    if(!was_pending || !nonce || !mac || !hmac ||
       (uint32_t)(now_ms-state->issued_ms)>OT_WIFI_OWNER_CHALLENGE_TTL_MS)return false;
    uint8_t mismatch=0;
    for(unsigned i=0;i<OT_WIFI_OWNER_NONCE_LEN;i++)
        mismatch|=(uint8_t)(state->nonce[i]^nonce[i]);
    if(mismatch)return false;
    uint8_t payload[sizeof(prefix)-1+OT_WIFI_OWNER_NONCE_LEN];
    uint8_t expected[OT_WIFI_OWNER_MAC_LEN]={0};
    memcpy(payload,prefix,sizeof(prefix)-1);
    memcpy(payload+sizeof(prefix)-1,nonce,OT_WIFI_OWNER_NONCE_LEN);
    bool ok=hmac(payload,sizeof(payload),expected);
    uint8_t delta=0;
    for(unsigned i=0;i<OT_WIFI_OWNER_MAC_LEN;i++)delta|=(uint8_t)(expected[i]^mac[i]);
    memset(expected,0,sizeof(expected));
    memset(payload,0,sizeof(payload));
    return ok && delta==0;
}
