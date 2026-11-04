/* SPDX-License-Identifier: GPL-3.0-only
 * Six channels always share one screen. Main-loop rendering only, <=20 Hz. */
static uint8_t snow_channels_active;
static uint32_t snow_channels_ms,snow_channels_sig;
static void snow_channels_draw(int patterns)
{
    static const char *const roles[]={"KK","SD","HH","PR","BS","CH"};
    static const char *const knobs[]={"K1","K2","K3","K4","ALG","PRE"};
    static const char *const blacks[]={"F#3","G#3","A#3","C#4","D#4","F#4"};
    static const char *const whites[]={"F3","G3","A3","B3","C4","D4"};
    cv_oy=0;
    uint32_t sig=ux.gen*997u+bank_gen()*31u+snow_pattern_half*131u+song.sel*4099u+patterns+perf_solo*65537u;
    for(uint32_t k=0;k<NTRK;k++)sig=(sig^(uint32_t)trk[k].p[P_LEVEL]^(uint32_t)trk[k].p[P_MUTE]<<16^snow_pattern_current(k)*8179u)*16777619u;
    if(snow_channels_active&&!ui.force&&sig==snow_channels_sig)return;
    if(snow_channels_active&&!ui.force&&fm1_ms-snow_channels_ms<50u)return;
    snow_channels_active=1;snow_channels_ms=fm1_ms;snow_channels_sig=sig;
    cv_begin(240,18,T_BG);
    cv_rect(0,0,240,18,T_THEME);snow_text(5,6,patterns?"[SEQ] PATTERNS":ui.layer?"[GLO] MIXER":"MIXER",T_INK,28);cv_blit(0,0);
    for(uint32_t k=0;k<6u;k++) {
        cv_begin(40,204,T_BG);int32_t x=0;track_t *t=&trk[k];
        cv_rect(x+39,0,1,179,T_LINE);cv_rect(x+2,4,35,14,k==song.sel?T_THEME:T_SURF);
        snow_text(x+14,8,roles[k],k==song.sel?T_INK:T_TEXT,2);
        if(patterns) {
            for(uint32_t j=0;j<8u;j++) {
                uint32_t slot=snow_pattern_half*8u+j,cur=lists.cur[k],pending=bank_next(k);
                int32_t y=24+(int32_t)j*18;uint16_t fill=cur==slot?T_THEME:T_SURF;
                cv_rect(x+3,y,33,15,fill);
                if(pending==slot)cv_frame(x+2,y-1,35,17,T_ACCENT);
                char p[2]={(char)('A'+slot),0};snow_text(x+16,y+4,p,cur==slot?T_INK:T_TEXT,1);
                if(snow_pattern_used(k,slot))cv_rect(x+29,y+10,3,3,cur==slot?T_INK:T_MID);
            }
        } else {
            snow_text(x+11,24,"LVL",T_MID,3);
            snow_icon(x+19,47,k,0,1,t->p[P_MUTE]?T_DIM:T_THEME);
            cv_frame(x+14,69,11,47,T_LINE);
            int32_t level=(t->p[P_LEVEL]-TP[P_LEVEL].min)*43/(TP[P_LEVEL].max-TP[P_LEVEL].min);
            cv_rect(x+16,114-level,7,level,t->p[P_MUTE]?T_DIM:T_THEME);
            char b[24];const char *unit;param_format(track_desc(t,P_LEVEL),t->p[P_LEVEL],b,&unit);snow_text(x+3,121,b,T_TEXT,6);
            uint32_t muted=t->p[P_MUTE]!=0,solo=(perf_solo>>k)&1u;
            cv_rect(x+2,138,35,23,muted?T_THEME:T_SURF);snow_text(x+6,141,"MUTE",muted?T_INK:T_TEXT,4);snow_text(x+9,152,blacks[k],muted?T_INK:T_MID,3);
            cv_rect(x+2,165,35,23,solo?T_THEME:T_SURF);snow_text(x+6,168,"SOLO",solo?T_INK:T_TEXT,4);snow_text(x+12,179,whites[k],solo?T_INK:T_MID,2);
        }
        snow_text(x+11,patterns?174:196,knobs[k],T_THEME,3);cv_blit((int32_t)k*40,18);
    }
    cv_begin(240,18,T_BG);
    if(patterns){snow_text(5,1,"BLACK:CHANNEL  WHITE:PATTERN",T_MID,38);snow_text(5,10,"OCT +/- : A-H / I-P",T_THEME,38);}
    else {snow_text(4,6,ui.layer?"E4 ALL  F4 FILL  G4 TAP   O- UNDO":"HOLD GLO: MUTE / SOLO / TAP",T_MID,39);}
    cv_blit(0,222);ui.force=0;if(ui.msg_t)ui.msg_t--;
}
