/* SPDX-License-Identifier: GPL-3.0-only
 * STATTIC HOME: user reference pages on the native 240x240 pixel grid.
 * Dirty strips, <=20Hz, no audio-thread drawing or additional framebuffer. */
#include "stattic_art.h"
static uint8_t snow_home_active;
static uint8_t stattic_overview;
static struct { uint32_t body,head,scope_ms,body_ms,card_blits,scope_blits; } snow_home_state;
static const char *const ST_ROLES[]={"DR","BS","SY","CH"};
static const char *const ST_MIX_NAMES[]={"VOL","PAN","FLR","WDT"};
static const uint8_t ST_MIX_IDS[]={P_LEVEL,P_PAN,SN_DJ_FILTER,SN_WIDTH};
static const char *const ST_GEN_NAMES[]={"G/SEQ","G/PRM","G/LFO","G/FX"};
/* Fixed 5x7 dot grid. Integer native dots, never resized JPEG text. */
static int32_t stattic_text(int32_t x,int32_t y,const char *s,uint16_t ink,int32_t w,int32_t h,uint32_t max)
{
    for(uint32_t n=0;*s&&n<max;n++,x+=w+2){
        uint32_t c=(uint8_t)*s++;if(c>='a'&&c<='z')c-=32;if(c<32||c>95)c='?';
        for(int32_t r=0;r<7;r++)for(int32_t b=0;b<5;b++)if((SN_FONT[c-32][r]>>(4-b))&1u){
            int32_t xx=b*w/5,yy=r*h/7,dw=(b+1)*w/5-xx,dh=(r+1)*h/7-yy;
            if(w>=12&&dw>2)dw--;if(h>=17&&dh>2)dh--;
            cv_rect(x+xx,y+yy,dw,dh,ink);
        }
    }return x;
}
static void snow_text_big(int32_t x,int32_t y,const char *s,uint16_t ink,uint32_t max)
{(void)stattic_text(x,y,s,ink,10,14,max);}
static void stattic_hint(int32_t x,int32_t y,const char *s,uint16_t ink)
{
    for(;*s&&x<236;s++,x+=6){
        if(*s=='|')cv_rect(x+2,y+3,1,1,ink);
        else {char c[2]={*s,0};snow_text(x,y,c,ink,1);}
    }
}
static uint32_t snow_home_percent(const track_t *t,uint32_t id)
{
    int32_t v=stattic_base_value(t,id);
    if(id==P_PAN||id==SN_DJ_FILTER||id==SN_WIDTH)return (uint32_t)(v<0?50-((-v*50+32)/64):50+(v*50+31)/63);
    return (stattic_param_position(t,id)+5u)/10u;
}
static void snow_pixel_bar(int32_t x,int32_t y,int32_t w,int32_t h,uint32_t pct,int centered,uint16_t ink)
{
    int32_t end=(int32_t)clamp((int32_t)pct,0,100)*w/100,start=centered?w/2:0;
    if(end<start){int32_t a=start;start=end;end=a;}
    for(int32_t xx=start;xx<end;xx++){
        uint32_t sx=(uint32_t)(xx-start)*92u/(uint32_t)(end-start);
        for(int32_t yy=0;yy<h;yy++){
            int32_t py=y+yy+cv_oy;if((uint32_t)py>=cv_h)continue;
            uint32_t sy=(uint32_t)yy*17u/(uint32_t)h;
            if((ST_BAR[sy][sx/8u]>>(7u-sx%8u))&1u)cv_px[(uint32_t)py*cv_w+(uint32_t)(x+xx)]=swap16(ink);
        }
    }
}
static void stattic_graph(uint32_t page)
{
    int32_t top=page==0u?92:99;
    uint32_t art=stattic_art_pick[page],height=stattic_art_height[page],period=stattic_art_height[art];
    for(uint32_t yy=0;yy<height;yy++){
        int32_t py=top+(int32_t)yy;if((uint32_t)(py+cv_oy)>=cv_h)continue;
        const uint8_t *row=ST_STATIC_ROWS[ST_STATIC_INDEX[art][(yy+stattic_scroll[page])%period]];
        uint16_t *dst=cv_px+(uint32_t)(py+cv_oy)*cv_w+16u;
        uint16_t on=swap16(T_TEXT),off=swap16(0);
        for(uint32_t b=0;b<26u;b++){uint8_t bits=row[b];for(uint32_t j=0;j<8u;j++)*dst++=(bits>>(7u-j))&1u?on:off;}
    }
}
static void stattic_small_icon(uint32_t kind,int32_t x,int32_t y)
{
    static const uint8_t widths[]={34,49,61,49},heights[]={27,25,21,30};
    static const uint8_t *const whites[]={&ST_BADGE_0_WHITE[0][0],&ST_BADGE_1_WHITE[0][0],&ST_BADGE_2_WHITE[0][0],&ST_BADGE_3_WHITE[0][0]};
    static const uint8_t *const themes[]={&ST_BADGE_0_THEME[0][0],&ST_BADGE_1_THEME[0][0],&ST_BADGE_2_THEME[0][0],&ST_BADGE_3_THEME[0][0]};
    uint32_t w=widths[kind],h=heights[kind],stride=(w+7u)/8u;
    for(uint32_t yy=0;yy<h;yy++){
        int32_t py=y+(int32_t)yy+cv_oy;if((uint32_t)py>=cv_h)continue;
        for(uint32_t xx=0;xx<w;xx++){
            uint32_t b=yy*stride+xx/8u,bit=7u-xx%8u;
            uint16_t ink=(themes[kind][b]>>bit)&1u?T_THEME:(whites[kind][b]>>bit)&1u?T_TEXT:T_BG;
            cv_px[(uint32_t)py*cv_w+(uint32_t)x+xx]=swap16(ink);
        }
    }
}
/* Preserve the helper glyph size. Two clear rows between lines instead
 * of one; four complete lines fit through screen row 239. */
