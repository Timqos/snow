// SPDX-License-Identifier: GPL-3.0-only
import fs from "node:fs";
import assert from "node:assert/strict";
const bytes = fs.readFileSync(process.argv[2]);
async function boot(flash) {
  const {instance} = await WebAssembly.instantiate(bytes, {});
  const x=instance.exports; x._initialize(); x.web_nor_erase(); if(flash)new Uint8Array(x.memory.buffer,x.web_nor(),x.web_nor_size()).set(flash); x.web_boot(); return x;
}
function render(x,ms) { let peak=0, hash=0; for(let i=0;i<ms*44.1;i+=128) { x.web_render(128); for(const s of new Float32Array(x.memory.buffer,x.web_out_l(),128)) { assert(Number.isFinite(s)); peak=Math.max(peak,Math.abs(s)); hash=(Math.imul(hash,31)+Math.round(s*32768))|0; } } return {peak,hash}; }
const splash=await boot();const screen=x=>Array.from(new Uint16Array(x.memory.buffer,x.web_screen(),240*240));
render(splash,300);const forming=screen(splash);render(splash,650);const complete=screen(splash);
assert.notDeepEqual(forming,complete);render(splash,450);assert.deepEqual(screen(splash),complete);
render(splash,300);assert.notDeepEqual(screen(splash),complete);
console.log('WASM BOOT PASSED: animated logo, readable hold and transition to the instrument');
const x=await boot(); render(x,1800); x.web_snow_demo();
const values=Array.from({length:6},(_,t)=>x.web_snow_value(t,0));
x.web_snow_save(); x.web_snow_mask(62); x.web_snow_control(0,10);
assert.equal(x.web_snow_value(0,0),values[0]);
assert.equal(x.web_snow_value(1,0),values[1]+10);
assert.equal(x.web_snow_reload(63),1);
assert.deepEqual(Array.from({length:6},(_,t)=>x.web_snow_value(t,0)),values);
x.web_buttons(1<<10); render(x,70); x.web_buttons(0); render(x,70);
assert.equal(x.web_playing(),1);
const result=render(x,4000); assert(result.peak>0.01 && result.peak<1);
x.web_snow_control(0,25); assert.equal(x.web_snow_reload(63),1); assert.equal(x.web_playing(),1);
const y=await boot(); render(y,1800); y.web_snow_demo();
y.web_buttons(1<<10); render(y,70); y.web_buttons(0); render(y,70);
assert.deepEqual(render(y,4000),result);
console.log("SNOW WASM MIX PASSED: demo, six-machine mix, mask, Control All, RAM reload while playing, deterministic audio; peak",result.peak);

// Exercise the actual browser-to-firmware adapter, including metadata in ready.
const count=x.web_snow_param_count();assert(count>100);
const str=(ptr)=>{const m=new Uint8Array(x.memory.buffer);let end=ptr;while(m[end])end++;return new TextDecoder().decode(m.subarray(ptr,end));};
const find=(group,name)=>Array.from({length:count},(_,i)=>i).find(i=>str(x.web_snow_param_group(i))===group&&str(x.web_snow_param_name(0,i))===name);
const alg=find('OSCILLATORS / ALGORITHM','ALGORITHM'),wave=find('OSCILLATORS / ALGORITHM','WAVE 1');
const attack=find('AMP ADSR','ATTACK'),density=find('GENERATIVE SEQUENCER','DENSITY');
assert(alg>=0&&wave>=0&&attack>=0&&density>=0);
assert.equal(str(x.web_snow_param_enum(0,alg,5)),'FOLD');
assert.equal(x.web_snow_preset(0,8),0); // Instrument preset 9 remains a kick on channel 1.
assert.equal(x.web_snow_set(0,alg,99),0);assert.equal(x.web_snow_param_value(0,alg),5);
x.web_snow_set(0,wave,2);x.web_snow_set(0,attack,75);
assert.equal(x.web_snow_param_value(0,wave),2);assert.equal(x.web_snow_param_value(0,attack),75);
assert(x.web_snow_param_text(0,attack)>0);
x.web_snow_set(0,density,50);assert.equal(x.web_snow_generate(0,1),0);
assert.equal(x.web_snow_pattern(0,1),0);assert.equal(x.web_snow_pattern_get(0),1);
assert.equal(x.web_snow_pattern_get(1),0);assert.equal(x.web_snow_pattern(6,0),1);
assert.equal(x.web_snow_set(6,alg,1),1);assert.equal(x.web_snow_set(0,count,1),1);
const changed=render(x,1500);assert(changed.peak>0&&changed.peak<1);
let Processor;globalThis.registerProcessor=(name,klass)=>{Processor=klass;};
globalThis.AudioWorkletProcessor=class {constructor(){this.messages=[];this.port={postMessage:(m)=>this.messages.push(m)};}};
await import('../web/emu/worklet.js');
const worklet=new Processor();
await worklet.onMessage({type:'load',wasm:bytes.buffer.slice(bytes.byteOffset,bytes.byteOffset+bytes.byteLength)});
const ready=worklet.messages.find(m=>m.type==='ready');
assert.equal(ready.snowSchema.params.length,count);assert.equal(ready.snowSchema.presets.length,10);
await worklet.onMessage({type:'snow-track',track:2});
await worklet.onMessage({type:'snow-set',track:2,index:alg,value:3});
assert.equal(worklet.ex.web_snow_param_value(2,alg),3);
await worklet.onMessage({type:'snow-generate',track:2,vary:true});
await worklet.onMessage({type:'snow-pattern',track:2,slot:2});
worklet.publish();const frame=worklet.messages.findLast(m=>m.type==='frame');
assert.equal(frame.snow.selected,2);assert.equal(frame.snow.patterns[2],2);assert.equal(frame.snow.values[alg],3);
assert.equal(frame.snow.text.length,count);
console.log('SNOW 0.6 WASM + WORKLET PASSED:',count,'editable parameters, algorithms, waveforms, ADSR, generation, independent patterns and control messages');

