// SPDX-License-Identifier: GPL-3.0-only
import {Updater} from './fm1ota.js';
import {verifyPackage,InstallSession} from './core.js';
const $=id=>document.getElementById(id);
let config,image,session,writing=false,connecting=false;
const say=text=>{$('install-status').textContent=text;};
const log=text=>{$('log').textContent+=text+'\n';};
function saveArchive(archive){
 const url=URL.createObjectURL(new Blob([JSON.stringify(archive,null,2)],{type:'application/json'}));
 const a=document.createElement('a');a.href=url;a.download=`Snow-backup-${new Date().toISOString().replace(/[:.]/g,'-')}.json`;document.body.append(a);a.click();a.remove();setTimeout(()=>URL.revokeObjectURL(url),1000);
}
function update(){
 const ready=!!session?.mode&&!writing&&!connecting;
 for(const id of ['backup-confirmed','risk-confirmed','external-confirmed'])$(id).disabled=!ready;
 $('connect').disabled=!image||writing||connecting||!navigator.requestMIDIAccess;
 $('install').disabled=!ready||!$('backup-confirmed').checked||!$('risk-confirmed').checked||(session.mode!=='backup'&&!$('external-confirmed').checked);
 $('download-backup').disabled=writing||connecting;
}
for(const id of ['backup-confirmed','risk-confirmed','external-confirmed'])$(id).onchange=update;
window.addEventListener('beforeunload',event=>{if(writing){event.preventDefault();event.returnValue='An FM-1 firmware write is in progress.';}});
$('download-backup').onclick=()=>{if(session?.archive&&!writing)saveArchive(session.archive);};
$('connect').onclick=async()=>{
 if(connecting||writing||!image)return;connecting=true;session=null;
 for(const id of ['backup-confirmed','risk-confirmed','external-confirmed'])$(id).checked=false;
 $('download-backup').hidden=true;$('external-notice').hidden=true;$('external-label').hidden=true;$('backup-result').textContent='';update();
 try{
  const midi=await navigator.requestMIDIAccess({sysex:true});
  session=new InstallSession(new Updater(midi,log),image,config,{download:saveArchive});
  const result=await session.prepare((done,total)=>say(`Saving and validating the complete backup: ${Math.round(done/Math.max(1,total)*100)}%`));
  $('external-notice').hidden=result.mode==='backup';$('external-label').hidden=result.mode==='backup';
  $('backup-result').textContent=result.mode==='backup'?`${result.device}: complete backup downloaded and validated. Locate the JSON file in Downloads before confirming below.`:result.mode==='resume'?`${result.device}: update loader detected. A backup cannot be exported now. Confirm your existing backup to resume.`:`${result.device}: automatic complete backup is unavailable. Export using this firmware’s own tools before continuing.`;
  $('download-backup').hidden=!session.archive;say('Review the backup and installation notices below. Nothing has been written.');
 }catch(error){session=null;say(error.message||'Connection or backup failed. No firmware was written.');log(String(error));}
 finally{connecting=false;update();}
};
$('install').onclick=async()=>{
 if($('install').disabled||writing||!session)return;writing=true;update();
 try{
  const done=await session.install({backupConfirmed:$('backup-confirmed').checked,riskConfirmed:$('risk-confirmed').checked,externalConfirmed:$('external-confirmed').checked,onStep:(step,value)=>{
   log(`${step}${value===undefined?'':' '+value}`);
   const text={start:'Starting the model-checked update. Keep USB and power connected.',verify:'The running firmware is checking the package.',loader:'Waiting for the FM-1 update loader to reconnect…',write:'Writing Snow. Do not unplug, refresh or close this tab.',dropped:'The connection dropped. Waiting for the same loader to resume…',resume:'Resuming the interrupted write…',reboot:'Firmware written. Waiting for the FM-1 to restart…',done:'Installation completed. Check the version in MENU / ABOUT and keep your backup.'};say(text[step]||step);
   if(step==='verify')$('progress').value=Math.min(5,+value*5/80);if(step==='write')$('progress').value=5+(+value)*.95;if(step==='done')$('progress').value=100;
  }});
  if(done){$('progress').value=100;say('Snow installation completed. Check MENU / ABOUT on the FM-1. Keep your backup for rollback.');}
 }catch(error){say(`Update stopped: ${error.message}. Keep the backup and read the recovery section before retrying.`);log(String(error));}
 finally{writing=false;update();}
};
try{
 const res=await fetch('config.json',{cache:'no-store'});if(!res.ok)throw new Error('Cannot load the release configuration.');config=await res.json();
 const pkg=await fetch(config.package,{cache:'no-store'});if(!pkg.ok)throw new Error('Cannot download the firmware package.');
 image=await verifyPackage(new Uint8Array(await pkg.arrayBuffer()),config);
 say(navigator.requestMIDIAccess?`${config.version}: firmware identity and SHA-256 verified. Connect only the FM-1 you intend to update.`:'Web MIDI SysEx is unavailable. Use desktop Chrome or Edge on this HTTPS page.');
}catch(error){say(error.message+' No device connection or write was attempted.');}
update();
