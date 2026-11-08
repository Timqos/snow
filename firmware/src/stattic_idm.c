/* SPDX-License-Identifier: GPL-3.0-only
 * STATTIC drum generation (2026-11-08, the owner: IDM-like broken rhythms,
 * micro-drumming, as trap builds its beats but not trap itself, and as little
 * repetition as possible). Main loop only, never the audio callback.
 *
 * One category (kick, snare, hat, percussion) at a time, on its own lanes:
 *   base     the SLOOP/STATTIC phrase (snow_phrase), as before
 *   feel     (kick, snare) half-time (snare on 3, a broken kick around it),
 *            straight (snare on 2 and 4), or the base as it is. Half-time
 *            comes more often from 118 BPM up.
 *   grids    each beat of hat and percussion may take a grid of its own:
 *            a roll (ratchets x2/x3), eighth triplets (nudged steps), a ramp
 *            up or down (x1..x4), a drag (laid back), or a gap; kick doubles
 *   bursts   micro-drumming: 2..5 steps of quiet ratchets on hat, pedal hat
 *            and the percussion lanes; snare fills at a phrase's end
 *   memory   the last eight results of each category: a new one too close to
 *            one of them is made again (up to six tries, the most different kept)
 * COMPLEX (GEN 3) sets how far: 0 is the base alone, as before. A step's nudge
 * is shared by every lane on it, so a category nudges only steps it has alone. */
#define ST_IDM_MEM 8u
static struct { uint64_t hit,rat; } st_idm_seen[4][ST_IDM_MEM] __attribute__((section(".pool")));
static uint8_t st_idm_head[4];
enum { ST_FEEL_BASE, ST_FEEL_HALF, ST_FEEL_STRAIGHT };

