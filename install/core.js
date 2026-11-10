// SPDX-License-Identifier: GPL-3.0-only
// STATTIC's backup-first flow uses the existing, model-checked FM-1 updater.
import {productOf,logicalImage,sha256hex,validateStockPackage} from './fm1pkg.js';
import {sameModel,IS_OTA} from './fm1ota.js';
import {captureBackup,BackupConnection,readBackup} from './fm1backup.js';
import {captureNativeBackup,validateBackup} from '../shared/midi.mjs';
export async function verifyPackage(bytes,config){
 if(productOf(bytes)!==config.product||config.product!=='FM-1_990')throw new Error('This package is not the STATTIC FM-1 package. Nothing was written.');
 if(await sha256hex(bytes)!==config.sha256)throw new Error('Firmware checksum mismatch. Reload the installer. Nothing was written.');
 return logicalImage(bytes);
}
export async function verifyFactoryPackage(bytes){
 const checked=await validateStockPackage(bytes);
 if(!/^FM-1_0(?:15|16)$/.test(checked.product))throw new Error('Select original FM-1 V15 or V16 firmware, not an SMK package. Nothing was written.');
 return {...checked,factory:true,version:`Factory V${Number(checked.product.split('_')[1])}`};
}
// INFO extension parser, after Skiff's index_pkg.html (GPL-3.0-only).
export function archiveInfo(a){
 let pos=a.indexOf(0);if(pos<0)throw new Error('Invalid firmware INFO reply');
 const version=String.fromCharCode(...a.slice(0,pos));pos++;
 const engines=a[pos];pos+=5;
 for(let i=0;i<engines;i++){const end=a.indexOf(0,pos);if(end<0)throw new Error('Invalid firmware INFO reply');pos=end+1;}
 const ext=a.slice(pos+1);
 const supported=/^(?:FELUCCA|STATTIC) /i.test(version)&&ext[0]===16&&ext[1]===0x55&&ext[2]===1&&ext[4]===0x4d&&ext[5]===1&&ext[8]===0x42&&ext[9]===1&&!!(ext[10]&1);
 return {version,supported};
}
export class InstallSession{
 constructor(updater,image,config,{download,capture=captureBackup,connection=(i,o)=>new BackupConnection(i,o)}={}){
  this.updater=updater;this.image=image;this.config=config;this.download=download;this.capture=capture;this.connection=connection;this.mode=null;this.archive=null;this.busy=false;this.ports=null;
 }
 async prepare(progress=()=>{}){
  if(this.busy)throw new Error('An operation is already running.');
  this.busy=true;this.mode=null;this.archive=null;
  try{
   let dev=await this.updater.find();
   if(!dev){await new Promise(r=>setTimeout(r,800));dev=await this.updater.find();}   // ((2026-11-09) a second look before giving up)
   if(!dev)throw new Error('FM-1 not found. Check USB, stop playback and close other MIDI apps.');
   const input=dev.link.input,output=dev.link.output;dev.link.close();
   if(!sameModel(dev.id.model,'FM-1'))throw new Error('STATTIC is for the FM-1 only. No update was started.');
   if(this.config.factory&&dev.id.text===this.config.product)throw new Error('This FM-1 already runs the selected factory firmware. Nothing was written.');
   if(this.config.factory&&IS_OTA(dev.id))throw new Error('Finish the interrupted update first. Factory return needs running firmware to check the matching startup head. Nothing was written.');
   this.ports={input:input.id,output:output.id};this.device=dev.id.text;
   if(IS_OTA(dev.id)){this.mode='resume';return {mode:this.mode,device:this.device};}
   let link=this.connection(input,output);
   try{
    let reply;
    try{reply=await link.request([1,[]],/^FM-1_0(?:15|16)$/.test(dev.id.text)?{tries:1,timeout:1500}:{});}   // (factory firmware does not answer: no retries)
    catch(e){if(e.code==='timeout'&&/^FM-1_0(?:15|16)$/.test(dev.id.text)){this.mode='external';return {mode:this.mode,device:this.device};}throw e;}
    const info=archiveInfo(reply);
    if(/STATTIC 0\.9\.0-S1/.test(info.version)){
      const adapter={call:(cmd,args=[])=>link.request([cmd,args])};
      const archive=await captureNativeBackup(adapter,info.version,progress);validateBackup(archive);
      await this.download(archive);this.archive=archive;this.mode='backup';
      return {mode:this.mode,device:this.device,archive};
    }
    if(!info.supported){this.mode='external';return {mode:this.mode,device:this.device};}
    const archive=await this.capture((r,o)=>link.request(r,o),info.version,progress);
    readBackup(archive); // a failed/incomplete export never enables installation
    await this.download(archive);this.archive=archive;this.mode='backup';
    return {mode:this.mode,device:this.device,archive};
   }finally{link.close();}
  }finally{this.busy=false;}
 }
 async install({backupConfirmed=false,riskConfirmed=false,externalConfirmed=false,onStep=()=>{}}={}){
  if(this.busy)throw new Error('An operation is already running.');
  if(!this.mode)throw new Error('Connect and complete the backup step first.');
  if(!backupConfirmed||!riskConfirmed||(this.mode!=='backup'&&!externalConfirmed))throw new Error('Confirm that your backup is saved on this computer before writing.');
  if(this.mode==='backup'&&!this.archive)throw new Error('The backup is missing. Nothing was written.');
  this.busy=true;
  try{
   const dev=await this.updater.find();if(!dev)throw new Error('Reconnect the same FM-1 and start the backup step again.');
   const same=sameModel(dev.id.model,'FM-1')&&dev.link.input.id===this.ports.input&&dev.link.output.id===this.ports.output;
   const ota=IS_OTA(dev.id);dev.link.close();
   if(!same||ota!==(this.mode==='resume')){this.mode=null;throw new Error('The connected device or update mode changed. Start again and back up that device. Nothing was written.');}
   if(ota){if(!await this.updater.resume(this.image,onStep,this.config.product))throw new Error('The interrupted update did not finish. Keep the backup and follow the recovery guide.');}
   else await this.updater.install(this.image,this.config.product,onStep);
   this.mode=null;return true;
  }finally{this.busy=false;}
 }
}
