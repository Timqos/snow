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
console.log("SNOW WASM PASSED: demo, six-machine mix, mask, Control All, RAM reload while playing, deterministic audio; peak",result.peak);
