// SPDX-License-Identifier: GPL-3.0-only
import assert from 'node:assert/strict';
import fs from 'node:fs';
import {SerialConsole,parseStatus,parseMemory,validateMap,captureSample,CaptureLog,MAX_RAW,MAX_SAMPLES} from '../web/snow/console/core.mjs';
const map=JSON.parse(fs.readFileSync(new URL('../web/snow/console/maps.json',import.meta.url))).maps[0];
validateMap(map);
const status=(late=1050,uptime=10000,version=map.version,boots=4)=>`status\r\nfelucca ${version}\r\nuptime_ms ${uptime}\r\ncpu_pct 80\r\naudio_late ${late}\r\nboots ${boots}\r\ncpu2 1\r\ncpu_mhz 360\r\npreset <img src=x onerror=bad()>\r\n> `;
const words=values=>{const b=new Uint8Array(values.length*4),v=new DataView(b.buffer);values.forEach((x,i)=>v.setUint32(i*4,x,true));return b;};
function memoryDump(region,bytes){
  let s='';for(let i=0;i<bytes.length;i+=16)s+=`${(region.address+i).toString(16).padStart(8,'0')}: ${Array.from(bytes.slice(i,i+16),b=>b.toString(16).padStart(2,'0')).join(' ')}\r\n`;
  return s+'> ';
}
function fixture(){
  const data={late_base:words([800]),peak:words([190000]),had_late:Uint8Array.of(1),event:words([167155,70000,30000,42000,7000]),late_counter:words([1050]),rendered_voices:Uint8Array.of(1,1,1,1,1,4),part_cost:new Uint8Array(12)};
  data.detail=words([10000,20000,30000,40000,50000,60000,12000,96000,60000,42,21,20]);
  let raw=1050,uptime=10000,version=map.version,boots=4,unstable=false,unstableDetail=false,reads=0;
  const requested=[];
  return {requested,set(values){({raw=raw,uptime=uptime,version=version,boots=boots,unstable=unstable,unstableDetail=unstableDetail}=values);},
    async request(cmd){assert.equal(cmd,'status');requested.push(cmd);return status(raw,uptime,version,boots);},
    setMap(m){this.map=m;},async memory(name){requested.push(name);if(name==='late_counter')return words([raw]);
      if(name==='event'&&unstable)return words([167155+reads++,70000,30000,42000,7000]);
      const result=data[name].slice();if(name==='detail'&&unstableDetail)result[0]=reads++;
      return result;}};
}
assert.equal(parseStatus(status()).preset,'<img src=x onerror=bad()>');
assert.throws(()=>parseStatus('status\r\n> '),/Incomplete/);
assert.deepEqual(parseMemory(memoryDump(map.regions.event,words([1,2,3,4,5])),map.regions.event),words([1,2,3,4,5]));
assert.throws(()=>parseMemory('01c00000: 00\r\n> ',map.regions.event),/Incomplete/);
assert.throws(()=>validateMap({...map,regions:{...map.regions,event:{address:0x10008,size:20}}}),/Invalid/);
const fake=fixture();const first=await captureSample(fake,[map]);
assert.equal(first.late,250);assert.equal(first.delta_late,0);assert.equal(first.late_scope,'since boot');
assert.equal(first.stages.consistent,true);assert.equal(first.stages.ch_synth_us,70000/24);
assert.deepEqual(first.current_rendered_voices,[1,1,1,1,1,4]);
fake.set({raw:1053,uptime:12000});const second=await captureSample(fake,[map],first);
assert.equal(second.late,253);assert.equal(second.delta_late,3);assert(!second.reboot);
fake.set({unstable:true});assert.equal((await captureSample(fake,[map],second)).stages.consistent,false);
const detailedMap={...map,version:'test-rc10',detail_schema:1,regions:{...map.regions,detail:{address:0x1c1c000,size:48}}};
validateMap(detailedMap);
assert.throws(()=>validateMap({...detailedMap,detail_schema:2}),/schema/);
assert.throws(()=>validateMap({...detailedMap,regions:{...detailedMap.regions,detail:{address:0x10000,size:48}}}),/Invalid/);
const detail=fixture();detail.set({version:detailedMap.version});const full=await captureSample(detail,[detailedMap]);
assert.deepEqual(full.stages.track_wall_us,[10000,20000,30000,40000,50000,60000].map(x=>x/24));
assert.equal(full.stages.serial_us,500);assert.equal(full.stages.primary_work_us,4000);
assert.equal(full.stages.worker_work_us,2500);assert.equal(full.stages.worker_tracks,42);
assert.equal(full.stages.primary_irq_overhead_pct,7.8125);
detail.set({unstableDetail:true});assert.equal((await captureSample(detail,[detailedMap])).stages.consistent,false);
const old=fixture();old.set({version:'v2.2'});const basic=await captureSample(old,[map]);
assert.equal(basic.late_scope,'since connection');assert.equal(basic.late,0);assert.deepEqual(old.requested,['status']);
old.set({raw:1055,uptime:13000});assert.equal((await captureSample(old,[map],basic)).delta_late,5);
old.set({raw:7,uptime:1,boots:5});const restart=await captureSample(old,[map],basic);assert(restart.reboot);assert.equal(restart.delta_late,0);
const wrapped={...basic,raw_late:0xfffffffe,status:{...basic.status,uptime_ms:0xffffff00}};
old.set({raw:2,uptime:50,boots:4});const wrap=await captureSample(old,[map],wrapped);assert(!wrap.reboot);assert.equal(wrap.delta_late,4);

