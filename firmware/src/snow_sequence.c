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
    if(bank_next(channel)<NENT)return 2; /* do not overwrite the outgoing pattern while a launch is queued */
    track_t *t=&trk[channel];step_t steps[NSTEP];uint16_t scores[NSTEP];
    uint32_t length=(uint32_t)clamp(t->p[P_SLEN],1,NSTEP),kind=(uint32_t)t->p[SN_G_KIND];
    uint32_t seed=(uint32_t)(t->p[SN_G_SEED]+(vary?1:0))&127u;
    uint32_t style=(uint32_t)t->p[SN_G_STYLE]%NELEM(SN_GROOVES),anchor=SN_GROOVES[style].anchor[channel];
    uint32_t complexity=(uint32_t)clamp(t->p[SN_G_COMPLEX],0,100),human=(uint32_t)clamp(t->p[SN_G_HUMAN],0,100);
    uint32_t rng=0x51f15e5du^(seed+1u)*0x9e3779b9u^(channel+1u)*0x85ebca6bu^(style+1u)*0x27d4eb2du;
    uint32_t density=(uint32_t)clamp(t->p[SN_G_DENS],0,100),sync=(uint32_t)clamp(t->p[SN_G_SYNC],0,100);
    uint32_t base=0;for(uint32_t i=0;i<16u;i++)base+=(anchor>>i)&1u;
    base=(base*length+8u)/16u;if(!base)base=1;
    uint32_t hits=density<=50u?(base*density+25u)/50u:base+((length-base)*(density-50u)+25u)/50u;
    uint32_t j=0,span=(uint32_t)clamp(t->p[SN_G_SPAN],1,24),rotate=(uint32_t)t->p[SN_G_ROTATE]%16u;
    int32_t walk=0;
    memcpy(steps,t->step,sizeof steps);
    uint32_t existing=0;
    for(uint32_t i=0;i<length;i++)existing+=steps[i].time==ST_NOTE && steps[i].n;
    if(kind!=1u || !existing) {
        for(uint32_t i=0;i<length;i++) {
            uint32_t pulse=(i+16u-rotate)%16u,r=snow_rand(&rng),strong=(anchor>>pulse)&1u;
            uint32_t ghost=(anchor>>((pulse+1u)&15u))&1u;
            int32_t score=strong?260-(int32_t)complexity:20+(int32_t)(complexity*ghost);
            score+=(int32_t)(r%(32u+(uint32_t)t->p[SN_G_VARIANT]*2u));
            score+=(int32_t)((pulse&1u)?sync:100u-sync);
            if((style==0u || style==14u) && (pulse+seed)%3u==0u)score+=(int32_t)complexity;
            if(channel==2u && !(pulse&1u))score+=80;
            if(channel==4u && pulse%4u==0u)score+=30;
            scores[i]=(uint16_t)clamp(score,0,1000);
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
        uint32_t r=snow_rand(&rng),pulse=(i+16u-rotate)%16u,strong=(anchor>>pulse)&1u;
        int32_t degree=0;
        walk=clamp(walk+(int32_t)(r%5u)-2,-(int32_t)span,(int32_t)span);
        switch(t->p[SN_G_CONTOUR]) {
        case 1:degree=(int32_t)(j%span);break;
        case 2:degree=-(int32_t)(j%span);break;
        case 3:{uint32_t x=j%(span*2u);degree=(int32_t)(x<span?x:span*2u-x);break;}
        case 4:{static const int8_t motif[8]={0,2,1,4,0,2,-1,3};degree=motif[j&7u]+(int32_t)((seed>>3)&3u)-1;break;}
        default:degree=walk;break;
        }
        if(channel==4u && strong && r%100u>complexity)degree=(i/16u)&1u?4:0;
        if(channel==5u && t->p[SN_G_CONTOUR]==0) {
            static const int8_t progression[4]={0,3,5,4};degree=progression[(j+(seed>>4))&3u];
        }
        if(kind!=0u && (channel>=4u || kind==1u)) {
            steps[i].n=channel==5u?3:1;
            for(uint32_t note=0;note<steps[i].n;note++)steps[i].note[note]=(uint8_t)snow_degree(t,t->p[SN_G_BASE],degree+(int32_t)note*2);
        }else if(kind==0u && t->step[i].n) {
            memcpy(steps[i].note,t->step[i].note,sizeof steps[i].note);steps[i].n=t->step[i].n;
        }
        int32_t vel=strong?104:channel==1u?45:channel==2u?62:78;
        vel+=(int32_t)((r>>8)%21u)-10;
        if((r>>16)%100u<(uint32_t)t->p[SN_G_ACCENT] && strong)vel=120;
        steps[i].vel=(uint8_t)clamp(vel,25,127);
        int32_t nudge=((int32_t)((r>>20)%17u)-8)*(int32_t)human/100;
        if((style==8u || style==10u || style==15u || style==18u) && (pulse&1u))nudge+=6;
        steps[i].nudge=(int8_t)clamp(nudge,NUDGE_MIN,NUDGE_MAX);
        if((channel==2u || channel==3u || style==0u || style==14u || style==16u) && !strong &&
           ((r>>24)%100u)<complexity/3u)step_set_ratchet(&steps[i],2u+((r>>28)&1u));
        j++;
    }
    if(style==5u && channel>=4u) {
        int have=0;
        for(uint32_t i=0;i<length;i++) {
            if(steps[i].time==ST_NOTE)have=1;
            else if(have)steps[i].time=ST_TIE;
        }
    }
    motion_store_t preserved=motion;
    load_begin(t,UNDO_PAT);
    uint32_t guard=motion_guard();
    memcpy(t->step,steps,sizeof steps);t->p[SN_G_SEED]=(int16_t)seed;motion=preserved;
    motion_unguard(guard);load_end(t);ui.force=1;
    return 0;
}
static int snow_generate_scene(uint32_t style,int vary,int set_tempo)
{
    if(style>=NELEM(SN_GROOVES))return 1;
    for(uint32_t k=0;k<NTRK;k++)if(bank_next(k)<NENT)return 2;
    if(set_tempo)song.g[G_BPM]=(int16_t)SN_GROOVES[style].bpm;
    for(uint32_t k=0;k<NTRK;k++) {
        trk[k].p[SN_G_STYLE]=(int16_t)style;
        (void)snow_generate(k,vary);
    }
    return 0;
}
static void snow_sequence_poll(void)
{
    for(uint32_t k=0;k<NTRK;k++) {
        uint32_t pass=take_pass[k],mode=(uint32_t)trk[k].p[SN_G_LIVE];
        if(!song.playing){snow_gen_pass[k]=0;continue;}
        if(snow_gen_pass[k]==pass)continue;
        uint32_t was=snow_gen_pass[k];snow_gen_pass[k]=(uint16_t)pass;
        if(was && pass>1u && mode && (pass-1u)%(1u<<(mode-1u))==0u)(void)snow_generate(k,1);
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
static int snow_pattern_used(uint32_t channel,uint32_t slot)
{
    if(channel>=NTRK || slot>=NENT)return 0;
    return ent_on(channel,slot) || (slot==snow_pattern_current(channel) && !seq_is_empty(&trk[channel]));
}
static void snow_pattern_knob(uint32_t channel,int32_t steps)
{
    if(channel>=NTRK)return;
    uint32_t current=snow_pattern_current(channel);
    int rc=snow_pattern_select(channel,(uint32_t)clamp((int32_t)current+steps,0,NENT-1));
    snow_pattern_half=(uint8_t)(snow_pattern_current(channel)/8u);
    if(rc==4)ui_message("PATTERNS FULL");
    ui.force=1;
}
