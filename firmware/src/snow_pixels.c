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
/* Fixed 40x40 reference silhouettes. Only local details respond to position.
 * No animation clock in the audio thread and no drifting bounding boxes. */
#define SN_ICON_N 40u
static uint32_t snow_sprite_base(uint32_t kind,uint32_t x,uint32_t y)
{
    return (SN_SPRITE_ROWS[SN_SPRITE_INDEX[kind%10u][y]][x/8u]>>(7u-x%8u))&1u;
}
static uint32_t snow_icon_pixel(uint32_t kind,uint32_t x,uint32_t y,uint32_t position)
{
    uint32_t on=snow_sprite_base(kind,x,y),q=position>1000u?1000u:position;
    if(!q)return on;
    int32_t xx=(int32_t)x,yy=(int32_t)y,phase=(int32_t)(q*127u/1000u),eye=phase%12;
    if(eye>6)eye=12-eye;
    /* Holes/pupils stay inside the head. The silhouette never changes. */
    if(kind==2u&&y>=8u&&y<=15u&&((x>=6u&&x<=16u)||(x>=22u&&x<=32u))){
        int32_t cx=x<20u?11:27,dx=xx-cx-eye+3,dy=yy-10-(phase/8)%2;
        if(dx<0)dx=-dx;return !(dy>=0&&dy<=3&&dx>=dy-1&&dx<=dy);
    }
    if(kind==4u&&y>=15u&&y<=20u&&((x>=9u&&x<=17u)||(x>=25u&&x<=33u))){
        int32_t cx=(x<20u?13:29)+eye-3;return !(yy>=16&&xx>=cx-1&&xx<=cx+1);
    }
    if(kind==5u){
        for(uint32_t i=0;i<2;i++){
            int32_t dx=xx-(i?29:10),dy=yy-17;
            if(dx*dx+dy*dy<=10){
                if(dx*dx+dy*dy<=1)return 1;
                static const int8_t vx[8]={3,2,0,-2,-3,-2,0,2},vy[8]={0,2,3,2,0,-2,-3,-2};
                uint32_t at=((uint32_t)phase+i*2u)&7u;
                return (dx==vx[at]&&dy==vy[at])||(dx==-vx[at]&&dy==-vy[at]);
            }
        }
    }
    /* Moving ink travels through existing lit cells, never outside the motif. */
    if(on&&x>2u&&x<37u&&y>2u&&y<37u&&kind!=2u&&kind!=4u&&kind!=5u){
        if(((int32_t)x+(int32_t)y*2-phase%17+34)%17>3)return 2;
    }
    return on;
}
static void snow_icon(int32_t cx,int32_t cy,uint32_t kind,uint32_t position,uint32_t scale,uint16_t ink)
{
    uint32_t stride=scale==1u?2u:1u,mul=scale==2u?2u:1u,n=40u/stride;
    int32_t origin=-(int32_t)(n*mul)/2;
    uint16_t dim=ux_mix(T_BG,ink,70);
    for(uint32_t y=0;y<n;y++)for(uint32_t x=0;x<n;){
        uint32_t start=x,value=snow_icon_pixel(kind,x*stride,y*stride,position);
        while(value&&x<n&&snow_icon_pixel(kind,x*stride,y*stride,position)==value)x++;
        if(x>start)cv_rect(cx+origin+(int32_t)(start*mul),cy+origin+(int32_t)(y*mul),(int32_t)((x-start)*mul),(int32_t)mul,value==2?dim:ink);
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
