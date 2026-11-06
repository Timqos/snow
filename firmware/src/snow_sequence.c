/* SPDX-License-Identifier: GPL-3.0-only
 * Snow 0.4: role-aware random phrases and a separate parameter-lock generator.
 * Interaction reference: the user's Grbox.html (GEN RND / GEN PARM / PARM AMT).
 * All generation runs in the main loop. Only bounded state copies disable IRQs. */
static uint16_t snow_gen_pass[NTRK];
static uint32_t snow_rand(uint32_t *state){uint32_t x=*state;x^=x<<13;x^=x>>17;x^=x<<5;return *state=x;}
static uint32_t snow_hash(uint32_t x){x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;return x^(x>>16);}
static uint32_t snow_seed(const track_t *t){return (uint32_t)t->p[SN_G_SEED]|(uint32_t)t->p[SN_G_SEED_B]<<7|(uint32_t)t->p[SN_G_SEED_C]<<14|(uint32_t)t->p[SN_G_SEED_D]<<21;}
static uint32_t snow_next_seed(uint32_t s){return (s+0x09e3779bu)&0x0fffffffu;} /* 268 million states */
static void snow_seed_put(track_t *t,uint32_t s){t->p[SN_G_SEED]=s&127u;t->p[SN_G_SEED_B]=(s>>7)&127u;t->p[SN_G_SEED_C]=(s>>14)&127u;t->p[SN_G_SEED_D]=(s>>21)&127u;}
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

static uint32_t snow_int(uint32_t *r,uint32_t lo,uint32_t hi){return lo+snow_rand(r)%(hi-lo+1u);}
static int snow_prob(uint32_t *r,uint32_t pct){return snow_rand(r)%100u<pct;}
static void snow_euclid(step_t *s,uint32_t len,uint32_t pulses,uint32_t offset,uint32_t *rng,uint32_t lo,uint32_t hi)
{
    uint32_t bucket=0;
    for(uint32_t i=0;i<len;i++) {
        bucket+=pulses;
        if(bucket>=len){bucket-=len;uint32_t at=(i+offset)%len;s[at].time=ST_NOTE;s[at].n=1;s[at].vel=(uint8_t)snow_int(rng,lo,hi);}
    }
}
static uint32_t snow_pulses(uint32_t *r,uint32_t low,uint32_t high,uint32_t len,uint32_t density)
{
    uint32_t base=(snow_int(r,low,high)*len+16u)/32u;if(!base)base=1;
    return density<=50u?(base*density+25u)/50u:base+((len-base)*(density-50u)+25u)/50u;
}
static uint64_t snow_hits(const step_t *s,uint32_t len){uint64_t m=0;for(uint32_t i=0;i<len;i++)if(s[i].time==ST_NOTE&&s[i].n)m|=(uint64_t)1u<<i;return m;}
/* Four independently varied bars. Only bar 1 -> bar 3 may repeat. Retain
 * explicit 0/100% density and MELODY's user-owned rhythm in snow_phrase. */
