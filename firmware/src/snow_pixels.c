/* SPDX-License-Identifier: GPL-3.0-only */
#include "snow_font.h"
#include "snow_icons.h"
#include "snow_channel_icons.h"
static int32_t snow_text(int32_t x,int32_t y,const char *s,uint16_t ink,uint32_t max)
{
    uint32_t n=0;
    while(*s&&n<max){uint32_t c=(uint8_t)*s++;if(c>='a'&&c<='z')c-=32;if(c<32||c>95)c='?';
        for(uint32_t r=0;r<7;r++)for(uint32_t b=0;b<5;b++)if((SN_FONT[c-32][r]>>(4-b))&1u)cv_rect(x+(int32_t)b,y+(int32_t)r,1,1,ink);
        x+=6;n++;
    }
    return x;
}
static void snow_text_box(int32_t x,int32_t y,int32_t width,int32_t height,const char *text,uint16_t ink)
{
    aafont_t f=AF_S;if(height<(int32_t)f.caph)f.caph=(uint8_t)height;
    f.asc=f.caph;cv_text_fit(x,y,&f,text,ink,cv_bg,width);
}
/* Binary pixel art only: moving parts, never a travelling shade or tint.
 * Position is the knob value, not a clock. Drawing stays in the main loop. */
#define SN_ICON_N 40u
static uint32_t snow_sprite_base(uint32_t kind,int32_t x,int32_t y)
{
    if(kind<5u||(uint32_t)x>=40u||(uint32_t)y>=40u)return 0;
    return (SN_SPRITE_ROWS[SN_SPRITE_INDEX[(kind-5u)%5u][y]][(uint32_t)x/8u]>>(7u-(uint32_t)x%8u))&1u;
}
static const int8_t SN_TURN[32]={0,12,24,36,45,53,59,63,64,63,59,53,45,36,24,12,0,-12,-24,-36,-45,-53,-59,-63,-64,-63,-59,-53,-45,-36,-24,-12};
static uint32_t snow_icon_pixel(uint32_t kind,uint32_t x,uint32_t y,uint32_t position)
{
    uint32_t q=position>1000u?1000u:position,at=q*31u/1000u;
    int32_t xx=(int32_t)x,yy=(int32_t)y,wave=SN_TURN[at],on=snow_sprite_base(kind,xx,yy);
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
/* Replay bounded XOR deltas into a small stack buffer. No allocation, timer,
 * flash writes or audio-thread work. Position 0..1000 chooses a supplied pose. */
static int snow_channel_decode(uint32_t kind,uint32_t position,uint8_t *pixels)
{
    uint32_t q=position>1000u?1000u:position,frame=(q*(SN_CHANNEL_POSES-1u)+500u)/1000u;
    const uint8_t *p=SN_CHANNEL_DATA+SN_CHANNEL_OFFSET[kind],*end=SN_CHANNEL_DATA+SN_CHANNEL_OFFSET[kind+1u];
    memset(pixels,0,SN_CHANNEL_BYTES);
    for(uint32_t f=0;f<=frame;f++){
        uint32_t at=0;
        while(at<SN_CHANNEL_BYTES){
            if(p>=end)return 0;
            uint32_t op=*p++,n=(op&127u)+1u;
            if(n>SN_CHANNEL_BYTES-at)return 0;
            if(!(op&128u)){if((uint32_t)(end-p)<n)return 0;for(uint32_t j=0;j<n;j++)pixels[at+j]^=*p++;}
            at+=n;
        }
    }
    return 1;
}
static uint32_t snow_channel_pixel(const uint8_t *p,uint32_t x,uint32_t y)
{
    uint32_t i=y*SN_CHANNEL_SIDE+x;return (p[i/4u]>>(6u-2u*(i%4u)))&3u;
}
static void snow_channel_icon(int32_t cx,int32_t cy,uint32_t kind,uint32_t position,uint32_t scale,uint16_t ink)
{
    uint8_t pixels[SN_CHANNEL_BYTES];if(!snow_channel_decode(kind,position,pixels))return;
    uint32_t n=scale==1u?20u:SN_CHANNEL_SIDE,mul=scale==2u?2u:1u;
    /* 33 native pixels at 2x: exactly the former KK bounds (13,115)..(79,181).
     * The outer native pixel is empty in every pose, so no stroke touches it. */
    int32_t origin=-(int32_t)(n/2u*mul);uint16_t light=ink==T_THEME?T_TEXT:T_MID;
    for(uint32_t y=0;y<n;y++)for(uint32_t x=0;x<n;){
        uint32_t first=x,v=snow_channel_pixel(pixels,x*SN_CHANNEL_SIDE/n,y*SN_CHANNEL_SIDE/n);
        if(v){while(x<n&&snow_channel_pixel(pixels,x*SN_CHANNEL_SIDE/n,y*SN_CHANNEL_SIDE/n)==v)x++;
            cv_rect(cx+origin+(int32_t)(first*mul),cy+origin+(int32_t)(y*mul),(int32_t)((x-first)*mul),(int32_t)mul,v==1u?light:ink);
        }else x++;
    }
}
/* KK/SD/HH/PR: draw the supplied geometry at the actual output resolution.
 * No bitmap resize, frame atlas, antialiasing or timer. All four silhouettes
 * have exactly the same 64x64 ink bounds inside their 66x66 HOME cell. */
typedef struct { int32_t x,y,n; uint16_t ink; } snow_geo_t;
static void snow_geo_rect(const snow_geo_t *g,int32_t x,int32_t y,int32_t w,int32_t h)
{
    int32_t x0=x*g->n/66,y0=y*g->n/66,x1=(x+w)*g->n/66,y1=(y+h)*g->n/66;
    if(x1>x0&&y1>y0)cv_rect(g->x+x0,g->y+y0,x1-x0,y1-y0,g->ink);
}
static void snow_geo_disk(const snow_geo_t *g,int32_t cx,int32_t cy)
{
    for(int32_t y=-5;y<=5;y++){
        int32_t x=5;while(x*x+y*y>25)x--;
        snow_geo_rect(g,cx-x,cy+y,x*2+1,1);
    }
}
static void snow_geo_pair(const snow_geo_t *g,int32_t left,int32_t top,int32_t middle,int32_t gap)
{
    int32_t upper=middle-gap,lower=middle+gap,bottom=top+30;
    for(int32_t y=top;y<=upper;y++)snow_geo_rect(g,left,y,1+14*(y-top)/(upper-top),1);
    for(int32_t y=lower;y<=bottom;y++)snow_geo_rect(g,left,y,1+14*(bottom-y)/(bottom-lower),1);
}
static void snow_geometric_icon(int32_t cx,int32_t cy,uint32_t kind,uint32_t position,uint32_t scale,uint16_t ink)
{
    int32_t q=position>1000u?1000:(int32_t)position,n=scale==2u?66:scale==1u?20:33;
    snow_geo_t g={cx-(n/2)+(scale==2u),cy-(n/2)+(scale==2u),n,ink};
    if(kind==0u){ /* KK: square and three progressively wider filled bars. */
        int32_t a=12+(q*6+500)/1000,b=26+(q*12+500)/1000,c=46+(q*8+500)/1000;
        snow_geo_rect(&g,(66-a)/2,1,a,a);snow_geo_rect(&g,(66-b)/2,28,b,8);
        snow_geo_rect(&g,(66-c)/2,46,c,5);snow_geo_rect(&g,1,62,64,3);
    }else if(kind==1u){ /* SD: 5x5 checkerboard, fixed centres and corner anchors. */
        for(int32_t r=0;r<5;r++)for(int32_t c=0;c<5;c++)if(!((r+c)&1)){
            int32_t pulse=q<=500?q*2:(1000-q)*2;
            int32_t size=((r==0||r==4)&&(c==0||c==4))?12:((r+c)&2)?12-(q*6+500)/1000:12-(pulse*4+500)/1000;
            int32_t pad=(12-size)/2;snow_geo_rect(&g,1+c*13+pad,1+r*13+pad,size,size);
        }
    }else if(kind==2u){ /* HH: four pairs of filled triangles in each of two rows. */
        for(int32_t r=0;r<2;r++)for(int32_t c=0;c<4;c++){
            static const uint8_t left[4]={1,17,34,50};
            int32_t d=(q*(4+c%3)+500)/1000,top=1+r*33,middle=top+15+(((r+c)&1)?d:-d);
            snow_geo_pair(&g,left[c],top,middle,1+(q*3+500)/1000);
        }
    }else{ /* PR: the supplied staggered dot/dash pattern. */
        int32_t d=(q*6+500)/1000-3,m=(q*8+500)/1000-4,l=15-(q*5+500)/1000,s=4+(q*6+500)/1000;
        snow_geo_disk(&g,6,6);snow_geo_disk(&g,42+d,6);
        snow_geo_disk(&g,24-d,24);snow_geo_disk(&g,59,24);
        snow_geo_disk(&g,33+m,42);snow_geo_disk(&g,6,59);snow_geo_disk(&g,42-d,59);
        snow_geo_rect(&g,24-l/2,4,l,4);snow_geo_rect(&g,59-s/2,4,s,4);
        snow_geo_rect(&g,6-s/2,22,s,4);snow_geo_rect(&g,42-l/2,22,l,4);
        snow_geo_rect(&g,16-l/2,40,l,4);snow_geo_rect(&g,50-l/2,40,l,4);
        snow_geo_rect(&g,24-l/2,57,l,4);snow_geo_rect(&g,59-s/2,57,s,4);
    }
}
static void snow_icon(int32_t cx,int32_t cy,uint32_t kind,uint32_t position,uint32_t scale,uint16_t ink)
{
    if(kind<4u){snow_geometric_icon(cx,cy,kind,position,scale,ink);return;}
    if(kind==4u){snow_channel_icon(cx,cy,0,position,scale,ink);return;}
    uint32_t n=scale==1u?20u:40u,mul=scale==2u?2u:1u;
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
    int32_t rh=(height-20)/(int32_t)rows;
    uint64_t locks=motion_lock_steps(k);
    for(uint32_t i=0;i<len;i++) {
        const step_t *s=&t->step[i];int32_t x=4+(int32_t)(i%16u)*14,y=3+(int32_t)(i/16u)*rh;
        int32_t h=rh<24?rh-3:rh-18;if(h>48)h=48;
        cv_rect(x,y,11,h,T_SURF);
        if(s->time==ST_NOTE&&s->n){int32_t fill=(int32_t)(s->vel?s->vel:96u)*(h-2)/127;if(fill<2)fill=2;cv_rect(x+1,y+h-1-fill,9,fill,T_THEME);}
        else if(s->time==ST_TIE)cv_rect(x+1,y+h/2,9,1,T_MID);
        if((locks>>i)&1u)cv_rect(x+7,y+1,3,3,T_TEXT);
        if(rh>=20){char num[4];fmt_int(num,(int32_t)i+1);snow_text_box(x,y+h+2,13,14,num,T_DIM);}
        if(step_ratchet(s)>1u){char n[2]={(char)('0'+step_ratchet(s)),0};snow_text_box(x+3,y+1,7,h-2,n,T_TEXT);}
        if(song.playing&&t->seq_idx%len==i)cv_frame(x-1,y-1,13,h+2,T_TEXT);
    }
    char n[16];fmt_int(n,(int32_t)len);str_cpy(n+str_len(n)," STEPS",sizeof n-str_len(n));
    snow_text_box(4,height-16,232,14,n,T_MID);
}
