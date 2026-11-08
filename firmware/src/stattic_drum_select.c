/* SPDX-License-Identifier: GPL-3.0-only */
/* A catalog ID identifies a shot, not a key position. Category checks apply
 * to load, generation and playback; missing user data falls back to factory. */
static int32_t stattic_shot_zone(uint32_t shot,uint32_t category)
{
    static const uint8_t note[4]={36,38,42,60};
    if(shot<ST_FACTORY_SHOTS)return shot/10u==category?(int32_t)shot:-1;
    if(shot<ST_USER_SHOT||shot>=ST_USER_SHOT+64u)return -1;
    uint32_t user=shot-ST_USER_SHOT,slot=user/16u,zone=user%16u;
    if(!stattic_user_catalog[slot]||zone>=usr_nz[slot])return -1;
    const smp_zone_t *z=&usr_zone[slot][zone];
    return z->n>=22u&&z->lo==note[category]&&z->hi==note[category]?(int32_t)(DZ_USR+user):-1;
}
static void stattic_drum_random(uint8_t *picks,uint32_t *state)
{
    for(uint32_t lane=0;lane<16;lane++){
        uint8_t valid[74];uint32_t count=0,cat=ST_LANE_CATEGORY[lane];
        for(uint32_t shot=0;shot<128;shot++)if(stattic_shot_zone(shot,cat)>=0)valid[count++]=(uint8_t)shot;
        uint32_t x=*state;x^=x<<13;x^=x>>17;x^=x<<5;*state=x?x:0x6d2b79f5u;
        uint32_t pick=*state%count;
        if(count>1u&&valid[pick]==picks[lane])pick=(pick+1u)%count;
        picks[lane]=valid[pick];
    }
}
