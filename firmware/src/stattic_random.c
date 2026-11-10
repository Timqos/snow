/* SPDX-License-Identifier: GPL-3.0-only
 * Sound design, not independent full-range dice. The six instrument bodies
 * are invariants; oscillator/envelope/filter/motion/FX choices are generated
 * from a fresh 32-bit state. No factory preset selection or short patch list.
 * Runs in the main loop; only the bounded commit holds the audio guard. */
static int8_t stattic_k2_style=-1;                       /* (the K2 style drawn last: 7 = CH's PAD) */
static void snow_family_mask(uint32_t family,uint32_t *mask)
{
    memset(mask,0,((P_COUNT+31u)/32u)*sizeof *mask);
    for(uint32_t i=0;i<NPAGES;i++)if(PAGES[i].fam==family)
        for(uint32_t c=0;c<4u;c++){
            uint32_t id=PAGES[i].id[c];if(id<P_COUNT)mask[id/32u]|=1u<<(id%32u);
        }
    if(family==FAM_EDIT){
        for(uint32_t id=SN_ALG;id<=SN_KEYTRACK;id++)mask[id/32u]|=1u<<(id%32u);
        for(uint32_t id=P_ATK;id<=P_REL;id++)mask[id/32u]|=1u<<(id%32u);
        static const uint8_t extra[]={P_GLIDE,P_GLMODE,P_PRIO,P_ALLOC,P_DETUNE,P_SPRD,P_VOICE};
        for(uint32_t i=0;i<NELEM(extra);i++)mask[extra[i]/32u]|=1u<<(extra[i]%32u);
    }
    if(family==FAM_LFO){
        for(uint32_t id=P_LD_PIT;id<=P_LD_AMP;id++)mask[id/32u]|=1u<<(id%32u);
        for(uint32_t id=P_ED_FLT;id<=P_ED_SHP;id++)mask[id/32u]|=1u<<(id%32u);
        for(uint32_t id=P_M1SRC;id<=P_M4AMT;id++)mask[id/32u]|=1u<<(id%32u);
    }    /* (fix) the mixer is the player's: a page of the family may show MUTE or
     * LEVEL, but no generator randomises them. HOME 2 K2 muted channels at random
     * (MUTE drawn 0/1) and K4 drew each channel's LEVEL. */
    mask[P_MUTE/32u]&=~(1u<<(P_MUTE%32u));
    mask[P_LEVEL/32u]&=~(1u<<(P_LEVEL%32u));
}
static void snow_edit_mask(uint32_t *mask){snow_family_mask(FAM_EDIT,mask);}
static uint32_t snow_sound_state[7];
static uint32_t ss_rand(uint32_t *s){uint32_t x=*s;x^=x<<13;x^=x>>17;x^=x<<5;return *s=x;}
static int16_t ss_range(uint32_t *s,int32_t lo,int32_t hi){return (int16_t)(lo+(int32_t)(ss_rand(s)%(uint32_t)(hi-lo+1)));}
static int ss_chance(uint32_t *s,uint32_t pct){return ss_rand(s)%100u<pct;}
static int16_t ss_harmonic(uint32_t *s){return (int16_t)(16*ss_range(s,1,4));}
static void snow_design_fx(uint32_t channel,uint32_t *r,int16_t *p)
{
    /* Dry instrument first: bounded sends, one gentle colour processor.
     * Modulated feedbacks never approach self-oscillation, and crush keeps bits. */
    int colour=ss_range(r,0,4),space=channel>=4u;
    p[P_DLY]=ss_range(r,0,space?24:10);p[P_REV]=ss_range(r,0,space?32:17);p[P_CHOR]=ss_range(r,0,space?24:8);
    p[SN_DTIME]=ss_range(r,12,49);p[SN_DSYNC]=ss_chance(r,60)?ss_range(r,5,9):0;
    p[SN_DFB]=ss_range(r,12,48);p[SN_DCOLOR]=ss_range(r,53,106);p[SN_DWIDTH]=ss_range(r,30,108);
    p[SN_RTYPE]=ss_range(r,0,2);p[SN_RSIZE]=ss_range(r,30,78);p[SN_RDAMP]=ss_range(r,56,104);p[SN_RPRE]=ss_range(r,0,18);
    p[SN_CRATE]=ss_range(r,20,62);p[SN_CDEPTH]=ss_range(r,12,37);p[SN_CFB]=ss_range(r,0,12);
    p[SN_FX_BYPASS]=0;p[P_DIST]=colour==1?ss_range(r,3,13):0;p[P_DTYPE]=0;p[P_DTONE]=ss_range(r,-16,12);
    p[SN_TMIX]=colour==2?ss_range(r,10,35):0;p[SN_TDRIVE]=ss_range(r,12,35);p[SN_TTONE]=ss_range(r,85,124);
    p[SN_TWOW]=ss_range(r,0,9);p[SN_TFLUT]=ss_range(r,0,5);
    p[SN_BMIX]=colour==3?ss_range(r,4,13):0;p[SN_BBITS]=ss_range(r,12,16);p[SN_BRATE]=ss_range(r,1,2);p[SN_BTONE]=ss_range(r,90,124);
    static const uint8_t inserts[]={1,6,7,8};
    p[P_ITYPE]=inserts[ss_rand(r)%NELEM(inserts)];p[P_IMIX]=colour==4?ss_range(r,5,22):0;
    p[P_IA]=ss_range(r,p[P_ITYPE]==1?8:20,p[P_ITYPE]==1?26:63);
    p[P_IB]=ss_range(r,p[P_ITYPE]==1?82:12,p[P_ITYPE]==1?119:40);p[P_IC]=ss_range(r,p[P_ITYPE]==1?90:0,p[P_ITYPE]==1?108:14);
}
/* (2026-11-09, the owner) SY's K2: one turn in every four (its place in the four drawn at random) makes an
 * 8-bit / FX sound instead of a keys one. Six kinds, dealt from a shuffled deck (each once before any again, never
 * the same twice running), each with its own ranges: BLIP, ZAP (falls), LASER (rises), NOISE HIT, CHIP ARP (in the
 * track's scale), GLITCH. Its TRIM (P_ED_FX) evens their level with the keys sounds. */
