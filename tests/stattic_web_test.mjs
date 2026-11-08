// SPDX-License-Identifier: GPL-3.0-only
import assert from 'node:assert/strict';
import fs from 'node:fs';
import {SMP,imaEncode,readSlot,buildEncodedSlot,crc32,parseWav} from '../web/stattic/shared/samples.mjs';
import {capabilities,uploadSlot,captureNativeBackup,validateBackup,restoreBackup} from '../web/stattic/shared/midi.mjs';
const module=new WebAssembly.Module(fs.readFileSync('build/stattic-web/stattic.wasm'));assert.deepEqual(WebAssembly.Module.imports(module),[]);
const w=new WebAssembly.Instance(module,{}).exports;w._initialize?.();w.stattic_sim_init();
let requests=0;
const link={call:async(cmd,args=[])=>{requests++;const req=Uint8Array.from([125,70,76,cmd,...args]);assert(req.length<=1024);new Uint8Array(w.memory.buffer,w.stattic_sim_request_buffer(),req.length).set(req);const n=w.stattic_sim_request(req.length);assert(n>5,`Firmware command ${cmd} gave no reply`);return Array.from(new Uint8Array(w.memory.buffer,w.stattic_sim_response_buffer()+5,n-6));}};
const caps=await capabilities(link);assert.equal(caps.version,'STATTIC 0.9.0-S1');assert(caps.parameters>127&&caps.engineBase>127);assert.equal(caps.playing,false);
const v14=v=>[v+8192&127,(v+8192)>>7&127],r14=a=>(a[0]|a[1]<<7)-8192;
const id=caps.engineBase;
await link.call(79,[0,id&127,id>>7,1,...v14(100)]);let param=await link.call(79,[0,id&127,id>>7,0]);assert.equal(r14(param.slice(3)),100,'full parameter IDs survive SysEx');
const audio=Int16Array.from({length:11025},(_,i)=>Math.round(Math.sin(i*.04)*24000*Math.exp(-i/3500))),shot={category:0,n:audio.length,data:imaEncode(audio).data};
const eleven=buildEncodedSlot('ELEVEN',Array.from({length:11},()=>shot));assert(eleven.length<=SMP.SLOT_SIZE);assert.throws(()=>buildEncodedSlot('TWELVE',Array.from({length:12},()=>shot)),/full/);
for(let slot=0;slot<4;slot++){
 const bytes=buildEncodedSlot('STATTIC',Array.from({length:4},(_,category)=>({...shot,category})));
 assert.deepEqual(readSlot(bytes).map(s=>s.category),[0,1,2,3]);await uploadSlot(link,slot,bytes);
}
const corrupted=eleven.slice();corrupted[600]^=1;assert.throws(()=>readSlot(corrupted),/CRC/);
const backup=await captureNativeBackup(link,caps.version);assert.equal(validateBackup(backup),backup);assert(backup.objects.find(e=>e.id===0).length>10000);
const rawBackup=structuredClone(backup);rawBackup.objects[0].crc32^=1;assert.throws(()=>validateBackup(rawBackup),/integrity/);
const partialBackup=structuredClone(backup);partialBackup.objects=partialBackup.objects.filter(e=>e.id!==2);assert.throws(()=>validateBackup(partialBackup),/incomplete/);
w.stattic_sim_init();await restoreBackup(link,backup);const restored=await captureNativeBackup(link,caps.version);
for(let id=32;id<36;id++)assert.equal(restored.objects.find(e=>e.id===id).data,backup.objects.find(e=>e.id===id).data);
function render(ms){for(let i=0;i<Math.ceil(ms*44.1/32);i++){w.stattic_sim_render();if(!(i%16))w.stattic_sim_frame();}w.stattic_sim_frame();}
render(10);for(let i=0;i<3;i++){w.stattic_sim_encoder(1,1);w.stattic_sim_frame();}assert.equal(w.stattic_sim_state(0),2);
const mode=w.stattic_sim_state(6),octave=w.stattic_sim_state(7);w.stattic_sim_button(13,1);w.stattic_sim_frame();render(700);w.stattic_sim_button(13,0);w.stattic_sim_frame();assert.notEqual(w.stattic_sim_state(6),mode);assert.equal(w.stattic_sim_state(7),octave);
w.stattic_sim_button(13,1);w.stattic_sim_frame();render(25);w.stattic_sim_button(13,0);w.stattic_sim_frame();assert.equal(w.stattic_sim_state(7),octave+1);
let last=-1,seen=new Set();for(let i=0;i<12;i++){w.stattic_sim_encoder(3+i%4,1);w.stattic_sim_frame();const art=w.stattic_sim_state(8);assert.notEqual(art,last);last=art;if(!(i%3))seen=new Set();assert(!seen.has(art));seen.add(art);}
w.stattic_sim_button(10,1);w.stattic_sim_frame();w.stattic_sim_button(10,0);render(10);assert.equal((await capabilities(link)).playing,true);
const begin=await link.call(11,[0]);assert.equal(begin[1],3,'device refuses sample erases during PLAY');await assert.rejects(()=>uploadSlot(link,0,eleven),/Stop playback/);
w.stattic_sim_panic();render(10);assert.equal((await capabilities(link)).playing,false);
console.log(`STATTIC web/C integration: four category banks, 60 KiB capacity, 14-bit parameters, native CRC/readback, backup/restore, CH hold/short press, art shuffle, STOP guard (${requests} real firmware replies)`);
