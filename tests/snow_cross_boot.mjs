// SPDX-License-Identifier: GPL-3.0-only
// Actual released WASM parsers with the other firmware's persisted flash.
import fs from 'node:fs';
import assert from 'node:assert/strict';
async function device(file,flash){
 const {instance}=await WebAssembly.instantiate(fs.readFileSync(file),{}),x=instance.exports;
 x._initialize();x.web_nor_erase();
 if(flash)new Uint8Array(x.memory.buffer,x.web_nor(),x.web_nor_size()).set(flash);
 x.web_boot();
 const render=ms=>{for(let i=0;i<ms*44.1;i+=128){x.web_render(128);for(const s of new Float32Array(x.memory.buffer,x.web_out_l(),128))assert(Number.isFinite(s)&&Math.abs(s)<=1);}};
 const tap=b=>{x.web_buttons(1<<b);render(70);x.web_buttons(0);render(70);};
 const dump=()=>new Uint8Array(x.memory.buffer,x.web_nor(),x.web_nor_size()).slice();
 render(1800);return {x,render,tap,dump};
}
const [snowFile,skiffFile]=process.argv.slice(2);assert(snowFile&&skiffFile);
const snow=await device(snowFile);snow.x.web_snow_demo();
for(let ch=0;ch<6;ch++)snow.x.web_snow_random_sound(ch);
snow.tap(7);snow.tap(13);snow.tap(13);snow.render(2500);
assert.equal(snow.x.web_test_user_preset(0),1);assert(snow.x.web_flash_writes_count()>5);
const initial=snow.dump();
assert(initial.subarray(0xe5000,0xe7000).some(v=>v!==255),'Snow autosave must be present');
const skiff=await device(skiffFile,initial);skiff.tap(10);assert.equal(skiff.x.web_playing(),1);skiff.render(1000);skiff.tap(10);
const back=await device(snowFile,skiff.dump());back.tap(10);assert.equal(back.x.web_playing(),1);back.render(1000);
console.log('CROSS-BOOT PASSED: saved Snow preset/autosave → Skiff → Snow; both boot, UI responds, finite audio. Musical interchange is not asserted.');
