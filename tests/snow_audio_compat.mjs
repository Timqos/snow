// SPDX-License-Identifier: GPL-3.0-only
// Compare the exact former production WASM with the candidate's neutral path.
import fs from 'node:fs';
import assert from 'node:assert/strict';
async function boot(path){const {instance}=await WebAssembly.instantiate(fs.readFileSync(path),{});const x=instance.exports;x._initialize();x.web_nor_erase();x.web_boot();return x;}
function render(x,ms){for(let n=0;n<ms*44.1;n+=128)x.web_render(128);}
const old=await boot(process.argv[2]),next=await boot(process.argv[3]);
for(const x of [old,next]){render(x,1800);x.web_snow_demo();x.web_buttons(1<<10);render(x,70);x.web_buttons(0);render(x,70);}
for(let block=0;block<1500;block++){
 old.web_render(128);next.web_render(128);
 for(const out of ['web_out_l','web_out_r']){
  const a=new Uint32Array(old.memory.buffer,old[out](),128),b=new Uint32Array(next.memory.buffer,next[out](),128);
  assert.deepEqual(b,a,`neutral audio changed in ${out}, block ${block}`);
 }
}
console.log('NEUTRAL AUDIO COMPATIBILITY PASSED: 192000 stereo samples bit-identical to previous production firmware');
