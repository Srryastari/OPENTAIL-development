#include "trigger_manager.h"
#include <string.h>

static const char *const names[]={"disabled","park","brake","left","right","reverse","aux1","aux2","aux3"};

void ot_tm_defaults(ot_tm_profile_t *p){
    if(!p)return;
    for(int i=0;i<OT_IN_COUNT;i++)p->function[i]=(uint8_t)(i+1);
}
bool ot_tm_valid(const ot_tm_profile_t *p){
    if(!p)return false;
    for(int i=0;i<OT_IN_COUNT;i++)if(p->function[i]>OT_TM_AUX3)return false;
    return true;
}
const char *ot_tm_name(uint8_t f){return f<=OT_TM_AUX3?names[f]:"invalid";}
int ot_tm_from_name(const char *s){
    if(!s)return -1;
    for(int i=0;i<=OT_TM_AUX3;i++)if(strcmp(s,names[i])==0)return i;
    return -1;
}
uint8_t ot_tm_resolve(const ot_tm_profile_t *p,uint8_t bits){
    if(!ot_tm_valid(p))return 0;
    uint8_t mask=0;
    for(int i=0;i<OT_IN_COUNT;i++){
        if((bits&(uint8_t)(1u<<i)) && p->function[i]!=OT_TM_DISABLED)
            mask|=(uint8_t)(1u<<(p->function[i]-1u));
    }
    return mask;
}