#define ST_HINT_W 8
#define ST_HINT_H 14
#define ST_HINT_PITCH 16
static void stattic_footer(uint32_t page)
{
    int32_t y=178;const char *a="OCT +/-:OCTAVE",*b="";
    if(page==0u&&song.sel==2u){a="OCT+ HLD N/CHO";b="OCT +/-:OCTAVE";}
    if(page==1u){a="OCT- CLEAR PT";b="OCT+ CLEAR ALL";}
    if(page==3u){a="OCT +/-";b="RANDOM PATT";}
    if(page==4u){a="M/S: KEY";b="W:MUTE B:SOLO";}
    char label[9]="PAGE 1/5";label[5]=(char)('1'+page);
    stattic_text(6,y,a,T_TEXT,ST_HINT_W,ST_HINT_H,14);
    stattic_text(6,y+ST_HINT_PITCH,b,T_TEXT,ST_HINT_W,ST_HINT_H,14);
    stattic_text(156,y,label,T_THEME,ST_HINT_W,ST_HINT_H,8);
    stattic_text(166,y+ST_HINT_PITCH,"SELECT",T_MID,ST_HINT_W,ST_HINT_H,6);
    if(page==3u){
        static const char *const keys[]={"K1:DR","K2:BS","K3:SY","K4:CH"};
        for(uint32_t n=0;n<4;n++)stattic_text(n&1u?134:6,y+(2u+n/2u)*ST_HINT_PITCH,keys[n],T_TEXT,ST_HINT_W,ST_HINT_H,5);
    }else if(page==4u){
        stattic_text(6,y+ST_HINT_PITCH*2,"K1:SLT CH",T_TEXT,ST_HINT_W,ST_HINT_H,9);
        stattic_text(134,y+ST_HINT_PITCH*2,"K2:VOL",T_TEXT,ST_HINT_W,ST_HINT_H,6);
    }else{
        static const char *const keys[3][4]={{"K1:TONE","K2:SHAPE","K3:SWEEP","K4:DECAY"},{"K1:G/SEQ","K2:G/PRM","K3:G/LFO","K4:G/FX"},{"K1:VOL","K2:PAN","K3:FLR","K4:WDT"}};
        for(uint32_t n=0;n<4;n++){
            static const char *const drk[4]={"K1:G/KICK","K2:G/SNR","K3:G/HAT","K4:G/PRC"};
            const char *text=page==1u&&song.sel==TRK_DRUM?drk[n]:keys[page][n];
            stattic_text(n&1u?134:6,y+(2u+n/2u)*ST_HINT_PITCH,text,T_TEXT,ST_HINT_W,ST_HINT_H,9);
        }
    }
}
static void stattic_page_body(uint32_t page)
{
    uint32_t k=song.sel;track_t *t=TSEL;
    if(page<3u){
        cv_rect(15,33,48,27,T_THEME);stattic_text(21,36,ST_ROLES[stattic_home_role(k)],T_INK,16,21,2);
        char name[20];stattic_sound_name(t,name);
        if(k==2u){
            stattic_text(73,37,t->p[SN_PLAY_MODE]?"N":"A",T_THEME,11,20,1);
            for(uint32_t i=0;name[i]&&i<13u;i++){char c[2]={name[i],0};stattic_text(90+(int32_t)i*7,40,c,T_TEXT,6,15,1);}
        }else stattic_text(73,40,name,T_TEXT,6,15,13);
        char pat[4]="P";fmt_int(pat+1,(int32_t)stattic_pattern_current(k)+1);
        cv_frame(184,31,44,31,T_THEME);stattic_text(191,36,pat,T_THEME,13,21,2);
        if(page==0u){
            uint32_t id=(is_drum(t)?P_E1:P_E0)+stattic_macro[k];char pct[6];fmt_int(pct,(int32_t)snow_home_percent(t,id));str_cpy(pct+str_len(pct),"%",sizeof pct-str_len(pct));
            stattic_text(15,68,"PRM",T_TEXT,14,20,3);stattic_text(73,68,track_desc(t,id)->label,T_THEME,14,20,6);stattic_text(179,68,pct,T_TEXT,11,20,4);
        }else if(page==1u){
            stattic_text(16,71,"GEN",T_TEXT,14,20,3);stattic_small_icon(0,76,66);
            cv_rect(125,65,103,30,T_THEME);{static const char *const drn[4]={"KICK","SNARE","HAT","PERC"};
            stattic_text(134,71,song.sel==TRK_DRUM?drn[stattic_focus[1]&3u]:ST_GEN_NAMES[stattic_focus[1]],T_INK,13,21,6);}
        }else{
            stattic_text(16,71,"MIXER",T_TEXT,13,20,5);
            cv_rect(99,65,62,30,T_THEME);stattic_text(108,71,ST_MIX_NAMES[stattic_focus[2]],T_INK,13,21,3);
            char pct[6];fmt_int(pct,(int32_t)snow_home_percent(t,ST_MIX_IDS[stattic_focus[2]]));str_cpy(pct+str_len(pct),"%",sizeof pct-str_len(pct));stattic_text(169,71,pct,T_TEXT,11,20,4);
        }
        stattic_graph(page);
    }else if(page==3u){
        stattic_text(16,37,"PATTERN SW",T_THEME,12,17,10);stattic_small_icon(2,166,34);
        for(uint32_t col=0;col<NTRK;col++){
            uint32_t ch=stattic_role_track(col);int32_t x=16+(int32_t)col*55;stattic_text(x,62,ST_ROLES[stattic_home_role(ch)],T_TEXT,12,17,2);
            uint32_t cur=stattic_current[ch],pending=stattic_pattern_pending(ch),focus=stattic_pattern_current(ch);
            for(uint32_t p=0;p<3u;p++){
                int32_t yy=88+(int32_t)p*29;int active=cur==p;
                if(active)cv_rect(x-1,yy-1,26,23,T_THEME);
                if(pending==p||(!active&&song.sel==ch&&focus==p))cv_frame(x-2,yy-2,28,25,T_THEME);
                char b[3]={'P',(char)('1'+p),0};stattic_text(x+2,yy+2,b,active?T_INK:T_THEME,10,17,2);
                if(p<2u)cv_rect(x+11,yy+23,2,6,T_TEXT);
            }
        }
    }else{
        stattic_text(16,37,"M/S VOLUME",T_THEME,13,18,10);stattic_small_icon(3,173,31);
        for(uint32_t row=0;row<NTRK;row++){
            uint32_t ch=stattic_role_track(row);int32_t y=63+(int32_t)row*26;if(ch==song.sel)cv_rect(15,y-2,28,21,T_THEME);
            stattic_text(17,y,ST_ROLES[stattic_home_role(ch)],ch==song.sel?T_INK:T_TEXT,10,17,2);
            uint32_t mute=(uint32_t)trk[ch].p[P_MUTE],solo=(song.solo>>ch)&1u;
            if(mute)cv_rect(51,y-2,14,21,T_THEME);if(solo)cv_rect(79,y-2,14,21,T_THEME);
            stattic_text(53,y,"M",mute?T_INK:T_TEXT,10,17,1);stattic_text(81,y,"S",solo?T_INK:T_TEXT,10,17,1);
            if((stattic_pending_mute_bits()>>ch)&1u)cv_frame(51,y-2,14,21,mute?T_TEXT:T_THEME);
            if((stattic_pending_solo_bits()>>ch)&1u)cv_frame(79,y-2,14,21,solo?T_TEXT:T_THEME);
            uint32_t pct=snow_home_percent(&trk[ch],P_LEVEL);snow_pixel_bar(100,y-1,92,17,pct,0,T_TEXT);
            char v[5];fmt_int(v,(int32_t)pct);str_cpy(v+str_len(v),"%",sizeof v-str_len(v));stattic_text(196,y,v,T_TEXT,8,17,4);
        }
    }
    stattic_footer(page);
}
static void snow_home_draw(void)
{

    cv_oy=0;uint32_t page=stattic_overview?(stattic_overview==1u?SN_HOME_PAT:SN_HOME_MS):stattic_home_page,k=song.sel;track_t *t=TSEL;int fresh=!snow_home_active;snow_home_active=1;
    if(fresh){snow_home_state.body=~0u;snow_home_state.head=~0u;snow_home_state.body_ms=0;}
    uint32_t beat=clk_beat%16u;
    uint32_t head=beat*313u+(uint32_t)song.g[G_BPM]+(uint32_t)song.playing*1024u+(uint32_t)batt_shown()*8192u+stattic_palette_revision*65537u;
    if(head!=snow_home_state.head){
        snow_home_state.head=head;cv_begin(240,32,T_BG);
        if(song.playing){cv_rect(20,14,2,9,T_THEME);for(int32_t i=0;i<5;i++)cv_rect(22+i,14+i,1,9-i*2,T_THEME);}else cv_rect(20,14,9,9,T_MID);
        char b[12];fmt_int(b,(int32_t)(beat/4u)+1);uint32_t n=str_len(b);b[n]='.';fmt_int(b+n+1,(int32_t)(beat%4u)+1);stattic_text(36,15,b,T_TEXT,7,10,4);
        fmt_int(b,song.g[G_BPM]);stattic_text(74,15,b,T_TEXT,7,10,3);stattic_text(101,15,"BPM",T_MID,7,10,3);stattic_text(132,13,"STATTIC",T_THEME,7,12,7);
        cv_frame(201,13,22,11,T_MID);cv_rect(223,17,3,4,T_MID);int32_t batt=batt_shown();cv_rect(203,15,batt==4?18:clamp(batt,0,3)*6,7,T_TEXT);cv_blit(0,0);
    }
    uint32_t sig=stattic_palette_revision*977u+k*31u+page*65537u+t->preset*7919u+t->user*131u+stattic_revision*8179u+song.solo*11717u;
    for(uint32_t ch=0;ch<NTRK;ch++)sig=(sig^stattic_revision^(uint32_t)trk[ch].p[P_LEVEL]^(uint32_t)trk[ch].p[P_MUTE]<<16^stattic_pattern_current(ch))*16777619u;
    if(page<3u)sig^=stattic_scroll[page]*104729u+stattic_focus[page]*349u+stattic_art_pick[page]*194713u;
    if(page==SN_HOME_MIX)for(uint32_t id=0;id<4u;id++)sig=(sig^(uint32_t)t->p[ST_MIX_IDS[id]])*16777619u;
    sig^=(uint32_t)t->p[SN_PLAY_MODE]*32993u;
    sig^=((uint32_t)stattic_pending_mute_bits()|((uint32_t)stattic_pending_solo_bits()<<6))*63997u;
    sig^=(uint32_t)(ui.msg_t!=0)*301u+(uint32_t)(stattic_history_at!=0)*619u;
    if((sig!=snow_home_state.body||ui.force)&&(fresh||fm1_ms-snow_home_state.body_ms>=50u)){
        snow_home_state.body=sig;snow_home_state.body_ms=fm1_ms;snow_home_state.card_blits++;
        for(uint32_t tile=0;tile<3u;tile++){uint32_t y=32u+tile*70u,h=y+70u>240u?240u-y:70u;cv_begin(240,h,T_BG);cv_oy=-(int32_t)y;stattic_page_body(page);cv_blit(0,y);}cv_oy=0;
    }
    ui_timers();ui.force=0;
}
