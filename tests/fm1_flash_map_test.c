/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>
#include "../firmware/hal/fm1_flash_map.h"
int main(void)
{
    unsigned accepted=0;
    for(uint32_t off=0;off<0x100000u;off+=0x1000u)if(FL_GUARD(off,0x1000u)){
        accepted++;
        assert(off>=0x97000u); /* application writes cannot enter bootstrap or code */
        assert(off!=0xe9000u&&off<0xff000u); /* BTIF and final reserved sector */
    }
    assert(accepted>0);
    assert(!FL_GUARD(0,0x1000u)&&!FL_GUARD(0x4000u,0x1000u));
    assert(!FL_GUARD(0x96fffu,2)&&!FL_GUARD(0xe8fffu,2)&&!FL_GUARD(0xfefffu,2));
    assert(!FL_GUARD(0xffffffffu,2)&&!FL_GUARD(0x97000u,0xffffffffu));
    assert(FL_JEDEC_OK(0x856014u)&&FL_JEDEC_OK(0xb36014u));
    assert(!FL_JEDEC_OK(0)&&!FL_JEDEC_OK(0xffffffu));
    printf("FM-1 FLASH MAP PASSED: %u allowed sectors, bootstrap/code/BTIF/reserved/overflow blocked\n",accepted);
    return 0;
}
