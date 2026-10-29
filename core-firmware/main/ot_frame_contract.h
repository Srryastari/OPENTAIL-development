#pragma once
/* OpenTail CORE: a refresh is complete only when every configured physical
 * output submitted its transfer. This is independent of vehicle zone counts.
 * led_strip_refresh_async_done() must additionally succeed before using this
 * predicate to acknowledge Stop or increment completed-frame throughput.
 */
#include <stdbool.h>
#include <stddef.h>

static inline bool ot_frame_all_submitted(const bool *submitted,size_t count){
    if(!submitted || count==0u)return false;
    for(size_t i=0;i<count;i++)if(!submitted[i])return false;
    return true;
}
