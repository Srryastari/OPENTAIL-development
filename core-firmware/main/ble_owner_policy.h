#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
/* Skip authorizes only the first encrypted bonded peer of a fresh controller.
 * Existing-key migration is completed by proof of that key, never by a stranger.
 * A configured SMP passkey admits additional phones only after MITM-authenticated
 * bonding. Remembered owner keys remain valid after a PIN is set. */
static inline bool ot_owner_enrollment_allowed(bool storage_fault,bool owned,
        bool same_peer,bool encrypted,bool bonded,bool pin_enabled,bool authenticated){
    return !storage_fault&&encrypted&&bonded&&
        (!owned||same_peer||(pin_enabled&&authenticated));
}
static inline bool ot_owner_pin_valid(bool enabled,unsigned pin){
    return !enabled||pin<=999999u;
}

/* NimBLE public/random identity aliases (2/3) describe the same identity as
 * public/random (0/1). Never compare an OTA RPA, or mask arbitrary types. */
static inline bool ot_owner_peer_same(uint8_t a,const uint8_t av[6],uint8_t b,const uint8_t bv[6]){
    return a<=3 && b<=3 && (a&1u)==(b&1u) && memcmp(av,bv,6)==0;
}