static uint8_t st_sy_slot,st_sy_fx_at,st_sy_deck[6],st_sy_left,st_sy_last=255;
static int stattic_sy_fx_turn(uint32_t *r)
{
    if(!st_sy_slot)st_sy_fx_at=(uint8_t)ss_range(r,0,3);
    int fx=st_sy_slot==st_sy_fx_at;st_sy_slot=(uint8_t)((st_sy_slot+1u)&3u);
    return fx;
}
static uint32_t stattic_sy_fx_kind(uint32_t *r)
{
    if(!st_sy_left){
        for(uint32_t i=0;i<6u;i++)st_sy_deck[i]=(uint8_t)i;
        for(uint32_t i=5u;i>0;i--){uint32_t j=ss_rand(r)%(i+1u);uint8_t x=st_sy_deck[i];st_sy_deck[i]=st_sy_deck[j];st_sy_deck[j]=x;}
        if(st_sy_deck[5]==st_sy_last){uint32_t j=ss_rand(r)%5u;uint8_t x=st_sy_deck[5];st_sy_deck[5]=st_sy_deck[j];st_sy_deck[j]=x;}
        st_sy_left=6;
    }
    return st_sy_last=st_sy_deck[--st_sy_left];
}
static void snow_design_sy_fx(uint32_t *r,int16_t *p)
{
    static const uint8_t pws[]={16,32,64,96};
    static const int8_t trim[6]={10,4,6,12,-8,6};      /* (2026-11-10: 3 dB lower, as their own delays add level now) */   /* 1/2 dB, BLIP ZAP LASER NOISE ARP GLITCH: their loudest 23 ms
                                                         * near the keys sounds' (host, 240 turns: the keys ~70 dB) */
    uint32_t kind=stattic_sy_fx_kind(r);
    p[SN_ALG]=0;p[SN_PAMT]=0;p[SN_RATIO3]=8;p[SN_WAVE2]=0;p[SN_WAVE3]=0;p[SN_SUB]=0;p[SN_MIX]=0;p[SN_FB_ROUTE]=0;
    p[SN_FILTER]=0;p[SN_RESO]=0;p[SN_MODE]=1;p[SN_FINE]=0;p[SN_NOISE]=0;p[P_E5]=0;p[SN_FMDEC]=0;
    p[SN_WAVE1]=ss_chance(r,60)?4:ss_chance(r,70)?3:ss_chance(r,50)?1:2;   /* pulse, square, tri, saw */
    p[SN_PW]=pws[ss_rand(r)%NELEM(pws)];p[SN_RATIO]=16;p[P_E1]=64;
    p[SN_TUNE]=(int16_t)(ss_chance(r,70)?0:12*ss_range(r,-1,1));
    p[P_E0]=ss_range(r,0,14);p[P_E2]=0;p[P_E4]=ss_range(r,0,50);p[P_E6]=ss_range(r,100,127);p[P_E7]=ss_range(r,0,20);
    p[P_ATK]=0;p[P_GLIDE]=0;
    /* (2026-11-10, the owner) the pitch never slides: the moves are steps of the track's scale (CHIP ARP), the FM
     * on whole harmonics; the 8-bit character stays (pulse/square, short and bright, noise, glitchy steps) */
    static const uint8_t harm[]={48,64,80,96,112};
    switch(kind){
    case 0:   /* BLIP: a short tone, now and then a quick step up the scale */
        if(ss_chance(r,50)){p[SN_ALG]=2;p[SN_RATIO3]=ss_range(r,1,4);}
        p[P_DEC]=ss_range(r,25,50);p[P_SUS]=0;p[P_REL]=ss_range(r,8,22);break;
    case 1:   /* ZAP: falls fast down the scale (8-5-3-1) */
        p[SN_ALG]=4;p[SN_RATIO3]=ss_range(r,1,5);
        p[P_DEC]=ss_range(r,40,62);p[P_SUS]=ss_range(r,0,30);p[P_REL]=ss_range(r,12,30);break;
    case 2:   /* LASER: climbs fast up the scale (1-3-5-8) */
        p[SN_ALG]=3;p[SN_RATIO3]=ss_range(r,1,5);
        p[P_DEC]=ss_range(r,40,64);p[P_SUS]=ss_range(r,0,40);p[P_REL]=ss_range(r,12,30);break;
    case 3:   /* NOISE HIT: hiss and an air burst over a rough FM body */
        p[SN_NOISE]=ss_range(r,70,127);p[P_E5]=ss_range(r,40,127);p[SN_PDEC]=ss_range(r,6,22);
        p[P_E0]=ss_range(r,40,110);p[SN_RATIO]=harm[ss_rand(r)%NELEM(harm)];p[P_E2]=ss_range(r,30,90);p[SN_FMDEC]=ss_range(r,8,30);
        p[P_E6]=ss_range(r,60,120);
        p[P_DEC]=ss_range(r,22,44);p[P_SUS]=0;p[P_REL]=ss_range(r,8,20);break;
    case 4:   /* CHIP ARP: the note stepped through the scale */
        p[SN_ALG]=ss_range(r,1,5);p[SN_RATIO3]=ss_range(r,8,60);
        p[P_DEC]=ss_range(r,60,90);p[P_SUS]=ss_range(r,50,110);p[P_REL]=ss_range(r,15,40);break;
    default:  /* GLITCH: heavy FM, a broken fast arp in the scale */
        p[P_E0]=ss_range(r,90,127);p[SN_RATIO]=harm[ss_rand(r)%NELEM(harm)];p[P_E2]=ss_range(r,60,127);p[SN_FMDEC]=ss_range(r,5,25);
        if(ss_chance(r,70)){p[SN_ALG]=5;p[SN_RATIO3]=ss_range(r,1,15);}
        p[P_E7]=ss_range(r,10,30);
        p[P_DEC]=ss_range(r,20,50);p[P_SUS]=ss_range(r,0,40);p[P_REL]=ss_range(r,8,24);break;
    }
    p[P_ED_FX]=trim[kind];
}
/* (2026-11-10, the owner) SY's percussive effects: static, crackle, a broken radio, ticks, a hiss swell, a broken
 * glitch; short hits with noise and bitcrush (stattic_runtime adds the crush to K2's mask, and takes it off
 * again with the next K2 sound that is not one of these). The notes stay in the scale (whole harmonics only) */