assert.equal(str(x.web_snow_role(0)),'KICK');assert.equal(str(x.web_snow_preset_name(0,8)),'SOFT LOW');
assert.equal(str(x.web_snow_preset_name(1,8)),'BRUSH');assert.equal(x.web_snow_preset(0,10),1);
const select=worklet.messages.filter(m=>m.type==='snow-schema').at(-1);
await worklet.onMessage({type:'snow-schema'});const schema=worklet.messages.at(-1).schema;
assert.equal(schema.role,'HAT');assert.equal(schema.presets.length,10);assert.equal(schema.colors.length,10);
for(const group of ['CHANNEL DELAY','CHANNEL REVERB','CHANNEL CHORUS','TAPE','BITCRUSH'])assert(schema.params.some(p=>p.group===group));
await worklet.onMessage({type:'snow-scene',vary:true});
await worklet.onMessage({type:'snow-genparm',all:true});
worklet.publish();const scene=worklet.messages.at(-1).snow;assert.equal(scene.used.length,6);assert.equal(scene.active.length,6);
console.log('SNOW 0.6 ROLES / FX / RANDOM PASSED: 60 role-specific presets, per-channel FX controls, random phrases, auto locks and pattern grid metadata');
// Simultaneous racks run in the same firmware audio path as the browser and device.
const z=await boot();render(z,1800);z.web_snow_demo();
function parameter(group,name){return Array.from({length:z.web_snow_param_count()},(_,i)=>i).find(i=>{
 const mem=new Uint8Array(z.memory.buffer);const read=p=>{let e=p;while(mem[e])e++;return new TextDecoder().decode(mem.subarray(p,e));};
 return read(z.web_snow_param_group(i))===group&&read(z.web_snow_param_name(0,i))===name;
});}
const rack=[['CHANNEL DELAY','DELAY',85],['CHANNEL DELAY','TIME',73],['CHANNEL DELAY','FEEDBACK',110],
 ['CHANNEL REVERB','REVERB',90],['CHANNEL REVERB','DECAY',115],['CHANNEL CHORUS','CHORUS',65],
 ['TAPE','MIX',80],['TAPE','WOW',100],['BITCRUSH','MIX',35],['BITCRUSH','BITS',7],['BITCRUSH','HOLD',5]];
