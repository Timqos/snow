/* SPDX-License-Identifier: GPL-3.0-only */
#include "snow_font.h"
static int32_t snow_text(int32_t x,int32_t y,const char *s,uint16_t ink,uint32_t max)
{
    uint32_t n=0;
    while(*s&&n<max){uint32_t c=(uint8_t)*s++;if(c>='a'&&c<='z')c-=32;if(c<32||c>95)c='?';
        for(uint32_t r=0;r<7;r++)for(uint32_t b=0;b<5;b++)if((SN_FONT[c-32][r]>>(4-b))&1u)cv_rect(x+(int32_t)b,y+(int32_t)r,1,1,ink);
        x+=6;n++;
    }
    return x;
}
static int snow_icon_bit(uint32_t kind,int32_t x,int32_t y,uint32_t frame)
{
    int32_t ax=x<0?-x:x,ay=y<0?-y:y,phase=(int32_t)(frame&3u);
    if((ax==7&&ay==7))return 1; /* four fixed anchors: identical bounds in every frame */
    switch(kind%6u) {
    case 0:return ax+ay==7 || (ax+ay==2+phase%2) || (!x&&y>=3&&y<=6);
    case 1:return (ax==0&&ay>=3&&ay<=6)||(ay==0&&ax>=3&&ax<=6)||(ax+ay==3+(phase&1))||(ax==4&&ay==4&&((frame>>1)&1));
    case 2:return (ay==5&&ax<=5)||(ax==5&&ay<=5)||((x-y+phase*2+24)%6==0&&ax<=3&&ay<=3);
    case 3:return (x==-5&&y>=-1-phase&&y<=6)||(x==0&&y>=-5+phase&&y<=6)||(x==5&&y>=-3&&y<=6)||(y==6&&ax<=5);
    case 4:return (x*x+2*y*y>=43&&x*x+2*y*y<=65)||(ay<=1&&ax<=4&&((x+phase+8)%3!=0));
    default:return (ax==ay&&ax<=5)||(!x&&ay<=6)||(!y&&ax<=6)||(ax+ay==3+(phase&1));
    }
}
static void snow_icon(int32_t cx,int32_t cy,uint32_t kind,uint32_t frame,uint32_t scale,uint16_t ink)
{
    for(int32_t y=-7;y<=7;y++)for(int32_t x=-7;x<=7;x++)if(snow_icon_bit(kind,x,y,frame))cv_rect(cx+x*(int32_t)scale,cy+y*(int32_t)scale,(int32_t)scale,(int32_t)scale,ink);
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