static void snow_design_sy_perc(uint32_t *r,int16_t *p)
{
    static const int8_t trim[6]={2,0,4,8,2,4};          /* (1/2 dB: their loudest 50 ms near the keys sounds') */
    static const uint8_t harm[]={48,64,80,96,112};
    uint32_t kind=(uint32_t)ss_range(r,0,5);
    p[SN_ALG]=0;p[SN_PAMT]=0;p[SN_RATIO3]=8;p[SN_WAVE2]=0;p[SN_WAVE3]=0;p[SN_SUB]=0;p[SN_MIX]=0;p[SN_FB_ROUTE]=0;
    p[SN_FILTER]=0;p[SN_RESO]=0;p[SN_MODE]=1;p[SN_FINE]=0;p[P_E1]=64;p[SN_TUNE]=0;p[P_GLIDE]=0;
    p[SN_WAVE1]=ss_chance(r,60)?0:ss_chance(r,50)?1:3;p[SN_RATIO]=16;
    p[P_E4]=ss_range(r,0,40);p[P_ATK]=0;p[P_SUS]=0;
    p[SN_BMIX]=ss_range(r,30,80);p[SN_BBITS]=ss_range(r,4,9);p[SN_BRATE]=ss_range(r,2,10);p[SN_BTONE]=ss_range(r,70,120);
    switch(kind){
    case 0:   /* STATIC: a burst of hiss through the crusher */
        p[SN_NOISE]=ss_range(r,90,127);p[P_E5]=ss_range(r,60,127);p[SN_PDEC]=ss_range(r,4,14);
        p[P_E0]=ss_range(r,0,20);p[P_E2]=0;p[P_E6]=ss_range(r,70,110);p[P_E7]=ss_range(r,0,20);
        p[P_DEC]=ss_range(r,55,70);p[P_REL]=ss_range(r,40,60);break;
    case 1:   /* CRACKLE: bright noise, few bits, slow hold */
        p[SN_NOISE]=ss_range(r,100,127);p[P_E5]=ss_range(r,30,90);p[SN_PDEC]=ss_range(r,2,10);
        p[P_E0]=ss_range(r,0,10);p[P_E6]=ss_range(r,90,127);p[P_E7]=ss_range(r,20,40);
        p[SN_BMIX]=ss_range(r,60,100);p[SN_BBITS]=ss_range(r,3,6);p[SN_BRATE]=ss_range(r,6,16);
        p[P_DEC]=ss_range(r,50,64);p[P_REL]=ss_range(r,36,56);break;
    case 2:   /* RADIO: a rough FM body, noise, band-limited, crushed */
        p[P_E0]=ss_range(r,60,110);p[SN_RATIO]=harm[ss_rand(r)%NELEM(harm)];p[P_E2]=ss_range(r,60,127);p[SN_FMDEC]=ss_range(r,5,20);
        p[SN_NOISE]=ss_range(r,30,60);p[P_E5]=ss_range(r,20,60);p[SN_PDEC]=ss_range(r,6,16);
        p[P_E6]=ss_range(r,50,80);p[P_E7]=ss_range(r,10,30);
        p[P_DEC]=ss_range(r,58,74);p[P_REL]=ss_range(r,40,62);break;
    case 3:   /* TICK: a short metal tick */
        p[P_E0]=ss_range(r,20,60);p[SN_RATIO]=harm[2u+ss_rand(r)%3u];p[P_E2]=ss_range(r,80,127);p[SN_FMDEC]=ss_range(r,4,14);
        p[SN_NOISE]=ss_range(r,0,20);p[P_E5]=ss_range(r,40,100);p[SN_PDEC]=ss_range(r,2,8);p[P_E6]=ss_range(r,90,127);
        p[SN_BMIX]=ss_range(r,0,40);
        p[P_DEC]=ss_range(r,48,60);p[P_REL]=ss_range(r,36,52);break;
    case 4:   /* SWELL: hiss rising, then gone */
        p[SN_NOISE]=ss_range(r,80,127);p[P_E5]=0;p[P_E0]=ss_range(r,0,16);p[P_E6]=ss_range(r,60,100);
        p[P_ATK]=ss_range(r,68,84);p[P_DEC]=ss_range(r,60,72);p[P_REL]=ss_range(r,40,60);break;
    default:  /* BROKEN: heavy FM, a broken fast arp in the scale, crushed */
        p[P_E0]=ss_range(r,90,127);p[SN_RATIO]=harm[ss_rand(r)%NELEM(harm)];p[P_E2]=ss_range(r,60,127);p[SN_FMDEC]=ss_range(r,5,25);
        p[SN_ALG]=5;p[SN_RATIO3]=ss_range(r,1,8);p[SN_NOISE]=ss_range(r,10,40);p[P_E6]=ss_range(r,70,110);p[P_E7]=ss_range(r,10,30);
        p[P_DEC]=ss_range(r,58,72);p[P_REL]=ss_range(r,40,60);break;
    }
    p[P_ED_FX]=trim[kind];
}
/* (2026-11-10, the owner: SY/CH sometimes came out far too loud) the level a K2 sound's own space and sustain add:
 * a wet sum (REV + DLY) past 90, a long delay feedback, a sustain past 90; in TRIM steps (1/2 dB), never up */
