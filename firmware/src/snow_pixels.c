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
/* The six existing motifs are deformed by absolute parameter position 0..1000.
 * No clock, autoplay, modulo frame counter or return-to-idle frame. Thirty
 * local rows keep the original 30x30 bounds without a second framebuffer. */
#define SN_ICON_N 30u
static void snow_icon_dot(uint32_t *rows,int32_t x,int32_t y)
{
    x+=14;y+=14;
    for(int32_t dy=0;dy<2;dy++)for(int32_t dx=0;dx<2;dx++)
        if((uint32_t)(x+dx)<SN_ICON_N&&(uint32_t)(y+dy)<SN_ICON_N)rows[y+dy]|=1u<<(x+dx);
}
static void snow_icon_line(uint32_t *rows,int32_t x,int32_t y,int32_t x1,int32_t y1)
{
    int32_t dx=x1>x?x1-x:x-x1,sx=x<x1?1:-1,dy=y1>y?y-y1:y1-y,sy=y<y1?1:-1,err=dx+dy;
    for(;;){snow_icon_dot(rows,x,y);if(x==x1&&y==y1)break;int32_t e=err*2;if(e>=dy){err+=dy;x+=sx;}if(e<=dx){err+=dx;y+=sy;}}
}
static void snow_icon_diamond(uint32_t *r,int32_t x,int32_t y,int32_t rx,int32_t ry)
{
    snow_icon_line(r,x,y-ry,x+rx,y);snow_icon_line(r,x+rx,y,x,y+ry);
    snow_icon_line(r,x,y+ry,x-rx,y);snow_icon_line(r,x-rx,y,x,y-ry);
}
static void snow_icon_rows(uint32_t kind,uint32_t position,uint32_t *rows)
{
    int32_t q=(int32_t)(position>1000u?1000u:position);memset(rows,0,SN_ICON_N*sizeof *rows);
    /* Existing corner pixels keep the same box/centre throughout the morph. */
    snow_icon_dot(rows,-14,-14);snow_icon_dot(rows,14,-14);snow_icon_dot(rows,-14,14);snow_icon_dot(rows,14,14);
    switch(kind%6u) {
    case 0: { /* Kick: pressure opens the diamond and its central membrane. */
        int32_t width=7+q*7/1000,core=1+q*6/1000;
        snow_icon_diamond(rows,0,0,width,12);snow_icon_diamond(rows,0,0,core,1+q*9/1000);
        snow_icon_line(rows,0,core,0,14);snow_icon_line(rows,-core,0,core,0);
        if(q>450)snow_icon_line(rows,-3,10,3,10);break;
    }
    case 1: { /* Snare: all eight star edges broaden; satellite stars open. */
        int32_t w=1+q*6/1000,rx=8+q*5/1000,ry=13-q*4/1000,tilt=-5+q*9/1000;
        int32_t px[8]={tilt,w,rx,w,-tilt,-w,-rx,-w},py[8]={-ry,-w,tilt,w,ry,w,-tilt,-w};
        for(uint32_t i=0;i<8;i++)snow_icon_line(rows,px[i],py[i],px[(i+1u)&7u],py[(i+1u)&7u]);
        snow_icon_diamond(rows,-11+q*3/1000,-10+q*5/1000,1+q*2/1000,1+q*2/1000);
        snow_icon_diamond(rows,-10,10,3-q*2/1000,3-q*2/1000);break;
    }
    case 2: { /* Hat: the square opens while every internal rake changes tilt. */
        int32_t r=9+q*4/1000,lean=-7+q*14/1000;
        snow_icon_line(rows,-r,-r,r,-r);snow_icon_line(rows,r,-r,r,r);
        snow_icon_line(rows,r,r,-r,r);snow_icon_line(rows,-r,r,-r,-r);
        for(int32_t i=-1;i<=1;i++)snow_icon_line(rows,clamp(i*4-lean,-r+2,r-2),-r+3,clamp(i*4+lean,-r+2,r-2),r-3);
        break;
    }
    case 3: { /* Percussion: the three original columns exchange height. */
        int32_t y[3]={3-q*15/1000,-12+q*11/1000,-2-q*11/1000};
        for(int32_t i=0;i<3;i++)snow_icon_line(rows,(i-1)*10,y[i],(i-1)*10,12);
        snow_icon_line(rows,-10,12,10,12);break;
    }
    case 4: { /* Bass: the oval compresses horizontally and opens vertically. */
        int32_t rx=13-q*4/1000,ry=5+q*8/1000,ix=rx-2,iy=ry-2;
        for(int32_t y=-ry;y<=ry;y++)for(int32_t x=-rx;x<=rx;x++)
            if(x*x*ry*ry+y*y*rx*rx<=rx*rx*ry*ry && x*x*iy*iy+y*y*ix*ix>=ix*ix*iy*iy)snow_icon_dot(rows,x,y);
        for(int32_t j=-1;j<=1;j++){int32_t yy=j*(1+q*4/1000),waist=8-q*5/1000;
            snow_icon_line(rows,-waist,yy,0,yy+j*q*7/1000);snow_icon_line(rows,0,yy+j*q*7/1000,waist,yy);}
        break;
    }
    default: { /* Chord: the same eight spokes weave around an opening core. */
        static const int8_t x[8]={0,9,13,9,0,-9,-13,-9},y[8]={-13,-9,0,9,13,9,0,-9};
        int32_t cx[8],cy[8];
        for(uint32_t i=0;i<8;i++){cx[i]=x[(i+1u)&7u]*q/1500;cy[i]=y[(i+1u)&7u]*q/1800;}
        for(uint32_t i=0;i<8;i++){
            snow_icon_line(rows,cx[i],cy[i],x[i],y[i]);
            snow_icon_line(rows,cx[i],cy[i],cx[(i+1u)&7u],cy[(i+1u)&7u]);
        }break;
    }
    }
}
static void snow_icon(int32_t cx,int32_t cy,uint32_t kind,uint32_t position,uint32_t scale,uint16_t ink)
{
    uint32_t rows[SN_ICON_N];snow_icon_rows(kind,position,rows);
    uint32_t stride=scale==1u?2u:1u,n=SN_ICON_N/stride;int32_t origin=scale==1u?-7:-14;
    for(uint32_t y=0;y<n;y++)for(uint32_t x=0;x<n;) {
        uint32_t start=x;
        do {uint32_t mask=((1u<<stride)-1u)<<(x*stride),on=rows[y*stride]&mask;
            if(stride==2u)on|=rows[y*stride+1u]&mask;if(!on)break;x++;
        }while(x<n);
        if(x>start)cv_rect(cx+origin+(int32_t)start,cy+origin+(int32_t)y,(int32_t)(x-start),1,ink);
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
