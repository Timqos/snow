/* SPDX-License-Identifier: GPL-3.0-only
 * Snow 0.4: role-aware random phrases and a separate parameter-lock generator.
 * Interaction reference: the user's Grbox.html (GEN RND / GEN PARM / PARM AMT).
 * All generation runs in the main loop. Only bounded state copies disable IRQs. */
static int8_t stattic_phrase_micro[NSTEP];
static int32_t scale_snap(const track_t *t,int32_t note)
{
    uint32_t mask=scale_mask(t);int32_t root=t->p[P_ROOT];
    while(note>0&&!((mask>>((uint32_t)(note-root+120)%12u))&1u))note--;
    return note;
}
#define NUDGE_MIN MICRO_MIN
#define NUDGE_MAX MICRO_MAX
#define SN_G_LOCKS SN_G_STYLE

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
/* (2026-11-09, the owner: an AMBIENT generator next to the energetic one, GENERATE = AMBIENT in EVOLVE)
 * Few, long, slow notes. BS: on the strong beats (the bar's 1, sometimes its 3), held to the next.
 * CH: a chord a bar or two, held, now and then floated off the 1, a slow progression. SY: a sparse line,
 * one to three long notes a bar (DENSITY), some bars resting, drawn from a few shapes: drops, a rising arc,
 * an answer a bar later, a high echo. Bar 3 may return bar 1. Every note is tied to the next. */