static int16_t stattic_space_trim(const int16_t *p)
{
    int32_t t=0,wet=p[P_REV]+p[P_DLY];
    if(wet>90)t-=(wet-90)/7;
    if(p[SN_DFB]>55&&p[P_DLY]>50)t-=3;
    if(p[P_SUS]>90)t-=(p[P_SUS]-90)/6;
    return (int16_t)(t<-10?-10:t);
}
static void snow_design_sound_from(const track_t *t,uint32_t channel,uint32_t seed,const uint32_t *mask,int16_t *p)
{
    uint32_t r=seed?seed:0x6d2b79f5u;int melodic=channel>=4u;
    /* Every EDIT page participates; the rules below couple selectors and
     * the parameters which define register, articulation and effect budgets. */
    for(uint32_t id=0;id<P_COUNT;id++)if((mask[id/32u]>>(id%32u))&1u){
        const param_desc_t *d=id>=P_IA&&id<=P_IC?ins_desc(p[P_ITYPE],id-P_IA):track_desc(t,id);
        p[id]=ss_range(&r,d->min,d->max);
    }
    p[P_E1]=ss_chance(&r,75)?64:0; /* exact harmonic modulator ratios */
    p[SN_RATIO]=ss_harmonic(&r);p[SN_RATIO3]=ss_harmonic(&r);
    p[SN_FINE]=ss_range(&r,-5,5);p[SN_PW]=ss_range(&r,35,91);
    p[SN_MIX]=ss_range(&r,0,23);p[P_E4]=ss_range(&r,0,9);
    p[SN_FB_ROUTE]=ss_range(&r,0,1);p[SN_SUB]=ss_range(&r,0,22);
    p[P_E5]=ss_range(&r,0,15);p[P_E7]=ss_range(&r,0,18);
    p[P_E2]=ss_range(&r,58,72);p[P_E3]=ss_range(&r,58,70);
    p[SN_WAVE3]=ss_range(&r,0,1);p[P_GLIDE]=ss_range(&r,0,melodic?18:4);
    p[P_GLMODE]=ss_range(&r,0,1);p[P_PRIO]=ss_range(&r,0,2);p[P_ALLOC]=ss_range(&r,0,1);
    p[P_DETUNE]=ss_range(&r,0,melodic?18:6);p[P_SPRD]=ss_range(&r,20,90);
    p[SN_MODE]=melodic?1:0;p[SN_KEYTRACK]=channel>=3u?127:0;
    p[P_VOICE]=channel==5u?V_POLY:V_MONO;
    p[P_ATK]=ss_range(&r,0,5);p[P_SUS]=0;p[P_REL]=ss_range(&r,20,55);
    int keys_style=-1,percfx=0;                      /* (SY, CH: the ambient style drawn below; SY's percussive effects) */
    stattic_k2_style=-1;
    if(channel==0u){ /* Kick: a low fundamental and a brief pitch transient. */
        int body=ss_range(&r,0,5);
        p[SN_TUNE]=ss_range(&r,-34,-22);p[SN_WAVE1]=body==4?1:0;p[SN_WAVE2]=ss_range(&r,0,1);
        p[SN_ALG]=body==3?1:body==5?4:0;p[P_E0]=ss_range(&r,0,body==3?58:42);
        p[SN_NOISE]=ss_range(&r,0,5);p[SN_RATIO3]=ss_chance(&r,65)?8:16;
        p[SN_SUB]=ss_range(&r,0,body==0?30:18);p[SN_MIX]=ss_range(&r,0,12);
        p[P_E5]=ss_range(&r,0,8);p[SN_PAMT]=ss_range(&r,20,38);p[SN_PDEC]=ss_range(&r,6,14);
        p[SN_FMDEC]=ss_range(&r,18,49);p[SN_FILTER]=1;p[SN_RESO]=ss_range(&r,0,12);
        p[P_E6]=ss_range(&r,83,115);p[P_DEC]=ss_range(&r,body==0?83:49,body==0?96:73);
        p[P_ATK]=ss_range(&r,0,7);
    } else if(channel==1u){ /* Snare: pitched skin plus a coloured noise spring. */
        int brush=ss_chance(&r,12),band=ss_chance(&r,20);
        p[SN_TUNE]=ss_range(&r,-14,4);p[SN_WAVE1]=ss_range(&r,0,1);p[SN_WAVE2]=ss_range(&r,0,1);
        p[SN_ALG]=ss_chance(&r,25)?4:ss_chance(&r,30)?2:0;p[P_E0]=ss_range(&r,8,58);
        p[SN_NOISE]=ss_range(&r,brush?112:87,brush?127:119);p[SN_SUB]=ss_range(&r,0,20);
        p[SN_PAMT]=ss_range(&r,4,12);p[SN_PDEC]=ss_range(&r,7,19);p[SN_FMDEC]=ss_range(&r,23,52);
        p[SN_FILTER]=band?3:1;p[SN_RESO]=ss_range(&r,0,18);p[P_E6]=ss_range(&r,band?68:103,band?94:123);
        p[P_ATK]=ss_range(&r,brush?17:0,brush?34:4);p[P_DEC]=ss_range(&r,brush?70:49,brush?84:79);
    } else if(channel==2u){ /* Hat: broad high-frequency noise, restrained metal. */
        static const uint8_t ratios[]={31,37,47,53,61,79,97};int open=ss_chance(&r,22);
        p[SN_ALG]=ss_chance(&r,75)?3:4;p[SN_WAVE1]=ss_chance(&r,65)?3:4;p[SN_WAVE2]=ss_chance(&r,50)?3:4;
        p[SN_TUNE]=ss_range(&r,21,35);p[SN_NOISE]=ss_range(&r,111,127);
        p[SN_RATIO]=ratios[ss_rand(&r)%NELEM(ratios)];p[SN_RATIO3]=ss_range(&r,31,113);
        p[P_E0]=ss_range(&r,0,29);p[P_E5]=ss_range(&r,0,7);p[SN_SUB]=ss_range(&r,0,7);
        p[SN_PAMT]=0;p[P_E2]=64;p[SN_PDEC]=ss_range(&r,4,12);p[SN_FMDEC]=ss_range(&r,15,35);
        p[SN_FILTER]=ss_chance(&r,65)?3:2;p[SN_RESO]=ss_range(&r,0,10);p[P_E6]=ss_range(&r,87,111);
        p[P_DEC]=ss_range(&r,open?74:32,open?86:63);p[P_ATK]=ss_range(&r,0,4);p[P_E7]=ss_range(&r,0,9);
    } else if(channel==3u){ /* Tuned percussion: skin, wood, metal or a noisy body. */
        int family=ss_range(&r,0,3);p[SN_TUNE]=(int16_t)(12*ss_range(&r,-1,1));
        p[SN_WAVE1]=family==0?1:0;p[SN_WAVE2]=ss_range(&r,0,1);
        p[SN_ALG]=family==1?0:family==2?ss_range(&r,0,2):ss_chance(&r,25)?4:0;
        p[SN_RATIO]=family==1?16:ss_range(&r,19,99);p[SN_RATIO3]=ss_chance(&r,60)?16:32;
        p[P_E0]=ss_range(&r,family==1?0:25,family==1?27:81);p[SN_NOISE]=ss_range(&r,0,family==3?38:12);
        p[SN_SUB]=ss_range(&r,6,24);p[SN_PAMT]=family==1?ss_range(&r,6,16):0;
        p[SN_PDEC]=ss_range(&r,8,22);p[SN_FMDEC]=ss_range(&r,30,75);p[SN_FILTER]=1;
        p[SN_RESO]=ss_range(&r,0,15);p[P_E6]=ss_range(&r,73,118);p[P_ATK]=ss_range(&r,0,10);
        p[P_DEC]=ss_range(&r,family==2?75:34,family==2?89:71);
    } else if(channel==6u&&stattic_sy_fx_turn(&r)){   /* (SY: an 8-bit / FX sound, one turn in four) */
        snow_design_sy_fx(&r,p);
    } else if(channel==6u&&ss_chance(&r,20)){           /* (2026-11-10, the owner) SY: a percussive effect, one turn in seven or so */
        snow_design_sy_perc(&r,p);percfx=1;
    } else if(channel>=5u){ /* KEYS (SY, CH): electric pianos, organs, glassy and soft keys */
        if(channel==6u)p[P_ED_FX]=0;                  /* (SY: a keys sound, no FX trim) */
        static const uint8_t waves[]={0,0,0,1,5,4,2};static const uint8_t ratios[]={16,16,16,32,32,48,56,64,112};
        int kind=ss_range(&r,0,6);
        p[SN_ALG]=0;p[SN_WAVE2]=0;p[SN_WAVE3]=0;p[SN_SUB]=0;p[SN_MIX]=0;p[SN_FB_ROUTE]=0;
        p[SN_FILTER]=0;p[SN_RESO]=0;p[SN_PAMT]=0;p[SN_RATIO3]=8;
        p[SN_WAVE1]=waves[kind];p[SN_RATIO]=ratios[ss_rand(&r)%NELEM(ratios)];p[P_E1]=64;
        p[SN_TUNE]=(int16_t)(ss_chance(&r,80)?0:12*ss_range(&r,-1,1));p[SN_FINE]=ss_range(&r,-4,4);
        p[P_E0]=ss_range(&r,kind==4?4:18,kind==4?24:72);           /* TONE: FM depth */
        p[P_E2]=ss_range(&r,kind==4?0:40,kind==4?20:120);          /* HAMMER */
        p[P_E4]=ss_range(&r,20,100);p[P_E5]=ss_range(&r,0,28);       /* VELO, AIR */
        p[P_E6]=ss_range(&r,72,124);p[P_E7]=ss_range(&r,0,24);       /* BRIGHT, DRIVE */
        p[SN_NOISE]=ss_chance(&r,25)?ss_range(&r,1,8):0;p[SN_PDEC]=ss_range(&r,30,65);
        p[SN_FMDEC]=kind==4?0:ss_range(&r,70,100);p[SN_PW]=ss_range(&r,30,64);p[SN_MODE]=1;
        p[P_ATK]=ss_range(&r,0,ss_chance(&r,20)?60:6);p[P_DEC]=ss_range(&r,60,95);
        p[P_SUS]=ss_range(&r,kind==4?110:20,kind==4?127:100);p[P_REL]=ss_range(&r,30,80);
        p[P_GLIDE]=ss_range(&r,0,4);
        /* (2026-11-09, the owner: ambient IDM. Smooth scapes, warm dark keys, short plucks and mallets,
         * quiet glassy bells on top, short blips; as different as can be, one turn in seven keeps the
         * wider draw above as it was). A style sets its envelope, brightness and FM; everything else stays
         * drawn as above, so two sounds of a style still differ */
        static const uint8_t style_w[7]={22,16,16,12,10,9,15};   /* SCAPE DARK PLUCK MALLET BELL BLIP OPEN */
        static const int8_t style_trim[8]={-3,0,2,2,-5,3,0,0};      /* (SY: their loudness against each other) */
        uint32_t x=(uint32_t)ss_range(&r,0,99);keys_style=0;
        while(keys_style<6&&x>=style_w[keys_style]){x-=style_w[keys_style];keys_style++;}
        if(channel==5u&&ss_chance(&r,30))keys_style=7;          /* (2026-11-10, the owner) CH: a PAD, one turn in three or so */
        static const uint8_t harm[]={16,16,32,32,48},mallet[]={48,64,64,112},bell[]={56,56,88,112,112};
        switch(keys_style){
        case 0:                                                  /* SCAPE: slow, smooth, dark, held */
            p[SN_WAVE1]=ss_chance(&r,60)?0:ss_chance(&r,60)?1:5;p[SN_RATIO]=harm[ss_rand(&r)%NELEM(harm)];
            p[P_E0]=ss_range(&r,4,30);p[P_E2]=ss_range(&r,0,30);p[SN_FMDEC]=ss_range(&r,40,90);
            p[P_ATK]=ss_range(&r,34,92);p[P_DEC]=ss_range(&r,82,100);p[P_SUS]=ss_range(&r,100,127);p[P_REL]=ss_range(&r,64,108);
            p[P_E6]=ss_range(&r,36,72);p[P_E5]=ss_range(&r,0,10);p[P_E7]=ss_range(&r,0,10);p[P_E4]=ss_range(&r,10,40);
            p[SN_NOISE]=ss_chance(&r,30)?ss_range(&r,1,6):0;p[P_GLIDE]=ss_range(&r,0,14);
            break;
        case 1:                                                  /* DARK KEYS: warm, round */
            p[SN_WAVE1]=ss_chance(&r,70)?0:1;p[SN_RATIO]=harm[ss_rand(&r)%NELEM(harm)];
            p[P_E0]=ss_range(&r,14,44);p[P_E2]=ss_range(&r,30,90);p[SN_FMDEC]=ss_range(&r,60,95);
            p[P_ATK]=ss_range(&r,0,6);p[P_DEC]=ss_range(&r,70,95);p[P_SUS]=ss_range(&r,40,100);p[P_REL]=ss_range(&r,40,80);
            p[P_E6]=ss_range(&r,44,78);p[P_E7]=ss_range(&r,0,14);
            break;
        case 2:                                                  /* PLUCK: short, a bright hit falling warm */
            p[P_E0]=ss_range(&r,20,60);p[P_E2]=ss_range(&r,70,127);p[SN_FMDEC]=ss_range(&r,40,75);
            p[P_ATK]=ss_range(&r,0,2);p[P_DEC]=ss_range(&r,28,58);p[P_SUS]=ss_range(&r,0,24);p[P_REL]=ss_range(&r,24,55);
            p[P_E6]=ss_range(&r,54,96);
            break;
        case 3:                                                  /* MALLET: marimba, vibes, kalimba */
            p[SN_WAVE1]=0;p[SN_RATIO]=mallet[ss_rand(&r)%NELEM(mallet)];
            p[P_E0]=ss_range(&r,12,36);p[P_E2]=ss_range(&r,90,127);p[SN_FMDEC]=ss_range(&r,30,60);
            p[P_ATK]=0;p[P_DEC]=ss_range(&r,45,75);p[P_SUS]=ss_range(&r,0,30);p[P_REL]=ss_range(&r,30,60);
            p[P_E6]=ss_range(&r,58,100);
            break;
        case 4:                                                  /* BELL: glass and tines, quiet, shining on top */
            p[SN_WAVE1]=0;p[SN_RATIO]=bell[ss_rand(&r)%NELEM(bell)];
            p[P_E0]=ss_range(&r,18,48);p[P_E2]=ss_range(&r,40,100);p[SN_FMDEC]=ss_range(&r,60,95);
            p[P_ATK]=ss_range(&r,0,3);p[P_DEC]=ss_range(&r,70,98);p[P_SUS]=ss_range(&r,10,60);p[P_REL]=ss_range(&r,50,90);
            p[P_E6]=ss_range(&r,96,124);
            break;
        case 5:                                                  /* BLIP: a short pure tick */
            p[SN_WAVE1]=ss_chance(&r,60)?0:ss_chance(&r,50)?1:3;
            p[P_E0]=ss_range(&r,0,20);p[P_E2]=ss_range(&r,40,127);
            p[P_ATK]=0;p[P_DEC]=ss_range(&r,18,36);p[P_SUS]=0;p[P_REL]=ss_range(&r,10,30);
            p[P_E6]=ss_range(&r,60,100);
            break;
        case 7:                                                  /* PAD (CH): atmospheric, low, warm, a little lo-fi */
            p[SN_WAVE1]=ss_chance(&r,50)?0:ss_chance(&r,50)?1:5;p[SN_RATIO]=ss_chance(&r,70)?16:32;
            p[P_E0]=ss_range(&r,4,24);p[P_E2]=ss_range(&r,0,20);p[SN_FMDEC]=ss_range(&r,70,100);
            p[P_ATK]=ss_range(&r,50,100);p[P_DEC]=ss_range(&r,90,100);p[P_SUS]=ss_range(&r,110,127);p[P_REL]=ss_range(&r,80,115);
            p[P_E6]=ss_range(&r,30,60);p[P_E4]=ss_range(&r,10,30);p[P_E5]=ss_range(&r,0,6);p[P_E7]=ss_range(&r,4,22);
            p[SN_NOISE]=ss_range(&r,2,7);p[P_GLIDE]=ss_range(&r,0,4);
            if(ss_chance(&r,30))p[SN_TUNE]=-12;               /* (lower now and then; never higher) */
            break;
        default:break;                                           /* OPEN: the draw above */
        }
        if(channel==6u)p[P_ED_FX]=style_trim[keys_style];
        /* (2026-11-10, the owner) SY, CH: K2 never moves the notes off the scale: no fine detune, the FM
         * modulator only on whole harmonics (1x..7x: a half-way ratio such as 3.5x gives partials between
         * the notes), the shape centred, a glide no more than a touch */
        p[SN_FINE]=0;p[P_E1]=64;
        {int32_t q=(p[SN_RATIO]+8)/16;p[SN_RATIO]=(int16_t)(16*clamp(q,1,7));}
        if(p[P_GLIDE]>6)p[P_GLIDE]=ss_range(&r,0,6);
        if(channel==5u){                                 /* (2026-11-09, the owner) CH: no high chords. Never an octave up,
                                                          * no high bell partials (the modulator at most 4x) */
            static const uint8_t ch_ratio[]={16,16,32,32,48,64,64};
            if(p[SN_TUNE]>0)p[SN_TUNE]=0;
            if(p[SN_RATIO]>64)p[SN_RATIO]=ch_ratio[ss_rand(&r)%NELEM(ch_ratio)];
        }
    } else { /* Bass/chord: stable note tracking, harmonic ratios, gated envelopes. */
        static const uint8_t waves[]={0,1,2,4,5};int voice=ss_range(&r,0,4);
        p[SN_ALG]=ss_chance(&r,30)?4:ss_range(&r,0,2);
        p[SN_WAVE1]=waves[voice];p[SN_WAVE2]=ss_chance(&r,65)?0:ss_chance(&r,50)?1:5;
        if(channel==4u){                                 /* (2026-11-10, the owner) BS: sine basses of every kind, now and then
                                                          * a triangle or a square, never a saw (nor a saw-bright FM) */
            static const uint8_t bw[10]={0,0,0,0,0,0,1,1,3,5};
            voice=ss_chance(&r,65)?0:1;p[SN_WAVE1]=bw[ss_rand(&r)%10u];
            p[SN_WAVE2]=ss_chance(&r,80)?0:1;p[SN_ALG]=ss_chance(&r,25)?4:ss_chance(&r,70)?0:ss_range(&r,1,2);
        }
        p[SN_TUNE]=(int16_t)(channel==4u?(ss_chance(&r,65)?-24:-12):12*ss_range(&r,-1,1));
        p[SN_NOISE]=ss_range(&r,0,4);p[P_E0]=ss_range(&r,voice==0?12:0,channel==4u?68:61);
        if(channel==4u)p[P_E0]=ss_range(&r,0,ss_chance(&r,70)?30:46);   /* (TONE: an FM colour on the sine, not a saw's edge) */
        p[SN_RATIO3]=ss_chance(&r,channel==4u?65:20)?8:ss_harmonic(&r);
        p[SN_SUB]=ss_range(&r,channel==4u?10:0,channel==4u?38:18);
        p[SN_FILTER]=1;p[SN_RESO]=ss_range(&r,0,channel==4u?31:22);
        p[P_E6]=ss_range(&r,channel==4u?53:69,channel==4u?104:119);
        p[SN_PAMT]=ss_range(&r,0,channel==4u?3:1);p[P_E2]=ss_range(&r,62,66);
        p[SN_PDEC]=ss_range(&r,4,12);p[SN_FMDEC]=ss_range(&r,35,94);
        p[P_ATK]=ss_range(&r,0,channel==4u?12:76);p[P_DEC]=ss_range(&r,52,95);
        p[P_SUS]=ss_range(&r,channel==4u?76:58,117);p[P_REL]=ss_range(&r,channel==4u?21:40,channel==4u?64:87);
        p[P_GLIDE]=ss_range(&r,0,channel==4u?17:7);
    }
    /* Movement is shallow and timbral. Pitch/FM transients above already
     * express the instrument; a second random pitch envelope must not undo it. */
    p[P_ED_PIT]=0;p[P_ED_SHP]=ss_range(&r,-3,4);p[P_ED_FLT]=ss_range(&r,0,channel==4u?19:10);
    p[P_LD_SHP]=ss_range(&r,-3,3);p[P_LD_FLT]=ss_range(&r,-3,3);p[P_LD_AMP]=ss_range(&r,0,4);
    p[P_LRATE]=ss_range(&r,18,83);p[P_LWAVE]=ss_range(&r,0,2);p[P_LPHASE]=ss_range(&r,0,127);
    p[P_LFADE]=ss_range(&r,0,60);p[P_LSYNC]=ss_chance(&r,35)?ss_range(&r,4,9):0;p[P_LTRIG]=ss_range(&r,0,1);
    static const uint8_t destinations[]={1,2,5,7},keys_dest[]={1,5,7};
    p[SN_L2_DEST]=channel>=5u?keys_dest[ss_rand(&r)%NELEM(keys_dest)]:destinations[ss_rand(&r)%NELEM(destinations)];   /* (SY, CH: not SHAPE) */p[SN_L2_DEPTH]=ss_range(&r,-5,5);
    p[SN_L2_RATE]=ss_range(&r,15,76);p[SN_L2_WAVE]=ss_range(&r,0,2);p[SN_L2_SYNC]=ss_chance(&r,35)?ss_range(&r,4,9):0;
    p[SN_L2_PHASE]=ss_range(&r,0,127);p[SN_L2_FADE]=ss_range(&r,0,63);p[SN_L2_TRIG]=ss_range(&r,0,1);
    p[P_EG2_ATK]=ss_range(&r,0,42);p[P_EG2_DEC]=ss_range(&r,40,93);p[P_EG2_SUS]=ss_range(&r,0,86);p[P_EG2_REL]=ss_range(&r,30,77);
    snow_design_fx(channel,&r,p);
    if(channel>=5u){
        /* (2026-11-09, the owner: atmosphere) SY and CH: each K2 sound comes with its own space, drawn wide
         * (stattic_runtime adds the rack's sends and shapes to K2's mask on these two): SCAPE in a big room,
         * PLUCK, BELL and BLIP into delays, the keys and mallets in between; SY's 8-bit sounds into a delay */
        static const uint8_t rv_lo[8]={60,34,28,40,52,22,30,70},rv_hi[8]={110,72,62,80,98,56,74,112};
        static const uint8_t dl_lo[8]={8,12,40,24,42,48,10,4},dl_hi[8]={42,46,84,62,88,92,60,36};
        uint32_t st=keys_style<0?5u:(uint32_t)keys_style;
        p[P_REV]=ss_range(&r,rv_lo[st],rv_hi[st]);p[P_DLY]=ss_range(&r,dl_lo[st],dl_hi[st]);
        p[SN_RSIZE]=ss_range(&r,st==0u||st==4u||st==7u?96:72,127);p[SN_RDAMP]=ss_range(&r,20,80);
        p[SN_RPRE]=ss_range(&r,0,st==0u?44:28);p[SN_RTYPE]=ss_chance(&r,70)?ss_range(&r,1,2):0;   /* (longer tails) */
        p[SN_DSYNC]=ss_chance(&r,70)?ss_range(&r,5,9):0;p[SN_DTIME]=ss_range(&r,18,72);
        p[SN_DFB]=ss_range(&r,st==2u||st==4u||st==5u?38:22,st==2u||st==4u||st==5u?74:58);
        p[SN_DCOLOR]=ss_range(&r,44,100);p[SN_DWIDTH]=ss_range(&r,48,124);
        p[P_CHOR]=ss_range(&r,st==7u?16:0,st==0u||st==1u||st==7u?46:24);
        if(st==7u){                                      /* (PAD: worn tape, lo-fi: stattic_runtime adds it to K2's mask) */
            p[SN_TMIX]=ss_range(&r,30,80);p[SN_TDRIVE]=ss_range(&r,20,50);p[SN_TTONE]=ss_range(&r,60,95);
            p[SN_TWOW]=ss_range(&r,6,22);p[SN_TFLUT]=ss_range(&r,2,10);
        }
        stattic_k2_style=(int8_t)(percfx?9:st);
    }
    if(channel==6u)p[P_ED_FX]=(int16_t)clamp(p[P_ED_FX]+stattic_space_trim(p),-64,63);   /* (CH: in stattic_runtime, with its own) */
    if(channel>=5u&&!percfx){                            /* (2026-11-10, the owner) SY, CH: the shortest sounds last ~0.7-1 s, and
                                                          * every note fades out at its end, never cut off */
        if(p[P_SUS]<40&&p[P_DEC]<90)p[P_DEC]=ss_range(&r,90,96);
        if(channel==6u&&p[P_REL]<72)p[P_REL]=ss_range(&r,72,86);   /* (~0.2-0.5 s; CH keeps its two-bar release, below) */
        if(channel==6u&&!ss_chance(&r,10)){             /* (2026-11-10, the owner) SY: long sounds with a little attack; one in ten as drawn */
            int32_t was=p[P_SUS]<40?12:p[P_DEC]<100||p[P_REL]<92?5:0;     /* (as loud as before: a short sound made long is trimmed) */
            p[P_ED_FX]=(int16_t)clamp(p[P_ED_FX]-was,-64,63);
            p[P_ATK]=(int16_t)(p[P_ATK]>62?p[P_ATK]:ss_range(&r,45,62));  /* (~25-90 ms; a slower one kept) */
            if(p[P_DEC]<100)p[P_DEC]=ss_range(&r,100,110);            /* (1.4-2.9 s) */
            if(p[P_SUS]<70)p[P_SUS]=ss_range(&r,70,105);
            if(p[P_REL]<92)p[P_REL]=ss_range(&r,92,104);              /* (0.8-1.9 s tails) */
        }
    }
    /* CH keeps its two-bar release, or the user's manual choice. */
    if(channel==5u)p[P_REL]=stattic_base_value(t,P_REL);
    if(channel>=4u&&p[P_ATK]<ST_ATK_MIN)p[P_ATK]=(int16_t)(ST_ATK_MIN+ss_range(&r,0,3));   /* (the owner: BS, SY, CH attack 40 ms at least) */
    for(uint32_t id=0;id<P_COUNT;id++)if((mask[id/32u]>>(id%32u))&1u){
        const param_desc_t *d=id>=P_IA&&id<=P_IC?ins_desc(p[P_ITYPE],id-P_IA):track_desc(t,id);
        p[id]=(int16_t)clamp(p[id],d->min,d->max);
    }

}
/* A section command changes its own controls, including the four dynamic MOD
 * slots. Depth budgets keep the existing instrument playable. */