for(let ch=0;ch<6;ch++)for(const [group,name,value] of rack){const i=parameter(group,name);assert(i>=0,`${group}/${name}`);assert.equal(z.web_snow_set(ch,i,value),0);}
z.web_buttons(1<<10);render(z,70);z.web_buttons(0);render(z,70);
const racks=render(z,8000);assert(racks.peak>0.02&&racks.peak<1);
console.log('SIX CONCURRENT FX RACKS PASSED: finite bounded audio, peak',racks.peak);
// Value-linked UI and full EDIT randomization through the production adapter.
const target=find('GENERATIVE SEQUENCER','TARGET');assert(target>=0);
const shortNames=['KK','SN','HH','PR','BS','CH'];
for(let ch=0;ch<6;ch++){
 assert.equal(str(x.web_snow_param_enum(ch,target,0)),shortNames[ch]);
 assert.equal(str(x.web_snow_param_enum(ch,target,1)),'ALL');
}
const v=await boot();render(v,1800);v.web_snow_demo();
const beforeGrid=Array.from(new Uint16Array(v.memory.buffer,v.web_snow_grid(1),64));
const other=Array.from({length:count},(_,i)=>v.web_snow_param_value(0,i));
const old=Array.from({length:count},(_,i)=>v.web_snow_param_value(1,i));
assert.equal(v.web_snow_random_sound(1),0);
const changedSound=Array.from({length:count},(_,i)=>v.web_snow_param_value(1,i));
assert.notDeepEqual(changedSound,old);
assert.deepEqual(Array.from({length:count},(_,i)=>v.web_snow_param_value(0,i)),other);
assert.deepEqual(Array.from(new Uint16Array(v.memory.buffer,v.web_snow_grid(1),64)),beforeGrid);
assert.equal(v.web_snow_random_sound(6),1);
await worklet.onMessage({type:'snow-track',track:3});
await worklet.onMessage({type:'snow-random-sound',track:3});
assert.equal(worklet.messages.at(-1).type,'snow-schema');
assert.equal(worklet.messages.at(-1).schema.params.find(p=>p.name==='TARGET').options[0],'PR');
// Stress random sounds while the native sequence and UI are running.
v.web_buttons(1<<10);render(v,70);v.web_buttons(0);render(v,70);
for(let i=0;i<24;i++){assert.equal(v.web_snow_random_sound(i%6),0);const a=render(v,120);assert(a.peak<=1);}
assert.equal(v.web_playing(),1);
console.log('SNOW 0.6 RANDOM EDIT / TARGET PASSED: 96 EDIT parameters, role labels, unchanged sequence/other channels, live random-patch audio');
// The device-only page does not request the removed editor's large metadata.
await worklet.onMessage({type:'snow-detail',on:false});
worklet.sentDraws=-1;worklet.sentLeds='';worklet.publish();
const lean=worklet.messages.at(-1);
assert.deepEqual(Object.keys(lean.snow).sort(),['color','mask','selected']);
assert.equal(lean.fb.length,240*240);assert(lean.leds.length>=8);
await worklet.onMessage({type:'snow-detail',on:true});worklet.publish();
assert.equal(worklet.messages.at(-1).snow.values.length,count);
console.log('DEVICE-ONLY WORKLET PASSED: native screen/LEDs/theme retained, unused editor metadata disabled');
// The actual printed LFO / FX buttons and OCT+, with all six tracks playing.
const section=await boot();render(section,1800);section.web_snow_demo();
const gesture=b=>{section.web_buttons(1<<b);render(section,70);section.web_buttons(0);render(section,70);};
gesture(10);assert.equal(section.web_playing(),1);
for(const button of [3,0]){
 gesture(button);
 for(let ch=0;ch<6;ch++){
  section.web_snow_select(ch);
  for(let press=0;press<8;press++){
   const other=Array.from({length:6},(_,k)=>Array.from({length:count},(_,i)=>section.web_snow_param_base(k,i)));
   const grids=Array.from({length:6},(_,k)=>Array.from(new Uint16Array(section.memory.buffer,section.web_snow_grid(k),64)));
   gesture(13);
   assert.notDeepEqual(Array.from({length:count},(_,i)=>section.web_snow_param_base(ch,i)),other[ch]);
   for(let k=0;k<6;k++){
    if(k!==ch)assert.deepEqual(Array.from({length:count},(_,i)=>section.web_snow_param_base(k,i)),other[k]);
    assert.deepEqual(Array.from(new Uint16Array(section.memory.buffer,section.web_snow_grid(k),64)),grids[k]);
   }
   const audio=render(section,180);assert(audio.peak<1);assert.equal(section.web_playing(),1);
  }
 }
}
console.log('PHYSICAL LFO / FX RANDOM PASSED: 96 repeated OCT+ gestures during six-track playback, isolated channels, unchanged notes, finite bounded audio');

