/* SPDX-License-Identifier: GPL-3.0-only */
#include "snow_font.h"
#include "snow_icons.h"
static int32_t snow_text(int32_t x,int32_t y,const char *s,uint16_t ink,uint32_t max)
{
    uint32_t n=0;
    while(*s&&n<max){uint32_t c=(uint8_t)*s++;if(c>='a'&&c<='z')c-=32;if(c<32||c>95)c='?';
        for(uint32_t r=0;r<7;r++)for(uint32_t b=0;b<5;b++)if((SN_FONT[c-32][r]>>(4-b))&1u)cv_rect(x+(int32_t)b,y+(int32_t)r,1,1,ink);
        x+=6;n++;
    }
    return x;
}
/* Binary pixel art only: moving parts, never a travelling shade or tint.
 * Position is the knob value, not a clock. Drawing stays in the main loop. */
#define SN_ICON_N 40u
static uint32_t snow_sprite_base(uint32_t kind,int32_t x,int32_t y)
{
    if((uint32_t)x>=40u||(uint32_t)y>=40u)return 0;
    return (SN_SPRITE_ROWS[SN_SPRITE_INDEX[kind%10u][y]][(uint32_t)x/8u]>>(7u-(uint32_t)x%8u))&1u;
}
static const int8_t SN_TURN[32]={0,12,24,36,45,53,59,63,64,63,59,53,45,36,24,12,0,-12,-24,-36,-45,-53,-59,-63,-64,-63,-59,-53,-45,-36,-24,-12};
static uint32_t snow_icon_pixel(uint32_t kind,uint32_t x,uint32_t y,uint32_t position)
{
    uint32_t q=position>1000u?1000u:position,at=q*31u/1000u;
    int32_t xx=(int32_t)x,yy=(int32_t)y,wave=SN_TURN[at],on=snow_sprite_base(kind,xx,yy);
    if(kind==0u){ /* Four snowflake arms flex around a fixed hub. */
        int32_t flex=(int32_t)q*2/1000;
        if(xx<14)xx-=flex;else if(xx>25)xx+=flex;
        if(yy<14)yy-=flex;else if(yy>25)yy+=flex;
        return snow_sprite_base(kind,xx,yy);
    }
    if(kind==1u||kind==3u){
        int32_t cy=kind==1u?20:16,dx=xx-20,dy=yy-cy;
        if(kind==1u||dx*dx+dy*dy<169){
            int32_t co=SN_TURN[(at+8u)&31u];
            return snow_sprite_base(kind,20+(dx*co+dy*wave)/64,cy+(dy*co-dx*wave)/64);
        }
    }
    if(kind==2u){ /* Eye arches keep their shape and X coordinates. */
        int32_t lift=wave/48;
        if(yy>=7&&yy<=16&&((xx>=5&&xx<=17)||(xx>=21&&xx<=33)))return snow_sprite_base(kind,xx,yy-lift);
        if(yy>=16&&yy<=24&&xx>=17&&xx<=22){int32_t h=2+(int32_t)q*5/1000;return !(yy>=20-h/2&&yy<20+(h+1)/2);}
    }
    if(kind==4u){
        int32_t phase=(int32_t)(q*127u/1000u),eye=phase%12;if(eye>6)eye=12-eye;
        if(yy<6){int32_t tilt=(eye-3)/2;return snow_sprite_base(kind,xx+(xx<20?tilt:-tilt)*(yy<3?1:0),yy);}
        if(q&&yy>=15&&yy<=20&&((xx>=9&&xx<=17)||(xx>=25&&xx<=33))){
            int32_t cx=(xx<20?13:29)+eye-3;return !(yy>=16&&xx>=cx-1&&xx<=cx+1);
        }
    }
    if(kind==5u&&q){ /* Cassette transport retained unchanged. */
        int32_t phase=(int32_t)(q*127u/1000u);
        for(uint32_t i=0;i<2;i++){
            int32_t dx=xx-(i?29:10),dy=yy-17;
            if(dx*dx+dy*dy<=10){
                if(dx*dx+dy*dy<=1)return 1;
                static const int8_t vx[8]={3,2,0,-2,-3,-2,0,2},vy[8]={0,2,3,2,0,-2,-3,-2};
                uint32_t a=((uint32_t)phase+i*2u)&7u;
                return (dx==vx[a]&&dy==vy[a])||(dx==-vx[a]&&dy==-vy[a]);
            }
        }
    }
    if(kind==6u){ /* Fixed arrowheads; four shafts extend and retract independently. */
        static const int8_t ax[4]={8,30,18,30},ay[4]={8,8,21,31};
        for(uint32_t i=0;i<4;i++){
            int32_t dx=xx-ax[i],dy=yy-ay[i],h=i==2u?7:4;
            int32_t tail=3+(SN_TURN[(at+i*5u)&31u]+64)/24,stroke=dx+dy;
            if(stroke<0)stroke=-stroke;
            if((dx>=-tail&&dx<=4&&stroke<=(i==2u?1:0))||
               (dx<=4&&dy>=-4&&4-dx+dy+4<=h))return 1;
        }return 0;
    }
    if(kind==7u){ /* Alternating capsule rows flow in opposite directions. */
        int32_t d=(int32_t)(q*32u/1000u);return snow_sprite_base(kind,(xx+40+((yy/7)&1?d:-d))%40,yy);
    }
    if(kind==8u){ /* Envelope nodes, connecting lines and their dotted posts. */
        const int32_t px[4]={3,13,23,35};int32_t py[4]={18-wave/16,10+wave/16,18+wave/12,5-wave/20};
        for(uint32_t i=0;i<4;i++){
            int32_t dx=xx-px[i],dy=yy-py[i];if(dx<0)dx=-dx;if(dy<0)dy=-dy;
            if((dx<=2&&!dy)||(dy<=2&&!dx))return 1;
            if(xx==px[i]&&yy>py[i]+3&&yy<38&&!(yy%3))return 1;
            if(i<3&&xx>=px[i]&&xx<=px[i+1]&&yy==py[i]+(py[i+1]-py[i])*(xx-px[i])/(px[i+1]-px[i]))return 1;
        }return 0;
    }
    if(kind==9u){ /* Sparkle arms open while the two satellite diamonds breathe. */
        if(xx<13){int32_t cy=yy<20?6:33,r=yy<20?4+wave/48:3-wave/48,dx=xx-6,dy=yy-cy;if(dx<0)dx=-dx;if(dy<0)dy=-dy;return dx+dy==r;}
        int32_t dx=xx-25,dy=yy-21;if(dx<0)dx=-dx;if(dy<0)dy=-dy;
        int32_t h=14+wave/32,arm=12-wave/32;
        int32_t w=dy<3?arm-(arm-3)*dy/3:3*(h-dy)/(h-3);
        int32_t next=dy+1<3?arm-(arm-3)*(dy+1)/3:3*(h-dy-1)/(h-3);
        return dy<=h&&dx>=next&&dx<=w;
    }
    return (uint32_t)on;
}
static void snow_icon(int32_t cx,int32_t cy,uint32_t kind,uint32_t position,uint32_t scale,uint16_t ink)
{
    uint32_t n=scale==1u?20u:scale==2u&&kind<5u?36u:40u,mul=scale==2u?2u:1u;
    int32_t origin=-(int32_t)(n*mul)/2;
    for(uint32_t y=0;y<n;y++)for(uint32_t x=0;x<n;){
        uint32_t start=x;
        while(x<n&&snow_icon_pixel(kind,x*40u/n,y*40u/n,position))x++;
        if(x>start)cv_rect(cx+origin+(int32_t)(start*mul),cy+origin+(int32_t)(y*mul),(int32_t)((x-start)*mul),(int32_t)mul,ink);
        else x++;
    }
}
static uint32_t snow_param_position(const track_t *t,uint32_t id)
{
    if(id>=P_COUNT)return 0;
    const param_desc_t *d=track_desc(t,id);int32_t v=motion_base_value(t,id);
    if(id>=P_E0&&id<=P_E3)v+=snow_offset[snow_role(t)][id-P_E0];
    return d->max>d->min?(uint32_t)clamp((v-d->min)*1000/(d->max-d->min),0,1000):0u;
}
static int snow_gen_page(const page_t *p){return p->fam==FAM_SEQ&&p->graph==GR_NONE;}
static void snow_gen_grid(const track_t *t,int32_t height)
{
    uint32_t len=(uint32_t)clamp(t->p[P_SLEN],1,NSTEP),rows=(len+15u)/16u,k=trk_index(t);
    int32_t rh=(height-12)/(int32_t)rows;
    uint64_t locks=motion_lock_steps(k);
    for(uint32_t i=0;i<len;i++) {
        const step_t *s=&t->step[i];int32_t x=4+(int32_t)(i%16u)*14,y=3+(int32_t)(i/16u)*rh;
        int32_t h=rh<20?rh-3:rh-12;if(h>48)h=48;
        cv_rect(x,y,11,h,T_SURF);
        if(s->time==ST_NOTE&&s->n){int32_t fill=(int32_t)(s->vel?s->vel:96u)*(h-2)/127;if(fill<2)fill=2;cv_rect(x+1,y+h-1-fill,9,fill,T_THEME);}
        else if(s->time==ST_TIE)cv_rect(x+1,y+h/2,9,1,T_MID);
        if((locks>>i)&1u)cv_rect(x+7,y+1,3,3,T_TEXT);
        if(rh>=20){char num[4];fmt_int(num,(int32_t)i+1);snow_text(x,y+h+2,num,T_DIM,2);}
        if(step_ratchet(s)>1u){char n[2]={(char)('0'+step_ratchet(s)),0};snow_text(x+3,y+1,n,T_TEXT,1);}
        if(song.playing&&t->seq_idx%len==i)cv_frame(x-1,y-1,13,h+2,T_TEXT);
    }
    char n[16];fmt_int(n,(int32_t)len);str_cpy(n+str_len(n)," STEPS",sizeof n-str_len(n));
    snow_text(4,height-8,n,T_MID,12);
}
