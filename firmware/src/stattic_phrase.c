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
 * (2026-11-10, the owner: "variety without end, but harmony and the style kept") Few, long, slow notes, and
 * the parts listen to each other. Every bar has a chord of the scale: CH builds it over the bass it hears
 * (on the bass's note, or with the bass as its third or fifth), BS plays under the chords it hears (their
 * root, sometimes the third or the fifth), SY sings over CH's chords (or BS's), chord notes on its strong
 * beats. A part with nobody to hear walks a progression of its own, drawn anew each time. Then each part
 * draws its manner: BS whole notes, a fifth on the 3, a walk into the next bar, a dub figure, octaves, a
 * pickup, a drone; CH held, struck again, broken, floated, held over two bars, voiced next to the chord
 * before, sometimes with a 7th or suspended; SY a motif (one of 24 rhythms, ten shapes) developed bar by
 * bar: moved to the next chord, turned upside down, shifted, cut short, answered an octave up, rested or
 * replaced. Bar 3 may return bar 1. Every note is tied on (SY: a while) */
static uint32_t amb_n(const track_t *t){uint32_t m=scale_mask(t),n=0;for(uint32_t i=0;i<12u;i++)n+=(m>>i)&1u;return n?n:7u;}
static int32_t amb_mod(int32_t d,int32_t n){d%=n;return d<0?d+n:d;}
static int32_t amb_deg(const track_t *t,int32_t note)        /* scale steps from the BASE NOTE */
{
    uint32_t mask=scale_mask(t),root=(uint32_t)t->p[P_ROOT]%12u;
    int32_t base=scale_snap(t,clamp(t->p[SN_G_BASE],0,127)),d=0;note=scale_snap(t,clamp(note,0,127));
    if(note>=base){for(int32_t x=base+1;x<=note;x++)if((mask>>((uint32_t)(x+120-(int32_t)root)%12u))&1u)d++;}
    else for(int32_t x=base-1;x>=note;x--)if((mask>>((uint32_t)(x+120-(int32_t)root)%12u))&1u)d--;
    return d;
}
static const step_t *amb_part(uint32_t k)                     /* what a part plays (a pattern waiting first) */
{
    const stattic_change_t *q=&stattic_change[k];
    return q->armed&&(q->what&ST_CHANGE_PATTERN)?q->value.pattern.step:trk[k].step;
}
/* the degrees (mod n) sounding in bar b of a part, and its lowest note at the bar's start (else its first) */
static int amb_bar(const track_t *t,const step_t *s,uint32_t b,uint32_t blen,uint32_t *dm,int32_t *low)
{
    uint32_t start=b*blen,n=amb_n(t),got=0;int32_t first=999;*dm=0;*low=999;
    if(start>=NSTEP)return 0;
    int32_t i=(int32_t)start;while(i>0&&s[i].time==ST_TIE)i--;
    for(uint32_t x=(uint32_t)i;x<start+blen&&x<NSTEP;x++){
        if(x<start&&x!=(uint32_t)i)continue;
        if(!(s[x].time==ST_NOTE&&s[x].n))continue;
        int32_t lowest=999;
        for(uint32_t j=0;j<s[x].n&&j<4u;j++){int32_t d=amb_deg(t,s[x].note[j]);*dm|=1u<<amb_mod(d,(int32_t)n);if(d<lowest)lowest=d;got=1;}
        if(x<=start){if(lowest<*low)*low=lowest;}
        else if(first==999)first=lowest;
    }
    if(*low==999)*low=first==999?0:first;
    return got;
}
static int32_t amb_root(uint32_t dm,int32_t low,int32_t n)   /* the chord a set of degrees spells */
{
    int32_t best=amb_mod(low,n),score=-1;
    for(int32_t r=0;r<n;r++)if((dm>>r)&1u){
        int32_t sc=2*(int32_t)((dm>>amb_mod(r+2,n))&1u)+(int32_t)((dm>>amb_mod(r+4,n))&1u)+(r==amb_mod(low,n));
        if(sc>score){score=sc;best=r;}
    }
    return best;
}
static uint16_t amb_pc(const track_t *t,int32_t d){return (uint16_t)(1u<<(snow_degree(t,t->p[SN_G_BASE],d)%12));}
static uint16_t amb_triad(const track_t *t,int32_t r){return (uint16_t)(amb_pc(t,r)|amb_pc(t,r+2)|amb_pc(t,r+4));}
static int amb_rubs(uint16_t a,uint16_t ctx)                 /* a pitch a semitone from one in ctx (and not in it) */
{
    if(a&ctx)return 0;
    uint16_t near=(uint16_t)(((a<<1)|(a>>11)|(a>>1)|(a<<11))&0xFFFu);
    return (near&ctx)!=0;
}
/* the pitches sounding in bar b of a part */
static uint16_t amb_pcs(const step_t *s,uint32_t b,uint32_t blen)
{
    uint32_t start=b*blen;uint16_t m=0;if(start>=NSTEP)return 0;
    int32_t i=(int32_t)start;while(i>0&&s[i].time==ST_TIE)i--;
    for(uint32_t x=(uint32_t)i;x<start+blen&&x<NSTEP;x++){
        if(x<start&&x!=(uint32_t)i)continue;
        if(s[x].time==ST_NOTE)for(uint32_t j=0;j<s[x].n&&j<4u;j++)m|=(uint16_t)(1u<<(s[x].note[j]%12u));
    }
    return m;
}
/* (no long semitone rubs) a note that grates on the chord or on the others goes to the nearest chord note */
static int32_t amb_fit(const track_t *t,int32_t d,int32_t r,uint16_t ctx,uint32_t n)
{
    if(!amb_rubs(amb_pc(t,d),(uint16_t)(ctx|amb_triad(t,r))))return d;
    int32_t best=r,bd=1<<30;
    for(int32_t q=0;q<3;q++)for(int32_t o=-1;o<=1;o++){int32_t w=r+2*q+o*(int32_t)n,dd=w-d;if(dd<0)dd=-dd;
        if(dd<bd&&!amb_rubs(amb_pc(t,w),ctx)){bd=dd;best=w;}}
    return best;
}
static void amb_progression(int32_t *h,uint32_t bars,uint32_t *rng)
{
    static const int8_t next_of[7][5]={{3,4,5,1,2},{4,6,3,4,5},{5,3,0,1,3},{4,0,1,6,0},{0,5,3,2,0},{3,1,4,2,0},{0,2,5,3,0}};
    int32_t hold=snow_prob(rng,35)?2:1;
    h[0]=snow_prob(rng,70)?0:snow_prob(rng,50)?5:3;
    for(uint32_t b=1;b<bars;b++)h[b]=(b%(uint32_t)hold)?h[b-1]:next_of[h[b-1]%7][snow_int(rng,0,4)];
    if(bars==4u&&snow_prob(rng,30))h[3]=4;                   /* (now and then a turn home) */
}
static uint8_t stattic_amb_lead;                      /* (HOME 2 K1 on every part: the others follow CH) */
static void amb_note(step_t *s,uint32_t vel){*s=(step_t){.time=ST_NOTE,.n=1,.vel=(uint8_t)vel};}
static void snow_ambient(track_t *t,uint32_t channel,uint32_t seed,step_t *out,uint32_t len,uint32_t density,uint32_t *rng)
{
    static const uint8_t cells[24][5]={{1,0},{1,8},{1,4},{1,2},{2,0,8},{2,0,6},{2,0,10},{2,2,8},{2,4,12},{2,0,12},{2,6,14},{2,0,3},
        {3,0,3,8},{3,0,6,12},{3,0,4,10},{3,2,6,12},{3,0,8,14},{3,0,10,12},{3,4,8,11},{4,0,4,8,12},{4,0,3,6,12},{4,0,2,8,10},{4,0,6,8,14},{4,2,4,10,12}};
    static const int8_t shapes[10][3]={{2,2,3},{-2,-2,-3},{1,1,1},{-1,-1,-1},{1,-1,0},{2,-2,2},{4,-1,-1},{0,0,7},{-1,2,-1},{3,-1,-2}};
    uint32_t bars=len>=16u?len/16u:1u,blen=len>=16u?16u:len,n=amb_n(t);
    if(bars>4u)bars=4u;
    int32_t harm[4]={0,0,0,0},span=clamp(t->p[SN_G_SPAN],1,24);
    uint32_t heard=channel==5u?0u:2u,also=channel==6u?0u:0xFFu,have=0;      /* CH hears BS; BS and SY hear CH (SY then BS) */
    for(uint32_t pass=0;pass<2u&&!have;pass++){
        uint32_t k=pass?also:heard;if(k>2u)continue;
        const step_t *s=amb_part(k);
        for(uint32_t b=0;b<bars;b++){
            uint32_t dm;int32_t low;
            if(!amb_bar(t,s,b,blen,&dm,&low)){harm[b]=b?harm[b-1]:0;continue;}
            have=1;
            if(channel==5u){                                     /* CH over the bass: on it, or the bass its 3rd / 5th */
                int32_t bd=amb_mod(low,(int32_t)n),r=bd;uint32_t c=snow_int(rng,0,99);
                if(n==7u&&c>=55u)r=amb_mod(bd-(c<80u?2:4),7);
                if(n==7u&&r==6)r=bd;                             /* (no diminished chord) */
                harm[b]=r;
            }else harm[b]=k==0u?amb_mod(low,(int32_t)n):amb_root(dm,low,(int32_t)n);
        }
    }
    if(channel==5u&&stattic_amb_lead&&snow_prob(rng,40))have=0;   /* (all parts made at once: CH may lead anew) */
    if(!have)amb_progression(harm,bars,rng);
    uint16_t ctx[4]={0,0,0,0};
    for(uint32_t b=0;b<bars;b++){
        if(channel!=4u)ctx[b]|=amb_pcs(amb_part(0),b,blen);
        if(channel!=5u&&have)ctx[b]|=amb_pcs(amb_part(2),b,blen);
    }
    for(uint32_t i=0;i<len;i++){memset(&out[i],0,sizeof out[i]);out[i].time=ST_REST;}
    int32_t base=0;
    if(channel==4u){                                             /* ---- BS ---- */
        uint32_t style=snow_int(rng,0,6);int32_t prev=harm[0]>3?harm[0]-7:harm[0];
        for(uint32_t b=0;b<bars;b++){
            step_t *bar=out+b*blen;uint32_t st=snow_prob(rng,20)?snow_int(rng,0,6):style;
            if(b==2u&&bars==4u&&snow_prob(rng,30)&&harm[2]==harm[0]){memcpy(bar,out,blen*sizeof *out);continue;}
            int32_t r=harm[b],d=r;uint32_t c=snow_int(rng,0,99);
            if(have&&c>=70u)d=r+(c<85u?2:4);                     /* (the chord's 3rd or 5th in the bass) */
            if(d-prev>3)d-=(int32_t)n;else if(prev-d>3)d+=(int32_t)n;
            if(d>5)d-=(int32_t)n;
            if(d<-6)d+=(int32_t)n;
            prev=d;int32_t nx=b+1u<bars?harm[b+1u]:harm[0];if(nx-d>3)nx-=(int32_t)n;else if(d-nx>3)nx+=(int32_t)n;
            if(st==6u&&(!b||harm[b]!=harm[b-1u]))st=0;      /* (a drone only over the same chord) */
            d=amb_fit(t,d,r,ctx[b],n);amb_note(&bar[0],snow_int(rng,84,108));bar[0].note[0]=(uint8_t)snow_degree(t,t->p[SN_G_BASE],d);
            if(st==1u&&blen>8u){amb_note(&bar[8],snow_int(rng,70,96));bar[8].note[0]=(uint8_t)snow_degree(t,t->p[SN_G_BASE],amb_fit(t,d+(snow_prob(rng,70)?4:-3),r,ctx[b],n));}
            else if(st==2u&&blen>14u){uint32_t at=snow_prob(rng,50)?12u:14u;amb_note(&bar[at],snow_int(rng,64,90));bar[at].note[0]=(uint8_t)snow_degree(t,t->p[SN_G_BASE],amb_fit(t,nx+(nx>=d?-1:1),r,ctx[b],n));}
            else if(st==3u&&blen>10u){uint32_t at=snow_prob(rng,50)?10u:6u;amb_note(&bar[at],snow_int(rng,60,86));bar[at].note[0]=(uint8_t)snow_degree(t,t->p[SN_G_BASE],amb_fit(t,snow_prob(rng,60)?d:d+4,r,ctx[b],n));}
            else if(st==4u&&blen>8u){amb_note(&bar[8],snow_int(rng,66,90));bar[8].note[0]=(uint8_t)snow_degree(t,t->p[SN_G_BASE],d+(int32_t)n);}
            else if(st==5u&&b+1u<bars&&blen>14u){amb_note(&bar[14],snow_int(rng,62,84));bar[14].note[0]=(uint8_t)snow_degree(t,t->p[SN_G_BASE],amb_fit(t,nx,r,ctx[b],n));}
            else if(st==6u&&b)memset(&bar[0],0,sizeof bar[0]),bar[0].time=ST_REST;   /* (a drone: the note before goes on) */
        }
        if(out[0].time!=ST_NOTE){amb_note(&out[0],96);out[0].note[0]=(uint8_t)snow_degree(t,t->p[SN_G_BASE],harm[0]>3?harm[0]-7:harm[0]);}
    }else if(channel==5u){                                       /* ---- CH ---- */
        uint32_t style=snow_int(rng,0,5),voices=t->p[SN_PLAY_MODE]?1u:3u;int32_t last[4]={0,2,4,7};uint32_t lastn=0;
        for(uint32_t b=0;b<bars;b++){
            step_t *bar=out+b*blen;uint32_t st=snow_prob(rng,25)?snow_int(rng,0,5):style;
            if(b==2u&&bars==4u&&snow_prob(rng,30)&&harm[2]==harm[0]){memcpy(bar,out,blen*sizeof *out);continue;}
            if(st==4u&&(b&1u)&&harm[b]==harm[b-1])continue;              /* (held over two bars) */
            int32_t r=harm[b],c[4];uint32_t m=voices==1u?1u:snow_prob(rng,25)?4u:3u,colour=snow_int(rng,0,99);
            c[0]=r;c[1]=r+2;c[2]=r+4;c[3]=r+6;
            if(m==3u&&colour<12u)c[1]=r+1;else if(m==3u&&colour<22u)c[1]=r+3;   /* (sus2 / sus4) */
            if(m==3u&&c[1]!=r+2&&amb_rubs(amb_pc(t,c[1]),(uint16_t)(ctx[b]|amb_pc(t,r)|amb_pc(t,r+4))))c[1]=r+2;
            if(m==4u&&amb_rubs(amb_pc(t,c[3]),(uint16_t)(ctx[b]|amb_triad(t,r)))){c[3]=r+7+1;   /* (an added 9th) */
                if(amb_rubs(amb_pc(t,c[3]),(uint16_t)(ctx[b]|amb_triad(t,r))))m=3u;}
            if(voices==1u){c[0]=amb_fit(t,r+(int32_t)(snow_int(rng,0,2)*2u),r,ctx[b],n);}
            int32_t best[4]={c[0],c[1],c[2],c[3]},bestcost=1<<30;                              /* the voicing nearest the chord before */
            for(uint32_t inv=0;inv<3u;inv++){
                int32_t v[4],cost=0;
                for(uint32_t k=0;k<m;k++){v[k]=c[k]+(k<inv?7:0);while(v[k]>9)v[k]-=7;}
                for(uint32_t k=0;k<m&&lastn;k++){int32_t dd=v[k]-last[k<lastn?k:lastn-1u];cost+=dd<0?-dd:dd;}
                cost+=(int32_t)snow_int(rng,0,3);
                if(cost<bestcost){bestcost=cost;memcpy(best,v,sizeof v);}
            }
            memcpy(last,best,sizeof last);lastn=m;
            uint32_t at=st==5u?snow_int(rng,1,blen>6u?6u:blen-1u):0u;if(at>=blen)at=0;
            if(st==2u&&m>1u&&blen>=8u){                                  /* broken: one by one, then together */
                for(uint32_t k=0;k<m;k++){amb_note(&bar[k*2u],snow_int(rng,52,80));bar[k*2u].note[0]=(uint8_t)snow_degree(t,t->p[SN_G_BASE],best[k]);}
                at=snow_prob(rng,50)?8u:(uint32_t)(m*2u);if(at>=blen)continue;
            }
            bar[at]=(step_t){.time=ST_NOTE,.n=(uint8_t)m,.vel=(uint8_t)snow_int(rng,58,88)};
            for(uint32_t k=0;k<m;k++)bar[at].note[k]=(uint8_t)snow_degree(t,t->p[SN_G_BASE],best[k]);
            if(st==1u&&blen>8u&&at<8u){bar[8]=bar[at];bar[8].vel=(uint8_t)(bar[at].vel*3u/4u);}   /* struck again on the 3 */
        }
    }else{                                                       /* ---- SY ---- */
        uint32_t want=1u+density/30u;if(want>4u)want=4u;
        int32_t lift=snow_prob(rng,40)?(int32_t)n:0,ivl[3];uint32_t cell=0;
        int32_t prevstart=harm[0]+lift,prevnotes[4];uint32_t prevcount=0;uint8_t prevpos[4];
        for(uint32_t b=0;b<bars;b++){
            step_t *bar=out+b*blen;uint32_t dev=snow_int(rng,0,99);
            if(b==2u&&bars==4u&&snow_prob(rng,25)){memcpy(bar,out,blen*sizeof *out);continue;}
            int fresh=!b||dev<15u;
            if(b&&b+1u<bars&&dev>=15u&&dev<25u)continue;                       /* a bar of rest */
            if(fresh){
                do cell=snow_int(rng,0,23);while(cells[cell][0]>want);
                const int8_t *sh=shapes[snow_int(rng,0,9)];for(uint32_t k=0;k<3u;k++)ivl[k]=sh[k];
            }else if(dev<40u){for(uint32_t k=0;k<3u;k++)ivl[k]=-ivl[k];}           /* upside down */
            uint32_t cnt=cells[cell][0];uint8_t pos[4];
            for(uint32_t k=0;k<cnt;k++)pos[k]=cells[cell][1u+k];
            if(!fresh&&dev>=40u&&dev<55u){int32_t sh2=snow_prob(rng,50)?2:-2;for(uint32_t k=0;k<cnt;k++)pos[k]=(uint8_t)clamp((int32_t)pos[k]+sh2,0,(int32_t)blen-1);}
            if(!fresh&&dev>=55u&&dev<65u&&cnt>1u)cnt=(cnt+1u)/2u;               /* cut short */
            int32_t tones[3]={harm[b],harm[b]+2,harm[b]+4},start=tones[0]+lift,bd=1<<30;
            for(uint32_t k=0;k<3u;k++)for(int32_t o=-1;o<=1;o++){int32_t v=tones[k]+lift+o*(int32_t)n,dd=v-prevstart;if(dd<0)dd=-dd;if(dd<bd){bd=dd;start=v;}}
            int32_t v=start;
            for(uint32_t k=0;k<cnt&&pos[k]<blen;k++){
                if(k)v+=ivl[(k-1u)%3u];
                if(v>span+lift)v-=(int32_t)n;
                if(v<-span+lift)v+=(int32_t)n;
                if(k&&(pos[k]==8u||pos[k]==0u)){int32_t cb=v,cd=1<<30;for(uint32_t q=0;q<3u;q++)for(int32_t o=-1;o<=1;o++){int32_t w=tones[q]+o*(int32_t)n,dd=w-v;if(dd<0)dd=-dd;if(dd<cd){cd=dd;cb=w;}}v=cb;}   /* (a chord note on the 1 and the 3) */
                v=amb_fit(t,v,harm[b],ctx[b],n);
                amb_note(&bar[pos[k]],k?snow_int(rng,46,82):snow_int(rng,60,92));
                bar[pos[k]].note[0]=(uint8_t)snow_degree(t,t->p[SN_G_BASE],v);
                if(prevcount<4u){prevnotes[prevcount]=v;prevpos[prevcount]=pos[k];prevcount++;}
            }
            if(!fresh&&dev>=65u&&dev<75u&&b){                                    /* the bar before answered an octave up */
                for(uint32_t i=0;i<blen;i++)if(bar[(int32_t)i-(int32_t)blen].time==ST_NOTE&&bar[(int32_t)i-(int32_t)blen].n&&bar[i].time!=ST_NOTE){
                    bar[i]=bar[(int32_t)i-(int32_t)blen];bar[i].vel=(uint8_t)(bar[i].vel/2u+12u);bar[i].note[0]=(uint8_t)clamp(bar[i].note[0]+12,0,120);break;
                }
            }
            prevstart=start;prevcount=0;
        }
        (void)prevnotes;(void)prevpos;
        uint32_t lastat=len;for(uint32_t i=0;i<len;i++)if(out[i].time==ST_NOTE&&out[i].n)lastat=i;
        if(lastat<len){int32_t d=amb_deg(t,out[lastat].note[0]),b=(int32_t)(lastat/blen),cb=d,cd=1<<30;   /* (the last note rests on the chord) */
            for(int32_t q=0;q<3;q++)for(int32_t o=-1;o<=1;o++){int32_t w=harm[b<4?b:3]+2*q+o*(int32_t)n,dd=w-d;if(dd<0)dd=-dd;if(dd<cd){cd=dd;cb=w;}}
            out[lastat].note[0]=(uint8_t)snow_degree(t,t->p[SN_G_BASE],cb);}
    }
    (void)base;
    if(len==64u){                                         /* (the second half plays, and not as the first) */
        uint32_t first=64u,late=0;
        for(uint32_t i=0;i<64u;i++)if(out[i].time==ST_NOTE&&out[i].n){if(first==64u)first=i;if(i>=32u)late=1;}
        if(first==64u){first=0;out[0]=(step_t){.time=ST_NOTE,.n=1,.vel=80};out[0].note[0]=(uint8_t)snow_degree(t,t->p[SN_G_BASE],harm[0]);}
        if(!late){out[32u+first%32u]=out[first];}
        if(!memcmp(out,out+32,32u*sizeof *out)){
            uint32_t at=48u;while(at<64u&&!(out[at].time==ST_NOTE&&out[at].n))at++;
            if(at<64u&&channel==6u)for(uint32_t k=0;k<out[at].n;k++)out[at].note[k]=(uint8_t)snow_degree(t,out[at].note[k],2);
            else if(at<56u&&out[at+8u].time!=ST_NOTE){out[at+8u]=out[at];out[at+8u].vel=(uint8_t)(out[at].vel*3u/4u);}
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
                if(channel==5u)while(nd>9)nd-=7;                 /* (2026-11-09, the owner: no high chords) */
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
