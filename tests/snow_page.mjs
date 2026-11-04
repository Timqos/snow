// SPDX-License-Identifier: GPL-3.0-only
// Generated page wiring and preservation of a user's compatible 0.4 flash.
import fs from 'node:fs';
import vm from 'node:vm';
import assert from 'node:assert/strict';
const html=fs.readFileSync(process.argv[2],'utf8');
const ids=new Set([...html.matchAll(/id="([^"]+)"/g)].map(x=>x[1]));
for(const [,id]of html.matchAll(/snowById\('([^']+)'\)/g))assert(ids.has(id),id);
assert(html.indexOf('id="go"')<html.indexOf('id="snow-lab"'));
assert(/id="snow-target"[^>]*><option value="channel">KK<\/option><option value="all">ALL/.test(html));
assert(html.includes('id="snow-random-sound"'));
const load=html.slice(html.indexOf('function loadFlash()'),html.indexOf('function saveFlash('));assert(load);
function migrate(current,previous){
 const records=new Map();if(current)records.set('snow-fm1-prototype-05',current);if(previous)records.set('snow-fm1-prototype-04',previous);
 const original=records.get('snow-fm1-prototype-04');
 const box={STORE:'snow-fm1-prototype-05',snowUpgradeFrom04:false,localStorage:{getItem:k=>records.get(k)??null},atob,Uint8Array};
 const encoded=vm.runInNewContext(load+'; JSON.stringify({data:Object.fromEntries(Object.entries(loadFlash()).map(([k,v])=>[k,Array.from(v)])),migrated:snowUpgradeFrom04})',box);
 assert.equal(records.get('snow-fm1-prototype-04'),original);
 return JSON.parse(encoded);
}
const old=JSON.stringify({'4096':Buffer.from([11,22,33,44]).toString('base64')});
assert.deepEqual(migrate(null,old),{data:{'4096':[11,22,33,44]},migrated:true});
const own=JSON.stringify({'8192':Buffer.from([77,88]).toString('base64')});
assert.deepEqual(migrate(own,old),{data:{'8192':[77,88]},migrated:false});
assert.deepEqual(migrate(null,null),{data:{},migrated:false});
assert.deepEqual(migrate('invalid',null).data,{});
console.log('SNOW PAGE PASSED: generator defaults, random button wiring, 0.4 flash copied intact, 0.5 storage takes precedence');
