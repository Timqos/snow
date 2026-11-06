/* SPDX-License-Identifier: GPL-3.0-only */
/* The real Snow mixer on one host thread versus the existing Skiff second-
 * core worker. Equal voice budgets isolate scheduling from voice stealing;
 * the production two-core budget remains unchanged. */
#include "c1_host.h"
#define SNOW_RUNTIME_LIBRARY 1
#include "snow_runtime_test.c"

typedef struct { uint32_t hash,peak,jobs; } dual_result_t;

static dual_result_t render_scene(uint32_t scene,int dual,const char *mask)
{
    dual_result_t r={2166136261u,0,0};
    c1h_off=!dual;
    if(mask)setenv("C1_MASK",mask,1);
    web_nor_erase();web_boot();advance(SNOW_BOOT_MS+200);
    web_snow_demo();
    if(scene)for(uint32_t k=0;k<NTRK;k++)assert(!snow_random_sound(k));
    if(scene==2){
        for(uint32_t k=0;k<NTRK;k++){assert(!snow_pattern_select(k,1));assert(!snow_generate(k,1));assert(!snow_pattern_select(k,0));}
        go_home();snow_home_panel=SN_HOME_MS;
    }
    tap(B_PLAY);assert(song.playing);
    for(uint32_t b=0;b<1400;b++){
        if(scene==2&&b==300){for(uint32_t k=0;k<NTRK;k++)assert(!snow_pattern_select(k,1));layer_keys((1u<<2)|(1u<<13));}
        if(scene==2&&b==1000)layer_keys((1u<<2)|(1u<<13));
        if(b==600){
            /* The other core has finished this half before panel events. */
            for(uint32_t k=0;k<NTRK;k++)trk[k].p[SN_FX_BYPASS]=1;
        }
        if(b==660)for(uint32_t k=0;k<NTRK;k++)trk[k].p[SN_FX_BYPASS]=0;
        web_render(128);
        const float *left=web_out_l(),*right=web_out_r();
        for(uint32_t i=0;i<128;i++){
            uint32_t x,y;memcpy(&x,&left[i],4);memcpy(&y,&right[i],4);
            r.hash=pcm_hash(pcm_hash(r.hash,(int32_t)x),(int32_t)y);
            uint32_t a=(uint32_t)(fabs(left[i])*32768.0f);if(a>r.peak)r.peak=a;
            assert(isfinite(left[i])&&isfinite(right[i])&&fabs(left[i])<1&&fabs(right[i])<1);
        }
    }
    r.jobs=c1h_taken;
    if(scene==2){for(uint32_t k=0;k<NTRK;k++)assert(web_snow_pattern_active(k)==1);assert(!perf_solo&&!trk[1].p[P_MUTE]);}
    return r;
}

static dual_result_t fork_scene(uint32_t scene,int dual,const char *mask)
{
    int fd[2];assert(!pipe(fd));
    pid_t child=fork();assert(child>=0);
    if(!child){
        close(fd[0]);dual_result_t r=render_scene(scene,dual,mask);
        assert(write(fd[1],&r,sizeof r)==sizeof r);_exit(0);
    }
    close(fd[1]);dual_result_t r={0};
    assert(read(fd[0],&r,sizeof r)==sizeof r);close(fd[0]);
    int status;assert(waitpid(child,&status,0)==child&&WIFEXITED(status)&&!WEXITSTATUS(status));
    assert(r.hash&&r.peak>100);
    return r;
}

int main(void)
{
    setvbuf(stdout,0,_IOLBF,0);
    static const char *const splits[]={"1","7","21","42","62","rot","auto"};
    for(uint32_t scene=0;scene<3;scene++){
        dual_result_t reference=fork_scene(scene,0,0);
        for(uint32_t m=0;m<NELEM(splits);m++){
            dual_result_t got=fork_scene(scene,1,splits[m]);
            assert(got.hash==reference.hash&&got.peak==reference.peak&&got.jobs>0);
        }
        printf("Snow scene %u: one/two-core audio identical over seven real worker splits, including auto and FX bypass/re-enable\n",scene);
    }
    puts("SNOW DUAL-CORE MIXER PASSED (host threads, not physical CPU timing)");
    return 0;
}
