// SPDX-License-Identifier: GPL-3.0-only
import {pack7,unpack7,crc32,SMP} from './samples.mjs';
export const u32=n=>Array.from({length:5},(_,i)=>(n>>>i*7)&127);
export const r32=(a,i=0)=>((a[i]|a[i+1]<<7|a[i+2]<<14|a[i+3]<<21|a[i+4]<<28)>>>0);
export function download(name,data,type='application/octet-stream'){
  const url=URL.createObjectURL(new Blob([data],{type})),a=document.createElement('a');a.href=url;a.download=name;a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);
}
export class MidiLink {
  constructor(input,output){this.input=input;this.output=output;this.pending=null;this.queue=Promise.resolve();this.listener=e=>this.receive(e.data);input.addEventListener('midimessage',this.listener);}
  receive(data){if(data[0]!==240||data[1]!==125||data[2]!==70||data[3]!==76||data.at(-1)!==247)return;const p=this.pending;if(p&&data[4]===p.cmd){this.pending=null;clearTimeout(p.timer);p.resolve(Array.from(data.slice(5,-1)));}}
  call(cmd,args=[],timeout=10000){const run=()=>new Promise((resolve,reject)=>{
    const timer=setTimeout(()=>{this.pending=null;reject(new Error('FM-1 did not reply. Check USB and the installed version.'));},timeout);
    this.pending={cmd,resolve,reject,timer};try{this.output.send([240,125,70,76,cmd,...args,247]);}catch(e){this.pending=null;clearTimeout(timer);reject(e);}
  });const result=this.queue.then(run);this.queue=result.catch(()=>{});return result;}
  close(){this.input.removeEventListener('midimessage',this.listener);if(this.pending){clearTimeout(this.pending.timer);this.pending.reject(new Error('Disconnected'));this.pending=null;}}
}
export async function capabilities(link){
  const a=await link.call(78);if(a.length<15||a[0]!==1||a[1]!==4||a[2]!==3||a[3]!==64||a[4]!==4||a[5]!==60||a[6]!==16||a[7]!==83)throw new Error('Install STATTIC on SLOOP before using this manager');
  return {parameters:a[8]|a[9]<<7,engineBase:a[10]|a[11]<<7,playing:!!a[12],flash:!!a[13],version:String.fromCharCode(...a.slice(14,a.indexOf(0,14)<0?undefined:a.indexOf(0,14)))};
}
export async function listObjects(link){
  const a=await link.call(34);if(a[0]||a.length!==2+a[1]*11)throw new Error('Could not snapshot the device');
  return Array.from({length:a[1]},(_,i)=>{const p=2+i*11;return {id:a[p],length:r32(a,p+1),crc32:r32(a,p+6)};});
}
export async function readObject(link,entry,onProgress=()=>{}){
  if(entry.length>0x14000)throw new Error('Invalid backup object size');
  const bytes=new Uint8Array(entry.length);
  for(let off=0;off<bytes.length;off+=256){
    const count=Math.min(256,bytes.length-off),a=await link.call(35,[entry.id,...u32(off),count&127,count>>7]);
    if(a[0]!==entry.id||a[1]||r32(a,2)!==off||(a[7]|a[8]<<7)!==count)throw new Error('Invalid backup reply');
    const chunk=unpack7(a.slice(9));if(chunk.length!==count)throw new Error('Incomplete backup reply');bytes.set(chunk,off);onProgress(off+count,bytes.length);
  }
  if(crc32(bytes)!==entry.crc32)throw new Error('Backup CRC mismatch; nothing was written');return bytes;
}
const b64=bytes=>{let s='';for(const b of bytes)s+=String.fromCharCode(b);return btoa(s);};
const from64=s=>Uint8Array.from(atob(s),x=>x.charCodeAt(0));
export async function captureNativeBackup(link,version,progress=()=>{}){
  const list=await listObjects(link),objects=[],total=list.reduce((n,e)=>n+e.length,0);let done=0;
  for(const entry of list){const bytes=await readObject(link,entry,(n)=>progress(done+n,total));objects.push({...entry,data:b64(bytes)});done+=bytes.length;}
  return {format:'STATTIC-SLOOP-BACKUP',schema:1,model:'FM-1',firmware:version,created:new Date().toISOString(),objects};
}
export function validateBackup(backup){
  if(backup?.format!=='STATTIC-SLOOP-BACKUP'||backup.schema!==1||backup.model!=='FM-1'||!Array.isArray(backup.objects)||backup.objects.length>16)throw new Error('This is not a STATTIC/SLOOP backup');
  const seen=new Set();for(const e of backup.objects){if(!Number.isInteger(e.id)||!(e.id>=0&&e.id<=11||e.id>=32&&e.id<=35)||seen.has(e.id)||!Number.isInteger(e.length)||e.length<0||e.length>0x14000)throw new Error('Invalid backup inventory');seen.add(e.id);const bytes=from64(e.data);if(bytes.length!==e.length||crc32(bytes)!==e.crc32)throw new Error('Backup integrity check failed');}
  if(![0,1,2,3,4,5,6,7,8,9,32,33,34,35].every(id=>seen.has(id)))throw new Error('The backup is incomplete');return backup;
}
export async function uploadSlot(link,slot,bytes,progress=()=>{}){
  const c=await capabilities(link);if(c.playing||!c.flash)throw new Error('Stop playback on the FM-1 before writing');
  if(!Number.isInteger(slot)||slot<0||slot>3||bytes.length>SMP.SLOT_SIZE)throw new Error('Invalid sample bank');
  let a=await link.call(11,[slot]);if(a[0]!==slot||a[1])throw new Error('The FM-1 refused to erase this bank');
  for(let off=512;off<bytes.length;off+=256){a=await link.call(12,[slot,...u32(off).slice(0,3),...pack7(bytes.slice(off,off+256))]);if(a[0]!==slot||a[4]||((a[1]|a[2]<<7|a[3]<<14)!==off))throw new Error('Sample write failed');progress(off,bytes.length);}
  if(bytes.length){a=await link.call(13,[slot,...pack7(bytes.slice(0,480))]);if(a[0]!==slot||a[1])throw new Error('Sample CRC/header commit failed');}
  const list=await listObjects(link),entry=list.find(e=>e.id===32+slot),back=await readObject(link,entry);
  if(back.length!==bytes.length||back.some((n,i)=>n!==bytes[i]))throw new Error('Readback differs from the uploaded bank');progress(bytes.length,bytes.length);
}
export async function restoreBackup(link,backup,progress=()=>{}){
  validateBackup(backup);const c=await capabilities(link);if(c.playing||!c.flash)throw new Error('Stop playback before restoring');
  if(!/STATTIC.*0\.9\.0-S1/.test(backup.firmware))throw new Error('Restore this archive with the firmware that created it');
  let index=0;const ordered=[...backup.objects].sort((a,b)=>a.id===0?1:b.id===0?-1:a.id-b.id);
  for(const e of ordered){const bytes=from64(e.data);
    if(e.id>=32)await uploadSlot(link,e.id-32,bytes);
    else if(e.id!==8){let a=await link.call(36,[0,e.id,...u32(bytes.length),...u32(e.crc32)]);if(a[2])throw new Error('Restore begin refused');
      for(let off=0;off<bytes.length;off+=256){a=await link.call(36,[1,e.id,...u32(off),...pack7(bytes.slice(off,off+256))]);if(a[2])throw new Error('Restore write failed');}
      a=await link.call(36,[2,e.id]);if(a[2])throw new Error('Restore object rejected');}
    progress(++index,ordered.length);
  }
}
