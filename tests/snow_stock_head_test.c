/* SPDX-License-Identifier: GPL-3.0-only
 * Existing OTA fault tests, then a real V15/V16 bootstrap mismatch. No MIDI. */
#define main upstream_ota_tests
#include "ota_test.c"
#undef main
int main(int argc,char **argv)
{
    if(argc!=3)return 2;
    char *args[]={argv[0],argv[1],"official",0};
    if(upstream_ota_tests(3,args))return 1;
    FILE *f=fopen(argv[2],"rb");if(!f)return 2;
    uint8_t raw[0x5000],image[0x5000];size_t n=fread(raw,1,sizeof raw,f);fclose(f);
    if(n<0x4400)return 2;
    for(size_t i=0;i<20;i++)memcpy(image+i*47,raw+i*48,47);
    memcpy(image+20*47,raw+20*48,n-20*48);
    /* Both pinned official FM-1 packages use twenty marker blocks. */
    uint32_t off=0;
    uint8_t h[0x400];memcpy(h,image,sizeof h);ota_jl_enc(h,0x40);
    for(uint32_t i=0;i<ota_rd16(h+8);i++){
        uint8_t *e=h+0x40+i*0x50;ota_jl_enc(e,0x50);
        if(!ota_rd16(e)){off=ota_rd32(e+8);break;}
    }
    if(!off||off+0x4000>n-20)return 2;
    reset_dev();memcpy(nor,image+off,0x4000);
    int rc=ota_session();
    return check("real other-version bootstrap: rejected before erase/reset",
                 rc==-5&&!committed&&!erases&&area_erased());
}
