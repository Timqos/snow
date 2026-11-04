// SPDX-License-Identifier: GPL-3.0-only
import fs from "node:fs";
import assert from "node:assert/strict";
const bytes = fs.readFileSync(process.argv[2]);
async function boot() {
  const {instance} = await WebAssembly.instantiate(bytes, {});
  const x=instance.exports; x._initialize(); x.web_nor_erase(); x.web_boot(); return x;
}
function render(x,ms) { let peak=0, hash=0; for(let i=0;i<ms*44.1;i+=128) { x.web_render(128); for(const s of new Float32Array(x.memory.buffer,x.web_out_l(),128)) { assert(Number.isFinite(s)); peak=Math.max(peak,Math.abs(s)); hash=(Math.imul(hash,31)+Math.round(s*32768))|0; } } return {peak,hash}; }
const x=await boot(); render(x,1000); x.web_snow_demo();
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
const y=await boot(); render(y,1000); y.web_snow_demo();
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
const z=await boot();render(z,1000);z.web_snow_demo();
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
const v=await boot();render(v,1000);v.web_snow_demo();
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
