/* SPDX-License-Identifier: GPL-3.0-only */
/* Main-loop preparation, bounded publication, shared transport-bar commit.
 * This module never generates a phrase, rolls a random seed or writes flash
 * from the audio callback. The native SLOOP sequencer consumes committed data. */
static int stattic_step_on(const step_t *s) { return s->time==ST_NOTE&&s->n; }
#include "stattic_phrase.c"
#include "stattic_random.c"
#include "stattic_idm.c"

typedef struct { stattic_snapshot_t value;uint32_t session;uint8_t track; } stattic_undo_t;
/* Split fixed undo storage across the two native RAM regions. No allocation
 * and no reduction of the eight-entry history or effect buffers. */
static stattic_undo_t stattic_history_a[4] __attribute__((section(".lowram")));
static stattic_undo_t stattic_history_b[4];
static uint8_t stattic_history_base;
static stattic_undo_t *stattic_history_entry(uint32_t i)
{ i=(i+stattic_history_base)&7u;return i<4u?&stattic_history_a[i]:&stattic_history_b[i-4u]; }
static uint8_t stattic_history_n,stattic_history_at;
static uint32_t stattic_action=16,stattic_serial;
static struct { stattic_snapshot_t track[NTRK];int16_t g[G_COUNT];uint8_t valid,mask; } stattic_saved __attribute__((section(".pool")));
static uint8_t stattic_control_mask=15u;
static struct { int16_t g[G_COUNT];uint32_t bar;volatile uint8_t armed; } stattic_global_restore;
static uint16_t stattic_live_pass[NTRK];

