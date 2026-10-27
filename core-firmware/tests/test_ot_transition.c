#include <assert.h>
#include "ot_transition.h"
int main(void){
    const ot_transition_rgb_t red={255,20,10};
    const ot_transition_rgb_t black={0,0,0};
    assert(ot_transition_progress(0,450,OT_TRANSITION_SMOOTH)==0);
    assert(ot_transition_progress(450,450,OT_TRANSITION_SMOOTH)==1000);
    assert(ot_transition_progress(999,0,OT_TRANSITION_SMOOTH)==1000);
    assert(ot_transition_progress(449,450,OT_TRANSITION_SNAP)==0);
    assert(ot_transition_progress(450,450,OT_TRANSITION_SNAP)==1000);
    assert(ot_transition_progress(225,450,OT_TRANSITION_SMOOTH)==500);
    ot_transition_rgb_t mid=ot_transition_off(red,225,450,OT_TRANSITION_SMOOTH);
    assert(mid.r>0&&mid.r<255);
    ot_transition_rgb_t end=ot_transition_off(red,450,450,OT_TRANSITION_SMOOTH);
    assert(end.r==0&&end.g==0&&end.b==0);
    end=ot_transition_off(red,451,450,OT_TRANSITION_SNAP);
    assert(end.r==0&&end.g==0&&end.b==0);
    end=ot_transition_off(red,0,0,OT_TRANSITION_SMOOTH);
    assert(end.r==0&&end.g==0&&end.b==0);
    end=ot_transition_blend(black,red,1000);
    assert(end.r==255&&end.g==20&&end.b==10);
    return 0;
}
