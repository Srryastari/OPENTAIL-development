#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ot_wifi_owner_auth.h"
static unsigned calls;
static bool fake_owner_hmac(const uint8_t *p,size_t n,uint8_t out[32]){
    calls++;
    assert(n==sizeof("OpenTail-WiFi-v1\nPOST\n/api/v1/owner/stop\n")-1+16);
    for(unsigned i=0;i<32;i++)out[i]=(uint8_t)(p[i%n]^(uint8_t)i);
    return true;
}
static void expected_mac(const uint8_t n[16],uint8_t mac[32]){
    static const uint8_t prefix[]="OpenTail-WiFi-v1\nPOST\n/api/v1/owner/stop\n";
    uint8_t bytes[sizeof(prefix)-1+16];
    memcpy(bytes,prefix,sizeof(prefix)-1);
    memcpy(bytes+sizeof(prefix)-1,n,16);
    fake_owner_hmac(bytes,sizeof(bytes),mac);
}
int main(void){
    ot_wifi_owner_challenge_t state={0};uint8_t nonce[16],mac[32],wrong[16];
    for(int i=0;i<16;i++)nonce[i]=(uint8_t)i;
    expected_mac(nonce,mac);
    assert(!ot_wifi_owner_verify_stop(&state,nonce,mac,0,fake_owner_hmac));
    ot_wifi_owner_issue(&state,nonce,100u);
    assert(ot_wifi_owner_verify_stop(&state,nonce,mac,7999u,fake_owner_hmac));
    assert(!ot_wifi_owner_verify_stop(&state,nonce,mac,8000u,fake_owner_hmac));
    ot_wifi_owner_issue(&state,nonce,100u);
    memcpy(wrong,nonce,16);wrong[7]^=0x80;
    assert(!ot_wifi_owner_verify_stop(&state,wrong,mac,101u,fake_owner_hmac));
    assert(!ot_wifi_owner_verify_stop(&state,nonce,mac,101u,fake_owner_hmac));
    ot_wifi_owner_issue(&state,nonce,100u);
    mac[31]^=1u;
    assert(!ot_wifi_owner_verify_stop(&state,nonce,mac,101u,fake_owner_hmac));
    mac[31]^=1u;
    assert(!ot_wifi_owner_verify_stop(&state,nonce,mac,102u,fake_owner_hmac));
    ot_wifi_owner_issue(&state,nonce,100u);
    assert(!ot_wifi_owner_verify_stop(&state,nonce,mac,8101u,fake_owner_hmac));
    ot_wifi_owner_issue(&state,nonce,100u);
    assert(!ot_wifi_owner_verify_stop(&state,nonce,mac,101u,NULL));
    assert(!ot_wifi_owner_verify_stop(&state,nonce,mac,102u,fake_owner_hmac));
    assert(calls>=3u);
    puts("PASS owner-HMAC STOP scope, one-use nonce, stale deadline, mismatch and negative paths");
    return 0;
}
