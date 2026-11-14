#include <assert.h>
#include <stdio.h>
#include "ble_owner_policy.h"
int main(void){
    /* Every combination: insecure/unbonded/storage-fault links never enroll. */
    for(unsigned bits=0;bits<128;bits++){
        bool fault=bits&1,owned=bits&2,same=bits&4,encrypted=bits&8;
        bool bonded=bits&16,pin=bits&32,authenticated=bits&64;
        bool allowed=ot_owner_enrollment_allowed(fault,owned,same,encrypted,bonded,pin,authenticated);
        if(fault||!encrypted||!bonded)assert(!allowed);
        else if(!owned||same)assert(allowed);
        else assert(allowed==(pin&&authenticated));
    }
    assert(ot_owner_enrollment_allowed(false,false,false,true,true,false,false));
    /* Once claimed, Skip cannot enroll another phone, even if bonded. */
    assert(!ot_owner_enrollment_allowed(false,true,false,true,true,false,false));
    assert(ot_owner_enrollment_allowed(false,true,true,true,true,false,false));
    /* Set PIN later: remembered owner remains valid; new phone must prove PIN. */
    assert(ot_owner_enrollment_allowed(false,true,true,true,true,true,false));
    assert(!ot_owner_enrollment_allowed(false,true,false,true,true,true,false));
    assert(ot_owner_enrollment_allowed(false,true,false,true,true,true,true));
    assert(!ot_owner_enrollment_allowed(true,false,false,true,true,false,false));
    assert(ot_owner_pin_valid(true,0)&&ot_owner_pin_valid(true,999999));
    assert(!ot_owner_pin_valid(true,1000000));
    puts("PASS owner enrollment: 128 security combinations, fresh claim, reconnect, stranger rejection, PIN-after-Skip and fail-closed storage");
}
