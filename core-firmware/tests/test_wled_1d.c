#include <assert.h>
#include <stdio.h>
#include "wled_1d.h"
int main(void){
    assert(ot_wled_1d_count()==120u);
    assert(ot_wled_1d_by_id(0u)!=0);
    assert(ot_wled_1d_by_key("static")==ot_wled_1d_by_id(0u));
    assert(ot_wled_1d_by_key("not_in_catalog")==0);
    assert(ot_wled_1d_by_id(9999u)==0);
    ot_rgb_t fg={255,45,10},bg={3,4,5},accent={0,0,255};
    size_t changed=0;
    for(size_t i=0;i<ot_wled_1d_count();i++){
        const ot_wled_1d_meta_t *fx=ot_wled_1d_at(i);
        assert(fx&&fx->name&&fx->name[0]&&fx->key&&fx->key[0]);
        assert(ot_wled_1d_by_id(fx->id)==fx);
        assert(ot_wled_1d_by_key(fx->key)==fx);
        for(unsigned t=0;t<3000;t+=129){
            for(unsigned px=0;px<30;px++){
                ot_rgb_t a=ot_wled_1d_pixel(fx->id,px,30,t,85,210,fg,bg,accent);
                ot_rgb_t b=ot_wled_1d_pixel(fx->id,px,30,t,85,210,fg,bg,accent);
                assert(a.r==b.r&&a.g==b.g&&a.b==b.b);
                if(a.r||a.g||a.b)changed++;
                if(t>0){ot_rgb_t c=ot_wled_1d_pixel(fx->id,px,30,t+149,85,210,fg,bg,accent);
                    assert(c.r<=255&&c.g<=255&&c.b<=255);}
            }
        }
    }
    assert(changed>1000u);
    puts("120 WLED 1D catalog entries: metadata and deterministic pixel rendering passed");
    return 0;
}