static void stattic_pattern_capture(const track_t *t,stattic_pattern_t *p)
{
    memcpy(p->step,t->step,sizeof p->step);memcpy(p->micro,t->micro,sizeof p->micro);
    memcpy(p->fill,t->fill,sizeof p->fill);memcpy(p->lock,t->lock,sizeof p->lock);
}
static void stattic_pattern_apply(track_t *t,const stattic_pattern_t *p)
{
    memset(stattic_step_time[(uint32_t)(t-trk)],0,sizeof stattic_step_time[0]);
    seq_release(t);locks_restore(t);seq_out_track_off(t);
    memcpy(t->step,p->step,sizeof t->step);memcpy(t->micro,p->micro,sizeof t->micro);
    memcpy(t->fill,p->fill,sizeof t->fill);memcpy(t->lock,p->lock,sizeof t->lock);
    for(uint32_t i=0;i<NSTEP;i++){
        t->micro[i]=(int8_t)clamp(t->micro[i],MICRO_MIN,MICRO_MAX);
        if(!is_drum(t)){
            step_t *s=&t->step[i];if(s->n>4u)s->n=4;if(s->time>ST_REST)s->time=ST_REST;
            s->vel&=127u;for(uint32_t j=0;j<4;j++)s->note[j]&=127u;
        }
    }
    for(uint32_t i=0;i<NLOCK;i++){
        plock_t *l=&t->lock[i];
        if(l->step>=NSTEP||l->param>=P_COUNT||!p_lockable(l->param))l->step=LOCK_FREE;
        else {const param_desc_t *d=track_desc(t,l->param);l->val=(int16_t)clamp(l->val,d->min,d->max);}
    }
    t->seq_abs=SEQ_NONE;t->seq_idx=0;t->seq_n=t->seq_hold=t->rh_n=0;
    t->seq_active=(uint8_t)!track_empty(t);
}
static void stattic_capture(uint32_t k,stattic_snapshot_t *s)
{
    track_t *t=&trk[k];
    for(uint32_t id=0;id<P_COUNT;id++)s->p[id]=stattic_base_value(t,id);
    stattic_pattern_capture(t,&s->pattern);memcpy(s->offset,snow_offset[k],sizeof s->offset);
    memcpy(s->shots,stattic_drum_pick,sizeof s->shots);s->slot=stattic_current[k];
}
static void stattic_mask_all(uint32_t *mask)
{
    for(uint32_t i=0;i<ST_PARAM_WORDS;i++)mask[i]=0xffffffffu;
}
static uint32_t stattic_when(void) { return song.playing?clk_beat/4u+1u:0u; }
static void stattic_publish(uint32_t k,const stattic_snapshot_t *s,const uint32_t *mask,uint32_t what,uint32_t bar)
{
    stattic_change_t *q=&stattic_change[k];q->armed=0;
    q->value=*s;memcpy(q->mask,mask,sizeof q->mask);q->what=(uint8_t)what;q->slot=s->slot;
    q->bar=bar;q->serial=++stattic_serial;RING_PUBLISH();q->armed=1;
    stattic_revision++;stattic_ui_dirty=1;
}
static void stattic_undo_mark(const track_t *t,uint32_t session)
{
    if(!stattic_ready)return;
    uint32_t k=(uint32_t)(t-trk);
    if(stattic_history_at&&stattic_history_at==stattic_history_n){
        stattic_undo_t *last=&(*stattic_history_entry(stattic_history_at-1u));
        if(last->track==k&&last->session==session)return;
    }
    stattic_history_n=stattic_history_at;
    if(stattic_history_n==8u){
        stattic_history_base=(stattic_history_base+1u)&7u;
        stattic_history_n--;stattic_history_at--;
    }
    stattic_undo_t *u=&(*stattic_history_entry(stattic_history_n++));u->track=(uint8_t)k;u->session=session;
    stattic_capture(k,&u->value);stattic_history_at=stattic_history_n;
}
static int stattic_undo(int redo)
{
    uint32_t f=STATTIC_GUARD_SAVE(),mask[ST_PARAM_WORDS];stattic_mask_all(mask);
    /* (fix) undo/redo bring back sounds and patterns, never a MUTE from the time of the
     * snapshot: mute has its own bar queue (stattic_mute_set); restoring it here
     * re-muted channels the player had unmuted since */
    mask[P_MUTE/32u]&=~(1u<<(P_MUTE%32u));
    if(redo?stattic_history_at>=stattic_history_n:!stattic_history_at){STATTIC_GUARD_RESTORE(f);return 0;}
    uint32_t index=redo?stattic_history_at:stattic_history_at-1u,group=(*stattic_history_entry(index)).session;
    uint32_t bar=stattic_when();
    do{
        index=redo?stattic_history_at:stattic_history_at-1u;
        stattic_undo_t *u=&(*stattic_history_entry(index));stattic_snapshot_t now;
        if(stattic_change[u->track].armed)now=stattic_change[u->track].value;else stattic_capture(u->track,&now);
        stattic_publish(u->track,&u->value,mask,ST_CHANGE_PATTERN|ST_CHANGE_SOUND|ST_CHANGE_SHOTS,bar);
        u->value=now;
        if(redo)stattic_history_at++;else stattic_history_at--;
    }while((redo?stattic_history_at<stattic_history_n:stattic_history_at>0u)&&
        (*stattic_history_entry(redo?stattic_history_at:stattic_history_at-1u)).session==group);
    STATTIC_GUARD_RESTORE(f);return 1;
}
static void stattic_runtime_init(void)
{
    memset(stattic_banks,0,sizeof stattic_banks);memset(stattic_change,0,sizeof stattic_change);
    memset(stattic_current,0,sizeof stattic_current);memset(stattic_origin,0,sizeof stattic_origin);
    memset(stattic_step_time,0,sizeof stattic_step_time);
    memset(stattic_live_pass,0,sizeof stattic_live_pass);memset(snow_offset,0,sizeof snow_offset);
    stattic_history_base=stattic_history_n=stattic_history_at=0;stattic_saved.valid=0;stattic_control_mask=15;
    stattic_staging=0;stattic_ms_pending=stattic_home_ms=0;stattic_global_restore.armed=0;
    for(uint32_t k=0;k<NTRK;k++)for(uint32_t slot=0;slot<ST_PATTERNS;slot++)
        stattic_pattern_capture(&trk[k],&stattic_banks[k][slot]);
    memset(&stattic_art_shuffle,0,sizeof stattic_art_shuffle);
    stattic_art_shuffle.random=0x73746174u^fm1_ms;stattic_art_shuffle.last=stattic_art_shuffle.page=stattic_art_shuffle.knob=255;
    for(uint32_t page=0;page<3;page++){stattic_art_pick[page]=(uint8_t)page;stattic_scroll[page]=0;}
    stattic_ready=1;stattic_revision++;
}
static void stattic_runtime_start(void)
{
    memset(stattic_step_time,0,sizeof stattic_step_time);
    memset(stattic_origin,0,sizeof stattic_origin);memset(stattic_live_pass,0,sizeof stattic_live_pass);
    if(stattic_ms_pending)stattic_ms_bar=0;
    if(stattic_global_restore.armed)stattic_global_restore.bar=0;
    for(uint32_t k=0;k<NTRK;k++)if(stattic_change[k].armed)stattic_change[k].bar=0;
}
static void stattic_runtime_tick(void)
{
    if(!stattic_ready)return;
    uint32_t bar=clk_beat/4u;
    if(stattic_global_restore.armed&&(!song.playing||(int32_t)(bar-stattic_global_restore.bar)>=0)){
        for(uint32_t g=0;g<G_COUNT;g++)
            if(g!=G_LOAD&&g!=G_SAVE&&g!=G_CLRSEQ&&g!=G_INITSND&&g!=G_NEWPRJ&&g!=G_INFO)
                song.g[g]=(int16_t)clamp(stattic_global_restore.g[g],GP[g].min,GP[g].max);
        stattic_control_mask=stattic_saved.mask;stattic_global_restore.armed=0;
    }
    for(uint32_t k=0;k<NTRK;k++){
        stattic_change_t *q=&stattic_change[k];track_t *t=&trk[k];
        if(((stattic_staging>>k)&1u)||!q->armed||(song.playing&&(int32_t)(bar-q->bar)<0))continue;
        if(q->what&ST_CHANGE_PATTERN){
            stattic_pattern_capture(t,&stattic_banks[k][stattic_current[k]]);
            stattic_pattern_apply(t,&q->value.pattern);stattic_current[k]=q->slot%ST_PATTERNS;
            stattic_banks[k][stattic_current[k]]=q->value.pattern;
            stattic_origin[k]=song.playing?bar*4u:0u;
        }
        if((q->what&ST_CHANGE_FADE)&&k<4u)stattic_fade_stage[k]=1;
        if(q->what&ST_CHANGE_SOUND){
            locks_restore(t);
            for(uint32_t id=0;id<P_COUNT;id++)if((q->mask[id/32u]>>(id%32u))&1u)t->p[id]=q->value.p[id];
            memcpy(snow_offset[k],q->value.offset,sizeof q->value.offset);
            if(k==2u)t->p[P_CHORD]=t->p[SN_PLAY_MODE]?0:1;
        }
        if((q->what&ST_CHANGE_SHOTS)&&k==TRK_DRUM){
            for(uint32_t l=0;l<16;l++){
                uint8_t shot=q->value.shots[l];
                stattic_drum_pick[l]=stattic_shot_zone(shot,ST_LANE_CATEGORY[l])>=0?shot:(uint8_t)(ST_LANE_CATEGORY[l]*10u);
            }
            t->p[P_E0]=DRUM_STATTIC;
        }
        q->armed=0;stattic_revision++;stattic_ui_dirty=1;
    }
    if(stattic_ms_pending&&(!song.playing||(int32_t)(bar-stattic_ms_bar)>=0)){
        for(uint32_t k=0;k<NTRK;k++)trk[k].p[P_MUTE]=(stattic_ms_mute>>k)&1u;
        song.solo=stattic_ms_solo;stattic_ms_pending=0;stattic_ui_dirty=1;stattic_revision++;
    }
}
static void stattic_ms_begin(void)
{
    if(!stattic_ms_pending){
        stattic_ms_mute=0;for(uint32_t k=0;k<NTRK;k++)stattic_ms_mute|=(uint8_t)(!!trk[k].p[P_MUTE]<<k);
        stattic_ms_solo=song.solo;
    }
}
static void stattic_mute_set(uint32_t track,uint32_t value)
{
    if(track>=NTRK)return;uint32_t f=STATTIC_GUARD_SAVE();stattic_ms_begin();
    stattic_ms_mute=(uint8_t)((stattic_ms_mute&~(1u<<track))|((value!=0u)<<track));
    stattic_ms_bar=stattic_when();RING_PUBLISH();stattic_ms_pending=1;stattic_ui_dirty=1;STATTIC_GUARD_RESTORE(f);
}
static void stattic_solo_set(uint32_t bits)
{
    uint32_t f=STATTIC_GUARD_SAVE();stattic_ms_begin();stattic_ms_solo=(uint8_t)(bits&15u);
    stattic_ms_bar=stattic_when();RING_PUBLISH();stattic_ms_pending=1;stattic_ui_dirty=1;STATTIC_GUARD_RESTORE(f);
}
static void stattic_ms_key(uint32_t key,int down)
{
    if(!down)return;
    uint32_t white=0,black=0,rank=0;int is_black=0;
    for(uint32_t k=0;k<=key;k++){
        static const uint16_t black_mask=(1u<<1)|(1u<<3)|(1u<<5)|(1u<<8)|(1u<<10); /* F3-based keys */
        is_black=(black_mask>>(k%12u))&1u;
        if(is_black)rank=black++;else rank=white++;
    }
    if(rank>=STATTIC_CHANNELS)return;
    uint32_t track=stattic_role_track(rank);
    if(!stattic_ms_pending){
        stattic_ms_mute=0;for(uint32_t k=0;k<NTRK;k++)stattic_ms_mute|=(uint8_t)(!!trk[k].p[P_MUTE]<<k);
        stattic_ms_solo=song.solo;
    }
    if(is_black)stattic_ms_solo^=(uint8_t)(1u<<track);else stattic_ms_mute^=(uint8_t)(1u<<track);
    stattic_ms_bar=stattic_when();RING_PUBLISH();stattic_ms_pending=1;stattic_ui_dirty=1;
}
static uint32_t stattic_art_random(void)
{
    uint32_t r=stattic_art_shuffle.random;r^=r<<13;r^=r>>17;r^=r<<5;
    return stattic_art_shuffle.random=r?r:0x517cc1b7u;
}
static uint8_t stattic_art_next(void)
{
    if(!stattic_art_shuffle.left){
        const uint32_t n=ST_ART_COUNT;
        for(uint32_t i=0;i<n;i++)stattic_art_shuffle.bag[i]=(uint8_t)i;
        for(uint32_t i=n-1u;i>0;i--){uint32_t j=stattic_art_random()%(i+1u);
            uint8_t v=stattic_art_shuffle.bag[i];stattic_art_shuffle.bag[i]=stattic_art_shuffle.bag[j];stattic_art_shuffle.bag[j]=v;}
        if(stattic_art_shuffle.bag[n-1u]==stattic_art_shuffle.last){   /* (the bag is drawn from its end: never the last one twice) */
            uint32_t j=stattic_art_random()%(n-1u);uint8_t v=stattic_art_shuffle.bag[n-1u];
            stattic_art_shuffle.bag[n-1u]=stattic_art_shuffle.bag[j];stattic_art_shuffle.bag[j]=v;
        }
        stattic_art_shuffle.left=(uint8_t)n;
    }
    uint8_t next=stattic_art_shuffle.bag[--stattic_art_shuffle.left];stattic_art_shuffle.last=next;return next;
}
static void stattic_turn(uint32_t knob,int32_t detents)
{
    if(stattic_home_page<3u){
        uint32_t page=stattic_home_page;stattic_focus[page]=(uint8_t)(knob&3u);
        if(stattic_art_shuffle.page!=page||stattic_art_shuffle.knob!=(knob&3u)){
            stattic_art_pick[page]=stattic_art_next();stattic_art_shuffle.page=(uint8_t)page;stattic_art_shuffle.knob=(uint8_t)(knob&3u);
        }
        uint32_t h=ST_ART_PERIOD[stattic_art_pick[page]],steps=(uint32_t)(detents<0?-(int64_t)detents:detents);
        stattic_scroll[page]=(stattic_scroll[page]+steps*6u)%h;
    }
    stattic_ui_dirty=1;
}
static void stattic_touch(track_t *t,uint32_t id)
{
    uint32_t k=(uint32_t)(t-trk);
    stattic_change_t *q=&stattic_change[k];
    if(q->armed&&id<P_COUNT){q->mask[id/32u]&=~(1u<<(id%32u));q->value.p[id]=t->p[id];}
    if(k==2u&&(id==SN_PLAY_MODE||id==SN_CHORD))t->p[P_CHORD]=t->p[SN_PLAY_MODE]?0:1;
    if(is_drum(t)){if(id>=P_E1&&id<=P_E4)stattic_macro[k]=(uint8_t)(id-P_E1);}
    else if(id>=P_E0&&id<=P_E3)stattic_macro[k]=(uint8_t)(id-P_E0);
    stattic_ui_dirty=1;stattic_revision++;
}
static void stattic_parameter_locks(track_t *t,stattic_pattern_t *p,const int16_t *base,uint32_t role,uint32_t *r)
{
    uint32_t amount=(uint32_t)base[SN_G_STYLE],made=0;
    for(uint32_t i=0;i<NLOCK;i++)if(p->lock[i].step!=LOCK_FREE)made++;
    uint32_t first=snow_int(r,0,NSTEP-1u);
    for(uint32_t i=0;i<NSTEP&&made<NLOCK;i++){
        uint32_t step=(first+i)%NSTEP;
        int on=is_drum(t)?dstep_mask(&p->drum[step])!=0:stattic_step_on(&p->step[step]);
        if(!amount||!on||!snow_prob(r,amount*45u/100u))continue;
        uint32_t pick=snow_int(r,0,3),id=is_drum(t)?(pick==0?SN_DJ_FILTER:pick==1?P_PAN:pick==2?P_REV:P_DIST):
            pick==0?P_E6:pick==1?P_PAN:pick==2?P_REV:P_E0;
        const param_desc_t *d=track_desc(t,id);int32_t spread=id==P_PAN?8:id==P_REV?12+(int32_t)amount/4:8;
        uint32_t slot=0;while(slot<NLOCK&&p->lock[slot].step!=LOCK_FREE)slot++;if(slot==NLOCK)break;
        p->lock[slot]=(plock_t){(uint8_t)step,(uint8_t)id,(int16_t)clamp(base[id]+(int32_t)snow_int(r,0,spread*2u)-spread,d->min,d->max)};
        made++;
    }
    (void)role;
}
/* A lock's value: base moved by sp/3..sp (dir +1 up, -1 down, 0 either way;
 * turned round at the range's end), so it never equals what the step had. */
