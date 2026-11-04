/* SPDX-License-Identifier: GPL-3.0-only
 * 3x2 pixel HOME, based on the user's second diagram. No framebuffer and no
 * audio-thread animation. Dirty tiles: <=2/frame, icons <=12 Hz, scope <=20 Hz. */
static uint8_t snow_home_active;
static struct { uint32_t card[6],head,scope_ms,card_blits,scope_blits; } snow_home_state;
static void snow_home_draw(void)
{
    cv_oy=0;
    static const char *const abbr[]={"KK","SD","HH","PR","BS","CH"};
    int fresh=!snow_home_active;snow_home_active=1;
    if(fresh){for(uint32_t k=0;k<6;k++)snow_home_state.card[k]=~0u;snow_home_state.head=~0u;snow_home_state.scope_ms=0;}
    uint32_t beat=(uint32_t)((uint64_t)((take_pass[song.sel]?take_pass[song.sel]-1u:0u)*(uint32_t)TSEL->p[P_SLEN]+TSEL->seq_idx)*div_samples((uint32_t)TSEL->p[P_SDIV])/beat_samples());
    uint32_t head=beat*313u+(uint32_t)song.g[G_BPM]+(uint32_t)song.playing*1024u+(uint32_t)batt_shown()*8192u+ux.gen*65537u;
    if(head!=snow_home_state.head){
        snow_home_state.head=head;cv_begin(240,24,T_BG);
        if(song.playing){cv_rect(5,7,5,9,T_THEME);cv_rect(10,9,3,5,T_THEME);}else cv_rect(5,8,7,7,T_MID);
        char pos[12];fmt_int(pos,(int32_t)(beat/4u)+1);uint32_t pn=str_len(pos);pos[pn]='.';fmt_int(pos+pn+1,(int32_t)(beat%4u)+1);snow_text(19,8,pos,T_TEXT,4);char bpm[8];fmt_int(bpm,song.g[G_BPM]);snow_text(48,8,bpm,T_TEXT,3);snow_text(71,8,"BPM",T_MID,3);
        snow_text(119,8,"SNOW",T_THEME,4);cv_frame(211,8,21,9,T_MID);cv_rect(232,11,2,3,T_MID);
        int32_t batt=batt_shown();cv_rect(213,10,batt==4?17:clamp(batt,0,3)*5,5,T_TEXT);cv_blit(0,0);
    }
    uint32_t drawn=0;
    for(uint32_t order=0;order<6u;order++) {
        uint32_t k=(order+song.sel)%6u;track_t *t=&trk[k];
        uint32_t age=fm1_ms-snow_touch_state[k].ms,animate=snow_touch_state[k].valid&&age<800u&&!(ui_prefs&PREF_ANIM_OFF);
        uint32_t phase=animate?(age/84u+1u):0u;
        uint32_t pattern=snow_pattern_current(k),sig=ux.gen*977u+t->preset*31u+t->user*7919u+pattern*131u+(k==song.sel?0x40000000u:0u);
        sig^=snow_touch_state[k].serial*65537u+phase*104729u+(uint32_t)t->p[P_MUTE]*51u;
        if(snow_home_state.card[k]==sig||drawn>=2u)continue;
        snow_home_state.card[k]=sig;drawn++;snow_home_state.card_blits++;
        cv_begin(80,92,T_BG);cv_rect(79,1,1,90,T_LINE);cv_rect(2,91,76,1,T_LINE);
        cv_rect(5,3,23,12,k==song.sel?T_THEME:T_SURF);snow_text(10,6,abbr[k],k==song.sel?T_INK:T_TEXT,2);
        char pat[5]="P";fmt_int(pat+1,(int32_t)pattern+1);cv_rect(53,3,23,12,T_SURF);snow_text(55,6,pat,T_THEME,3);
        char name[20];sound_name(t,name);snow_text(4,21,name,T_TEXT,12);
        snow_icon(39,49,k,phase,2,t->p[P_MUTE]?T_DIM:animate?T_THEME:T_TEXT);
        if(snow_touch_state[k].valid) {
            uint32_t id=snow_touch_state[k].id;
            if(id<P_COUNT){const param_desc_t *d=track_desc(t,id);int32_t value=snow_touch_state[k].value;
                int32_t pct=d->max>d->min?clamp((value-d->min)*100/(d->max-d->min),0,100):0;
                snow_text(3,75,d->label,T_TEXT,8);char b[8];fmt_int(b,pct);str_cpy(b+str_len(b),"%",sizeof b-str_len(b));
                snow_text(54,75,b,T_THEME,4);cv_rect(3,85,pct*73/100,2,T_THEME);
            }
        }
        cv_blit((int32_t)(k%3u)*80,24+(int32_t)(k/3u)*92);
    }
    if(fresh||fm1_ms-snow_home_state.scope_ms>=50u){
        snow_home_state.scope_ms=fm1_ms;snow_home_state.scope_blits++;
        cv_begin(240,32,T_BG);uint32_t w=scope_w;int32_t peak=64;
        for(uint32_t x=0;x<240;x++){int32_t a=scope_buf[(w+SCOPE_N-240+x)&(SCOPE_N-1u)];if(a<0)a=-a;if(a>peak)peak=a;}
        int32_t last=15;
        for(uint32_t x=0;x<240;x++){
            int32_t v=scope_buf[(w+SCOPE_N-240+x)&(SCOPE_N-1u)],y=15-v*12/peak;
            if(x)cv_line((int32_t)x-1,last,(int32_t)x,y,T_THEME);last=y;
        }
        uint32_t note=60,have=0;
        for(uint32_t v=0;v<NVOICE;v++)if(TSEL->v[v].active){note=TSEL->v[v].note;have=1;break;}
        char n[8]="--";if(have){static const char *const notes[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};str_cpy(n,notes[note%12u],sizeof n);fmt_int(n+str_len(n),(int32_t)(note/12u)-1);}
        cv_rect(2,23,34,8,T_BG);snow_text(3,24,n,T_MID,5);cv_blit(0,208);
    }
    if(ui.hot_t)ui.hot_t--;if(ui.bpm_t)ui.bpm_t--;if(ui.msg_t)ui.msg_t--;ui.force=0;
}
