#pragma once
#include <stdbool.h>
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
