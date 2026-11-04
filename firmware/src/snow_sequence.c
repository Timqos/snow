/* SPDX-License-Identifier: GPL-3.0-only
 * Snow's original weighted-grid rhythm and contour/motif melody generator.
 * Runs in the main loop; only the bounded step copy holds the audio guard. */
static uint16_t snow_gen_pass[NTRK];
static uint32_t snow_rand(uint32_t *state)
{
    uint32_t x=*state;x^=x<<13;x^=x>>17;x^=x<<5;*state=x;return x;
}
static int32_t snow_degree(const track_t *t,int32_t base,int32_t degree)
{
    uint32_t mask=scale_mask(t),root=(uint32_t)t->p[P_ROOT]%12u;
    int32_t note=scale_snap(t,clamp(base,0,127)),dir=degree<0?-1:1;
    while(note<0)note+=12;
    int32_t valid=note;
    uint32_t n=(uint32_t)(degree<0?-degree:degree);
    while(n && note>0 && note<127) {
        note+=dir;
        if((mask>>((uint32_t)(note+12-(int32_t)root)%12u))&1u){n--;valid=note;}
    }
    return clamp(valid,0,127);
}
static int snow_generate(uint32_t channel,int vary)
{
    if(channel>=NTRK)return 1;
    track_t *t=&trk[channel];step_t steps[NSTEP];uint16_t scores[NSTEP];
    uint32_t length=(uint32_t)clamp(t->p[P_SLEN],1,NSTEP),kind=(uint32_t)t->p[SN_G_KIND];
    uint32_t seed=(uint32_t)(t->p[SN_G_SEED]+(vary?1:0))&127u;
    uint32_t rng=0x51f15e5du^(seed+1u)*0x9e3779b9u^(channel+1u)*0x85ebca6bu;
    uint32_t density=(uint32_t)clamp(t->p[SN_G_DENS],0,100),sync=(uint32_t)clamp(t->p[SN_G_SYNC],0,100);
    uint32_t hits=(length*density+50u)/100u,j=0,span=(uint32_t)clamp(t->p[SN_G_SPAN],1,24);
    int32_t walk=0;
    memcpy(steps,t->step,sizeof steps);
    uint32_t existing=0;
    for(uint32_t i=0;i<length;i++)existing+=steps[i].time==ST_NOTE && steps[i].n;
    if(kind!=1u || !existing) {
        for(uint32_t i=0;i<length;i++) {
            uint32_t anchor=i%4u==0u?100u:i%2u==0u?55u:10u;
            scores[i]=(uint16_t)(anchor*(100u-sync)/100u+((i&1u)?sync:0u)+(snow_rand(&rng)&63u));
            if(!i && density)scores[i]+=40;
            memset(&steps[i],0,sizeof steps[i]);steps[i].time=ST_REST;
        }
        for(uint32_t h=0;h<hits;h++) {
            uint32_t best=NSTEP;
            for(uint32_t i=0;i<length;i++)if(steps[i].time==ST_REST && (best==NSTEP || scores[i]>scores[best]))best=i;
            if(best==NSTEP)break;
            steps[best].time=ST_NOTE;steps[best].n=1;
            steps[best].note[0]=(uint8_t)snow_degree(t,t->p[SN_G_BASE],0);
        }
    }
    for(uint32_t i=0;i<length;i++)if(steps[i].time==ST_NOTE && steps[i].n) {
        uint32_t r=snow_rand(&rng);int32_t degree=0;
        walk=clamp(walk+(int32_t)(r%5u)-2,-(int32_t)span,(int32_t)span);
        switch(t->p[SN_G_CONTOUR]) {
        case 1:degree=(int32_t)(j%span);break;
        case 2:degree=-(int32_t)(j%span);break;
        case 3:{uint32_t x=j%(span*2u);degree=(int32_t)(x<span?x:span*2u-x);break;}
        case 4:{static const int8_t motif[8]={0,2,1,4,0,2,-1,3};degree=motif[j&7u]+(int32_t)((seed>>3)&3u)-1;break;}
        default:degree=walk;break;
        }
        if(kind!=0u) {
            steps[i].n=1;steps[i].note[0]=(uint8_t)snow_degree(t,t->p[SN_G_BASE],degree);
        }else if(t->step[i].n) {
            memcpy(steps[i].note,t->step[i].note,sizeof steps[i].note);steps[i].n=t->step[i].n;
        }
        steps[i].vel=(uint8_t)(74u+(r>>8)%34u);
        if((r>>16)%100u<(uint32_t)t->p[SN_G_ACCENT])steps[i].vel=120;
        j++;
    }
    motion_store_t preserved=motion;
    load_begin(t,UNDO_PAT);
    uint32_t guard=motion_guard();
    memcpy(t->step,steps,sizeof steps);t->p[SN_G_SEED]=(int16_t)seed;motion=preserved;
    motion_unguard(guard);load_end(t);
    ui.force=1;
    return 0;
}
static void snow_sequence_poll(void)
{
    for(uint32_t k=0;k<NTRK;k++) {
        uint32_t pass=take_pass[k],mode=(uint32_t)trk[k].p[SN_G_LIVE];
        if(!song.playing){snow_gen_pass[k]=0;continue;}
        if(snow_gen_pass[k]==pass)continue;
        uint32_t was=snow_gen_pass[k];snow_gen_pass[k]=(uint16_t)pass;
        if(was && mode && (pass-1u)%(1u<<(mode-1u))==0u)(void)snow_generate(k,1);
    }
}
static int snow_pattern_select(uint32_t channel,uint32_t slot)
{
    if(channel>=NTRK || slot>=NENT)return 1;
    bank_poll();
    if(trk_keep(channel))return 4;
    if(!ent_on(channel,slot)) {
        int rc=song_put(channel,slot,SPICK_NEW);if(rc)return rc;
    }
    return entry_go(channel,slot);
}
static uint32_t snow_pattern_current(uint32_t channel)
{
    if(channel>=NTRK)return 0;
    uint32_t pending=bank_next(channel);
    return pending<NENT?pending:lists.cur[channel]<NENT?lists.cur[channel]:0;
}
static void snow_pattern_knob(uint32_t channel,int32_t steps)
{
    if(channel>=NTRK)return;
    uint32_t current=snow_pattern_current(channel);
    int rc=snow_pattern_select(channel,(uint32_t)clamp((int32_t)current+steps,0,NENT-1));
    if(rc==4)ui_message("PATTERNS FULL");
    ui.force=1;
}
