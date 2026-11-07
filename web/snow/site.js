// SPDX-License-Identifier: GPL-3.0-only
// The instrument panel is the editor. No duplicate parameter/preset builder.
let snowLegacy04=false;
function snowReady(schema){
 send({type:'snow-detail',on:false}); // avoid per-frame editor metadata in the audio worklet
 if(snowLegacy04){const index=schema?.params.findIndex(p=>p.name==='TARGET');if(index>=0)for(let track=0;track<6;track++)send({type:'snow-set',track,index,value:0});snowLegacy04=false;}
}
function snowFrame(state){
 if(state?.color===undefined)return;
 const colours=['#39dbf5','#709bff','#ff9850','#71e8a0','#eeeeee','#ebc75f','#f594c2','#ba96ff','#ff6a76','#7ce7ce'];
 document.documentElement.style.setProperty('--snow-accent',colours[state.color]||colours[0]);
}