// First SEL page uses the physical encoders; CH release survives all random sections.
const harmony=await boot();render(harmony,1800);harmony.web_snow_select(5);
const mode=find('VOICE / MIX','MODE'),scale=find('GENERATIVE SEQUENCER','SCALE'),release=find('AMP ADSR','RELEASE');
assert(mode>=0&&scale>=0&&release>=0);
assert.equal(harmony.web_snow_param_max(5,scale),34);
assert.equal(harmony.web_snow_param_base(5,mode),0);
const press=b=>{harmony.web_buttons(1<<b);render(harmony,70);harmony.web_buttons(0);render(harmony,70);};
press(1); // SEL, page 1: MODE / ROOT / SCALE / QUANTIZE.
harmony.web_enc(3,1);render(harmony,90);assert.equal(harmony.web_snow_param_base(5,mode),1);
harmony.web_snow_save();harmony.web_enc(3,-1);render(harmony,90);assert.equal(harmony.web_snow_param_base(5,mode),0);
assert.equal(harmony.web_snow_reload(63),1);assert.equal(harmony.web_snow_param_base(5,mode),1);
for(let preset=0;preset<10;preset++){
 assert.equal(harmony.web_snow_preset(5,preset),0);
 assert.equal(harmony.web_snow_param_base(5,release),113);
 assert.equal(harmony.web_snow_param_base(5,mode),1);
 for(let i=0;i<20;i++){
  assert.equal(harmony.web_snow_random_sound(5),0);
  assert.equal(harmony.web_snow_param_base(5,release),113);
  assert.equal(harmony.web_snow_param_base(5,mode),1);
 }
}
console.log('WASM SEL / CH RELEASE PASSED: 35 scales, physical mode knob, snapshot recall, 200 sound randomisations keep 2BAR and NOTES');

// The production WebAssembly uses the actual HOME controls, not an external editor.
const home=await boot();render(home,1800);
const rotate=(encoder,n)=>{home.web_enc(encoder,n);render(home,100);};
const tempo=home.web_snow_tempo_get();rotate(0,2);assert.equal(home.web_snow_home_panel(),2);assert.equal(home.web_snow_tempo_get(),tempo);
rotate(0,-1);assert.equal(home.web_snow_home_panel(),1);
for(let ch=0;ch<6;ch++){
 home.web_snow_select(ch);assert.equal(home.web_snow_home_panel(),1);
 for(let knob=0;knob<4;knob++){
  const before=home.web_snow_home_generations(ch,knob),other=Array.from({length:6},(_,k)=>home.web_snow_home_generations(k,knob));
  rotate(3+knob,knob%2?-2:2);assert.equal(home.web_snow_home_generations(ch,knob),before+2);
  for(let k=0;k<6;k++)if(k!==ch)assert.equal(home.web_snow_home_generations(k,knob),other[k]);
 }
}
home.web_snow_select(5);home.web_buttons(1<<6);render(home,850);home.web_buttons((1<<6)|(1<<13));render(home,400);home.web_buttons(1<<6);render(home,70);home.web_buttons(0);render(home,100);
assert.equal(home.web_snow_param_base(5,mode),1);assert.equal(home.web_snow_home_panel(),1);
home.web_buttons(1<<6);render(home,70);home.web_buttons((1<<6)|(1<<13));render(home,70);home.web_buttons(1<<6);render(home,70);home.web_buttons(0);render(home,100);assert.equal(home.web_snow_param_base(5,mode),0);
assert.equal(home.web_snow_tempo_get(),tempo);
console.log('WASM HOME PASSED: three SELECT panels, exact detent counts for four generators on six channels, preserved focus/BPM and HOME+OCT+ N/CHO');

// A real persisted old session must not override CH's required startup release.
{
const saved=await boot();render(saved,1800);
const release=find('AMP ADSR','RELEASE'),sus=find('AMP ADSR','SUSTAIN');
assert(release>=0&&sus>=0);
const twoBar=saved.web_snow_param_value(5,release);
saved.web_snow_set(5,release,17);saved.web_snow_set(0,release,29);saved.web_snow_set(5,sus,93);
render(saved,4000);assert(saved.web_flash_writes_count()>0);
const restored=await boot(new Uint8Array(saved.memory.buffer,saved.web_nor(),saved.web_nor_size()).slice());render(restored,1800);
assert.equal(restored.web_snow_param_value(5,release),twoBar);
assert.equal(restored.web_snow_param_value(0,release),29);
assert.equal(restored.web_snow_param_value(5,sus),93);
for(let i=0;i<30;i++){restored.web_snow_random_sound(5);assert.equal(restored.web_snow_param_value(5,release),twoBar);}
restored.web_snow_set(5,release,103);restored.web_snow_random_sound(5);assert.equal(restored.web_snow_param_value(5,release),103);
console.log('CH STARTUP RELEASE PASSED: actual persisted session reboot restores 2BAR on CH only; EDIT random keeps release and manual edits remain available.');
}
