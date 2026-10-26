#include <assert.h>
#include <string.h>
#include "particle_system_1d.h"
int main(void){
    ot_particles_1d_t ps;
    ot_particles_1d_reset(&ps,42);
    assert(!ot_particles_1d_emit(&ps,0,0,0,(ot_wled_rgb_t){255,0,0}));
    assert(ot_particles_1d_count(&ps)==0);
    assert(ot_particles_1d_emit(&ps,3*256,512,1500,(ot_wled_rgb_t){255,10,0}));
    ot_particles_1d_step(&ps,1000,4,0,OT_PARTICLE_WRAP);
    assert(ot_particles_1d_count(&ps)==1);
    ot_wled_rgb_t pix[4]={0};
    ot_particles_1d_render(&ps,pix,4,255,false);
    assert(pix[1].r>0&&pix[1].g>0);
    memset(pix,0,sizeof(pix));
    ot_particles_1d_render(&ps,pix,4,255,true);
    assert(pix[2].r>0&&pix[1].r==0);
    ot_particles_1d_step(&ps,500,4,0,OT_PARTICLE_WRAP);
    assert(ot_particles_1d_count(&ps)==0);
    ot_particles_1d_reset(&ps,1);
    assert(ot_particles_1d_emit(&ps,3*256,512,1500,(ot_wled_rgb_t){255,0,0}));
    ot_particles_1d_step(&ps,1000,4,0,OT_PARTICLE_BOUNCE);
    memset(pix,0,sizeof(pix));
    ot_particles_1d_render(&ps,pix,4,255,false);
    assert(pix[2].r>0);
    ot_particles_1d_step(&ps,0,0,0,OT_PARTICLE_KILL);
    assert(ot_particles_1d_count(&ps)==0);
    for(int i=0;i<OT_PARTICLES_1D_MAX;i++)
        assert(ot_particles_1d_emit(&ps,i*256,0,1000,(ot_wled_rgb_t){1,2,3}));
    assert(!ot_particles_1d_emit(&ps,0,0,1000,(ot_wled_rgb_t){1,2,3}));
    assert(ot_particles_1d_count(&ps)==OT_PARTICLES_1D_MAX);
    return 0;
}
