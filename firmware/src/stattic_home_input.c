/* SPDX-License-Identifier: GPL-3.0-only */
/* HOME owns its encoders. Other buttons retain the native SLOOP screens/layers. */
static struct { uint32_t at;uint8_t armed,used; } stattic_oct;
static void stattic_home_input(uint32_t *pressed)
{
    uint32_t up=1u<<panel.btn[B_OCTUP],dn=1u<<panel.btn[B_OCTDN];int32_t s;
    if(stattic_home_page==0u&&song.sel==2u){
        if(*pressed&up){stattic_oct.at=fm1_ms;stattic_oct.armed=1;stattic_oct.used=0;}
        *pressed&=~up;
        if(stattic_oct.armed){
            if((fm1_in.buttons&up)&&!stattic_oct.used&&fm1_ms-stattic_oct.at>=600u){
                uint32_t f=STATTIC_GUARD_SAVE();
                stattic_undo_mark(TSEL,(stattic_action+=4u)|3u);
                TSEL->p[SN_PLAY_MODE]^=1;TSEL->p[P_CHORD]=TSEL->p[SN_PLAY_MODE]?0:1;
                panic_req|=(uint8_t)(1u<<song.sel);stattic_oct.used=1;stattic_touch(TSEL,SN_PLAY_MODE);
                STATTIC_GUARD_RESTORE(f);
            }
            if(!(fm1_in.buttons&up)){
                if(!stattic_oct.used&&song.octave<3)song.octave++;
                stattic_oct.armed=0;
            }
        }
    }else stattic_oct.armed=0;
    if(stattic_home_page==1u){
        if(*pressed&dn)stattic_clear(0);
        if(*pressed&up)stattic_clear(1);
        *pressed&=~(dn|up);
    }else if(stattic_home_page==3u){
        if(*pressed&(dn|up)){
            stattic_pattern_random();
        }
        *pressed&=~(dn|up);
    }
    if((s=panel_enc(EN_SELECT))!=0){
        stattic_home_page=(uint8_t)clamp((int32_t)stattic_home_page+s,0,4);
        stattic_oct.armed=0;ui.force=1;
    }
    if((s=panel_enc(EN_ALGO))!=0){
        uint32_t role=stattic_home_role(song.sel);
        track_select(stattic_role_track((uint32_t)clamp((int32_t)role+(s>0?1:-1),0,3)));
    }
    if((s=panel_enc(EN_PRESET))!=0&&stattic_home_page==0u){
        if(is_drum(TSEL)){
            (void)stattic_prepare(TRK_DRUM,1);
        }else{
            uint32_t f=STATTIC_GUARD_SAVE();stattic_undo_mark(TSEL,(stattic_action+=4u)|3u);
            int32_t n=ENGINES[TSEL->eng_req%NENGINES]->npresets;
            apply_preset_to(TSEL,(uint32_t)(((int32_t)TSEL->preset+s%n+n)%n));STATTIC_GUARD_RESTORE(f);
        }
        stattic_ui_dirty=1;stattic_revision++;
    }
    for(uint32_t knob=0;knob<4;knob++)if((s=panel_enc(EN_K1+knob))!=0){
        stattic_turn(knob,s);uint32_t k=song.sel;
        if(stattic_home_page==0u){
            uint32_t id=is_drum(TSEL)?P_E1+knob:P_E0+knob;
            const param_desc_t *d=track_desc(TSEL,id);uint32_t f=STATTIC_GUARD_SAVE();
            TSEL->p[id]=(int16_t)clamp(TSEL->p[id]+s,d->min,d->max);stattic_touch(TSEL,id);STATTIC_GUARD_RESTORE(f);
        }else if(stattic_home_page==1u){
            stattic_prepare_scope(k,knob);
        }
        else if(stattic_home_page==2u){
            static const uint8_t ids[]={P_LEVEL,P_PAN,SN_DJ_FILTER,SN_WIDTH};uint32_t id=ids[knob];
            const param_desc_t *d=track_desc(TSEL,id);uint32_t f=STATTIC_GUARD_SAVE();
            TSEL->p[id]=(int16_t)clamp(TSEL->p[id]+s,d->min,d->max);stattic_touch(TSEL,id);STATTIC_GUARD_RESTORE(f);
        }else if(stattic_home_page==3u){
            uint32_t track=stattic_role_track(knob),slot=stattic_pattern_current(track);
            (void)stattic_pattern_select(track,(uint32_t)clamp((int32_t)slot+s,0,2));
        }else if(knob==0u){
            uint32_t role=stattic_home_role(k);track_select(stattic_role_track((uint32_t)clamp((int32_t)role+s,0,3)));
        }else if(knob==1u){TSEL->p[P_LEVEL]=(int16_t)clamp(TSEL->p[P_LEVEL]+s,0,127);stattic_touch(TSEL,P_LEVEL);}
    }
    enc_hold=(1u<<NE)-1u;
}

static void stattic_performance_input(uint32_t *pressed,uint32_t *home)
{
    uint32_t buttons=fm1_in.buttons,glo=1u<<panel.btn[B_GLO];
    if(ui.menu||ui.confirm||!(buttons&glo))return;
    uint32_t save=1u<<panel.btn[B_SAVE],hm=1u<<panel.btn[B_HOME],up=1u<<panel.btn[B_OCTUP],dn=1u<<panel.btn[B_OCTDN];
    uint32_t actions=*pressed&(save|hm|up|dn);
    if(!(buttons&(1u<<panel.btn[B_EDIT])))actions&=~dn;
    if(actions){
        ui.layer_used=1;ui.layer_t0=fm1_ms;*pressed&=~actions;
        if(actions&save){stattic_temp_save();ui.hold_kind=0;ui_message("TEMP SAVED");}
        if(actions&hm){ui.home_t0=0;*home=0;ui_message(stattic_temp_load(15u)?"TEMP NEXT BAR":"NO TEMP SAVE");}
        if(actions&up)ui_message(stattic_temp_load(1u<<song.sel)?"TRACK NEXT BAR":"NO TEMP SAVE");
        if(actions&dn){stattic_control_mask^=(uint8_t)(1u<<song.sel);ui_message((stattic_control_mask>>song.sel)&1u?"CONTROL ALL: IN":"CONTROL ALL: OUT");}
    }
    if(buttons&(1u<<panel.btn[B_EDIT]))for(uint32_t k=0;k<4;k++){
        int32_t delta=panel_enc(EN_K1+k);if(delta){stattic_control_all(k,delta);if(ui.home)stattic_turn(k,delta);ui.layer_used=1;}
        enc_hold|=1u<<(EN_K1+k);
    }
    int32_t delta=panel_enc(EN_SELECT);
    if(delta){song.g[G_BPM]=(int16_t)clamp(song.g[G_BPM]+delta,GP[G_BPM].min,GP[G_BPM].max);ui.bpm_t=40;ui.layer_used=1;}
    enc_hold|=1u<<EN_SELECT;
}
