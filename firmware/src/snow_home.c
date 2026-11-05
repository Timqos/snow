/* SPDX-License-Identifier: GPL-3.0-only
 * Selected-channel HOME, user PDF layout. Fixed pixel cells; no panel frames.
 * Dirty regions only, body capped at 20 Hz, scope 20 Hz. No audio-thread drawing. */
static uint8_t snow_home_active;
static struct { uint32_t body,head,scope_ms,body_ms,card_blits,scope_blits; } snow_home_state;
static void snow_text_big(int32_t x,int32_t y,const char *s,uint16_t ink,uint32_t max)
{
    for(uint32_t n=0;*s&&n<max;n++,x+=12){uint32_t c=(uint8_t)*s++;if(c>='a'&&c<='z')c-=32;if(c<32||c>95)c='?';
        for(uint32_t row=0;row<7;row++)for(uint32_t b=0;b<5;b++)if((SN_FONT[c-32][row]>>(4-b))&1u)cv_rect(x+(int32_t)b*2,y+(int32_t)row*2,2,2,ink);}
}
static uint32_t snow_home_percent(const track_t *t,uint32_t id)
{
    int32_t v=motion_base_value(t,id);
    if(id==P_PAN||id==SN_DJ_FILTER||id==SN_WIDTH)return (uint32_t)(v<0?50+v*50/64:50+v*50/63);
    return snow_param_position(t,id)/10u;
}
static void snow_pixel_bar(int32_t x,int32_t y,int32_t w,int32_t h,uint32_t pct,int centered,uint16_t ink)
{
    cv_rect(x+2,y,w-4,1,T_MID);cv_rect(x+2,y+h-1,w-4,1,T_MID);cv_rect(x,y+2,1,h-4,T_MID);cv_rect(x+w-1,y+2,1,h-4,T_MID);
    cv_rect(x+1,y+1,1,1,T_MID);cv_rect(x+w-2,y+1,1,1,T_MID);cv_rect(x+1,y+h-2,1,1,T_MID);cv_rect(x+w-2,y+h-2,1,1,T_MID);
    int32_t end=(int32_t)pct*(w-6)/100,start=centered?(w-6)/2:0;if(end<start){int32_t a=start;start=end;end=a;}
    for(int32_t yy=2;yy<h-2;yy++)for(int32_t xx=start;xx<end;xx++)if((xx+yy)&1)cv_rect(x+3+xx,y+yy,1,1,ink);
    if(centered)cv_rect(x+w/2,y+2,1,h-4,T_MID);
}
static void snow_home_arrow(int32_t x,int32_t y)
{
    uint16_t ink=T_THEME;
    if(snow_home_panel==SN_HOME_MIX){cv_rect(x+5,y+4,3,9,ink);for(int32_t a=0;a<5;a++)cv_rect(x+6-a,y+a,1+a*2,1,ink);}
    else {int32_t d=snow_home_panel==SN_HOME_GEN?1:-1;cv_rect(x+2,y+5,10,3,ink);
        for(int32_t a=0;a<6;a++)cv_rect(x+(d>0?7+a:6-a),y+1+a,1,11-a*2,ink);}
}
static void snow_home_draw(void)
{
    static const char *const abbr[]={"KK","SD","HH","PR","BS","CH"};
    static const char *const mix_names[]={"VOL","PAN","FLR","WDT"};
    static const uint8_t mix_ids[]={P_LEVEL,P_PAN,SN_DJ_FILTER,SN_WIDTH};
    static const char *const gen_names[]={"GN/PRM","GN/SEQ","GN/LFO","GN/FX"};
    cv_oy=0;track_t *t=TSEL;uint32_t k=song.sel;
    int fresh=!snow_home_active;snow_home_active=1;
    if(fresh){snow_home_state.body=~0u;snow_home_state.head=~0u;snow_home_state.scope_ms=0;snow_home_state.body_ms=0;}
    uint32_t beat=(uint32_t)((uint64_t)((take_pass[k]?take_pass[k]-1u:0u)*(uint32_t)t->p[P_SLEN]+t->seq_idx)*div_samples((uint32_t)t->p[P_SDIV])/beat_samples());
    uint32_t head=beat*313u+(uint32_t)song.g[G_BPM]+(uint32_t)song.playing*1024u+(uint32_t)batt_shown()*8192u+ux.gen*65537u;
    if(head!=snow_home_state.head){
        snow_home_state.head=head;cv_begin(240,18,T_BG);
        if(song.playing){cv_rect(5,5,4,8,T_THEME);cv_rect(9,7,3,4,T_THEME);}else cv_rect(5,6,7,7,T_MID);
        char pos[12];fmt_int(pos,(int32_t)(beat/4u)+1);uint32_t pn=str_len(pos);pos[pn]='.';fmt_int(pos+pn+1,(int32_t)(beat%4u)+1);snow_text(18,6,pos,T_TEXT,4);
        char bpm[8];fmt_int(bpm,song.g[G_BPM]);snow_text(47,6,bpm,T_TEXT,3);snow_text(70,6,"BPM",T_MID,3);snow_text(122,6,"SNOW",T_THEME,4);
        cv_frame(212,6,20,9,T_MID);cv_rect(232,9,2,3,T_MID);int32_t batt=batt_shown();cv_rect(214,8,batt==4?16:clamp(batt,0,3)*5,5,T_TEXT);cv_blit(0,0);
    }
    uint32_t macro=snow_touch_state[k].macro,id=P_E0+macro,position=snow_param_position(t,id);
    uint32_t pattern=snow_pattern_current(k),pending=bank_next(k)<NENT,sig=ux.gen*977u+k*31u+t->preset*7919u+t->user*131u+pattern*8179u+snow_home_panel*65537u;
    sig^=pending*11717u+(ui.undo_w?9199u:0u);
    sig^=snow_touch_state[k].serial*104729u+position*349u+(uint32_t)t->p[SN_PLAY_MODE]*17u+(uint32_t)t->p[P_MUTE]*51u;
    for(uint32_t j=0;j<4;j++)sig=(sig^snow_home_percent(t,mix_ids[j])^snow_home_gen[k][j]*977u)*16777619u;
    if(sig!=snow_home_state.body&&(fresh||fm1_ms-snow_home_state.body_ms>=50u)){
        snow_home_state.body=sig;snow_home_state.body_ms=fm1_ms;snow_home_state.card_blits++;
        /* Two strips reuse the existing 240x138 canvas. No new framebuffer. */
        for(uint32_t tile=0;tile<2;tile++){
        cv_begin(240,95,T_BG);cv_oy=-(int32_t)(tile*95u);
        cv_rect(4,4,23,25,T_SURF);snow_text(10,13,abbr[k],T_THEME,2);
        for(uint32_t j=0;j<4;j++){
            int32_t x=31+(int32_t)j*43;char value[12];str_cpy(value,mix_names[j],sizeof value);fmt_int(value+3,(int32_t)snow_home_percent(t,mix_ids[j]));str_cpy(value+str_len(value),"%",sizeof value-str_len(value));
            snow_text(x,5,value,snow_home_panel==SN_HOME_MIX?T_THEME:T_TEXT,7);
            snow_pixel_bar(x,17,40,12,snow_home_percent(t,mix_ids[j]),j==1u||j==2u,T_THEME);
        }
        char pat[5]="P";fmt_int(pat+1,(int32_t)pattern+1);cv_rect(207,4,27,25,T_SURF);snow_text(211,13,pat,T_THEME,3);
        snow_pixel_bar(5,43,47,10,position/10u,0,T_THEME);char pct[8];fmt_int(pct,(int32_t)(position/10u));str_cpy(pct+str_len(pct),"%",sizeof pct-str_len(pct));snow_text(58,45,pct,T_THEME,4);
        snow_text_big(5,61,track_desc(t,id)->label,T_TEXT,7);
        char name[20];sound_name(t,name);snow_text(5,80,name,T_MID,14);
        snow_icon(45,129,k,position,2,t->p[P_MUTE]?T_MID:T_THEME);
        for(uint32_t j=0;j<3;j++)snow_home_arrow(93,48+(int32_t)j*53);
        for(uint32_t j=0;j<4;j++){
            int32_t x=142+(int32_t)(j%2u)*62,y=61+(int32_t)(j/2u)*65;
            snow_icon(x,y,6u+j,(snow_home_gen[k][j]%17u)*1000u/16u,0,T_THEME);
            snow_text(x-18,y+24,gen_names[j],T_TEXT,6);
        }
        if(k==5u){
            snow_pixel_bar(6,165,35,10,100u,0,T_MID);cv_rect(t->p[SN_PLAY_MODE]?9:26,167,12,6,T_THEME);
            snow_text(48,167,"N",t->p[SN_PLAY_MODE]?T_THEME:T_MID,1);snow_text(54,167,"/",T_MID,1);snow_text(60,167,"CHO",t->p[SN_PLAY_MODE]?T_MID:T_THEME,3);
        }
        snow_text(114,160,"SELECT:PANEL",T_MID,20);
        snow_text(114,169,k==5u?"HOME+OCT+:N/CHO":"OCT +/-:OCTAVE",T_MID,20);
        const char *hint=snow_home_panel==SN_HOME_GEN?"K1:SND K2:SEQ K3:LFO K4:FX":snow_home_panel==SN_HOME_MIX?"K1:VOL K2:PAN K3:FLR K4:WDT":"K1:TONE K2:SHAPE K3:SWEEP K4:DECAY";
        if(pending)hint="PATTERN QUEUED - WAIT";
        if(ui.undo_w)hint="UNDO: OCT- BACK / OCT+ REDO";
        snow_text(4,179,hint,T_MID,39);cv_blit(0,18+tile*95u);
        }
        cv_oy=0;
    }
    if(fresh||fm1_ms-snow_home_state.scope_ms>=50u){
        snow_home_state.scope_ms=fm1_ms;snow_home_state.scope_blits++;cv_begin(240,32,T_BG);
        uint32_t w=scope_w;int32_t peak=64;
        for(uint32_t x=0;x<240;x++){int32_t a=scope_buf[(w+SCOPE_N-240+x)&(SCOPE_N-1u)];if(a<0)a=-a;if(a>peak)peak=a;}
        int32_t last=15;for(uint32_t x=0;x<240;x++){int32_t v=scope_buf[(w+SCOPE_N-240+x)&(SCOPE_N-1u)],y=15-v*12/peak;if(x)cv_line((int32_t)x-1,last,(int32_t)x,y,T_THEME);last=y;}
        uint32_t note=60,have=0;for(uint32_t v=0;v<NVOICE;v++)if(t->v[v].active){note=t->v[v].note;have=1;break;}
        char n[8]="--";if(have){static const char *const notes[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};str_cpy(n,notes[note%12u],sizeof n);fmt_int(n+str_len(n),(int32_t)(note/12u)-1);}
        cv_rect(2,23,34,8,T_BG);snow_text(3,24,n,T_MID,5);cv_blit(0,208);
    }
    if(ui.hot_t)ui.hot_t--;if(ui.bpm_t)ui.bpm_t--;if(ui.msg_t)ui.msg_t--;ui.force=0;
}
