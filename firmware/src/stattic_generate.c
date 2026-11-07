/* SPDX-License-Identifier: GPL-3.0-only
 * HOME 2: prepare on the UI thread, publish at the shared bar. No generator,
 * random source, bank decode or flash write runs in the audio interrupt.
 * Candidate copies live in the pool, not in the small interrupt/worker stack. */
typedef struct {
    int16_t p[P_COUNT];
    step_t step[NSTEP];
    motion_store_t motion;
    uint32_t mask[(P_COUNT+31u)/32u],at;
    uint16_t seq;
    uint8_t what,pid;
    volatile uint8_t armed;
} stattic_gen_t;
static stattic_gen_t stattic_gen[NTRK] __attribute__((section(".pool")));
static track_t stattic_candidate __attribute__((section(".pool")));
static volatile uint8_t stattic_gen_error;
static void stattic_gen_drop(void)
{
    for(uint32_t k=0;k<NTRK;k++)stattic_gen[k].armed=0;
    stattic_gen_error=0;
}
static void stattic_gen_cancel(uint32_t channel)
{
    uint32_t f=motion_guard();stattic_gen[channel%NTRK].armed=0;motion_unguard(f);
}
/* Overlay a track's owned events without disturbing the other five tracks. */
static int stattic_gen_sequence(uint32_t channel)
{
    const stattic_gen_t *q=&stattic_gen[channel%NTRK];return q->armed&&(q->what&UNDO_PAT);
}
static int stattic_motion_overlay(motion_store_t *out,uint32_t k,const motion_store_t *in)
{
    uint32_t n=0;
    for(uint32_t i=0;i<out->count;i++)n+=(out->event[i].place>>6)!=k;
    if(n+in->count>MOTION_MAX)return 1;
    n=0;
    for(uint32_t i=0;i<out->count;i++)if((out->event[i].place>>6)!=k)out->event[n++]=out->event[i];
    for(uint32_t i=0;i<in->count;i++)out->event[n++]=in->event[i];
    out->count=(uint8_t)n;out->on=(uint8_t)((out->on&~(1u<<k))|(in->on&(1u<<k)));
    return 0;
}
static int stattic_gen_prepare(uint32_t channel,uint32_t knob)
{
    if(channel>=NTRK||knob>=4u)return 1;
    if(bank_next(channel)<NENT)return 2;
    track_t *t=&trk[channel],*c=&stattic_candidate;
    stattic_gen_t *q=&stattic_gen[channel];motion_store_t next;
    uint32_t mask[(P_COUNT+31u)/32u]={0},f=motion_guard();
    uint32_t previous=q->armed?q->seq:0;
    for(uint32_t id=0;id<P_COUNT;id++)c->p[id]=motion_base_value(t,id);
    memcpy(c->step,t->step,sizeof c->step);next=motion;
    for(uint32_t k=0;k<NTRK;k++)if(stattic_gen_sequence(k))
        if(stattic_motion_overlay(&next,k,&stattic_gen[k].motion)){motion_unguard(f);return 3;}
    if(previous){
        for(uint32_t id=0;id<P_COUNT;id++)if((q->mask[id/32u]>>(id%32u))&1u)c->p[id]=q->p[id];
        if(q->what&UNDO_PAT)memcpy(c->step,q->step,sizeof c->step);
    }
    motion_unguard(f);
    uint32_t what=knob?UNDO_SOUND|(knob==3u?UNDO_FX:0):UNDO_PAT;
    if(!knob){
        step_t steps[NSTEP];uint32_t len=(uint32_t)clamp(c->p[P_SLEN],1,NSTEP),seed=snow_seed(c);
        uint64_t old=snow_hits(c->step,len);
        for(uint32_t attempt=0;attempt<32u;attempt++){
            seed=snow_next_seed(seed);snow_phrase(c,channel,seed,steps);
            if(c->p[SN_G_KIND]==1||!c->p[SN_G_DENS]||c->p[SN_G_DENS]==100||len<4||snow_hits(steps,len)!=old)break;
        }
        memcpy(c->step,steps,sizeof steps);snow_seed_put(c,seed);
        snow_remove_auto(&next,channel);seed=snow_next_seed(seed);
        snow_parameter_plan(t,c->p,channel,seed,c->step,&next);snow_seed_put(c,seed);
        static const uint8_t seeds[]={SN_G_SEED,SN_G_SEED_B,SN_G_SEED_C,SN_G_SEED_D};
        for(uint32_t i=0;i<NELEM(seeds);i++)mask[seeds[i]/32u]|=1u<<(seeds[i]%32u);
    }else snow_design_section(t,channel,knob==1u?FAM_EDIT:knob==2u?FAM_LFO:FAM_FX,mask,c->p);
    /* The old candidate may have entered while preparation ran. In that
     * case this detent gets its own undo and the following bar, not its past. */
    f=motion_guard();
    if(bank_next(channel)<NENT){motion_unguard(f);return 2;}
    int continuing=previous&&q->armed&&q->seq==previous;
    if(!continuing){
        undo_label="HOME GENERATE";load_snapshot(t,what);
        q->seq=undo.seq;q->what=0;memset(q->mask,0,sizeof q->mask);q->pid=trk_pid[channel];
    }else {
        int32_t slot=undo_find(q->seq);
        if(slot>=0)(*undo_entry(slot)).what|=(uint8_t)what;
    }
    q->armed=0;q->what|=(uint8_t)what;
    for(uint32_t i=0;i<NELEM(mask);i++)q->mask[i]|=mask[i];
    memcpy(q->p,c->p,sizeof q->p);
    if(q->what&UNDO_PAT){
        memcpy(q->step,c->step,sizeof q->step);memset(&q->motion,0,sizeof q->motion);
        q->motion.on=next.on&(uint8_t)(1u<<channel);
        for(uint32_t i=0;i<next.count;i++)if((next.event[i].place>>6)==channel)
            q->motion.event[q->motion.count++]=next.event[i];
    }
    q->at=((launch_steps>>4)+1u)<<4;RING_PUBLISH();q->armed=1;
    if(!song.playing)stattic_gen_apply(); /* STOP during preparation has immediate semantics. */
    motion_unguard(f);return 0;
}
static void stattic_gen_apply(void)
{
    uint32_t due=0,count=motion.count;
    for(uint32_t k=0;k<NTRK;k++){
        stattic_gen_t *q=&stattic_gen[k];
        if(!q->armed||(song.playing&&(int32_t)(launch_steps-q->at)<0))continue;
        if(q->pid!=trk_pid[k]){q->armed=0;continue;}
        due|=1u<<k;
        if(q->what&UNDO_PAT)count=count-motion_count(&trk[k])+q->motion.count;
    }
    /* A manual edit can fill the event pool after preparation. Reject the
     * complete batch in that case; never truncate locks or apply half of it. */
    if(count>MOTION_MAX){
        stattic_gen_error=1;
        for(uint32_t k=0;k<NTRK;k++)if((due>>k)&1u)stattic_gen[k].armed=0;
        return;
    }
    /* Remove every outgoing owner's events first: transient occupancy must
     * not truncate a group whose final combined size passed the check. */
    uint32_t replace=0,n=0;
    for(uint32_t k=0;k<NTRK;k++)if(((due>>k)&1u)&&(stattic_gen[k].what&UNDO_PAT))replace|=1u<<k;
    for(uint32_t i=0;i<motion.count;i++)if(!((replace>>(motion.event[i].place>>6))&1u))motion.event[n++]=motion.event[i];
    motion.count=(uint8_t)n;motion.on&=(uint8_t)~replace;
    for(uint32_t k=0;k<NTRK;k++)if((due>>k)&1u){
        stattic_gen_t *q=&stattic_gen[k];track_t *t=&trk[k];
        motion_restore(t);
        if(q->what&UNDO_PAT){
            seq_release(t);memcpy(t->step,q->step,sizeof q->step);
            (void)stattic_motion_overlay(&motion,k,&q->motion);
            t->rat_left=t->nud=t->rskip_n=t->rh_n=0;
            t->seq_idx=(uint16_t)(t->p[P_SLEN]>1?t->p[P_SLEN]-1:0);
            t->seq_cnt=0;t->seq_pos=0x7FFFFFFFu;take_p0[k]=take_pass[k];
        }
        for(uint32_t id=0;id<P_COUNT;id++)if((q->mask[id/32u]>>(id%32u))&1u)t->p[id]=q->p[id];
        motion_rebase(t);q->armed=0;
    }
}
/* Undo before the bar cancels without changing the sounding track. Store
 * the future values in the existing undo slot, so normal REDO still works. */
static int stattic_gen_undo(uint32_t seq)
{
    for(uint32_t k=0;k<NTRK;k++){
        stattic_gen_t *q=&stattic_gen[k];
        if(!q->armed||q->seq!=seq)continue;
        uint32_t f=motion_guard();int32_t slot=undo_find(seq);
        if(!q->armed||slot<0){motion_unguard(f);return 0;}
        undo_t *u=&(*undo_entry(slot));track_t *t=&trk[k];
        for(uint32_t id=0;id<P_COUNT;id++)u->p[id]=((q->mask[id/32u]>>(id%32u))&1u)?q->p[id]:motion_base_value(t,id);
        if(q->what&UNDO_PAT){memcpy(u->step,q->step,sizeof u->step);u->motion_backup=q->motion;}
        else motion_snapshot_track(t,&u->motion_backup);
        u->keep=0;q->armed=0;motion_unguard(f);return 1;
    }
    return 0;
}