static void snow_ambient(track_t *t,uint32_t channel,uint32_t seed,step_t *out,uint32_t len,uint32_t density,uint32_t *rng)
{
    static const int8_t prog[8][4]={{0,5,3,4},{0,3,5,4},{0,-2,3,0},{0,4,5,3},{0,6,3,5},{0,2,5,4},{0,3,0,-2},{0,5,1,3}};
    static const int8_t bassdeg[6]={0,0,4,-3,3,5};
    uint32_t bars=len>=16u?len/16u:1u,blen=len>=16u?16u:len,pr=snow_int(rng,0,7),shape=snow_int(rng,0,3);
    uint32_t chords_per=snow_prob(rng,40)?2u:1u,inv=snow_int(rng,0,2);
    int32_t span=clamp(t->p[SN_G_SPAN],1,24),walk=(int32_t)snow_int(rng,0,4),lift=snow_prob(rng,50)?7:0;
    for(uint32_t i=0;i<len;i++){memset(&out[i],0,sizeof out[i]);out[i].time=ST_REST;}
    for(uint32_t b=0;b<bars;b++){
        step_t *bar=out+b*blen;
        if(b==2u&&bars==4u&&snow_prob(rng,35)){memcpy(bar,out,blen*sizeof *out);continue;}
        int32_t harm=prog[pr][(b/chords_per)&3u];
        if(channel==4u){                                         /* BS: the 1, sometimes the 3 */
            int32_t d=snow_prob(rng,60)?harm:bassdeg[snow_int(rng,0,5)];
            if(snow_prob(rng,92)){bar[0]=(step_t){.time=ST_NOTE,.n=1,.vel=(uint8_t)snow_int(rng,84,108)};bar[0].note[0]=(uint8_t)snow_degree(t,t->p[SN_G_BASE],d);}
            if(blen>8u&&snow_prob(rng,15u+density/3u)){bar[8]=(step_t){.time=ST_NOTE,.n=1,.vel=(uint8_t)snow_int(rng,70,96)};bar[8].note[0]=(uint8_t)snow_degree(t,t->p[SN_G_BASE],snow_prob(rng,50)?d:d+4);}
            else if(blen>14u&&snow_prob(rng,8)){bar[14]=(step_t){.time=ST_NOTE,.n=1,.vel=(uint8_t)snow_int(rng,60,84)};bar[14].note[0]=(uint8_t)snow_degree(t,t->p[SN_G_BASE],harm);}
        }else if(channel==5u){                                   /* CH: a held chord a bar (or two) */
            if(chords_per==2u&&(b&1u)&&snow_prob(rng,80))continue;
            uint32_t at=snow_prob(rng,75)?0u:snow_int(rng,1,blen>6u?6u:blen-1u),n=t->p[SN_PLAY_MODE]?1u:snow_prob(rng,30)?4u:3u;
            if(at>=blen)at=0;
            bar[at]=(step_t){.time=ST_NOTE,.n=(uint8_t)n,.vel=(uint8_t)snow_int(rng,58,88)};
            for(uint32_t k=0;k<n;k++){int32_t nd=harm+(int32_t)k*2;if(n>1u&&k<inv)nd+=7;bar[at].note[k]=(uint8_t)snow_degree(t,t->p[SN_G_BASE],nd);}
        }else{                                                   /* SY: a sparse, slow line */
            if(snow_prob(rng,shape==2u?(b&1u?70u:5u):18u))continue;          /* (resting bars; shape 2 answers a bar later) */
            uint32_t want=1u+density/45u+(snow_prob(rng,30)?1u:0u),pos=snow_int(rng,0,3)*2u;
            if(want>3u)want=3u;
            for(uint32_t k=0;k<want&&pos<blen;k++){
                int32_t step=shape==1u?1+(int32_t)snow_int(rng,0,1):(int32_t)snow_int(rng,0,4)-2;
                walk=clamp(walk+step,-span,span);
                bar[pos]=(step_t){.time=ST_NOTE,.n=1,.vel=(uint8_t)snow_int(rng,48,92)};
                bar[pos].note[0]=(uint8_t)snow_degree(t,t->p[SN_G_BASE],harm+walk+lift);
                if(shape==3u&&k==0u&&pos+3u<blen&&snow_prob(rng,50)){bar[pos+3u]=bar[pos];bar[pos+3u].vel=(uint8_t)(bar[pos].vel/2u+10u);bar[pos+3u].note[0]=(uint8_t)clamp(bar[pos].note[0]+12,0,120);}
                pos+=snow_int(rng,3,want>1u?7u:10u);
            }
        }
    }
    if(len==64u){                                         /* (the second half plays, and not as the first) */
        uint32_t first=64u,late=0;
        for(uint32_t i=0;i<64u;i++)if(out[i].time==ST_NOTE&&out[i].n){if(first==64u)first=i;if(i>=32u)late=1;}
        if(first==64u){first=0;out[0]=(step_t){.time=ST_NOTE,.n=1,.vel=80};out[0].note[0]=(uint8_t)snow_degree(t,t->p[SN_G_BASE],0);}
        if(!late){out[32u+first%32u]=out[first];}
        if(!memcmp(out,out+32,32u*sizeof *out)){
            uint32_t at=48u;while(at<64u&&!(out[at].time==ST_NOTE&&out[at].n))at++;
            if(at<64u)for(uint32_t k=0;k<out[at].n;k++)out[at].note[k]=(uint8_t)snow_degree(t,out[at].note[k],channel==4u?4:2);
            else{out[56]=out[first];out[56].vel=(uint8_t)(out[first].vel*3u/4u);}
        }
    }
    for(uint32_t i=0;i<len;i++)if(out[i].time==ST_NOTE&&out[i].n){        /* every note held to the next */
        uint32_t limit=channel==6u?snow_int(rng,4,12):len;
        for(uint32_t j=1;j<limit&&i+j<len&&out[i+j].time==ST_REST;j++){memset(&out[i+j],0,sizeof out[i+j]);out[i+j].time=ST_TIE;}
        uint32_t human=(uint32_t)(t->p[SN_G_HUMAN]+9)/10u;
        stattic_phrase_micro[i]=(int8_t)((int32_t)snow_int(rng,0,human*2u)-(int32_t)human);
    }
    (void)seed;
}
static void snow_phrase(track_t *t,uint32_t channel,uint32_t seed,step_t *out)
{
    uint32_t len=(uint32_t)clamp(t->p[P_SLEN],1,NSTEP),density=(uint32_t)t->p[SN_G_DENS],kind=(uint32_t)t->p[SN_G_KIND];
    uint32_t family=snow_hash(seed)%12u,rng=snow_hash(seed^(channel+1u)*0x9e3779b9u)|1u;
    uint32_t reach=2u+(uint32_t)t->p[SN_G_VARIANT]*len/127u;if(reach>=len)reach=len-1u;
    uint32_t rotate=(uint32_t)t->p[SN_G_ROTATE],offset=snow_int(&rng,0,reach),complex=(uint32_t)t->p[SN_G_COMPLEX];
    int preserve=kind==1u&&snow_hits(t->step,len)!=0;
    memcpy(out,t->step,sizeof t->step);
    memset(stattic_phrase_micro,0,sizeof stattic_phrase_micro);
    if(kind==3u&&channel>=4u&&density>0u){snow_ambient(t,channel,seed,out,len,density,&rng);return;}
    if(kind==3u)kind=2u;                               /* (AMBIENT on DR: its own generator, as ALL) */
    if(!preserve) {
        for(uint32_t i=0;i<len;i++){memset(&out[i],0,sizeof out[i]);out[i].time=ST_REST;}
        uint32_t pulses=0;
        if(channel==0u)pulses=snow_pulses(&rng,family==1||family==7?2:family==5?8:4,family==1||family==7?6:family==5?17:12,len,density);
        if(channel==1u)pulses=snow_pulses(&rng,2,7,len,density);
        if(channel==2u)pulses=snow_pulses(&rng,9,28,len,density);
        if(channel==3u){static const uint8_t primes[]={3,5,7,11,13,17};uint32_t p=primes[snow_int(&rng,0,5)];pulses=(p*len+16u)/32u;if(!pulses)pulses=1;pulses=density<=50?(pulses*density+25)/50:pulses+((len-pulses)*(density-50)+25)/50;}
        if(channel==4u)pulses=snow_pulses(&rng,family<4?3:6,family<4?9:16,len,density);
        if(channel==5u)pulses=snow_pulses(&rng,1,family<9?4:8,len,density);
        if(channel==6u)pulses=snow_pulses(&rng,4,family<6u?12:20,len,density);
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
            stattic_phrase_micro[i]=(int8_t)((int32_t)snow_int(&rng,0,human*2u)-(int32_t)human);
            if(family==1u&&(i&1u))stattic_phrase_micro[i]=(int8_t)clamp(stattic_phrase_micro[i]+4,NUDGE_MIN,NUDGE_MAX);
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
