/* SPDX-License-Identifier: GPL-3.0-only */
/* Preserve the note mapping made at note-on, even when scale, chord or the
 * selected track changes before note-off. A full table falls back to the raw
 * native input path, so no key silently disappears. */
static struct { uint8_t track,ch,raw,n,note[4]; } stattic_midi_held[24];
static void stattic_midi_drop(const track_t *t)
{
    uint32_t k=(uint32_t)(t-trk)+1u;
    for(uint32_t i=0;i<NELEM(stattic_midi_held);i++)if(stattic_midi_held[i].track==k)stattic_midi_held[i].track=0;
}
static int stattic_note_other_owner(uint32_t track,uint32_t note)
{
    for(uint32_t i=0;i<NELEM(stattic_midi_held);i++)if(stattic_midi_held[i].track==track+1u)
        for(uint32_t j=0;j<stattic_midi_held[i].n;j++)if(stattic_midi_held[i].note[j]==note)return 1;
    for(uint32_t k=0;k<27u;k++)if(kb_kind[k]==KS_NOTE&&kb_trk[k]==track)
        for(uint32_t j=0;j<kb_n[k];j++)if(kb_nt[k][j]==note)return 1;
    return 0;
}
static void stattic_midi_off(track_t *t,uint32_t ch,uint32_t raw)
{
    uint32_t k=trk_index(t),found=0;
    for(uint32_t i=0;i<NELEM(stattic_midi_held);i++){
        if(stattic_midi_held[i].track!=k+1u||stattic_midi_held[i].raw!=raw||
            (ch<16u&&stattic_midi_held[i].ch!=ch))continue;
        uint8_t notes[4];uint32_t n=stattic_midi_held[i].n;
        memcpy(notes,stattic_midi_held[i].note,4);stattic_midi_held[i].track=0;found=1;
        for(uint32_t j=0;j<n;j++)if(!stattic_note_other_owner(k,notes[j]))input_off(t,notes[j]);
    }
    if(!found&&!stattic_note_other_owner(k,raw))input_off(t,raw);
}
static uint32_t stattic_midi_pitch(const track_t *t,uint32_t raw)
{
    int32_t n=clamp((int32_t)raw+t->p[P_TRANS],0,127);
    if(t->p[P_QUANT]){
        uint32_t mask=scale_mask(t),root=(uint32_t)t->p[P_ROOT];
        while(n>0&&!((mask>>((n+120-(int32_t)root)%12))&1u))n--;
    }
    return (uint32_t)n;
}
static void stattic_midi_on(track_t *t,uint32_t ch,uint32_t raw,uint32_t vel)
{
    uint32_t at=NELEM(stattic_midi_held),k=trk_index(t);
    for(uint32_t i=0;i<NELEM(stattic_midi_held);i++){
        if(stattic_midi_held[i].track==k+1u&&stattic_midi_held[i].ch==ch&&stattic_midi_held[i].raw==raw){
            stattic_midi_off(t,ch,raw);at=i;break;
        }
        if(!stattic_midi_held[i].track)at=i;
    }
    if(at==NELEM(stattic_midi_held)){input_on(t,raw,vel);return;}
    uint32_t n=stattic_midi_pitch(t,raw);uint8_t notes[4]={(uint8_t)n};
    uint32_t count=t->p[P_CHORD]?chord_notes(t,n,notes):1u;
    stattic_midi_held[at].track=(uint8_t)(k+1u);stattic_midi_held[at].raw=(uint8_t)raw;
    stattic_midi_held[at].ch=(uint8_t)ch;stattic_midi_held[at].n=(uint8_t)count;
    memcpy(stattic_midi_held[at].note,notes,4);
    for(uint32_t j=0;j<count;j++)input_on(t,notes[j],vel);
}
