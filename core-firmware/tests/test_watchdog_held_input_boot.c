#include "core_logic.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/* Same source debouncer as production. Host-simulate all 256 held masks.
 * First-frame trigger state must NOT drop while the 4x10ms sampler warms up.
 * Electrical assertion polarity and actual GPIO timing still need a bench. */
int main(void){
    for(unsigned held=0u;held<256u;held++){
        ot_inputs_t debouncer={0};
        bool levels[OT_IN_COUNT]={false};
        for(unsigned i=0;i<OT_IN_COUNT;i++){
            levels[i]=(held&(1u<<i))!=0u;
            debouncer.settled[i]=levels[i];
            debouncer.pending[i]=levels[i];
            debouncer.ticks[i]=4u;
        }
        /* Repeated held input and first 10/20/30ms cycles never drop. */
        for(unsigned tick=0;tick<4u;tick++){
            const uint8_t effective=ot_debounce(&debouncer,levels,4u);
            assert(effective==(uint8_t)held);
        }
        /* Genuine release must still obey the four-sample debounce, not
         * become stuck forever because the initial state was seeded. */
        for(unsigned i=0;i<OT_IN_COUNT;i++)levels[i]=false;
        for(unsigned tick=0;tick<4u;tick++){
            const uint8_t effective=ot_debounce(&debouncer,levels,4u);
            if(tick<3u)assert(effective==(uint8_t)held);
            else assert(effective==0u);
        }
    }
    puts("PASS #113 256 boot-held GPIO masks: no first-frame false release, "
         "four-sample release still correct (HOST ONLY)");
    return 0;
}
