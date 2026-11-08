// SPDX-License-Identifier: GPL-3.0-only
// Test the exact shipped installer modules, without contacting physical MIDI.
import fs from 'node:fs';
import path from 'node:path';
import {pathToFileURL} from 'node:url';
import assert from 'node:assert/strict';
import vm from 'node:vm';
const dir=path.resolve(process.argv[2]);
const {InstallSession,verifyPackage,archiveInfo}=await import(pathToFileURL(path.join(dir,'core.js')));
const {BACKUP_IDS,bkCrc}=await import(pathToFileURL(path.join(dir,'fm1backup.js')));
const config=JSON.parse(fs.readFileSync(path.join(dir,'config.json'),'utf8'));
const notice=fs.readFileSync(path.join(dir,'index.html'),'utf8');
assert(notice.indexOf('id="responsibility"')>=0&&notice.indexOf('id="responsibility"')<notice.indexOf('id="install"'));
assert.equal((notice.match(/id="risk-confirmed"/g)||[]).length,1);
const bytes=new Uint8Array(fs.readFileSync(path.resolve(dir,config.package)));
const image=await verifyPackage(bytes,config);assert(image.length>100000);
const bad=bytes.slice();bad[bad.length-10]^=1;await assert.rejects(()=>verifyPackage(bad,config),/checksum/);
await assert.rejects(()=>verifyPackage(bytes,{...config,product:'SMK-37 Pro_700'}),/FM-1/);
const data=Uint8Array.from([1,2,3,4]);
const archive={format:'felucca-backup',version:1,firmware:'STATTIC v0.8.0-rc2-stattic',objects:BACKUP_IDS.map(id=>({id,size:id<2?4:0,crc:id<2?bkCrc(data):0,data:id<2?Buffer.from(data).toString('base64'):''}))};
function info(supported=true,prefix='STATTIC v0.8.0-rc2-stattic'){return [...Buffer.from(prefix),0,0,0,0,0,0,0,...(supported?[16,0x55,1,0,0x4d,1,0,0,0x42,1,1]:[])];}
assert(archiveInfo(info()).supported);assert(archiveInfo(info(true,'FELUCCA v0.7.4-rc1-snow')).supported);
const makeDevice=(model='FM-1',input='input-a')=>({id:{model,text:model==='FM-1'?'FM-1_700':model==='ota-FM-1'?'ota-FM-1_700':'SMK-37 Pro_700'},link:{input:{id:input},output:{id:'output-a'},close(){}}});
function fixture({supported=true,failBackup=false,failDownload=false,model='FM-1'}={}){
 const events=[];let device=makeDevice(model);
 const updater={find:async()=>device,install:async()=>events.push('write'),resume:async()=>{events.push('resume');return true;}};
 const session=new InstallSession(updater,image,config,{connection:()=>({request:async()=>info(supported),close(){}}),capture:async()=>{events.push('backup');if(failBackup)throw new Error('Backup failed');return archive;},download:async()=>{events.push('download');if(failDownload)throw new Error('Download failed');}});
 return {session,events,setDevice:v=>{device=v;}};
}
const ack={backupConfirmed:true,riskConfirmed:true};
let f=fixture();await assert.rejects(()=>f.session.install(ack),/backup step/);assert.deepEqual(f.events,[]);
await f.session.prepare();assert.deepEqual(f.events,['backup','download']);
await assert.rejects(()=>f.session.install(),/Confirm/);await assert.rejects(()=>f.session.install({backupConfirmed:true}),/Confirm/);assert(!f.events.includes('write'));
await f.session.install(ack);assert.deepEqual(f.events,['backup','download','write']);await assert.rejects(()=>f.session.install(ack),/backup step/);
for(const kind of ['failBackup','failDownload']){f=fixture({[kind]:true});await assert.rejects(()=>f.session.prepare(),/failed/);await assert.rejects(()=>f.session.install(ack),/backup step/);assert(!f.events.includes('write'));}
f=fixture({supported:false});assert.equal((await f.session.prepare()).mode,'external');await assert.rejects(()=>f.session.install(ack),/Confirm/);await f.session.install({...ack,externalConfirmed:true});assert.deepEqual(f.events,['write']);
f=fixture({model:'ota-FM-1'});assert.equal((await f.session.prepare()).mode,'resume');await assert.rejects(()=>f.session.install(ack),/Confirm/);await f.session.install({...ack,externalConfirmed:true});assert.deepEqual(f.events,['resume']);
f=fixture({model:'SMK-37 Pro'});await assert.rejects(()=>f.session.prepare(),/FM-1 only/);assert.deepEqual(f.events,[]);
f=fixture();await f.session.prepare();f.setDevice(makeDevice('FM-1','input-b'));await assert.rejects(()=>f.session.install(ack),/device or update mode changed/);assert(!f.events.includes('write'));
f=fixture();await f.session.prepare();f.setDevice(makeDevice('ota-FM-1'));await assert.rejects(()=>f.session.install(ack),/device or update mode changed/);assert(!f.events.includes('write'));
// Execute the shipped app's event handlers as well as its session core. No MIDI
// permission is requested on page load; only the explicit final click writes.
const html=notice;
const elements=new Map([...html.matchAll(/<[^>]+id="([^"]+)"[^>]*>/g)].map(([tag,id])=>[id,{disabled:/\bdisabled\b/.test(tag),hidden:/\bhidden\b/.test(tag),checked:false,textContent:'',value:0}]));
const events=[];let midiRequests=0;
class UIUpdater{async find(){return makeDevice();}async install(){events.push('write');}}
class UISession extends InstallSession{constructor(updater,img,cfg,opts){super(updater,img,cfg,{...opts,connection:()=>({request:async()=>info(),close(){}}),capture:async()=>{events.push('backup');return archive;}});}}
const context={console,Uint8Array,Date,Blob,URL:{createObjectURL:()=>'/local-backup',revokeObjectURL(){}},setTimeout:()=>0,
 document:{getElementById:id=>{assert(elements.has(id),id);return elements.get(id);},body:{append(){}},createElement:()=>({click(){events.push('download');},remove(){}})},
 window:{addEventListener(){}},navigator:{requestMIDIAccess:async()=>{midiRequests++;return {}; }},
 fetch:async url=>url==='config.json'?{ok:true,json:async()=>config}:{ok:true,arrayBuffer:async()=>bytes.slice().buffer},
 Updater:UIUpdater,InstallSession:UISession,verifyPackage};
const app=fs.readFileSync(path.join(dir,'app.js'),'utf8').replace(/^import .*;\n/gm,'');
await vm.runInNewContext('(async()=>{'+app+'\n})()',context);
const el=id=>elements.get(id);
assert(el('package-hash').textContent.includes(config.sha256));
assert.equal(midiRequests,0);assert.equal(el('connect').disabled,false);assert.equal(el('install').disabled,true);
await el('install').onclick();assert.deepEqual(events,[]);
await el('connect').onclick();assert.equal(midiRequests,1);assert.deepEqual(events,['backup','download']);assert.equal(el('install').disabled,true);
el('backup-confirmed').checked=true;el('backup-confirmed').onchange();assert.equal(el('install').disabled,true);
el('risk-confirmed').checked=true;el('risk-confirmed').onchange();assert.equal(el('install').disabled,false);
await el('install').onclick();assert.deepEqual(events,['backup','download','write']);assert.equal(el('install').disabled,true);
console.log('STATTIC INSTALLER PASSED: exact package/hash, validated backup before write, failed backup/download blocked, consent, model/device-change guards, explicit external backup/resume');

// Exercise the new STATTIC backup branch against the actual firmware editor.
const wasmModule=new WebAssembly.Module(fs.readFileSync('build/stattic-web/stattic.wasm'));
const w=new WebAssembly.Instance(wasmModule,{}).exports;w._initialize?.();w.stattic_sim_init();
const nativeEvents=[];
const native=new InstallSession({find:async()=>makeDevice(),install:async()=>nativeEvents.push('write')},image,config,{
 connection:()=>({request:async([cmd,args])=>{const req=Uint8Array.from([125,70,76,cmd,...args]);new Uint8Array(w.memory.buffer,w.stattic_sim_request_buffer(),req.length).set(req);const n=w.stattic_sim_request(req.length);assert(n>5);return Array.from(new Uint8Array(w.memory.buffer,w.stattic_sim_response_buffer()+5,n-6));},close(){}}),
 download:async backup=>{assert.equal(backup.format,'STATTIC-SLOOP-BACKUP');assert.equal(backup.objects.length,14);nativeEvents.push('download');}
});
await native.prepare();assert.deepEqual(nativeEvents,['download']);await native.install(ack);assert.deepEqual(nativeEvents,['download','write']);
console.log('STATTIC native backup-before-install branch: real firmware object inventory/CRC, no physical MIDI');
