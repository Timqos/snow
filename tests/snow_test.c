/* SPDX-License-Identifier: GPL-3.0-only */
#define FELUCCA_SNOW 1
#include <assert.h>
#include "../web/emu/felucca_web.c"
static int32_t samples[44100];
static void advance(uint32_t ms) { for(uint32_t n=0;n<ms*441u/10u;n+=128)web_render(128); }
static void tap(uint32_t b) {web_buttons(1u<<b);advance(60);web_buttons(0);advance(60);}
static uint32_t sound(uint32_t track,uint32_t frames,uint32_t gate)
{
    track_t *t=&trk[track];voice_t *v=&t->v[0];int32_t block[CTL],last=0;
    memset(v,0,sizeof *v);v->note=60;v->vel=100;v->gate=v->active=1;v->stage=1;
    snow_note_on(t,v);uint32_t hash=0,peak=0;
    for(uint32_t pos=0;pos<frames;pos+=CTL) {
        if(pos>=gate)v->gate=0;
        snow_block(t);
        esync_coef(t);
        vmod_t m={.pitch16=60*16,.shape=64<<8,.amp0=last};m.amp1=snow_amp(t,v,0);last=m.amp1;
        memset(block,0,sizeof block);snow_render(t,v,block,CTL,&m);
        for(uint32_t i=0;i<CTL && pos+i<frames;i++) {
            int32_t x=block[i];if(pos+i<44100u)samples[pos+i]=x;
            uint32_t a=(uint32_t)(x<0?-x:x);if(a>peak)peak=a;
            hash=hash*31u+(uint32_t)x;
        }
    }
    assert(peak>50 && peak<60000);
    return hash;
}
static void synth_tests(void)
{
    for(uint32_t k=0;k<NTRK;k++) {
        assert(SNOW_ENG[k].presets==SNOW_PATCHES[k] && SNOW_ENG[k].npresets==10);
        assert(!strcmp(SNOW_ENG[k].name,SN_ROLES[k]));
        for(uint32_t other=0;other<NTRK;other++)assert(eng_allowed(k,ENGI_SNOW+other)==(k==other));
        uint32_t hashes[10];
        for(uint32_t p=0;p<10;p++) {
            apply_preset_to(&trk[k],p);hashes[p]=sound(k,16384,8192);
            for(uint32_t j=0;j<p;j++)assert(hashes[j]!=hashes[p]);
        }
    }
    apply_preset_to(&trk[0],0);(void)sound(0,22050,22050);
    uint32_t crossings=0;int64_t energy=0;
    for(uint32_t i=2206;i<8820;i++){crossings+=samples[i-1]<=0 && samples[i]>0;energy+=samples[i]<0?-samples[i]:samples[i];}
    double hz=(double)crossings*44100.0/(8820-2206);
    printf("Kick body after transient: %.1f Hz, %lld energy\n",hz,(long long)energy);
    assert(hz>=30 && hz<=90 && energy>10000);
    /* Every channel can use every algorithm and every oscillator waveform. */
    for(uint32_t a=0;a<6;a++) {
        apply_preset_to(&trk[4],0);trk[4].p[SN_ALG]=(int16_t)a;trk[4].p[SN_MIX]=55;
        uint32_t previous=0;
        for(uint32_t w=0;w<6;w++) {
            trk[4].p[SN_WAVE1]=(int16_t)w;trk[4].p[SN_PW]=w==4?40:64;
            uint32_t h=sound(4,8192,4096);assert(!w || h!=previous);previous=h;
        }
    }
    apply_preset_to(&trk[4],0);trk[4].p[SN_MODE]=1;trk[4].p[P_ATK]=0;
    uint32_t fast=sound(4,8192,4000);trk[4].p[P_ATK]=85;assert(sound(4,8192,4000)!=fast);
    trk[4].p[P_ATK]=0;trk[4].p[P_REL]=100;assert(sound(4,8192,4000)!=fast);
    trk[4].p[SN_L2_DEPTH]=40;trk[4].p[SN_L2_DEST]=1;
    uint32_t lfo1=trk[4].lfo_ph;int32_t before=snow_l2[4].value;
    for(uint32_t i=0;i<200;i++)snow_block(&trk[4]);
    assert(trk[4].lfo_ph==lfo1 && snow_l2[4].value!=before);
    uint32_t pages=0;sel_set(0);
    for(uint32_t i=0;i<NPAGES;i++)if(PAGES[i].fam==FAM_EDIT && page_visible(i))pages++;
    printf("Visible EDIT pages: %u\n",pages);assert(pages>=18);
    for(uint32_t i=0;i<NPAGES;i++)if(PAGES[i].id[0]==P_ATK)assert(page_visible(i));
}
static void generation_tests(void)
{
    seq_stop();memset(&motion,0,sizeof motion);
    for(uint32_t k=0;k<NTRK;k++) {
        track_t *t=&trk[k];t->p[P_SLEN]=32;t->p[P_ROOT]=2;t->p[P_SCALE]=2;
        t->p[SN_G_KIND]=2;t->p[SN_G_DENS]=50;t->p[SN_G_COMPLEX]=30;t->p[SN_G_VARIANT]=55;
        uint64_t masks[256];uint32_t unique=0,notes=0;uint32_t firstnote=128;int pitched=0;
        for(uint32_t i=0;i<256;i++) {
            assert(!snow_generate(k,1));uint64_t mask=snow_hits(t->step,32);
            if(i)assert(mask!=masks[i-1]);masks[i]=mask;
            int seen=0;for(uint32_t j=0;j<i;j++)seen|=mask==masks[j];unique+=!seen;
            for(uint32_t j=0;j<32;j++)if(t->step[j].time==ST_NOTE&&t->step[j].n){notes++;
                assert(t->step[j].vel>0);assert(k==5?(t->step[j].n>=3&&t->step[j].n<=4):t->step[j].n==1);
                for(uint32_t n=0;n<t->step[j].n;n++)assert(scale_snap(t,t->step[j].note[n])==t->step[j].note[n]);
                if(firstnote==128)firstnote=t->step[j].note[0];else pitched|=firstnote!=t->step[j].note[0];
            }
        }
        printf("Role %u: %u/256 distinct hit masks, %u notes\n",k+1,unique,notes);assert(unique>40);if(k>=3)assert(pitched);
        step_t first[NSTEP];memcpy(first,t->step,sizeof first);assert(!snow_generate(k,0));assert(!memcmp(first,t->step,sizeof first));
        t->p[SN_G_DENS]=0;assert(!snow_generate(k,0));assert(!snow_hits(t->step,32));
        t->p[SN_G_DENS]=100;assert(!snow_generate(k,0));assert(snow_hits(t->step,32)==0xffffffffu);t->p[SN_G_DENS]=50;
    }
    apply_preset_to(&trk[3],0);assert(trk[3].p[SN_KEYTRACK]==127);
    track_t *t=&trk[0];t->p[P_SLEN]=32;assert(!snow_generate(0,1));
    motion_set_lock(t,0,P_E0,99);t->p[SN_G_LOCKS]=100;
    step_t old[NSTEP];memcpy(old,t->step,sizeof old);
    for(uint32_t i=0;i<30;i++){assert(!snow_generate_parameters(0));assert(!memcmp(old,t->step,sizeof old));assert(motion.count>1&&motion.count<=9);assert(motion.event[0].value==99&&!motion.event[0].generated);}
    uint32_t seed=snow_seed(t);t->p[SN_G_LIVE]=1;song.playing=1;take_pass[0]=2;snow_gen_pass[0]=1;
    snow_sequence_poll();assert(snow_seed(t)!=seed);seed=snow_seed(t);snow_sequence_poll();assert(snow_seed(t)==seed);
    song.playing=0;t->p[SN_G_LIVE]=0;
    assert(!snow_generate_scene(1));for(uint32_t k=0;k<NTRK;k++)trk[k].p[SN_G_LOCKS]=100;
    assert(!snow_parameter_scene());assert(motion.count<=49);
    assert(!project_save(1));uint32_t owned=0;for(uint32_t i=0;i<motion.count;i++)owned+=motion.event[i].generated;
    seed=snow_seed(&trk[5]);assert(owned);project_load(1);uint32_t loaded=0;for(uint32_t i=0;i<motion.count;i++)loaded+=motion.event[i].generated;
    assert(loaded==owned&&snow_seed(&trk[5])==seed);
    int16_t fx=trk[0].p[SN_DTIME];assert(!snow_clear_sequence(1));assert(!motion.count);
    for(uint32_t k=0;k<NTRK;k++)assert(!snow_hits(trk[k].step,NSTEP));assert(trk[0].p[SN_DTIME]==fx);
    puts("Random phrases / GEN PARM / ownership / pitched percussion / CLEAR passed");
}
static void ornament_tests(void)
{
    track_t *t=&trk[2];step_t s={0};s.time=ST_NOTE;s.n=1;s.cond=TC_ORNAMENT;step_set_ratchet(&s,3);t->p[P_SDIV]=2;
    for(uint32_t len=4;len<=64;len*=2){t->p[P_SLEN]=(int16_t)len;memset(snow_orn_bar,255,sizeof snow_orn_bar);memset(snow_orn_step,0,sizeof snow_orn_step);
        uint32_t rolls[16]={0};
        for(uint32_t i=0;i<128;i++){take_pass[2]=(uint16_t)(i/len+1);t->fire_idx=(uint16_t)(i%len);uint32_t bar=(uint32_t)((uint64_t)i*div_samples(2)/(beat_samples()*4u));uint32_t hits=snow_step_hits(t,&s);assert(hits>=1);if(hits>1){assert(bar&1u);rolls[bar]++;assert(snow_step_hits(t,&s)==3);}}
        for(uint32_t bar=0;bar<16;bar++)assert(rolls[bar]<=1);
    }
    puts("Rare triplets: primary hit every bar, <=1 ornament every second bar passed");
}
static void patterns_and_snapshot(void)
{
    web_snow_demo();seq_stop();
    step_t a[NSTEP],other[NSTEP];memcpy(a,trk[0].step,sizeof a);memcpy(other,trk[1].step,sizeof other);
    assert(!snow_pattern_select(0,1));assert(lists.cur[0]==1);
    assert(snow_pattern_current(0)==1);
    trk[0].step[2]=(step_t){{70},1,ST_NOTE,0,100};
    assert(!snow_pattern_select(0,0));assert(lists.cur[0]==0);
    assert(!memcmp(a,trk[0].step,sizeof a) && !memcmp(other,trk[1].step,sizeof other));
    assert(!snow_pattern_select(0,1));assert(trk[0].step[2].note[0]==70);
    snow_temp_save();motion_store_t locks=motion;
    snow_mask=62;snow_control_all(0,10);assert(!snow_offset[0][0] && snow_offset[1][0]==10);
    assert(!memcmp(&locks,&motion,sizeof locks));
    tap(B_PLAY);advance(500);assert(song.playing);
    uint32_t pos=trk[0].seq_pos,index=trk[0].seq_idx,writes=web_flash_writes_count();
    trk[0].step[2].note[0]=80;assert(snow_temp_reload(63));
    assert(song.playing && trk[0].seq_pos==pos && trk[0].seq_idx==index);
    assert(trk[0].step[2].note[0]==70 && web_flash_writes_count()==writes);
    seq_stop();memset(snow_offset,0,sizeof snow_offset);snow_mask=63;
    trk[0].p[P_E0]=40;snow_control_all(0,10);project_t project;project_capture(&project);
    assert(project.t[0].p[P_E0]==50 && !project_restore_runtime(&project));
    assert(trk[0].p[P_E0]==50 && !snow_offset[0][0]);
}
static void fx_tests(void)
{
    seq_stop();track_t *t=&trk[0];apply_preset_to(t,0);
    int32_t b[CTL];
    /* A channel's impulse must never write another channel's state. */
    memset(snow_fx,0,sizeof snow_fx);t->p[P_DLY]=110;t->p[P_REV]=110;t->p[P_CHOR]=100;
    t->p[SN_DTIME]=5;t->p[SN_DFB]=85;
    int64_t tail=0;
    for(uint32_t block=0;block<1400;block++) {
        memset(b,0,sizeof b);if(!block)b[0]=60000;
        snow_fx_process(t,b,CTL);
        if(block>100)for(uint32_t i=0;i<CTL;i++){tail+=llabs(b[i]);assert(llabs(b[i])<500000);}
    }
    snow_fx_t *other=&snow_fx[1];const uint8_t *bytes=(const uint8_t *)other;
    for(uint32_t i=0;i<sizeof *other;i++)assert(bytes[i]==0);
    assert(tail>10000);assert(snow_fx[0].pos==1400*CTL);
    for(uint32_t block=0;block<4000;block++) {
        memset(b,0,sizeof b);snow_fx_process(t,b,CTL);
        if(block>=3980)for(uint32_t i=0;i<CTL;i++)assert(!b[i]&&!snow_fx[0].side[i]);
    }
    memset(b,0,sizeof b);snow_fx_process(&trk[1],b,CTL);
    for(uint32_t i=0;i<CTL;i++)assert(!b[i]&&!snow_fx[1].side[i]);
    /* Tape and bitcrush have audible, independently controlled responses. */
    uint32_t hash[3];
    for(uint32_t mode=0;mode<3;mode++) {
        memset(snow_fx,0,sizeof snow_fx);t->p[P_DLY]=t->p[P_REV]=t->p[P_CHOR]=0;
        t->p[SN_TMIX]=mode==1?100:0;t->p[SN_BMIX]=mode==2?100:0;
        t->p[SN_BBITS]=5;t->p[SN_BRATE]=9;uint32_t h=0;
        for(uint32_t block=0;block<100;block++) {
            for(uint32_t i=0;i<CTL;i++)b[i]=sine_i((block*CTL+i)*0x03124455u)*2;
            snow_fx_process(t,b,CTL);
            for(uint32_t i=0;i<CTL;i++)h=h*31u+(uint32_t)b[i];
        }
        hash[mode]=h;
        for(uint32_t block=0;block<1000;block++) {
            memset(b,0,sizeof b);snow_fx_process(t,b,CTL);
            if(block>=990)for(uint32_t i=0;i<CTL;i++)assert(!b[i]&&!snow_fx[0].side[i]);
        }
    }
    assert(hash[0]!=hash[1]&&hash[0]!=hash[2]&&hash[1]!=hash[2]);
    /* Every FX value survives permanent project capture/restore, including appended stable IDs. */
    t->p[SN_DTIME]=73;t->p[SN_RPRE]=29;t->p[SN_TMIX]=83;t->p[SN_BBITS]=6;
    project_t project;project_capture(&project);t->p[SN_DTIME]=1;t->p[SN_TMIX]=0;
    assert(!project_restore_runtime(&project));
    assert(t->p[SN_DTIME]==73&&t->p[SN_RPRE]==29&&t->p[SN_TMIX]==83&&t->p[SN_BBITS]==6);
    uint8_t packed[SID_COUNT+1u];int16_t restored[P_COUNT]={0};
    assert(sid_put(packed,t->p)==sizeof packed);assert(sid_get(restored,packed,sizeof packed)==sizeof packed);
    assert(!memcmp(restored,t->p,sizeof restored));assert(sid_p(SID_E0)==P_E0&&sid_p(SID_E0+8u)==P_LPAN0);
    trk[5].p[SN_BBITS]=3;trk[5].p[SN_RTYPE]=2;
    assert(!project_save(0));t->p[SN_DTIME]=1;trk[5].p[SN_BBITS]=16;
    project_load(0);assert(trk[0].p[SN_DTIME]==73&&trk[5].p[SN_BBITS]==3&&trk[5].p[SN_RTYPE]==2);
    puts("Independent FX: isolated state/tails, Tape, Bitcrush and project restoration passed");
}
static void pattern_controls(void)
{
    web_snow_demo();seq_stop();snow_pattern_half=0;
    web_buttons(1u<<B_SEQ);advance(700);assert(layer_open()==LAYER_SEQ);
    uint32_t before=snow_pattern_current(1),kick=snow_pattern_current(0),bass=snow_pattern_current(4);
    web_enc(EN_K1,1);advance(60);assert(snow_pattern_current(0)==kick+1&&snow_pattern_current(1)==before);
    uint32_t chord=snow_pattern_current(5);web_enc(EN_ALGO,1);advance(60);assert(snow_pattern_current(4)==bass+1);
    web_enc(EN_PRESET,1);advance(60);assert(snow_pattern_current(5)==chord+1);
    assert(snow_channels_active);assert(snow_pattern_current(0)==kick+1);
    web_buttons(0);advance(90);
    uint32_t current=snow_pattern_current(0),next=(current+1u)%ST_PATTERNS;
    assert(!snow_pattern_select(0,next));assert(!snow_pattern_select(0,current));
    while(pend_free()<PEND_N)assert(!pat_flush_one());
    tap(B_PLAY);assert(song.playing);
    assert(!snow_pattern_select(0,next));step_t outgoing[NSTEP];memcpy(outgoing,trk[0].step,sizeof outgoing);
    assert(snow_generate(0,1)==2&&!memcmp(outgoing,trk[0].step,sizeof outgoing));
    advance(4600);assert(snow_pattern_current(0)==next&&!snow_generate(0,1));seq_stop();
    puts("Held SEQ: all six pattern encoders on one screen passed");
}
static void icon_position_tests(void)
{
    web_snow_color(0);advance(100);
    /* User-requested invariant: identical visible 64x64 ink bounds, every
     * parameter position, with one filled system ink and a fixed outer guard. */
    for(uint32_t kind=0;kind<4;kind++){
        uint32_t changes=0,previous=0;
        for(uint32_t q=0;q<=1000;q++){
            cv_begin(100,100,T_BG);snow_icon(50,50,kind,q,2,T_THEME);
            uint32_t minx=100,miny=100,maxx=0,maxy=0,pixels=0,hash=2166136261u;
            for(uint32_t y=0;y<100;y++)for(uint32_t x=0;x<100;x++){
                uint16_t p=cv_px[y*100+x];assert(p==swap16(T_BG)||p==swap16(T_THEME));hash=(hash^p)*16777619u;
                if(p!=swap16(T_BG)){pixels++;if(x<minx)minx=x;if(x>maxx)maxx=x;if(y<miny)miny=y;if(y>maxy)maxy=y;}
            }
            assert(minx==19&&maxx==82&&miny==19&&maxy==82&&pixels>600);
            changes+=q&&hash!=previous;previous=hash;
        }assert(changes>=5);printf("Geometry %u: %u visible transitions, exact equal 64x64 bounds at all 1001 positions, filled theme ink passed\n",kind,changes);
    }
    FILE *ref=fopen("assets/snow-animation/reference.frames","rb");assert(ref);
    uint8_t expected[SN_CHANNEL_BYTES],pixels[SN_CHANNEL_BYTES];
    for(uint32_t f=0;f<SN_CHANNEL_POSES;f++){
        assert(fread(expected,1,sizeof expected,ref)==sizeof expected);
        uint32_t q=(f*1000u+16u)/32u;assert(snow_channel_decode(0,q,pixels));assert(!memcmp(expected,pixels,sizeof pixels));
        cv_begin(100,100,T_BG);snow_icon(50,50,4,q,2,T_THEME);
        for(uint32_t y=0;y<33;y++)for(uint32_t x=0;x<33;x++){
            uint32_t i=y*33u+x,v=(expected[i/4u]>>(6u-2u*(i%4u)))&3u;assert(v<3);
            uint16_t ink=swap16(v==1?T_TEXT:v==2?T_THEME:T_BG);
            for(uint32_t dy=0;dy<2;dy++)for(uint32_t dx=0;dx<2;dx++)assert(cv_px[(18+y*2+dy)*100+18+x*2+dx]==ink);
        }
    }
    assert(fgetc(ref)==EOF);fclose(ref);
    ref=fopen("tests/fixtures/snow-protected-icons.txt","r");assert(ref);
    uint32_t kind,scale,q,expected_hash,count=0;
    while(fscanf(ref,"%u %u %u %x",&kind,&scale,&q,&expected_hash)==4){
        cv_begin(100,100,T_BG);snow_icon(50,50,kind,q,scale,T_THEME);uint32_t hash=2166136261u;
        for(uint32_t i=0;i<10000;i++)hash=(hash^cv_px[i])*16777619u;
        assert(hash==expected_hash);count++;
    }fclose(ref);assert(count==594);
    go_home();snow_home_panel=SN_HOME_SYNTH;sel_set(0);ui.msg_t=ui.undo_w=0;trk[0].p[P_MUTE]=0;trk[0].p[P_E0]=0;snow_touch(&trk[0],P_E0,0);advance(150);
    uint16_t before[80*80];for(uint32_t y=0;y<80;y++)memcpy(before+y*80,web_fb+(107+y)*240+5,160);
    uint32_t blits=snow_home_state.card_blits;advance(1000);assert(snow_home_state.card_blits==blits);
    web_enc(EN_K1,1);advance(100);
    uint32_t diff=0;for(uint32_t y=0;y<80;y++)for(uint32_t x=0;x<80;x++)diff+=before[y*80+x]!=web_fb[(107+y)*240+5+x];assert(diff>=16);
    for(uint32_t c=0;c<NPALETTES;c++){
        web_snow_color(c);advance(100);uint32_t accent=0;
        for(uint32_t y=115;y<181;y++)for(uint32_t x=13;x<79;x++){
            uint16_t p=web_fb[y*240+x];assert(p==swap16(T_TEXT)||p==swap16(0));accent+=p==swap16(T_TEXT);
        }assert(accent>600);
    }
    puts("STATTIC static: white reference artwork, physical knob movement, fixed zone, ten themes and no idle animation passed");
    puts("Protected icons: BS, CH cassette and all four right-panel generators match 594 released snapshots at every scale");
}
static void random_sound_tests(void)
{
    seq_stop();memset(snow_offset,0,sizeof snow_offset);
    uint32_t initial_mask[(P_COUNT+31u)/32u];sel_set(0);snow_edit_mask(initial_mask);uint32_t count=0;
    for(uint32_t id=0;id<P_COUNT;id++)count+=(initial_mask[id/32u]>>(id%32u))&1u;
    printf("Random EDIT covers %u parameters\n",count);assert(count>=70);
    assert(!((initial_mask[P_LEVEL/32u]>>(P_LEVEL%32u))&1u));assert((initial_mask[SN_BMIX/32u]>>(SN_BMIX%32u))&1u);
    assert(snow_random_sound(NTRK)==1);
    for(uint32_t k=0;k<NTRK;k++){
        sel_set(k);assert(trk[k].p[SN_G_SCOPE]==0);const param_desc_t *target=track_desc(&trk[k],SN_G_SCOPE);
        assert(!strcmp(target->names[0],SN_TARGET_NAMES[k][0])&&!strcmp(target->names[1],"ALL"));
        int16_t params[NTRK][P_COUNT];step_t steps[NTRK][NSTEP];uint32_t eng[NTRK],preset[NTRK];
        for(uint32_t j=0;j<NTRK;j++){memcpy(params[j],trk[j].p,sizeof params[j]);memcpy(steps[j],trk[j].step,sizeof steps[j]);eng[j]=trk[j].eng_req;preset[j]=trk[j].preset;}
        motion_store_t locks=motion;int16_t low[P_COUNT],high[P_COUNT];for(uint32_t id=0;id<P_COUNT;id++){low[id]=32767;high[id]=-32768;}
        for(uint32_t run=0;run<40;run++){
            int16_t prev[P_COUNT];memcpy(prev,trk[k].p,sizeof prev);assert(!snow_random_sound(k));assert(memcmp(prev,trk[k].p,sizeof prev));
            for(uint32_t id=0;id<P_COUNT;id++){
                int included=(initial_mask[id/32u]>>(id%32u))&1u;
                if(!included)assert(trk[k].p[id]==params[k][id]);
                else {const param_desc_t*d=track_desc(&trk[k],id);if(trk[k].p[id]<d->min||trk[k].p[id]>d->max)printf("INVALID role=%u id=%u %s value=%d min=%d max=%d\n",k,id,d->label,trk[k].p[id],d->min,d->max);assert(trk[k].p[id]>=d->min&&trk[k].p[id]<=d->max);if(trk[k].p[id]<low[id])low[id]=trk[k].p[id];if(trk[k].p[id]>high[id])high[id]=trk[k].p[id];}
            }
            assert(trk[k].p[P_VOICE]==(k==5?V_POLY:V_MONO));
            assert(trk[k].p[SN_KEYTRACK]==(k>=3?127:0));assert(trk[k].p[SN_MODE]==(k>=4));
            if(k==0){assert(trk[k].p[SN_TUNE]>=-34&&trk[k].p[SN_TUNE]<=-22);assert(trk[k].p[SN_PDEC]<=21);}
            if(k==2)assert(trk[k].p[SN_NOISE]>=111);
            if(k>=3)assert(trk[k].p[SN_TUNE]%12==0);
            if(k>=4)assert(trk[k].p[P_SUS]>=58&&trk[k].p[SN_NOISE]<=4);
            uint32_t colours=(trk[k].p[P_DIST]>0)+(trk[k].p[SN_TMIX]>0)+(trk[k].p[SN_BMIX]>0)+(trk[k].p[P_IMIX]>0);assert(colours<=1);
            assert(trk[k].p[SN_DFB]<=48&&trk[k].p[SN_CFB]<=12&&trk[k].p[SN_BBITS]>=12);
            for(uint32_t j=0;j<NTRK;j++){assert(!memcmp(steps[j],trk[j].step,sizeof steps[j]));assert(trk[j].eng_req==eng[j]&&trk[j].preset==preset[j]);if(j!=k)assert(!memcmp(params[j],trk[j].p,sizeof params[j]));}
            assert(motion.count==locks.count);for(uint32_t e=0;e<locks.count;e++)assert(!memcmp(&motion.event[e],&locks.event[e],sizeof locks.event[e]));
        }
        for(uint32_t id=0;id<P_COUNT;id++)if((initial_mask[id/32u]>>(id%32u))&1u){
            int invariant=id==SN_MODE||id==SN_KEYTRACK||id==P_VOICE||id==P_ED_PIT||id==SN_FX_BYPASS||id==P_DTYPE||
                (k<4u&&id==P_SUS)||(k==5u&&id==P_REL)||(k==2u&&(id==SN_PAMT||id==P_E2))||((k==0u||k>=3u)&&id==SN_FILTER);
            assert(invariant||high[id]>low[id]);
        }
        int16_t previous[P_COUNT];memcpy(previous,trk[k].p,sizeof previous);snow_random_sound(k);undo_step(0);assert(!memcmp(previous,trk[k].p,sizeof previous));ui.undo_w=0;
    }
    sel_set(0);ui.home=0;ui.page=(uint8_t)page_first(FAM_EDIT);ui.force=1;advance(150);
    assert(graph_y()+graph_h()<=202u);
    uint16_t hint[240*8];memcpy(hint,web_fb+206*240,sizeof hint);advance(500);assert(!memcmp(hint,web_fb+206*240,sizeof hint));
    uint32_t ink=0;for(uint32_t i=0;i<240*8;i++)ink+=hint[i]==swap16(T_THEME);assert(ink>40);
    int16_t before[P_COUNT];memcpy(before,trk[0].p,sizeof before);int8_t octave=song.octave;tap(B_OCTUP);assert(memcmp(before,trk[0].p,sizeof before)&&song.octave==octave);
    /* Only the first EDIT page owns this shortcut. */
    ui.page++;ui.force=1;advance(80);memcpy(before,trk[0].p,sizeof before);tap(B_OCTUP);assert(!memcmp(before,trk[0].p,sizeof before));
    puts("Random EDIT: all exposed parameters, selected channel only, bounds, sequence/locks intact, undo, physical OCT+ passed");
}
static void section_random_tests(void)
{
    seq_stop();memset(&motion,0,sizeof motion);memset(snow_offset,0,sizeof snow_offset);
    static const uint32_t families[]={FAM_LFO,FAM_FX};
    for(uint32_t f=0;f<2;f++)for(uint32_t k=0;k<NTRK;k++){
        uint32_t family=families[f],mask[(P_COUNT+31u)/32u],count=0;sel_set(k);snow_family_mask(family,mask);
        int16_t low[P_COUNT],high[P_COUNT];for(uint32_t id=0;id<P_COUNT;id++){low[id]=32767;high[id]=-32768;count+=(mask[id/32u]>>(id%32u))&1u;}
        assert(count>=30);assert(!((mask[P_LEVEL/32u]>>(P_LEVEL%32u))&1u));assert(!((mask[SN_TUNE/32u]>>(SN_TUNE%32u))&1u));
        motion_set_lock(&trk[k],5,P_E0,33);motion_set_lock(&trk[k],7,P_LRATE,53);motion_store_t locks=motion;
        int16_t baseline[NTRK][P_COUNT];step_t steps[NTRK][NSTEP];
        for(uint32_t t=0;t<NTRK;t++){memcpy(baseline[t],trk[t].p,sizeof baseline[t]);memcpy(steps[t],trk[t].step,sizeof steps[t]);}
        for(uint32_t n=0;n<96;n++){
            int16_t before[P_COUNT];memcpy(before,trk[k].p,sizeof before);assert(!snow_random_section(k,family));assert(memcmp(before,trk[k].p,sizeof before));
            for(uint32_t id=0;id<P_COUNT;id++){
                if(!((mask[id/32u]>>(id%32u))&1u))assert(trk[k].p[id]==baseline[k][id]);
                else {const param_desc_t *d=track_desc(&trk[k],id);assert(trk[k].p[id]>=d->min&&trk[k].p[id]<=d->max);
                    if(trk[k].p[id]<low[id])low[id]=trk[k].p[id];if(trk[k].p[id]>high[id])high[id]=trk[k].p[id];}
            }
            for(uint32_t t=0;t<NTRK;t++){assert(!memcmp(steps[t],trk[t].step,sizeof steps[t]));if(t!=k)assert(!memcmp(baseline[t],trk[t].p,sizeof baseline[t]));}
            assert(!memcmp(&motion,&locks,sizeof locks));
            if(family==FAM_FX){assert(trk[k].p[P_SLDEPTH]<=42&&trk[k].p[P_DKAMT]<=48);assert(trk[k].p[SN_DFB]<=83&&trk[k].p[SN_CFB]<=29);}
            else {assert(trk[k].p[P_LD_PIT]>=-2&&trk[k].p[P_LD_PIT]<=2);assert(trk[k].p[SN_L2_DEST]!=9);}
            int16_t generated[P_COUNT];memcpy(generated,trk[k].p,sizeof generated);undo_step(0);assert(!memcmp(before,trk[k].p,sizeof before));
            undo_step(1);assert(!memcmp(generated,trk[k].p,sizeof generated));ui.undo_w=0;
        }
        for(uint32_t id=0;id<P_COUNT;id++)if((mask[id/32u]>>(id%32u))&1u)assert(id==SN_FX_BYPASS||(family==FAM_FX&&k>=4u&&id==P_SLCR)||high[id]>low[id]);
        printf("Random %s role %u: %u controls, bounds, isolated scope, notes/locks, undo/redo passed\n",family==FAM_LFO?"LFO":"FX",k+1,count);
        ui.home=ui.menu=ui.confirm=ui.layer=ui.ly=ui.shift=0;ui.page=(uint8_t)page_first(family);ui.force=1;advance(100);
        int16_t before[P_COUNT];memcpy(before,trk[k].p,sizeof before);int8_t oct=song.octave;tap(B_OCTUP);
        assert(memcmp(before,trk[k].p,sizeof before)&&song.octave==oct);assert(graph_y()+graph_h()<=202);
        uint16_t hint[240*8];memcpy(hint,web_fb+206*240,sizeof hint);advance(600);assert(!memcmp(hint,web_fb+206*240,sizeof hint));
        web_buttons(1u<<B_SAVE);advance(850);web_buttons(0);advance(80);assert(!memcmp(before,trk[k].p,sizeof before));ui.undo_w=0;
        ui.page++;ui.force=1;advance(80);memcpy(before,trk[k].p,sizeof before);tap(B_OCTUP);assert(!memcmp(before,trk[k].p,sizeof before));
        memset(&motion,0,sizeof motion);
    }
    assert(snow_random_section(NTRK,FAM_LFO)==1&&snow_random_section(0,FAM_SEQ)==1);
    go_home();advance(80);
    puts("Section commands: physical OCT+, actual held-SAVE undo, first page only, readable hints passed");
}
static int hash_order(const void *a,const void *b){uint64_t x=*(const uint64_t*)a,y=*(const uint64_t*)b;return x<y?-1:x>y;}
static void smart_diversity_tests(void)
{
    uint32_t mask[(P_COUNT+31u)/32u];uint64_t hashes[4096];int16_t p[P_COUNT];sel_set(0);snow_edit_mask(mask);
    for(uint32_t k=0;k<NTRK;k++){
        for(uint32_t n=0;n<4096;n++){
            snow_design_sound(&trk[k],k,(n+1u)*0x9e3779b9u,mask,p);
            uint64_t h=1469598103934665603ull;
            for(uint32_t id=0;id<P_COUNT;id++)if((mask[id/32u]>>(id%32u))&1u)h=(h^(uint16_t)p[id])*1099511628211ull;
            hashes[n]=h;
        }
        qsort(hashes,4096,sizeof hashes[0],hash_order);for(uint32_t n=1;n<4096;n++)assert(hashes[n]!=hashes[n-1]);
        printf("Smart sound role %u: 4096 generated patches without a repeated parameter set\n",k+1);
    }
}
static void interface_tests(void)
{
    go_home();advance(150);assert(snow_home_active);
    uint32_t cards=snow_home_state.card_blits,scopes=snow_home_state.scope_blits;snow_touch(&trk[0],P_E0,64);advance(1000);
    assert(snow_home_state.card_blits-cards<=14);assert(snow_home_state.scope_blits-scopes<=22);

    for(uint32_t c=0;c<UI_NPALETTES;c++){web_snow_color(c);advance(80);assert(web_snow_color_get()==c);}
    web_buttons(1u<<B_GLO);advance(700);assert(layer_open()==LAYER_GLO&&snow_channels_active);
    static const uint32_t knobs[]={EN_K1,EN_K2,EN_K3,EN_K4,EN_ALGO,EN_PRESET};
    for(uint32_t k=0;k<6;k++){trk[k].p[P_LEVEL]=70;web_enc(knobs[k],1);advance(60);assert(trk[k].p[P_LEVEL]==71);}
    for(uint32_t k=0;k<6;k++){uint32_t key=0;while(key<32&&(!key_black(key)||key_place(key)!=k))key++;assert(key<32);web_keys(1u<<key);advance(60);web_keys(0);advance(60);assert(trk[k].p[P_MUTE]);}
    for(uint32_t k=0;k<6;k++){uint32_t key=0;while(key<32&&(key_black(key)||key_place(key)!=k))key++;web_keys(1u<<key);advance(60);assert(perf_solo==(1u<<k));web_keys(0);advance(60);assert(!perf_solo);}
    int16_t offset=snow_offset[0][0];web_buttons((1u<<B_GLO)|(1u<<B_EDIT));advance(60);web_enc(EN_K1,1);advance(60);assert(snow_offset[0][0]==offset+1);web_buttons(1u<<B_GLO);advance(60);
    web_buttons((1u<<B_GLO)|(1u<<B_OCTDN));advance(60);web_buttons(1u<<B_GLO);advance(60);for(uint32_t k=0;k<6;k++)assert(!trk[k].p[P_MUTE]);
    web_buttons(0);advance(100);go_home();advance(100);
    ui.home=0;ui.page=(uint8_t)page_first(FAM_SEQ);while(!snow_gen_page(cur_page()))ui.page++;
    ui.force=1;advance(80);sel_set(0);trk[0].p[SN_G_SCOPE]=0;uint64_t mask=snow_hits(trk[0].step,trk[0].p[P_SLEN]);
    for(uint32_t i=0;i<12;i++){tap(B_OCTUP);uint64_t m=snow_hits(trk[0].step,trk[0].p[P_SLEN]);assert(m!=mask);mask=m;}
    step_t keep[NSTEP];memcpy(keep,trk[0].step,sizeof keep);trk[0].p[SN_G_LOCKS]=100;tap(B_OCTDN);assert(!memcmp(keep,trk[0].step,sizeof keep));
    web_buttons((1u<<B_SAVE)|(1u<<B_OCTDN));advance(60);web_buttons(0);advance(100);assert(!snow_hits(trk[0].step,NSTEP)&&cur_page()->fam==FAM_SEQ);
    puts("Pixel HOME / rate caps / ten colors / six GLO level encoders / six mutes / physical repeated OCT+ passed");
}
#ifndef SNOW_AUDITION
int main(void)
{
    setvbuf(stdout,0,_IOLBF,0);web_nor_erase();web_boot();advance(SNOW_BOOT_MS+200u);
    synth_tests();generation_tests();ornament_tests();patterns_and_snapshot();fx_tests();pattern_controls();interface_tests();icon_position_tests();random_sound_tests();smart_diversity_tests();section_random_tests();
    puts("ALL SNOW TESTS PASSED");return 0;
}

#endif
