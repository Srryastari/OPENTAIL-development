#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "wled_1d.h"
#include "show_logic.h"
static int equal(ot_rgb_t a,ot_rgb_t b){return a.r==b.r&&a.g==b.g&&a.b==b.b;}
int main(void){
    assert(ot_wled_1d_count()==120);
    assert(!ot_wled_1d_by_key("not_in_catalog"));
    assert(!ot_wled_1d_by_id(9999));
    ot_layout_t layout={{30,0,0,0,0,0,0,0},30,32};
    ot_show_t s={.mode=OT_SHOW_WLED_COMPAT,.color={255,45,10},
        .background={3,4,5},.accent={0,0,255},.brightness=32,.speed=85,
        .chain_mask=3,.section_mask=1,.intensity=210};
    size_t changed=0;
    for(size_t i=0;i<ot_wled_1d_count();++i){
        const ot_wled_1d_meta_t *fx=ot_wled_1d_at(i);
        assert(fx&&fx->name&&fx->name[0]&&fx->key&&fx->key[0]);
        assert(ot_wled_1d_by_id(fx->id)==fx);
        assert(ot_wled_1d_by_key(fx->key)==fx);
        s.compat_id=fx->id;
        assert(ot_show_valid(&s,&layout));
        assert(equal(ot_show_pixel(&s,&layout,0,30,100),(ot_rgb_t){0,0,0}));
        assert(equal(ot_show_pixel(&s,&layout,1,30,100),(ot_rgb_t){0,0,0}));
        for(unsigned t=0;t<3000;t+=129){
            for(unsigned px=0;px<30;++px){
                ot_rgb_t a=ot_show_pixel(&s,&layout,0,px,t);
                ot_rgb_t b=ot_show_pixel(&s,&layout,0,px,t);
                assert(equal(a,b));
                assert(a.r<=32&&a.g<=32&&a.b<=32);
                if(!equal(a,ot_show_pixel(&s,&layout,0,px,t+149)))changed++;
            }
        }
        s.chain_mask=1;
        assert(equal(ot_show_pixel(&s,&layout,1,0,450),(ot_rgb_t){0,0,0}));
        s.chain_mask=3;
        s.section_mask=2;
        assert(equal(ot_show_pixel(&s,&layout,0,0,450),(ot_rgb_t){0,0,0}));
        s.section_mask=1;
    }
    assert(changed>800);
    s.compat_id=9999;assert(!ot_show_valid(&s,&layout));
    puts("120 WLED-compatible 1D modes: catalog, masks, deterministic bounds passed");
    return 0;
}