static void snow_four_bars(step_t *out,uint32_t channel,uint32_t *rng,int repeat13)
{
    uint32_t masks[4];
    for(uint32_t b=0;b<4;b++){
        step_t *bar=out+b*16u;
        if(b==2u&&repeat13)memcpy(bar,out,16u*sizeof *out);
        uint32_t start=snow_int(rng,0,15);
        for(uint32_t attempt=0;attempt<17u;attempt++){
            uint32_t mask=(uint32_t)snow_hits(bar,16),same=!mask;
            for(uint32_t p=0;p<b;p++)if(!(b==2u&&p==0u)&&mask==masks[p])same=1;
            if(!same){masks[b]=mask;break;}
            uint32_t at=(start+attempt*5u)&15u;
            if(channel==5u)at=(at&3u)*4u;
            if(channel==0u&&!at)at=15u;      /* Keep the kick's downbeat foundation. */
            memset(&bar[at],0,sizeof bar[at]);
            if(!(mask&(1u<<at))){bar[at].time=ST_NOTE;bar[at].n=1;bar[at].vel=(uint8_t)snow_int(rng,channel==2u?45:80,115);}
            else bar[at].time=ST_REST;
        }
        masks[b]=(uint32_t)snow_hits(bar,16);
    }
}
static void snow_phrase(track_t *t,uint32_t channel,uint32_t seed,step_t *out)
{
    uint32_t len=(uint32_t)clamp(t->p[P_SLEN],1,NSTEP),density=(uint32_t)t->p[SN_G_DENS],kind=(uint32_t)t->p[SN_G_KIND];
    uint32_t family=snow_hash(seed)%12u,rng=snow_hash(seed^(channel+1u)*0x9e3779b9u)|1u;
    uint32_t reach=2u+(uint32_t)t->p[SN_G_VARIANT]*len/127u;if(reach>=len)reach=len-1u;
    uint32_t rotate=(uint32_t)t->p[SN_G_ROTATE],offset=snow_int(&rng,0,reach),complex=(uint32_t)t->p[SN_G_COMPLEX];
    int preserve=kind==1u&&snow_hits(t->step,len)!=0;
    memcpy(out,t->step,sizeof t->step);
    if(!preserve) {
        for(uint32_t i=0;i<len;i++){memset(&out[i],0,sizeof out[i]);out[i].time=ST_REST;}
        uint32_t pulses=0;
        if(channel==0u)pulses=snow_pulses(&rng,family==1||family==7?2:family==5?8:4,family==1||family==7?6:family==5?17:12,len,density);
        if(channel==1u)pulses=snow_pulses(&rng,2,7,len,density);
        if(channel==2u)pulses=snow_pulses(&rng,9,28,len,density);
        if(channel==3u){static const uint8_t primes[]={3,5,7,11,13,17};uint32_t p=primes[snow_int(&rng,0,5)];pulses=(p*len+16u)/32u;if(!pulses)pulses=1;pulses=density<=50?(pulses*density+25)/50:pulses+((len-pulses)*(density-50)+25)/50;}
        if(channel==4u)pulses=snow_pulses(&rng,family<4?3:6,family<4?9:16,len,density);
        if(channel==5u)pulses=snow_pulses(&rng,1,family<9?4:8,len,density);
        snow_euclid(out,len,pulses,(offset+rotate+(uint32_t)t->p[SN_G_SYNC]/20u)%len,&rng,channel==2u?35:channel==1u?85:65,channel==2u?105:124);
        if(channel==0u&&density>0&&density<100){
            /* Metered foundation, broken syncopation, half-time and sparse
             * phrases are independent constructions, not one stored beat. */
            if(family==0u||family==3u||family==8u){
                for(uint32_t i=0;i<len;i++){memset(&out[i],0,sizeof out[i]);out[i].time=ST_REST;}
                uint32_t stride=family==3u?8u:4u;
                for(uint32_t pos=0;pos<len;pos+=stride){
                    if(pos&&snow_prob(&rng,100u-density))continue;
                    uint32_t at=(pos+rotate)%len;out[at].time=ST_NOTE;out[at].n=1;out[at].vel=(uint8_t)snow_int(&rng,105,125);
                    if(pos+stride<len&&snow_prob(&rng,15u+complex/3u)){
                        at=(pos+snow_int(&rng,1,stride-1u)+rotate)%len;out[at].time=ST_NOTE;out[at].n=1;out[at].vel=(uint8_t)snow_int(&rng,62,105);
                    }
                }
            }else if(snow_prob(&rng,80)){
                uint32_t at=rotate%len;out[at].time=ST_NOTE;out[at].n=1;out[at].vel=(uint8_t)snow_int(&rng,109,127);
            }
        }
        /* Musical families are internal choices, with fresh probability/rotation, never a genre menu. */
        if(density>0&&density<100&&family==2u&&(channel==0u||channel==1u)) {
            for(uint32_t i=0;i<len;i++){memset(&out[i],0,sizeof out[i]);out[i].time=ST_REST;}
            static const uint8_t kick[]={0,10,16,26},snare[]={4,12,20,28};
            const uint8_t *a=channel?snare:kick;
            for(uint32_t j=0;j<4u;j++)if(snow_prob(&rng,density>=50?100:density*2u)) {
                uint32_t pos=(a[j]*len/32u+rotate)%len;
                if(channel&&pos&&snow_prob(&rng,35))pos--;
                out[pos].time=ST_NOTE;out[pos].n=1;out[pos].vel=(uint8_t)snow_int(&rng,100,124);
            }
        } else if(density>0&&density<100&&channel==1u&&family==0u) {
            for(uint32_t i=0;i<len;i++){memset(&out[i],0,sizeof out[i]);out[i].time=ST_REST;}
            for(uint32_t pos=8;pos<32;pos+=16){uint32_t i=(pos*len/32u+rotate)%len;out[i].time=ST_NOTE;out[i].n=1;out[i].vel=115;}
        }
        if(density>0&&density<100&&(channel==0u||channel==1u)) {
            for(uint32_t i=0;i<len;i++)if(out[i].time==ST_REST&&snow_prob(&rng,channel?5u+complex/12u:3u+complex/20u)) {
                out[i].time=ST_NOTE;out[i].n=1;out[i].vel=(uint8_t)snow_int(&rng,channel?30:55,channel?75:90);
            }
        }
        if(!density)for(uint32_t i=0;i<len;i++){memset(&out[i],0,sizeof out[i]);out[i].time=ST_REST;}
        if(density==100)for(uint32_t i=0;i<len;i++){out[i].time=ST_NOTE;out[i].n=1;out[i].vel=(uint8_t)snow_int(&rng,55,120);}
    }
    int repeat13=0;
    if(!preserve&&len==64u&&density>0&&density<100){repeat13=snow_prob(&rng,35);snow_four_bars(out,channel,&rng,repeat13);}
    int32_t walk=0,span=clamp(t->p[SN_G_SPAN],1,24);uint32_t note_index=0;
    static const int8_t progressions[8][4]={{0,5,3,4},{0,3,4,0},{0,4,5,3},{0,6,3,4},{0,2,5,4},{0,3,0,4},{0,5,1,4},{0,4,3,6}};
    uint32_t progression=snow_int(&rng,0,7),inversion=snow_int(&rng,0,2);
    for(uint32_t i=0;i<len;i++)if(out[i].time==ST_NOTE&&out[i].n) {
        if(kind!=0u&&(channel>=3u||kind==1u)) {
            int32_t degree=0;walk=clamp(walk+(int32_t)snow_int(&rng,0,2u+(uint32_t)t->p[SN_G_VARIANT]/21u)-(int32_t)(1u+(uint32_t)t->p[SN_G_VARIANT]/42u),-span,span);
            switch(t->p[SN_G_CONTOUR]) {
            case 1:degree=(int32_t)(note_index%(uint32_t)span);break;
            case 2:degree=-(int32_t)(note_index%(uint32_t)span);break;
            case 3:{uint32_t p=note_index%(uint32_t)(span*2);degree=(int32_t)(p<(uint32_t)span?p:(uint32_t)span*2-p);break;}
            case 4:degree=(int32_t)(snow_hash(seed+(note_index%8u)*977u)%(uint32_t)(span*2+1))-span;break;
            default:degree=walk;break;
            }
            if(channel==4u){
                static const int8_t riff[]={0,0,4,2,0,7,4,-3};
                if(family<4u)degree=riff[(note_index+(seed&7u))&7u];
                else degree=clamp(degree,-(span<5?span:5),span<9?span:9);
                if(i%8u==0u&&snow_prob(&rng,80))degree=0;
                degree=clamp(degree,-span,span);
            }
            if(channel==5u)degree=progressions[progression][i*4u/len]+(int32_t)(seed%3u)-1;
            out[i].n=(uint8_t)(channel==5u&&!t->p[SN_PLAY_MODE]?(family>=8u&&snow_prob(&rng,25)?4:3):1);
            for(uint32_t n=0;n<out[i].n;n++){
                int32_t nd=degree+(int32_t)n*2;
                if(channel==5u&&out[i].n>1u&&n<inversion)nd+=7;
                out[i].note[n]=(uint8_t)snow_degree(t,t->p[SN_G_BASE],nd);
            }
        } else if(kind==0u&&t->step[i].n) {out[i].n=t->step[i].n;memcpy(out[i].note,t->step[i].note,sizeof out[i].note);}
        else {out[i].n=1;out[i].note[0]=(uint8_t)snow_degree(t,t->p[SN_G_BASE],0);}
        if(!preserve) {
            uint32_t human=(uint32_t)(t->p[SN_G_HUMAN]+9)/10u;
            out[i].nudge=(int8_t)((int32_t)snow_int(&rng,0,human*2u)-(int32_t)human);
            if(family==1u&&(i&1u))out[i].nudge=(int8_t)clamp(out[i].nudge+4,NUDGE_MIN,NUDGE_MAX);
            if(snow_prob(&rng,(uint32_t)t->p[SN_G_ACCENT]))out[i].vel=(uint8_t)snow_int(&rng,106,127);
        }
        note_index++;
    }
    if(!preserve&&channel>=4u&&density>0&&density<100){
        /* Real sequencer ties carry note-offs across steps. Do not turn a
         * bass line or a harmonic phrase into a row of drum-length triggers. */
        for(uint32_t i=0;i<len;i++)if(out[i].time==ST_NOTE&&out[i].n){
            uint32_t hold=channel==5u?snow_int(&rng,family>=9u?1u:3u,family>=9u?4u:16u):snow_int(&rng,1u,family<6u?7u:3u);
            for(uint32_t j=1;j<hold&&i+j<len&&out[i+j].time==ST_REST;j++){
                memset(&out[i+j],0,sizeof out[i+j]);out[i+j].time=ST_TIE;
            }
            if(channel==4u&&snow_prob(&rng,15))out[i].flags|=SF_SLIDE;
            if(channel==5u)out[i].vel=(uint8_t)snow_int(&rng,78,112);
        }
    }
    if(repeat13)memcpy(out+32,out,16u*sizeof *out);
}
static void snow_remove_auto(motion_store_t *m,uint32_t channel)
{
    uint32_t n=0;for(uint32_t i=0;i<m->count;i++)if((m->event[i].place>>6)!=channel||!m->event[i].generated)m->event[n++]=m->event[i];
    m->count=(uint8_t)n;
}
static int snow_generate(uint32_t channel,int vary)
{
    if(channel>=NTRK)return 1;if(bank_next(channel)<NENT)return 2;
    track_t *t=&trk[channel];step_t steps[NSTEP];uint32_t len=(uint32_t)clamp(t->p[P_SLEN],1,NSTEP),seed=snow_seed(t);
    uint64_t old=snow_hits(t->step,len);
    for(uint32_t attempt=0;attempt<32u;attempt++) {
        if(vary)seed=snow_next_seed(seed);
        snow_phrase(t,channel,seed,steps);
        if(!vary||t->p[SN_G_KIND]==1||!t->p[SN_G_DENS]||t->p[SN_G_DENS]==100||len<4||snow_hits(steps,len)!=old)break;
    }
    motion_store_t next=motion;snow_remove_auto(&next,channel);
    load_begin(t,UNDO_PAT);uint32_t guard=motion_guard();
    memcpy(t->step,steps,sizeof steps);snow_seed_put(t,seed);motion=next;
    motion_unguard(guard);load_end(t);ui.force=1;return 0;
}
static void snow_auto_lock(motion_store_t *m,track_t *t,uint32_t step,uint32_t id,int32_t value)
{
    uint32_t k=trk_index(t),place=k<<6|step;
    for(uint32_t i=0;i<m->count;i++)if(m->event[i].place==place&&m->event[i].id==id)return;
    if(m->count>=MOTION_MAX)return;
    const param_desc_t *d=track_desc(t,id);value=param_fit(d,value);
    motion_event_t *e=&m->event[m->count++];memset(e,0,sizeof *e);
    e->place=(uint16_t)place;e->id=(uint8_t)id;e->value=(int8_t)value;e->lock=e->generated=1;m->on|=(uint8_t)(1u<<k);
}
static int snow_generate_parameters(uint32_t channel)
{
    if(channel>=NTRK)return 1;if(bank_next(channel)<NENT)return 2;
    track_t *t=&trk[channel];step_t steps[NSTEP];memcpy(steps,t->step,sizeof steps);
    motion_store_t next=motion;snow_remove_auto(&next,channel);
    uint32_t seed=snow_next_seed(snow_seed(t)),rng=snow_hash(seed^(channel+1u)*977u)|1u;
    uint32_t len=(uint32_t)clamp(t->p[P_SLEN],1,NSTEP),amt=(uint32_t)t->p[SN_G_LOCKS],made=0;
    uint32_t cap=next.count<52u?52u-next.count:0;if(cap>8u)cap=8u;
    for(uint32_t i=0;i<len;i++) {
        if(steps[i].cond==TC_ORNAMENT){steps[i].cond=TC_NONE;step_set_ratchet(&steps[i],1);}
        if(!amt||steps[i].time!=ST_NOTE||!steps[i].n||made>=cap||!snow_prob(&rng,amt*45u/100u))continue;
        uint32_t pick=snow_int(&rng,0,3),id=pick==0&&channel<3?SN_TUNE:pick==1?P_DEC:pick==2?P_E0:P_REV;
        if(channel>=4u)id=pick==0?P_E6:pick==1?P_PAN:pick==2?P_REV:channel==5u?P_E7:P_E0;
        int32_t base=motion_base_value(t,id),spread=id==SN_TUNE?(int32_t)(2u+amt/20u):id==P_REV?35:10+(int32_t)amt/4;
        if(channel>=4u)spread=id==P_E6?8+(int32_t)amt/7:id==P_REV?12+(int32_t)amt/4:id==P_PAN?2+(int32_t)amt/10:5+(int32_t)amt/20;
        int32_t v=base+(int32_t)snow_int(&rng,0,(uint32_t)spread*2u)-spread;
        uint32_t before=next.count;snow_auto_lock(&next,t,i,id,v);made+=next.count!=before;
        if(id==P_DEC&&made<cap&&snow_prob(&rng,40)){before=next.count;snow_auto_lock(&next,t,i,P_REV,(int32_t)snow_int(&rng,15,65));made+=next.count!=before;}
    }
    if(amt&&cap&&!made)for(uint32_t i=0;i<len;i++)if(steps[i].time==ST_NOTE&&steps[i].n){uint32_t id=channel>=4u?P_E6:P_E0;snow_auto_lock(&next,t,i,id,clamp(motion_base_value(t,id)+((seed&1u)?18:-18),0,127));break;}
    if(amt&&channel>=2u&&channel<=3u&&snow_prob(&rng,65)) {
        uint32_t candidate[NSTEP],n=0,period=div_samples((uint32_t)t->p[P_SDIV]),bar=beat_samples()*4u;
        for(uint32_t i=0;i<len;i++)if(steps[i].time==ST_NOTE&&steps[i].n&&!steps[i].cond) {
            uint32_t localbar=(uint32_t)((uint64_t)i*period/bar);
            if((uint64_t)len*period<bar*2u || (localbar&1u))candidate[n++]=i;
        }
        if(n){uint32_t at=candidate[snow_int(&rng,0,n-1u)];step_set_ratchet(&steps[at],snow_int(&rng,2,4));steps[at].cond=TC_ORNAMENT;}
    }
    load_begin(t,UNDO_PAT);uint32_t guard=motion_guard();memcpy(t->step,steps,sizeof steps);motion=next;snow_seed_put(t,seed);
    motion_unguard(guard);load_end(t);ui.force=1;return 0;
}
static int snow_generate_scene(int vary)
{
    for(uint32_t k=0;k<NTRK;k++)if(bank_next(k)<NENT)return 2;
    uint32_t seed=snow_seed(TSEL);
    for(uint32_t k=0;k<NTRK;k++){snow_seed_put(&trk[k],seed);int rc=snow_generate(k,vary);if(rc)return rc;}
    return 0;
}
static int snow_parameter_scene(void)
{
    for(uint32_t k=0;k<NTRK;k++)if(bank_next(k)<NENT)return 2;
    for(uint32_t k=0;k<NTRK;k++){int rc=snow_generate_parameters(k);if(rc)return rc;}return 0;
}
static int snow_clear_sequence(int all)
{
    uint32_t begin=all?0u:song.sel,end=all?NTRK:begin+1u;
    for(uint32_t k=begin;k<end;k++)if(bank_next(k)<NENT)return 2;
    for(uint32_t k=begin;k<end;k++) {
        track_t *t=&trk[k];load_begin(t,UNDO_PAT);uint32_t f=motion_guard();
        for(uint32_t i=0;i<NSTEP;i++){memset(&t->step[i],0,sizeof(step_t));t->step[i].time=ST_REST;}
        motion_unguard(f);load_end(t);
    }
    ui.force=1;return 0;
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
/* Decode/stage outside the audio interrupt, then publish a complete group
 * under one short guard. RANDOM PATT cannot split across two bar lines. */
static int stattic_pattern_prepare(uint32_t channel,uint32_t slot,uint32_t *ready)
{
    launch_t *q=&launch_q[channel];uint32_t b0,b1;
    if(ent_block(channel,slot,&b0,&b1))slot=b0;
    q->armed=0;chain_go[channel]=0;RING_PUBLISH();bank_poll_trk(channel);
    if(bank_cur(channel)==slot)return 0;       /* Selecting the playing pattern cancels the queued change. */
    int rc=stage_entry_copy(channel,slot,ST_LAUNCH_BAR,0,0,0);
    if(!(rc&~2))*ready|=1u<<channel;
    return rc;
}
static void stattic_patterns_arm(uint32_t ready)
{
    fm1_irq_off();uint32_t at=launch_at(LAUNCH_BAR);
    for(uint32_t k=0;k<NTRK;k++)if((ready>>k)&1u){
        launch_q[k].at=at;RING_PUBLISH();launch_q[k].armed=1;
    }
    fm1_irq_on();
}
static int snow_pattern_select(uint32_t channel,uint32_t slot)
{
    if(channel>=NTRK || slot>=ST_PATTERNS)return 1;
    bank_poll();
    if(!song.playing&&trk_keep(channel))return 4;
    if(!ent_on(channel,slot)) {
        int rc=song_put(channel,slot,SPICK_NEW);if(rc)return rc;
    }
    if(!song.playing)return entry_go(channel,slot);
    uint32_t ready=0;int rc=stattic_pattern_prepare(channel,slot,&ready);
    stattic_patterns_arm(ready);
    ui_message(rc==4?"PATTERNS FULL":rc==2?"AUTOMATION FULL":ready?"PATTERN AT NEXT BAR":"PATTERN CANCELLED");
    return rc;
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
    int rc=snow_pattern_select(channel,(uint32_t)clamp((int32_t)current+steps,0,ST_PATTERNS-1));
    snow_pattern_half=0;
    if(rc==4)ui_message("PATTERNS FULL");
    ui.force=1;
}
/* Pick existing music only. No generation, empty-slot creation or preset load. */
static int stattic_random_patterns(void)
{
    for(uint32_t k=0;k<NTRK;k++)if(bank_next(k)<NENT)return 2;
    uint32_t changed=0,ready=0,full=0;
    for(uint32_t k=0;k<NTRK;k++){
        uint32_t choices[ST_PATTERNS],n=0,current=snow_pattern_current(k);
        for(uint32_t p=0;p<ST_PATTERNS;p++)if(p!=current&&ent_on(k,p))choices[n++]=p;
        if(n){
            uint32_t slot=choices[rng()%n];
            int rc=song.playing?stattic_pattern_prepare(k,slot,&ready):snow_pattern_select(k,slot);
            if(rc&~2)return rc;full|=rc==2;changed++;
        }
    }
    if(song.playing)stattic_patterns_arm(ready);
    ui.force=1;return changed?(full?5:0):3;
}