static int16_t stattic_lock_val(const param_desc_t *d,int32_t base,int32_t sp,int32_t dir,uint32_t *r)
{
    int32_t m=sp/3+(int32_t)snow_int(r,0,(uint32_t)(sp-sp/3));
    m=dir?m*dir:snow_prob(r,50)?m:-m;
    if(base+m>d->max||base+m<d->min)m=-m;
    return (int16_t)clamp(base+m,d->min,d->max);
}
/* (the owner) HOME 2 K4 on BS: the steps' parameter locks anew, the notes stay.
 * PARM AMT sets how many (at 0 still a few: the knob asks for them). */
static void stattic_bass_locks(track_t *t,stattic_pattern_t *p,const int16_t *base,uint32_t *r)
{
    static const uint8_t ids[6]={P_E6,P_E0,P_E3,P_E7,P_REV,P_E2};   /* CUTOFF TONE DECAY DRIVE FX SEND SWEEP */
    static const uint8_t spread[6]={20,16,22,24,40,12};
    static const int8_t dir[6]={0,0,0,1,1,0};                           /* (DRIVE and FX SEND: only up) */
    uint32_t chance=25u+(uint32_t)base[SN_G_STYLE]*60u/100u,made=0,first=snow_int(r,0,NSTEP-1u);
    for(uint32_t i=0;i<NLOCK;i++)p->lock[i].step=LOCK_FREE;
    for(uint32_t i=0;i<NSTEP&&made<NLOCK;i++){
        uint32_t step=(first+i)%NSTEP;
        if(!stattic_step_on(&p->step[step])||!snow_prob(r,chance))continue;
        uint32_t pick=snow_int(r,0,5),id=ids[pick];
        p->lock[made++]=(plock_t){(uint8_t)step,(uint8_t)id,stattic_lock_val(track_desc(t,id),base[id],spread[pick],dir[pick],r)};
        if(made<NLOCK&&snow_prob(r,25)){                     /* (sometimes two on one step) */
            uint32_t two=(pick+1u+snow_int(r,0,4))%6u;id=ids[two];
            p->lock[made++]=(plock_t){(uint8_t)step,(uint8_t)id,stattic_lock_val(track_desc(t,id),base[id],spread[two],dir[two],r)};
        }
    }
}
/* (the owner) HOME 2 K1..K4 on DR: the category's own steps get their locks anew.
 * Locks act on the whole step: on a step another category also plays, a lock
 * of this category's kind is made anew, another kind stays. Kick: LENGTH BEND DRIVE; snare: FILTER LENGTH DRIVE; hat: FILTER
 * LENGTH PAN; percussion: PAN BEND FILTER. PARM AMT 0: none. */