static uint32_t st_idm_others(const dstep_t *d,uint32_t cat)
{
    uint32_t m=dstep_mask(d),o=0;
    for(uint32_t l=0;l<16u;l++)if(((m>>l)&1u)&&ST_LANE_CATEGORY[l]!=cat)o=1;
    return o;
}
static int st_idm_has(const dstep_t *d,uint32_t cat)
{
    uint32_t m=dstep_mask(d);
    for(uint32_t l=0;l<16u;l++)if(((m>>l)&1u)&&ST_LANE_CATEGORY[l]==cat)return 1;
    return 0;
}
static void st_idm_clear(stattic_pattern_t *p,uint32_t i,uint32_t cat)
{
    for(uint32_t l=0;l<16u;l++)if(ST_LANE_CATEGORY[l]==cat)dstep_clr(&p->drum[i],l);
}
/* the feel the snare already has: half-time when most bars hit on 3 and not on 2/4 */
static uint32_t stattic_feel_of(const stattic_pattern_t *p,uint32_t len)
{
    uint32_t half=0,straight=0,bars=len/16u;
    for(uint32_t b=0;b<bars;b++){
        const dstep_t *d=&p->drum[b*16u];
        int on3=st_idm_has(&d[8],1),on24=st_idm_has(&d[4],1)||st_idm_has(&d[12],1);
        half+=on3&&!on24;straight+=on24;
    }
    return bars&&half*2u>=bars?ST_FEEL_HALF:bars&&straight*2u>=bars?ST_FEEL_STRAIGHT:ST_FEEL_BASE;
}
static uint32_t stattic_feel_pick(uint32_t idm,uint32_t *r)
{
    if(!idm)return ST_FEEL_BASE;
    uint32_t bpm=(uint32_t)song.g[G_BPM],half=bpm>=118u?20u+idm/2u:8u+idm/5u,x=snow_int(r,0,99);
    return x<half?ST_FEEL_HALF:x<half+15u?ST_FEEL_STRAIGHT:ST_FEEL_BASE;
}
/* the half-time / straight foundation in place of the base's kick or snare */
static void st_idm_feel(step_t *ph,uint32_t len,uint32_t cat,uint32_t feel,uint32_t idm,uint32_t *r)
{
    if(feel==ST_FEEL_BASE||cat>1u||len<16u)return;
    if(cat==0u&&feel==ST_FEEL_STRAIGHT)return;                  /* (the base kick is metered already) */
    static const uint8_t kpos[9]={2,3,6,7,10,11,13,14,15},kw[9]={1,2,2,2,3,2,2,2,1};
    static const uint8_t ghost[7]={3,5,7,11,13,14,15};
    uint32_t bars=len/16u;
    for(uint32_t b=0;b<bars;b++){
        step_t *bar=ph+b*16u;
        if(b==2u&&snow_prob(r,35)){memcpy(bar,ph,16u*sizeof *ph);continue;}   /* (bar 3 may answer bar 1) */
        for(uint32_t i=0;i<16u;i++){memset(&bar[i],0,sizeof bar[i]);bar[i].time=ST_REST;}
        if(cat==0u){
            bar[0]=(step_t){.n=1,.time=ST_NOTE,.vel=(uint8_t)snow_int(r,112,127)};
            uint32_t n=snow_int(r,1,2u+idm/40u);
            for(uint32_t j=0;j<n;j++){
                uint32_t tot=0,x;for(uint32_t k=0;k<9u;k++)tot+=kw[k];
                x=snow_int(r,0,tot-1u);uint32_t k=0;while(x>=kw[k]){x-=kw[k];k++;}
                bar[kpos[k]]=(step_t){.n=1,.time=ST_NOTE,.vel=(uint8_t)snow_int(r,78,118)};
            }
        }else{
            if(feel==ST_FEEL_HALF)bar[8]=(step_t){.n=1,.time=ST_NOTE,.vel=(uint8_t)snow_int(r,112,127)};
            else{bar[4]=(step_t){.n=1,.time=ST_NOTE,.vel=(uint8_t)snow_int(r,108,124)};bar[12]=bar[4];}
            uint32_t n=snow_int(r,0,1u+idm/35u);
            for(uint32_t j=0;j<n;j++){uint32_t at=ghost[snow_int(r,0,6)];if(bar[at].time!=ST_NOTE)bar[at]=(step_t){.n=1,.time=ST_NOTE,.vel=(uint8_t)snow_int(r,30,52)};}
        }
    }
}
/* one beat (4 steps from s) of a category on lane ln: its own grid */
static void st_idm_beat(stattic_pattern_t *p,uint32_t s,uint32_t cat,uint32_t ln,uint32_t *r)
{
    uint32_t mode=snow_int(r,0,9),hits=0;
    for(uint32_t i=0;i<4u;i++)hits+=st_idm_has(&p->drum[s+i],cat);
    switch(mode){
    case 0:case 1:case 2:                                      /* roll: x2 (sometimes x3) */
        if(hits<2u)for(uint32_t i=0;i<4u;i++)if(snow_prob(r,70))dstep_set(&p->drum[s+i],ln,i?LV_GHOST:LV_SOFT,0);
        for(uint32_t i=0;i<4u;i++)for(uint32_t l=0;l<16u;l++)
            if(ST_LANE_CATEGORY[l]==cat&&dstep_has(&p->drum[s+i],l))dstep_set(&p->drum[s+i],l,dstep_lvl(&p->drum[s+i],l),snow_prob(r,25)?2u:1u);
        break;
    case 3:case 4:                                             /* eighth triplets: 0, 1+1/3, 3-1/3 */
        if(!st_idm_others(&p->drum[s+1],cat)&&!st_idm_others(&p->drum[s+3],cat)){
            for(uint32_t i=0;i<4u;i++)st_idm_clear(p,s+i,cat);
            dstep_set(&p->drum[s],ln,LV_NORM,0);dstep_set(&p->drum[s+1],ln,LV_SOFT,0);dstep_set(&p->drum[s+3],ln,LV_SOFT,0);
            p->micro[s+1]=21;p->micro[s+3]=-21;
            if(snow_prob(r,30))dstep_set(&p->drum[s+3],ln,LV_GHOST,2);   /* (the last one a 32T flurry) */
        }else dstep_set(&p->drum[s],ln,LV_SOFT,2);
        break;
    case 5:case 6:{                                            /* ramp: x1 x2 x3 x4, or down */
        int up=mode==5u;
        for(uint32_t i=0;i<4u;i++){
            uint32_t rat=up?i:3u-i;if(rat&&snow_prob(r,20))rat--;
            dstep_set(&p->drum[s+i],ln,up?(i<2u?LV_GHOST:LV_SOFT):(i<2u?LV_SOFT:LV_GHOST),rat);
        }
        break;}
    case 7:                                                    /* drag: laid back */
        for(uint32_t i=1;i<4u;i++)if(st_idm_has(&p->drum[s+i],cat)&&!st_idm_others(&p->drum[s+i],cat))p->micro[s+i]=(int8_t)(6u*i+snow_int(r,0,4));
        break;
    case 8:                                                    /* a gap */
        if(hits>1u)for(uint32_t i=1;i<4u;i++)st_idm_clear(p,s+i,cat);
        break;
    default:                                                   /* a single flam on its first hit */
        for(uint32_t i=0;i<4u;i++)for(uint32_t l=0;l<16u;l++)
            if(ST_LANE_CATEGORY[l]==cat&&dstep_has(&p->drum[s+i],l)){dstep_set(&p->drum[s+i],l,dstep_lvl(&p->drum[s+i],l),1);i=4u;break;}
        break;
    }
}
/* micro-drumming: a short burst of quiet ratchets */
static void st_idm_burst(stattic_pattern_t *p,uint32_t bar,uint32_t len,uint32_t cat,uint32_t *r)
{
    static const uint8_t hat[3]={4,4,6},perc[8]={7,9,10,11,12,13,14,15};
    uint32_t n=snow_int(r,2,5),s=bar+snow_int(r,cat==1u?10u:3u,16u-n);
    uint32_t ln=cat==2u?hat[snow_int(r,0,2)]:cat==3u?perc[snow_int(r,0,7)]:2u;
    for(uint32_t i=0;i<n&&s+i<len;i++){
        uint32_t rat=cat==1u?(i*3u+n-1u)/n:snow_int(r,0,3);    /* (a snare fill grows to x4) */
        if(cat!=1u&&rat==0u&&snow_prob(r,50))rat=1;
        uint32_t lvl=cat==1u?(i+1u==n?LV_HARD:i*2u<n?LV_GHOST:LV_SOFT):snow_prob(r,70)?LV_GHOST:LV_SOFT;
        if(cat!=1u&&i&&snow_prob(r,20))continue;               /* (holes in it) */
        dstep_set(&p->drum[s+i],ln,lvl,rat);
    }
}
/* one category, base + feel + grids + bursts, onto p (its lanes cleared first) */
static void stattic_dr_place(track_t *c,stattic_pattern_t *p,uint32_t cat,uint32_t seed,uint32_t feel,uint32_t *r)
{
    static const uint8_t lane[4]={0,2,4,13};
    uint32_t len=(uint32_t)clamp(c->p[P_SLEN],1,NSTEP),idm=(uint32_t)clamp(c->p[SN_G_COMPLEX],0,100);
    step_t phrase[NSTEP];
    for(uint32_t i=0;i<NSTEP;i++){st_idm_clear(p,i,cat);c->step[i].time=ST_REST;}
    snow_phrase(c,cat,seed,phrase);
    uint32_t dens=(uint32_t)c->p[SN_G_DENS];
    if(idm&&dens>0u&&dens<100u)st_idm_feel(phrase,len,cat,feel,idm,r);
    for(uint32_t i=0;i<NSTEP;i++)if(stattic_step_on(&phrase[i])){
        uint32_t ln=lane[cat];if(cat==2u&&snow_prob(r,14))ln=5;
        int alone=!st_idm_others(&p->drum[i],cat);
        dstep_set(&p->drum[i],ln,vel_lvl(phrase[i].vel),0);
        if(alone||!idm)p->micro[i]=stattic_phrase_micro[i];
    }
    if(!idm||!dens||dens>=100u)return;
    if(cat==0u){                                               /* kick doubles (x2), never on the downbeat */
        for(uint32_t i=1;i<len;i++)if(dstep_has(&p->drum[i],0)&&i%16u&&snow_prob(r,idm/7u))dstep_set(&p->drum[i],0,LV_SOFT,1);
        return;
    }
    for(uint32_t s=0;s+4u<=len;s+=4u)
        if(snow_prob(r,(cat==1u?idm/6u:idm*45u/100u)))st_idm_beat(p,s,cat,lane[cat],r);
    for(uint32_t b=0;b+16u<=len;b+=16u){
        if(cat==1u){if(((b/16u)&1u)&&snow_prob(r,idm*2u/3u))st_idm_burst(p,b,len,cat,r);continue;}
        if(snow_prob(r,idm*3u/5u))st_idm_burst(p,b,len,cat,r);
        if(snow_prob(r,idm/4u))st_idm_burst(p,b,len,cat,r);
    }
}
static void st_idm_sign(const stattic_pattern_t *p,uint32_t cat,uint32_t len,uint64_t *hit,uint64_t *rat)
{
    *hit=*rat=0;
    for(uint32_t i=0;i<len&&i<64u;i++)for(uint32_t l=0;l<16u;l++)
        if(ST_LANE_CATEGORY[l]==cat&&dstep_has(&p->drum[i],l)){*hit|=(uint64_t)1u<<i;if(dstep_rat(&p->drum[i],l))*rat|=(uint64_t)1u<<i;}
}
static uint32_t st_idm_pop(uint64_t x){uint32_t n=0;while(x){x&=x-1u;n++;}return n;}
/* the category made anew, not too close to its last eight */
static void stattic_dr_make(track_t *c,stattic_pattern_t *p,uint32_t cat,uint32_t seed,uint32_t feel,uint32_t *r)
{
    static dstep_t best[NSTEP] __attribute__((section(".pool"))),start[NSTEP] __attribute__((section(".pool")));
    static int8_t bestm[NSTEP] __attribute__((section(".pool"))),startm[NSTEP] __attribute__((section(".pool")));
    uint32_t len=(uint32_t)clamp(c->p[P_SLEN],1,NSTEP),bestd=0;cat&=3u;
    memcpy(start,p->drum,sizeof start);memcpy(startm,p->micro,sizeof startm);
    for(uint32_t t=0;t<6u;t++){
        if(t){memcpy(p->drum,start,sizeof start);memcpy(p->micro,startm,sizeof startm);}
        stattic_dr_place(c,p,cat,seed^t*0x9e3779b9u,feel,r);
        uint64_t h,rt;st_idm_sign(p,cat,len,&h,&rt);
        uint32_t near=~0u,n=st_idm_pop(h);
        for(uint32_t k=0;k<ST_IDM_MEM;k++){
            uint64_t sh=st_idm_seen[cat][k].hit,sr=st_idm_seen[cat][k].rat;
            if(!sh)continue;
            uint32_t d=st_idm_pop(h^sh)+st_idm_pop(rt^sr);if(d<near)near=d;
        }
        if(!t||near>bestd){bestd=near;memcpy(best,p->drum,sizeof best);memcpy(bestm,p->micro,sizeof bestm);}
        if(near>=(n/5u>3u?n/5u:3u))break;                      /* (different enough) */
    }
    memcpy(p->drum,best,sizeof best);memcpy(p->micro,bestm,sizeof bestm);
    uint64_t h,rt;st_idm_sign(p,cat,len,&h,&rt);
    if(h){uint32_t k=st_idm_head[cat]++%ST_IDM_MEM;st_idm_seen[cat][k].hit=h;st_idm_seen[cat][k].rat=rt;}
}