static void snow_design_lfo(uint32_t channel,uint32_t *r,int16_t *p)
{
    p[P_LRATE]=ss_range(r,15,90);p[P_LWAVE]=ss_range(r,0,4);p[P_LPHASE]=ss_range(r,0,127);
    p[P_LFADE]=ss_range(r,0,72);p[P_LSYNC]=ss_chance(r,65)?ss_range(r,2,10):0;
    p[P_LTRIG]=ss_range(r,0,1);p[P_LPOL]=ss_range(r,0,1);p[P_ESYNC]=ss_range(r,0,1);
    p[P_LD_PIT]=ss_range(r,channel<4?-1:-2,channel<4?1:2);
    p[P_LD_FLT]=ss_range(r,-18,18);p[P_LD_SHP]=ss_range(r,-14,14);p[P_LD_AMP]=ss_range(r,0,12);
    p[P_ED_PIT]=ss_range(r,-1,1);p[P_ED_FLT]=ss_range(r,-6,8);p[P_ED_SHP]=ss_range(r,-4,5);
    static const uint8_t dest2[]={1,2,4,5,7,8};
    p[SN_L2_RATE]=ss_range(r,12,86);p[SN_L2_WAVE]=ss_range(r,0,4);
    p[SN_L2_PHASE]=ss_range(r,0,127);p[SN_L2_FADE]=ss_range(r,0,72);
    p[SN_L2_SYNC]=ss_chance(r,65)?ss_range(r,2,10):0;p[SN_L2_TRIG]=ss_range(r,0,1);
    p[SN_L2_DEST]=dest2[ss_rand(r)%NELEM(dest2)];p[SN_L2_DEPTH]=ss_range(r,4,channel==0?8:18)*(ss_chance(r,50)?1:-1);
    if(channel>=5u){                                     /* (2026-11-10, the owner) SY, CH: the LFOs never touch the pitch:
                                                          * no LFO or envelope on PITCH, and LFO B not on SHAPE (on KEYS it
                                                          * detunes the modulator, the notes go out of the scale) */
        static const uint8_t keys2[]={1,4,7};            /* (TONE, DECAY, CUTOFF: not VELO or DRIVE, they change the loudness) */
        p[P_LD_PIT]=0;p[P_ED_PIT]=0;
        if(p[SN_L2_DEST]!=1&&p[SN_L2_DEST]!=4&&p[SN_L2_DEST]!=7)p[SN_L2_DEST]=keys2[ss_rand(r)%NELEM(keys2)];
    }
    static const uint8_t dest[]={MD_CUT,MD_SHP,MD_PAN,MD_E1,MD_E1+4,MD_E1+6};
    uint32_t active=(uint32_t)ss_range(r,1,4);
    for(uint32_t i=0;i<4;i++){
        p[P_M1SRC+i*3u]=ss_chance(r,70)?MS_LFO:ss_chance(r,50)?MS_SH:MS_SLEW;
        p[P_M1DST+i*3u]=dest[ss_rand(r)%NELEM(dest)];
        if(channel>=5u&&p[P_M1DST+i*3u]==MD_E1+4)p[P_M1DST+i*3u]=MD_CUT;   /* (SY, CH: not VELO: the loudness stays) */
        p[P_M1AMT+i*3u]=i<active?ss_range(r,2,7)*(ss_chance(r,50)?1:-1):0;
    }
}
static void snow_design_sound(const track_t *t,uint32_t channel,uint32_t seed,const uint32_t *mask,int16_t *p)
{
    for(uint32_t id=0;id<P_COUNT;id++)p[id]=stattic_base_value(t,id);
    snow_design_sound_from(t,channel,seed,mask,p);
}
/* Pure preparation: p is the caller's base/candidate, never the sounding
 * values. Descriptors still use the real track (its role is pointer-based). */