static uint32_t stattic_cat_hits(const dstep_t *d,uint32_t cat,int others)
{
    uint32_t m=dstep_mask(d),hit=0;
    for(uint32_t l=0;l<16u;l++)if((m>>l)&1u)hit|=(ST_LANE_CATEGORY[l]==cat)!=others;
    return hit;
}
static void stattic_drum_locks(track_t *t,stattic_pattern_t *p,const int16_t *base,uint32_t cat,const uint8_t *was,uint32_t *r)
{
    static const uint8_t ids[4][3]={{P_E4,P_E3,P_E2},{P_E1,P_E4,P_E2},{P_E1,P_E4,P_PAN},{P_PAN,P_E3,P_E1}};
    uint32_t amount=(uint32_t)base[SN_G_STYLE],made=0;uint8_t taken[NSTEP]={0};
    for(uint32_t i=0;i<NLOCK;i++){
        plock_t *l=&p->lock[i];if(l->step>=NSTEP)continue;
        int mine=l->param==ids[cat&3u][0]||l->param==ids[cat&3u][1]||l->param==ids[cat&3u][2];
        if(was[l->step]&&(mine||!stattic_cat_hits(&p->drum[l->step],cat,1)))l->step=LOCK_FREE;
        else{taken[l->step]=1;made++;}
    }
    if(!amount)return;
    uint32_t first=snow_int(r,0,NSTEP-1u);
    for(uint32_t i=0;i<NSTEP&&made<NLOCK;i++){
        uint32_t step=(first+i)%NSTEP;
        if(taken[step]||!stattic_cat_hits(&p->drum[step],cat,0)||!snow_prob(r,amount*70u/100u))continue;
        uint32_t id=ids[cat&3u][snow_int(r,0,2)];
        int32_t sp=id==P_PAN?12:id==P_E4?26:id==P_E1?40:20,dir=id==P_E1?-1:id==P_E2?1:0;   /* (FILTER only closes, DRIVE only adds) */
        uint32_t slot=0;while(slot<NLOCK&&p->lock[slot].step!=LOCK_FREE)slot++;if(slot==NLOCK)break;
        p->lock[slot]=(plock_t){(uint8_t)step,(uint8_t)id,stattic_lock_val(track_desc(t,id),base[id],sp,dir,r)};
        made++;
    }
}
/* (2026-11-08, the owner) CH's K2 sounds at about one loudness. A held three-note chord of the KEYS voice,
 * measured on the host for 300 random sounds (sustain, decay, drive and the wave explain 83 %, 1.6 dB rms):
 * ~ 0.126 dB a SUS step, 0.029 a DEC step, 0.103 a DRIVE step, and the wave. Above the presets' middle (70 dB
 * there) the sound's TRIM (P_ED_FX, 1/2 dB steps, applied with LEVEL) takes it down, at most 10 dB; quieter
 * sounds are left alone. Nothing else of the sound changes; a preset sets TRIM 0. */
