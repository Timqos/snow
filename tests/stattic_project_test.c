/* SPDX-License-Identifier: GPL-3.0-only */
#define STATTIC_RUNTIME_HARNESS_ONLY
#include "stattic_runtime_test.c"
#define PROJ_HOST 1
static uint32_t trk_def_engine(uint32_t k) { const uint8_t e[]={4,6,5};return k<3?e[k]:0; }
#include "../firmware/src/project.c"
int main(void)
{
    init();
    for(uint32_t k=0;k<NTRK;k++) {
        for(uint32_t slot=0;slot<3;slot++) {
            if(k<NPART)note(&stattic_banks[k][slot].step[63],40+k*12+slot);
            else dstep_set(&stattic_banks[k][slot].drum[63],slot,LV_HARD,0);
        }
        memcpy(trk[k].step,stattic_banks[k][0].step,sizeof trk[k].step);
        stattic_pattern_select(k,k%3);stattic_runtime_tick();
        snow_offset[k][0]=-37;snow_offset[k][1]=24;trk[k].p[P_PAN]=-31;
        trk[k].p[SN_L2_DEPTH]=-21;trk[k].p[SN_DJ_FILTER]=-15;
    }
    trk[3].p[P_E1]=90;trk[3].p[P_E2]=45;trk[3].p[P_E3]=64;trk[3].p[P_E4]=70;
    for(uint32_t i=0;i<16;i++)stattic_drum_pick[i]=(uint8_t)(ST_LANE_CATEGORY[i]*10u+8);
    drum_tune[0]=-3;drum_dec[1]=2;stattic_control_mask=5;
    project_t p,q;proj_capture(&p);assert(proj_ok(&p));assert(sizeof p<=12032);
    init();assert(proj_import(&q,&p,sizeof p));proj_apply(&q,1);
    for(uint32_t k=0;k<NTRK;k++) {
        assert(stattic_current[k]==k%3);assert(snow_offset[k][0]==-37&&snow_offset[k][1]==24);
        assert(trk[k].p[P_PAN]==-31&&trk[k].p[SN_L2_DEPTH]==-21&&trk[k].p[SN_DJ_FILTER]==-15);
        for(uint32_t slot=0;slot<3;slot++) {
            assert(!stattic_pattern_select(k,slot));stattic_runtime_tick();
            if(k<NPART)assert(trk[k].step[63].note[0]==40+k*12+slot);
            else assert(dstep_mask(&trk[k].dstep[63])==(1u<<slot));
        }
    }
    assert(trk[3].p[P_E1]==90&&trk[3].p[P_E2]==45&&trk[3].p[P_E3]==64&&trk[3].p[P_E4]==70);
    assert(drum_tune[0]==-3&&drum_dec[1]==2&&stattic_control_mask==5);
    for(uint32_t i=0;i<16;i++)assert(stattic_drum_pick[i]==ST_LANE_CATEGORY[i]*10u+8);
    p.magic=PROJ_MAGIC_V5;p.sum=proj_sum(&p);assert(!proj_import(&q,&p,sizeof p));
    p.magic=PROJ_MAGIC;p.sum=proj_sum(&p);p.t[0].step[0].vel^=1;assert(!proj_import(&q,&p,sizeof p));
    puts("project: all 12 patterns, shot selection, macros, signed parameters, control offsets; legacy/corruption rejected");
    return 0;
}