function port({silent=false,wrongBanner=false,signalError=false,blockedWrite=false}={}){
  let controller;const commands=[];const enc=new TextEncoder();
  const push=text=>{for(let i=0;i<text.length;i+=3)controller.enqueue(enc.encode(text.slice(i,i+3)));};
  const p={commands,readable:new ReadableStream({start(c){controller=c;},cancel(){p.cancelled=true;}}),
    writable:new WritableStream({write(bytes){const cmd=new TextDecoder().decode(bytes);commands.push(cmd);if(blockedWrite)return new Promise(()=>{});if(!silent)push(status());}}),
    async open(){p.opened=true;},async setSignals(s){if(signalError)throw new Error('DTR failed');if(s.dataTerminalReady)push(wrongBanner?'Other device\r\n> ':`\r\nFelucca ${map.version} console - 'help'\r\n> `);},
    async close(){p.closed=true;},push};return p;
}
const p=port();let raw='';const serial=new SerialConsole(p,{onRaw:s=>raw+=s,timeout:100});
await serial.open();assert(p.opened);assert(raw.includes('Felucca'));
await assert.rejects(serial.request('uboot yes'),/Only/);await assert.rejects(serial.request('memr 0x1c00000 256'),/Only/);
assert.equal(p.commands.length,0);assert.equal(parseStatus(await serial.request('status')).cpu_pct,80);
await serial.close();assert(p.closed&&p.cancelled);assert(p.commands.every(c=>c==='status\r'));
const slow=port({silent:true}),waiting=new SerialConsole(slow,{timeout:30});await waiting.open();
const timeout=waiting.request('status');await assert.rejects(waiting.request('status'),/already running/);
await assert.rejects(timeout,/timed out/);await waiting.close();
const blocked=port({blockedWrite:true}),backpressure=new SerialConsole(blocked,{timeout:30});await backpressure.open();
await assert.rejects(backpressure.request('status'),/timed out/);await backpressure.close();
const wrong=port({wrongBanner:true});await assert.rejects(new SerialConsole(wrong).open(),/identify/);assert(wrong.closed&&!wrong.commands.length);
const broken=port({signalError:true});await assert.rejects(new SerialConsole(broken).open(),/DTR failed/);assert(broken.closed);
const unplug=port({silent:true});let disconnected=false;const dying=new SerialConsole(unplug,{timeout:200,onDisconnect:()=>{disconnected=true;}});
await dying.open();const pending=dying.request('status');await dying.close();await assert.rejects(pending,/closed/);
const log=new CaptureLog();log.addRaw('x'.repeat(MAX_RAW+12));assert.equal(log.raw.length,MAX_RAW);assert.equal(log.raw_dropped,12);
log.sample(first);log.sample(second);log.event('rasp-marker',{note:'CH + KK + BS'});
assert(log.events.some(e=>e.type==='missed-audio-deadline'&&e.count===3));
assert.equal(log.export().samples[1].late,253);assert(log.export().raw);
for(let i=0;i<MAX_SAMPLES+1;i++)log.event('test');assert(log.events_dropped>0&&log.events.length===MAX_SAMPLES);
for(let i=0;i<MAX_SAMPLES+1;i++)log.sample({...first,delta_late:0});assert(log.samples_dropped>0&&log.samples.length===MAX_SAMPLES);
console.log('USB CONSOLE PASSED: fragmented CDC prompts, allowed reads only, timeout/close races, unknown firmware, exact-map decoding, coherent snapshot checks, counter/uptime wrap, reboot, markers and bounded export');
