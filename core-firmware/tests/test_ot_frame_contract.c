#include "ot_frame_contract.h"
#include <assert.h>
#include <stdio.h>

int main(void){
    const bool none[2]={false,false};
    const bool partial[2]={true,false};
    const bool full[2]={true,true};
    const bool single[1]={true};
    const bool future[5]={true,true,true,true,true};
    assert(!ot_frame_all_submitted(NULL,2));
    assert(!ot_frame_all_submitted(full,0));
    assert(!ot_frame_all_submitted(none,2));
    assert(!ot_frame_all_submitted(partial,2));
    assert(ot_frame_all_submitted(full,2));
    assert(ot_frame_all_submitted(single,1));
    assert(ot_frame_all_submitted(future,5));
    puts("CORE frame completion: no partial success and no fixed vehicle output count");
    return 0;
}
