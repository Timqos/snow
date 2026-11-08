/* SPDX-License-Identifier: GPL-3.0-only */
#define FELUCCA_STATTIC 1
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
static uint8_t nor[0x100000];
static int fail=-1;
static int cut(void) { if(!fail)return 1;if(fail>0)fail--;return 0; }
static int st_read(uint32_t a,void *p,uint32_t n) { assert(a+n<=sizeof nor);memcpy(p,nor+a,n);return 0; }
static int st_erase(uint32_t a) { assert(!(a&4095));if(cut())return -1;memset(nor+a,255,4096);return 0; }
static int st_prog(uint32_t a,const void *p,uint32_t n) {
    assert((a&255)+n<=256);if(cut())return -1;
    for(uint32_t i=0;i<n;i++)nor[a+i]&=((const uint8_t *)p)[i];return 0;
}
#include "../firmware/src/storage.c"
int main(void)
{
    uint8_t occupied[256]={0},old[12032],next[12032],got[12032];
    const uint32_t object[]={OBJ_PROJECT0,OBJ_PROJECT0+1,OBJ_PROJECT0+2,OBJ_PROJECT0+3,OBJ_AUTOSAVE,OBJ_SETTINGS,OBJ_UPRESET0,OBJ_UPRESET0+1};
    for(uint32_t i=0;i<sizeof object/sizeof *object;i++)for(uint32_t copy=0;copy<2;copy++) {
        uint32_t a=st_sector(object[i],copy),span=st_span(object[i]);assert(a+span<=sizeof nor);
        for(uint32_t j=0;j<span/4096;j++)assert(!occupied[a/4096+j]++);
    }
    for(uint32_t a=0xA0000;a<0xDC000;a+=4096)assert(!occupied[a/4096]);
    for(uint32_t a=0xE0000;a<0xE5000;a+=4096)assert(!occupied[a/4096]);
    memset(old,0x35,sizeof old);memset(next,0xAC,sizeof next);
    for(uint32_t obj=OBJ_PROJECT0;obj<OBJ_UPRESET0;obj++)for(int injection=0;injection<54;injection++) {
        memset(nor,255,sizeof nor);fail=-1;assert(!st_save(obj,old,sizeof old));
        fail=injection;(void)st_save(obj,next,sizeof next);fail=-1;
        assert(st_load(obj,got,sizeof got)==sizeof got);
        assert(!memcmp(got,old,sizeof got)||!memcmp(got,next,sizeof got));
        for(uint32_t a=0xA0000;a<0xDC000;a++)assert(nor[a]==255);
        assert(!st_save(obj,next,sizeof next));assert(st_load(obj,got,sizeof got)==sizeof got&&!memcmp(next,got,sizeof got));
    }
    puts("storage: 12 KiB A/B commits, 216 power-cut positions, non-overlapping sample/OTA/settings/preset regions");return 0;
}
