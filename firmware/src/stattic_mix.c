/* SPDX-License-Identifier: GPL-3.0-only */
/* STATTIC's channel processors inside SLOOP's native render-unit API.
 * One scratch/accumulator per core; all effect histories remain per track. */
static int32_t stattic_drum_tone,stattic_drum_amp=32767;
static void mix_part_run(track_t *t,uint32_t n,mixacc_t *mx,int32_t *b)
{
    uint32_t k=(uint32_t)(t-trk),i,start=STATTIC_PROFILE_TICKS(),synth_end;
    int32_t g0=32767-t->att,g1=gain_next(t),side[CTL];
    int mod=mod_begin(t);
    if(is_drum(t)){
        int32_t l[CTL]={0},r[CTL]={0},rv[CTL]={0},dl[CTL]={0};
        drums.a0=drums.a1=0;
        track_lfo_tick(t);snow_block(t);
        slicer_drums(l,r,rv,dl,n);
        int32_t tone=stattic_drum_macro(t,0),shape=stattic_drum_macro(t,1);
        int32_t amp=32767;
        if(t->p[SN_L2_DEST]==10)amp=clamp(32767+snow_l2[k].value*258,0,32767);
        if(t->p[P_LD_AMP])amp=mulq15(amp,32767-mulq15((t->lfo_val+32768)/2,t->p[P_LD_AMP]*258));
        int32_t amp0=stattic_drum_amp;stattic_drum_amp=amp;
        for(i=0;i<n;i++){
            int32_t x=(l[i]+r[i])/2;
            stattic_drum_tone+=mulq15(x-stattic_drum_tone,512+tone*254);
            b[i]=tone==127?x:stattic_drum_tone;side[i]=0;
            b[i]=mulq15(b[i],amp0+(((amp-amp0)*(int32_t)i)>>CTL_LOG2));
            if(shape)b[i]=b[i]+mulq15(clamp(b[i]*(32+shape)/32,-32768,32767)-b[i],shape*258);
        }
    }else{
        track_render(t,b,n);
        for(i=0;i<n;i++)side[i]=t->p[P_SPRD]?
            (stattic_spread[k][i]*2-b[i])*t->p[P_SPRD]/127:0;
        slicer_track(t,b,n);
    }
    synth_end=STATTIC_PROFILE_TICKS();
    tsvf_t fc;
    if(djf_block(&tflt[k],t->p[P_TFLT],&fc,tflt_reso()))
        tflt_run(&tflt[k],&fc,b,n,0);
    stattic_dist(t,b,n);
    if(t->p[P_ITYPE]|t->ins_run)track_insert(t,b,n);
    if((stattic_duck_on>>k)&1u)stattic_duck_apply(k,b,n);
    snow_fx_process(t,b,n);
    int performance=snow_mix_begin(t);
    int32_t lvl=expr_gain(LEVEL_Q12[clamp(t->p[P_LEVEL],0,127)],k);
    /* The drum player already applies its channel LEVEL before the rack.
     * Synth parts apply it here; pan/width cover the rack's complete output. */
    if(is_drum(t))lvl=4096;
    int32_t pan=t->p[P_PAN],gl=4096-(pan>0?pan*64:0),gr=4096+(pan<0?pan*64:0);
    int32_t l0=t->mix_on?t->lvl:lvl,p0=t->mix_on?t->gpl:gl,q0=t->mix_on?t->gpr:gr;
    t->lvl=mix_glide(l0,lvl);t->gpl=mix_glide(p0,gl);t->gpr=mix_glide(q0,gr);t->mix_on=1;
    int32_t pk=t->peak;
    for(i=0;i<n;i++){
        int32_t sd=snow_fx[k].side[i]+side[i],l=b[i]+sd,r=b[i]-sd;
        if(performance)snow_mix_sample(t,&l,&r);
        int32_t gain=g0+(((g1-g0)*(int32_t)i)>>CTL_LOG2);
        if(!is_drum(t))gain=mulq15(gain,duck.g0+(((duck.g1-duck.g0)*(int32_t)i)>>CTL_LOG2));
        int32_t lv=l0+(((t->lvl-l0)*(int32_t)i)>>CTL_LOG2);
        l=MULQ12R(l,lv,2);r=MULQ12R(r,lv,2);
        if(gain<32767){l=MULQ12R(l,gain>>3,4);r=MULQ12R(r,gain>>3,4);}
        int32_t pl=p0+(((t->gpl-p0)*(int32_t)i)>>CTL_LOG2),pr=q0+(((t->gpr-q0)*(int32_t)i)>>CTL_LOG2);
        l=(l*(pl>>4)+128)>>8;r=(r*(pr>>4)+128)>>8;
        mx->l[i]+=l;mx->r[i]+=r;
        int32_t a=l<0?-l:l,z=r<0?-r:r;if(z>a)a=z;if(a>pk)pk=a;
    }
    t->peak=pk;
    if(mod)mod_end(t);
    uint32_t end=STATTIC_PROFILE_TICKS();
    stattic_profile.current[k][0]+=synth_end-start;
    stattic_profile.current[k][1]+=end-synth_end;stattic_profile.current[k][2]+=end-start;
}
