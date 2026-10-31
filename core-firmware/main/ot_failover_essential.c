/* Standalone pure-C backup essential-light preparation.
 * No MCU-to-MCU transport, NVS writes, watchdog GPIO, mux/OE control, or
 * claim of physical automatic failover exists in this implementation.
 */
#include "ot_failover_essential.h"
#include "board_config.h"
#include <string.h>

static uint32_t feed(uint32_t h,uint8_t v){
    /* FNV-1a over CANONICAL fields; integrity only, never a MAC. */
    return (h^(uint32_t)v)*16777619u;
}
static uint32_t u16(uint32_t h,uint16_t v){
    h=feed(h,(uint8_t)v);return feed(h,(uint8_t)(v>>8));
}
static uint32_t u32(uint32_t h,uint32_t v){
    h=u16(h,(uint16_t)v);return u16(h,(uint16_t)(v>>16));
}
static uint32_t name_hash(uint32_t h,const char name[OT_OM_NAME_LEN]){
    /* Normalize memory after the first terminator: struct padding, unused
     * zone storage and stale name tails never affect the signed profile. */
    bool ended=false;
    for(unsigned i=0;i<OT_OM_NAME_LEN;i++){
        uint8_t c=(uint8_t)name[i];
        if(ended||c==0){ended=true;c=0;}
        h=feed(h,c);
    }
    return h;
}
static uint32_t digest(const ot_fo_profile_t *p){
    uint32_t h=2166136261u;
    h=u16(h,p->version);h=u32(h,p->generation);
    h=u16(h,p->trigger_revision);h=feed(h,p->required_signals);
    for(unsigned i=0;i<OT_IN_COUNT;i++)h=feed(h,p->trigger.function[i]);
    for(unsigned i=0;i<OT_OM_OUTPUTS;i++){
        const ot_om_output_t *o=&p->output[i];
        h=u16(h,p->output_revision[i]);
        h=name_hash(h,o->name);h=u16(h,o->active_pixels);
        h=feed(h,o->color_order);h=feed(h,o->zone_count);
        for(unsigned c=0;c<3;c++)h=feed(h,o->rgb_gain[c]);
        for(unsigned c=0;c<3;c++)h=feed(h,o->white_gain[c]);
        h=feed(h,o->gamma_enabled?1u:0u);
        for(unsigned j=0;j<o->zone_count;j++){
            const ot_om_zone_t *z=&o->zones[j];
            h=name_hash(h,z->name);h=u16(h,z->pixels);h=feed(h,z->role);
        }
    }
    return h;
}
static uint8_t essential_roles(const ot_fo_profile_t *p){
    uint8_t v=0;
    for(unsigned o=0;o<OT_OM_OUTPUTS;o++){
        for(unsigned z=0;z<p->output[o].zone_count;z++){
            switch(p->output[o].zones[z].role){
                case OT_OM_PARK_BRAKE:v|=OT_SIG_PARK|OT_SIG_BRAKE;break;
                case OT_OM_PARK:v|=OT_SIG_PARK;break;
                case OT_OM_BRAKE:v|=OT_SIG_BRAKE;break;
                case OT_OM_LEFT:v|=OT_SIG_LEFT;break;
                case OT_OM_RIGHT:v|=OT_SIG_RIGHT;break;
                case OT_OM_REVERSE:v|=OT_SIG_REVERSE;break;
                default:break;
            }
        }
    }
    return v;
}
bool ot_fo_profile_valid(const ot_fo_profile_t *p){
    if(!p || p->version!=OT_FO_PROFILE_VERSION || !p->generation ||
       p->required_signals!=OT_FO_REQUIRED_SIGNALS ||
       p->trigger_revision==0 || !ot_tm_valid(&p->trigger))return false;
    for(unsigned i=0;i<OT_OM_OUTPUTS;i++){
        if(p->output_revision[i]==0||!ot_om_valid(&p->output[i]))return false;
    }
    /* A stored empty/neutral fixture or missing independent essential input
     * mapping can NEVER qualify for backup, even with a valid checksum. */
    if((essential_roles(p)&p->required_signals)!=p->required_signals)return false;
    if((ot_tm_resolve(&p->trigger,0xffu)&p->required_signals)!=p->required_signals)
        return false;
    return p->digest==digest(p);
}
bool ot_fo_profile_seal(ot_fo_profile_t *dst,
                        const ot_om_output_t outputs[OT_OM_OUTPUTS],
                        const ot_tm_profile_t *trigger,
                        const uint16_t revisions[OT_OM_OUTPUTS],
                        uint16_t trigger_revision,uint32_t generation,
                        bool committed_active_snapshot){
    if(!dst)return false;
    memset(dst,0,sizeof(*dst));
    if(!committed_active_snapshot||!outputs||!trigger||!revisions||
       !generation||!trigger_revision)return false;
    dst->version=OT_FO_PROFILE_VERSION;dst->generation=generation;
    dst->trigger_revision=trigger_revision;dst->required_signals=OT_FO_REQUIRED_SIGNALS;
    dst->trigger=*trigger;
    for(unsigned i=0;i<OT_OM_OUTPUTS;i++){
        dst->output_revision[i]=revisions[i];
        dst->output[i]=outputs[i];
    }
    /* Validate all fields BEFORE calculating the digest and publishing any
     * proposed record; no persistent write or active RAM swap occurs here. */
    for(unsigned i=0;i<OT_OM_OUTPUTS;i++){
        if(!dst->output_revision[i]||!ot_om_valid(&dst->output[i])){
            memset(dst,0,sizeof(*dst));return false;
        }
    }
    if(!ot_tm_valid(&dst->trigger)||
       (essential_roles(dst)&OT_FO_REQUIRED_SIGNALS)!=OT_FO_REQUIRED_SIGNALS||
       (ot_tm_resolve(&dst->trigger,0xffu)&OT_FO_REQUIRED_SIGNALS)!=OT_FO_REQUIRED_SIGNALS){
        memset(dst,0,sizeof(*dst));return false;
    }
    dst->digest=digest(dst);
    return true;
}
bool ot_fo_profiles_match(const ot_fo_profile_t *a,const ot_fo_profile_t *b){
    if(!ot_fo_profile_valid(a)||!ot_fo_profile_valid(b))return false;
    if(a->digest!=b->digest||a->generation!=b->generation||
       a->trigger_revision!=b->trigger_revision)return false;
    for(unsigned i=0;i<OT_OM_OUTPUTS;i++)
        if(a->output_revision[i]!=b->output_revision[i])return false;
    return true;
}
bool ot_fo_can_activate_profile(const ot_fo_profile_t *primary_new,
                                const ot_fo_profile_t *backup_new,
                                uint8_t raw_inputs,uint8_t debounced_inputs){
    return !raw_inputs&&!debounced_inputs &&
           ot_fo_profiles_match(primary_new,backup_new);
}
ot_fo_decision_t ot_fo_assess_grant(ot_fo_role_t role,
                                    const ot_fo_profile_t *profile,
                                    const ot_fo_qualification_t *q){
    if((role!=OT_FO_PRIMARY && role!=OT_FO_BACKUP)||
       !ot_fo_profile_valid(profile)||!q)return OT_FO_DENY_INVALID;
    if(!q->interlock_physically_verified)return OT_FO_DENY_NO_INTERLOCK;
    if(q->primary_grant&&q->backup_grant)return OT_FO_DENY_CONTENTION;
    if(!(role==OT_FO_PRIMARY?q->primary_grant:q->backup_grant))
        return OT_FO_DENY_NO_GRANT;
    if(!q->local_rail_ok||!q->independent_input_sampler_ok||
       !q->completed_frame_progress_ok||!q->local_watchdog_ok||
       !q->same_committed_profile_on_both_mcus)return OT_FO_DENY_UNHEALTHY;
    if(!q->break_before_make_verified||!q->fresh_full_frame_after_handoff)
        return OT_FO_DENY_HANDOFF_INCOMPLETE;
    return OT_FO_GRANT_QUALIFIED; /* logical authorization ONLY; never toggles OE */
}
bool ot_fo_render_essential(const ot_fo_profile_t *p,unsigned output,
                             uint8_t independent_physical_inputs,
                             uint8_t brightness,ot_rgb_t *rgb,size_t capacity){
    if(!ot_fo_profile_valid(p)||output>=OT_OM_OUTPUTS||!rgb||
       capacity<p->output[output].active_pixels)return false;
    const ot_om_output_t *cfg=&p->output[output];
    const uint8_t mask=ot_tm_resolve(&p->trigger,independent_physical_inputs);
    if(brightness>OT_MAX_BRIGHTNESS)brightness=OT_MAX_BRIGHTNESS;
    ot_om_zone_cursor_t cursor={0};
    for(unsigned px=0;px<cfg->active_pixels;px++){
        int z=ot_om_section_next(cfg,&cursor,px);
        if(z<0)return false;
        const uint8_t role=cfg->zones[z].role;
        /* No AUX, decorative, effects, palette or gamma in backup essential mode. */
        ot_rgb_t color={0,0,0};
        if(role==OT_OM_PARK_BRAKE||role==OT_OM_PARK||role==OT_OM_BRAKE||
           role==OT_OM_LEFT||role==OT_OM_RIGHT||role==OT_OM_REVERSE)
            color=ot_om_trigger_color(role,mask,brightness);
        rgb[px]=color;
    }
    return true;
}
