// SPDX-License-Identifier: GPL-3.0-only
// Validate real manufacturer packages and the shipped factory-button flow.
// No physical MIDI access or flash writes occur in this test.
import fs from 'node:fs';
import path from 'node:path';
import vm from 'node:vm';
import assert from 'node:assert/strict';
import {pathToFileURL} from 'node:url';
const [folder,vendor]=process.argv.slice(2),dir=path.resolve(folder);
const {verifyFactoryPackage,InstallSession}=await import(pathToFileURL(path.join(dir,'core.js')));
const {BACKUP_IDS,bkCrc}=await import(pathToFileURL(path.join(dir,'fm1backup.js')));
const {stockRefusal}=await import(pathToFileURL(path.join(dir,'fm1ota.js')));
const packages={};
for(const version of ['015','016']){
 const bytes=new Uint8Array(fs.readFileSync(path.join(vendor,`FM-1_${version}.fwsc`)));
 const checked=await verifyFactoryPackage(bytes);
 assert.equal(checked.product,`FM-1_${version}`);assert.equal(checked.factory,true);
 const bad=bytes.slice();bad[bad.length-1]^=1;await assert.rejects(()=>verifyFactoryPackage(bad),/unmodified official/);
 packages[version]={bytes,checked};
}
const html=fs.readFileSync(path.join(dir,'factory.html'),'utf8');
assert(html.includes('data-install-mode="factory"'));
assert(html.includes('src="app.js?v='),'factory page must not run the cached pre-factory Snow installer');
const elements=new Map([...html.matchAll(/<[^>]+id="([^"]+)"[^>]*>/g)].map(([tag,id])=>[id,{disabled:/\bdisabled\b/.test(tag),hidden:/\bhidden\b/.test(tag),checked:false,textContent:'',value:0,files:[]}]));
const el=id=>{assert(elements.has(id),id);return elements.get(id);};
const data=Uint8Array.from([1,2,3,4]);
const archive={format:'felucca-backup',version:1,firmware:'FELUCCA v0.7.3-snow',objects:BACKUP_IDS.map(id=>({id,size:id<2?4:0,crc:id<2?bkCrc(data):0,data:id<2?Buffer.from(data).toString('base64'):''}))};
const info=[...Buffer.from(archive.firmware),0,0,0,0,0,0,0,16,0x55,1,0,0x4d,1,0,0,0x42,1,1];
let model='FM-1',identity='FM-1_700',midiRequests=0,fetches=0;
const events=[];
const device=()=>({id:{model,text:identity},link:{input:{id:'in'},output:{id:'out'},close(){}}});
class UIUpdater{
 async find(){return device();}
 async install(image,product){assert(el('factory-file').disabled);events.push(`write:${product}`);}
}
class UISession extends InstallSession{
 constructor(updater,image,config,opts){super(updater,image,config,{...opts,
  connection:()=>({request:async()=>info,close(){}}),
  capture:async()=>{events.push('backup');return archive;}});}
}
const context={console,Uint8Array,Date,Blob,setTimeout:()=>0,
 URL:{createObjectURL:()=>'/backup',revokeObjectURL(){}},
 document:{getElementById:el,body:{dataset:{installMode:'factory'},append(){}},createElement:()=>({click(){events.push('download');},remove(){}})},
 window:{addEventListener(){}},navigator:{requestMIDIAccess:async()=>{midiRequests++;return {}; }},
 fetch:async()=>{fetches++;throw new Error('Factory page must not fetch a Snow package');},
 Updater:UIUpdater,InstallSession:UISession,verifyFactoryPackage,stockRefusal};
const app=fs.readFileSync(path.join(dir,'app.js'),'utf8').replace(/^import .*;\n/gm,'');
assert(fs.readFileSync(path.join(dir,'app.js'),'utf8').includes('./core.js?v='),'factory-capable core must bypass an older cached module');
await vm.runInNewContext('(async()=>{'+app+'\n})()',context);
assert.equal(midiRequests,0);assert.equal(fetches,0);assert(el('connect').disabled&&el('install').disabled);
async function file(bytes){el('factory-file').files=[{size:bytes.length,arrayBuffer:async()=>bytes.slice().buffer}];await el('factory-file').onchange();}
await file(packages['015'].bytes);assert(!el('connect').disabled&&el('install').disabled);
await el('connect').onclick();assert.deepEqual(events,['backup','download']);
for(const id of ['backup-confirmed','risk-confirmed']){el(id).checked=true;el(id).onchange();}
assert(!el('install').disabled);
const bad=packages['015'].bytes.slice();bad[bad.length-1]^=1;
await file(bad);assert(el('connect').disabled&&el('install').disabled);
await el('install').onclick();assert(!events.some(x=>x.startsWith('write:')));
await file(packages['016'].bytes);assert(!el('connect').disabled&&el('install').disabled);
await el('connect').onclick();
for(const id of ['backup-confirmed','risk-confirmed']){el(id).checked=true;el(id).onchange();}
await el('install').onclick();assert.equal(events.at(-1),'write:FM-1_016');assert(el('install').disabled);
assert.equal(fetches,0);assert.equal(midiRequests,2);
for(const [m,id,reason] of [['ota-FM-1','ota-FM-1_700',/Finish the interrupted/],['FM-1','FM-1_016',/already runs/],['SMK-37 Pro','SMK-37 Pro_700',/FM-1 only/]]){
 model=m;identity=id;
 const session=new InstallSession(new UIUpdater(),packages['016'].checked.image,packages['016'].checked);
 await assert.rejects(()=>session.prepare(),reason);
}
console.log('FACTORY INSTALLER PASSED: real V15/V16 hashes/layouts, invalid file rejection, no automatic MIDI/write, backup before correct factory target, file-change invalidation, loader/model guards');
