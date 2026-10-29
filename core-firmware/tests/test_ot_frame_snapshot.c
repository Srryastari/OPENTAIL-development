#include "ot_frame_snapshot.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
int main(void){
    ot_frame_snapshot_info_t i={0};
    uint8_t data[9]={0},old[9]={0};
    ot_frame_snapshot_reset();
    assert(ot_frame_snapshot_info(&i) && !i.ready);
    assert(!ot_frame_snapshot_request(255));
    assert(ot_frame_snapshot_request(0));
    assert(!ot_frame_snapshot_request(0));
    assert(ot_frame_snapshot_begin(0,3,23));
    assert(!ot_frame_snapshot_request(1));
    assert(i.ready==false);
    ot_frame_snapshot_pixel(0,255,0,7);
    ot_frame_snapshot_pixel(1,0,0,0);
    ot_frame_snapshot_pixel(2,4,5,6);
    assert(!ot_frame_snapshot_chunk(0,23,0,3,data));
    ot_frame_snapshot_complete(true);
    assert(ot_frame_snapshot_info(&i) && i.ready && !i.pending);
    assert(i.output==0 && i.count==3 && i.sequence==23);
    assert(ot_frame_snapshot_chunk(0,23,0,3,data));
    const uint8_t expected[]={255,0,7,0,0,0,4,5,6};
    assert(memcmp(data,expected,sizeof expected)==0);
    assert(!ot_frame_snapshot_chunk(1,23,0,3,data));
    assert(!ot_frame_snapshot_chunk(0,22,0,3,data));
    assert(!ot_frame_snapshot_chunk(0,23,2,2,data));
    memcpy(old,data,sizeof old);
    assert(ot_frame_snapshot_request(1));
    assert(!ot_frame_snapshot_chunk(0,23,0,3,data)); /* stale invalidated */
    assert(ot_frame_snapshot_begin(1,3,24));
    ot_frame_snapshot_pixel(0,1,2,3);
    ot_frame_snapshot_complete(false);
    assert(ot_frame_snapshot_info(&i) && !i.ready && !i.pending);
    assert(!ot_frame_snapshot_chunk(1,24,0,1,data));
    assert(ot_frame_snapshot_request(1));
    assert(ot_frame_snapshot_begin(1,1,25));
    ot_frame_snapshot_pixel(0,8,9,10);
    ot_frame_snapshot_reset();  /* disconnect while rendering */
    ot_frame_snapshot_complete(true);
    assert(ot_frame_snapshot_info(&i) && !i.ready);
    assert(!ot_frame_snapshot_chunk(1,25,0,1,data));
    assert(ot_frame_snapshot_request(1));
    assert(ot_frame_snapshot_begin(1,1,26));
    ot_frame_snapshot_pixel(0,10,20,30);
    ot_frame_snapshot_complete(true);
    assert(ot_frame_snapshot_chunk(1,26,0,1,data));
    assert(data[0]==10 && data[1]==20 && data[2]==30);
    puts("PASS: authenticated-session snapshot state, immutable RGB888/sequence, stale rejection");
    return 0;
}