static int16_t stattic_ch_trim(const int16_t *p)
{
    static const int16_t wave8[6]={463,451,453,445,442,439};          /* SINE TRI SAW SQUARE PULSE ORGAN, 1/8 dB */
    int32_t est8=(16*p[P_SUS]+4*p[P_DEC]+13*p[P_E7])/16+wave8[(uint32_t)p[SN_WAVE1]%6u];
    int32_t over=est8-70*8;
    return (int16_t)(over>0?-clamp((over+2)/4,0,20):0);
}
static int stattic_prepare(uint32_t k,uint32_t knob)
{
    if(k>=NTRK||knob>(k==TRK_DRUM?7u:3u))return 1;
    track_t candidate;stattic_snapshot_t next;uint32_t mask[ST_PARAM_WORDS]={0},what=0,f=STATTIC_GUARD_SAVE();
    stattic_change_t *q=&stattic_change[k];uint32_t prior=q->armed?q->serial:0;
    if(q->armed){next=q->value;memcpy(mask,q->mask,sizeof mask);what=q->what;}else stattic_capture(k,&next);
    memset(&candidate,0,sizeof candidate);memcpy(candidate.p,next.p,sizeof candidate.p);memcpy(candidate.step,next.pattern.step,sizeof candidate.step);
    candidate.engine=candidate.eng_req=trk[k].engine;
    STATTIC_GUARD_RESTORE(f);
    uint32_t role=k==0u?4u:k==1u?6u:5u;
    if(knob==0u){
        uint32_t seed=snow_next_seed(snow_seed(&candidate));
        if(k==TRK_DRUM){
            memset(next.pattern.drum,0,sizeof next.pattern.drum);memset(next.pattern.micro,0,sizeof next.pattern.micro);
            uint32_t feel=stattic_feel_pick((uint32_t)clamp(candidate.p[SN_G_COMPLEX],0,100),&seed);
            for(uint32_t cat=0;cat<4;cat++)stattic_dr_make(&candidate,&next.pattern,cat,seed^((cat+1u)*977u),feel,&seed);
        }else{
            snow_phrase(&candidate,role,seed,next.pattern.step);
            memcpy(next.pattern.micro,stattic_phrase_micro,sizeof next.pattern.micro);
        }
        for(uint32_t i=0;i<NLOCK;i++)next.pattern.lock[i].step=LOCK_FREE;
        stattic_parameter_locks(&trk[k],&next.pattern,next.p,role,&seed);
        snow_seed_put(&candidate,seed);
        const uint8_t ids[]={SN_G_SEED,SN_G_SEED_B,SN_G_SEED_C,SN_G_SEED_D};
        for(uint32_t i=0;i<NELEM(ids);i++){next.p[ids[i]]=candidate.p[ids[i]];mask[ids[i]/32u]|=1u<<(ids[i]%32u);}
        what|=ST_CHANGE_PATTERN|ST_CHANGE_SOUND;
    }else if(knob>=4u&&k==TRK_DRUM){
        /* (the owner) HOME 2 on DR: K1 kick, K2 snare, K3 hat, K4 percussion. Each
         * makes that category's sequence anew (its lanes only, as K1 made all four)
         * and draws that category's shots; the other categories stay as they are */
        uint32_t cat=knob-4u,seed=snow_next_seed(snow_seed(&candidate));
        uint8_t shots[16],was[NSTEP];
        for(uint32_t i=0;i<NSTEP;i++)was[i]=(uint8_t)stattic_cat_hits(&next.pattern.drum[i],cat,0);
        uint32_t idm=(uint32_t)clamp(candidate.p[SN_G_COMPLEX],0,100);
        uint32_t feel=cat==0u?stattic_feel_of(&next.pattern,(uint32_t)clamp(candidate.p[P_SLEN],1,NSTEP)):cat==1u?stattic_feel_pick(idm,&seed):ST_FEEL_BASE;
        stattic_dr_make(&candidate,&next.pattern,cat,seed^((cat+1u)*977u),feel,&seed);
        stattic_drum_locks(&trk[k],&next.pattern,next.p,cat,was,&seed);
        memcpy(shots,next.shots,sizeof shots);stattic_drum_random(shots,&seed);
        for(uint32_t l=0;l<16u;l++)if(ST_LANE_CATEGORY[l]==cat)next.shots[l]=shots[l];
        snow_seed_put(&candidate,seed);
        const uint8_t ids[]={SN_G_SEED,SN_G_SEED_B,SN_G_SEED_C,SN_G_SEED_D};
        for(uint32_t i=0;i<NELEM(ids);i++){next.p[ids[i]]=candidate.p[ids[i]];mask[ids[i]/32u]|=1u<<(ids[i]%32u);}
        what|=ST_CHANGE_PATTERN|ST_CHANGE_SHOTS|ST_CHANGE_SOUND;
    }else if(knob==3u&&k==0u){
        uint32_t seed=snow_next_seed(snow_seed(&candidate));
        stattic_bass_locks(&trk[k],&next.pattern,next.p,&seed);
        snow_seed_put(&candidate,seed);
        const uint8_t ids[]={SN_G_SEED,SN_G_SEED_B,SN_G_SEED_C,SN_G_SEED_D};
        for(uint32_t i=0;i<NELEM(ids);i++){next.p[ids[i]]=candidate.p[ids[i]];mask[ids[i]/32u]|=1u<<(ids[i]%32u);}
        what|=ST_CHANGE_PATTERN|ST_CHANGE_SOUND;              /* (SOUND: only the seeds, the mask) */
    }else if(knob==1u&&k==TRK_DRUM){
        uint32_t seed=snow_next_seed(snow_seed(&candidate));stattic_drum_random(next.shots,&seed);
        snow_seed_put(&candidate,seed);
        const uint8_t ids[]={SN_G_SEED,SN_G_SEED_B,SN_G_SEED_C,SN_G_SEED_D};
        for(uint32_t i=0;i<NELEM(ids);i++){next.p[ids[i]]=candidate.p[ids[i]];mask[ids[i]/32u]|=1u<<(ids[i]%32u);}
        what|=ST_CHANGE_SHOTS|ST_CHANGE_SOUND;
    }else{
        uint32_t own[ST_PARAM_WORDS];
        snow_design_section(&trk[k],role,knob==1u?FAM_EDIT:knob==2u?FAM_LFO:FAM_FX,own,next.p);
        for(uint32_t i=0;i<ST_PARAM_WORDS;i++)mask[i]|=own[i];what|=ST_CHANGE_SOUND;
        if(knob==1u&&k!=TRK_DRUM)what|=ST_CHANGE_FADE;      /* (the owner) K2's new sound enters softly */
        if(knob==1u&&k==2u){                                 /* (the owner) CH: a loud new sound is trimmed down */
            next.p[P_ED_FX]=stattic_ch_trim(next.p);mask[P_ED_FX/32u]|=1u<<(P_ED_FX%32u);
        }
        if(k==1u){next.p[P_VOICE]=V_POLY;next.p[SN_PLAY_MODE]=1;}
        if(k==TRK_DRUM&&knob==2u){next.p[SN_L2_DEST]=(int16_t)(1u+(uint32_t)next.p[SN_L2_DEST]%4u);if(!next.p[SN_L2_DEPTH])next.p[SN_L2_DEPTH]=24;}
    }
    f=STATTIC_GUARD_SAVE();
    if(!prior||!q->armed||q->serial!=prior)stattic_undo_mark(&trk[k],(stattic_action+=4u)|3u);
    stattic_publish(k,&next,mask,what,stattic_when());
    STATTIC_GUARD_RESTORE(f);return 0;
}
/* A whole-channel generation can straddle a bar while preparing. Stage
 * all members without committing any; select one common future bar last. */