static void snow_design_section(track_t *t,uint32_t channel,uint32_t family,uint32_t *mask,int16_t *next)
{
    snow_family_mask(family,mask);
    if(!snow_sound_state[(uint32_t)(t-trk)%NTRK])snow_sound_state[(uint32_t)(t-trk)%NTRK]=rng()^fm1_ms^(channel+1u)*0x9e3779b9u;
    snow_sound_state[(uint32_t)(t-trk)%NTRK]+=0x9e3779b9u; /* full 32-bit seed walk, not a preset cycle */
    if(family==FAM_EDIT)snow_design_sound_from(t,channel,snow_sound_state[(uint32_t)(t-trk)%NTRK],mask,next);
    else {
        uint32_t r=snow_sound_state[(uint32_t)(t-trk)%NTRK];
        if(family==FAM_LFO)snow_design_lfo(channel,&r,next);
        else {
            snow_design_fx(channel,&r,next);
            /* Dedicated FX generation has an audible lead processor. EDIT's
             * instrument designer retains its gentle, body-preserving rack. */
            uint32_t lead=(uint32_t)ss_range(&r,0,5);
            next[P_DLY]=ss_range(&r,10,44);next[P_REV]=ss_range(&r,10,48);
            next[P_CHOR]=ss_range(&r,0,32);next[P_DTYPE]=ss_range(&r,0,1);
            next[SN_DFB]=ss_range(&r,35,83);next[SN_RSIZE]=ss_range(&r,55,108);
            next[SN_CDEPTH]=ss_range(&r,27,72);next[SN_CFB]=ss_range(&r,0,29);
            if(lead==0)next[P_DLY]=ss_range(&r,65,106);
            if(lead==1)next[P_REV]=ss_range(&r,68,112);
            if(lead==2)next[P_CHOR]=ss_range(&r,60,106);
            if(lead==3){next[SN_TMIX]=ss_range(&r,65,112);next[SN_TDRIVE]=ss_range(&r,30,76);next[SN_TWOW]=ss_range(&r,4,20);}
            if(lead==4){next[SN_BMIX]=ss_range(&r,40,86);next[SN_BBITS]=ss_range(&r,7,12);next[SN_BRATE]=ss_range(&r,2,9);}
            if(lead==5){next[P_IMIX]=ss_range(&r,50,98);next[P_DIST]=ss_range(&r,12,32);}
            next[P_SLCR]=channel<4u&&ss_chance(&r,20)?ss_range(&r,1,2):0;
            next[P_SLPAT]=ss_range(&r,1,16);next[P_SLRATE]=ss_range(&r,0,4);next[P_SLDEPTH]=ss_range(&r,12,42);
            next[P_DKSRC]=ss_chance(&r,25)?(int16_t)((channel+(uint32_t)ss_range(&r,1,5))%NTRK+1u):0;
            next[P_DKAMT]=ss_range(&r,12,48);next[P_DKREL]=ss_range(&r,40,84);
            if(t==&trk[1]||t==&trk[2]){                 /* (2026-11-09, the owner) SY, CH: delay and reverb anywhere up to
                                                         * ~90 %, the reverb's tail long (size, its longer models, less damping) */
                next[P_DLY]=ss_range(&r,0,114);next[P_REV]=ss_range(&r,6,114);
                next[SN_RSIZE]=ss_range(&r,80,127);next[SN_RTYPE]=ss_chance(&r,70)?ss_range(&r,1,2):0;
                next[SN_RDAMP]=ss_range(&r,16,72);next[SN_RPRE]=ss_range(&r,0,40);next[SN_DFB]=ss_range(&r,24,70);
            }
            if(t==TDRUM){                               /* (2026-11-09) DR's rack (shared with BS): a softer space */
                next[P_REV]=ss_range(&r,12,40);next[P_DLY]=ss_chance(&r,55)?ss_range(&r,4,26):0;
                if(next[P_CHOR]>30)next[P_CHOR]=ss_range(&r,0,30);
                if(next[SN_DFB]>60)next[SN_DFB]=ss_range(&r,30,60);
            }
            if(channel==4u){
                /* (the owner) BS has no rack: HOME 2 K4 draws its colour and its place in the mix
                 * instead: DRIVE (type, amount, tone), an INSERT now and then, how much it sends
                 * to the shared effects, and the kick ducking it (a sidechain pump, often) */
                static const uint8_t types[]={0,0,1,3,4};
                next[P_REV]=ss_chance(&r,60)?ss_range(&r,0,12):ss_range(&r,12,40);   /* FX SEND: mostly dry */
                next[P_DIST]=ss_chance(&r,70)?ss_range(&r,4,34):0;next[P_DTYPE]=types[ss_rand(&r)%NELEM(types)];
                next[P_DTONE]=ss_range(&r,-24,16);
                next[P_IMIX]=ss_chance(&r,25)?ss_range(&r,8,30):0;
                next[P_DKSRC]=ss_chance(&r,45)?(int16_t)(TRK_DRUM+1u):0;next[P_DKAMT]=ss_range(&r,16,56);
                next[P_SLCR]=0;
            }
        }
        for(uint32_t id=0;id<P_COUNT;id++)if((mask[id/32u]>>(id%32u))&1u){
            const param_desc_t *d=id>=P_IA&&id<=P_IC?ins_desc(next[P_ITYPE],id-P_IA):track_desc(t,id);
            next[id]=(int16_t)clamp(next[id],d->min,d->max);
        }
    }
}
