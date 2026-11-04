/* SPDX-License-Identifier: GPL-3.0-only
 * OCT+ on EDIT's first page: one undoable randomization of every parameter
 * exposed across EDIT, including the selected channel's complete effect rack.
 * The mask is derived from visible EDIT pages; sequencer/mixer/global values
 * and the other five instruments are outside this operation. Main loop only. */
static void snow_edit_mask(uint32_t *mask)
{
    memset(mask,0,((P_COUNT+31u)/32u)*sizeof *mask);
    for(uint32_t i=0;i<NPAGES;i++)if(PAGES[i].fam==FAM_EDIT&&page_visible(i))
        for(uint32_t c=0;c<4u;c++){
            uint32_t id=PAGES[i].id[c];if(id<P_COUNT)mask[id/32u]|=1u<<(id%32u);
        }
}
static int snow_random_sound(uint32_t channel)
{
    if(channel>=NTRK)return 1;
    uint32_t mask[(P_COUNT+31u)/32u];int16_t next[P_COUNT];track_t *t=&trk[channel];
    snow_edit_mask(mask);
    for(uint32_t id=0;id<P_COUNT;id++)next[id]=motion_base_value(t,id);
    for(uint32_t id=0;id<P_COUNT;id++)if((mask[id/32u]>>(id%32u))&1u){
        const param_desc_t *d=id>=P_IA&&id<=P_IC?ins_desc(next[P_ITYPE],id-P_IA):track_desc(t,id);
        uint32_t range=(uint32_t)(d->max-d->min+1);
        next[id]=(int16_t)(d->min+(int32_t)(rng()%range));
    }
    undo.keep=0;undo_label="RANDOM SOUND";load_begin(t,UNDO_SOUND);
    uint32_t guard=motion_guard();
    for(uint32_t id=0;id<P_COUNT;id++)if((mask[id/32u]>>(id%32u))&1u)t->p[id]=next[id];
    motion_unguard(guard);load_end(t);
    snow_touch(t,P_E0,t->p[P_E0]);ui.force=1;return 0;
}
