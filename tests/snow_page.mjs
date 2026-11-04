// SPDX-License-Identifier: GPL-3.0-only
import fs from 'node:fs';
import vm from 'node:vm';
import assert from 'node:assert/strict';
const html=fs.readFileSync(process.argv[2],'utf8');
assert(/<html lang="en">/.test(html));assert(!/[\u0400-\u04ff]/.test(html));
assert(!/snow-(?:lab|channels|generator-panel|sound-controls|pattern-grid|preset)|lab-control|<table|<select|type="range"/.test(html));
const deviceEnd=html.indexOf('<section class="install-strip"');assert(deviceEnd>html.indexOf('id="device"')&&deviceEnd<html.indexOf('<p class="keys">'));
assert(html.includes('href="install/"'));assert(html.includes('GEN RND'));assert(html.includes('CONTROL ALL'));assert(html.includes('Back up before you install.'));
const load=html.slice(html.indexOf('function loadFlash()'),html.indexOf('function saveFlash('));assert(load);
function migrate(current,old05,old04){
 const records=new Map();if(current)records.set('snow-fm1-prototype-06',current);if(old05)records.set('snow-fm1-prototype-05',old05);if(old04)records.set('snow-fm1-prototype-04',old04);
 const original=JSON.stringify([...records]);
 const box={STORE:'snow-fm1-prototype-06',snowLegacy04:false,localStorage:{getItem:k=>records.get(k)??null},atob,Uint8Array};
 const encoded=vm.runInNewContext(load+'; JSON.stringify({data:Object.fromEntries(Object.entries(loadFlash()).map(([k,v])=>[k,Array.from(v)])),legacy04:snowLegacy04})',box);
 assert.equal(JSON.stringify([...records]),original);return JSON.parse(encoded);
}
const old=JSON.stringify({'4096':Buffer.from([11,22,33,44]).toString('base64')}),own=JSON.stringify({'8192':Buffer.from([77,88]).toString('base64')});
assert.deepEqual(migrate(null,old,null),{data:{'4096':[11,22,33,44]},legacy04:false});
assert.deepEqual(migrate(null,null,old),{data:{'4096':[11,22,33,44]},legacy04:true});
assert.deepEqual(migrate(own,old,old),{data:{'8192':[77,88]},legacy04:false});
assert.deepEqual(migrate(null,null,null),{data:{},legacy04:false});assert.deepEqual(migrate('invalid',null,null).data,{});
console.log('SNOW PAGE PASSED: English only, unchanged device controls, installer immediately below, no parameter/preset builder, compatible browser saves copied intact');