static void stattic_group_end(uint32_t mask)
{
    uint32_t f=STATTIC_GUARD_SAVE(),bar=stattic_when();
    for(uint32_t k=0;k<NTRK;k++)if(((mask>>k)&1u)&&stattic_change[k].armed)stattic_change[k].bar=bar;
    RING_PUBLISH();stattic_staging&=(uint8_t)~mask;STATTIC_GUARD_RESTORE(f);
}
static void stattic_prepare_scope(uint32_t track,uint32_t knob)
{
    uint32_t mask=trk[track].p[SN_G_SCOPE]?15u:1u<<track,f=STATTIC_GUARD_SAVE();
    stattic_staging|=(uint8_t)mask;STATTIC_GUARD_RESTORE(f);
    for(uint32_t k=0;k<NTRK;k++)if((mask>>k)&1u)(void)stattic_prepare(k,k==TRK_DRUM&&knob<4u?knob+4u:knob);   /* (DR: by category) */
    stattic_group_end(mask);
}
static void stattic_pattern_random(void)
{
    static uint32_t seed=0x73f479a5u;seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;
    uint32_t r=seed^fm1_ms,f=STATTIC_GUARD_SAVE();stattic_staging=15u;STATTIC_GUARD_RESTORE(f);
    for(uint32_t k=0;k<NTRK;k++)(void)stattic_pattern_select(k,snow_int(&r,0,2));
    stattic_group_end(15u);
}
static int stattic_pattern_select(uint32_t k,uint32_t slot)
{
    if(k>=NTRK||slot>=ST_PATTERNS)return 1;
    uint32_t f=STATTIC_GUARD_SAVE(),mask[ST_PARAM_WORDS]={0};stattic_snapshot_t next;
    uint32_t what=ST_CHANGE_PATTERN;
    if(stattic_change[k].armed){
        if((stattic_change[k].what&ST_CHANGE_PATTERN)&&stattic_change[k].slot==slot){STATTIC_GUARD_RESTORE(f);return 0;}
        next=stattic_change[k].value;memcpy(mask,stattic_change[k].mask,sizeof mask);what|=stattic_change[k].what;
    }else stattic_capture(k,&next);
    if(slot==stattic_current[k])stattic_pattern_capture(&trk[k],&next.pattern);else next.pattern=stattic_banks[k][slot];
    next.slot=(uint8_t)slot;stattic_undo_mark(&trk[k],(stattic_action+=4u)|3u);
    stattic_publish(k,&next,mask,what,stattic_when());STATTIC_GUARD_RESTORE(f);return 0;
}
static void stattic_clear(int all)
{
    uint32_t group=(stattic_action+=4u)|3u,f=STATTIC_GUARD_SAVE(),mask[ST_PARAM_WORDS]={0},bar=stattic_when();
    for(uint32_t k=0;k<NTRK;k++)if(all||k==song.sel){
        stattic_snapshot_t next;uint32_t what=ST_CHANGE_PATTERN;
        memset(mask,0,sizeof mask);
        if(stattic_change[k].armed){next=stattic_change[k].value;memcpy(mask,stattic_change[k].mask,sizeof mask);what|=stattic_change[k].what;}
        else stattic_capture(k,&next);
        memset(&next.pattern,0,sizeof next.pattern);
        if(k!=TRK_DRUM)for(uint32_t i=0;i<NSTEP;i++)next.pattern.step[i].time=ST_REST;
        for(uint32_t i=0;i<NLOCK;i++)next.pattern.lock[i].step=LOCK_FREE;
        stattic_undo_mark(&trk[k],group);stattic_publish(k,&next,mask,what,bar);
    }
    STATTIC_GUARD_RESTORE(f);
}
static void stattic_temp_save(void)
{
    uint32_t f=STATTIC_GUARD_SAVE();for(uint32_t k=0;k<NTRK;k++)stattic_capture(k,&stattic_saved.track[k]);
    memcpy(stattic_saved.g,song.g,sizeof song.g);stattic_saved.mask=stattic_control_mask;stattic_saved.valid=1;
    STATTIC_GUARD_RESTORE(f);
}
static int stattic_temp_load(uint32_t tracks)
{
    if(!stattic_saved.valid)return 0;
    uint32_t f=STATTIC_GUARD_SAVE(),mask[ST_PARAM_WORDS],group=(stattic_action+=4u)|3u,bar=stattic_when();
    stattic_mask_all(mask);
    for(uint32_t k=0;k<NTRK;k++)if((tracks>>k)&1u){
        stattic_undo_mark(&trk[k],group);stattic_publish(k,&stattic_saved.track[k],mask,7u,bar);
    }
    if(tracks==15u){
        stattic_global_restore.armed=0;memcpy(stattic_global_restore.g,stattic_saved.g,sizeof song.g);
        stattic_global_restore.bar=bar;RING_PUBLISH();stattic_global_restore.armed=1;
    }
    STATTIC_GUARD_RESTORE(f);return 1;
}
static void stattic_control_all(uint32_t knob,int32_t delta)
{
    uint32_t f=STATTIC_GUARD_SAVE();for(uint32_t k=0;k<NTRK;k++)if((stattic_control_mask>>k)&1u)
        snow_offset[k][knob&3u]=(int16_t)clamp(snow_offset[k][knob&3u]+delta,-127,127);
    stattic_ui_dirty=1;stattic_revision++;STATTIC_GUARD_RESTORE(f);
}
static void stattic_runtime_poll(void)
{
    if(!stattic_ready)return;
    for(uint32_t k=0;k<NTRK;k++){
        uint32_t mode=(uint32_t)trk[k].p[SN_G_LIVE],pass=trk[k].pass;
        if(mode&&song.playing&&!stattic_change[k].armed&&pass!=stattic_live_pass[k]&&pass&&pass%(1u<<(mode-1u))==0u){
            stattic_live_pass[k]=(uint16_t)pass;(void)stattic_prepare(k,0);
        }
    }
}
