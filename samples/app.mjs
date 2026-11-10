// SPDX-License-Identifier: GPL-3.0-only
// STATTIC samples: connect, four banks, "+ Add sounds" and "✕" write to the FM-1 at once. No separate save step.
import {SMP,readSlot,buildEncodedSlot,convertShot} from '../shared/samples.mjs';
import {MidiLink,capabilities,listObjects,readObject,download,uploadSlot,captureNativeBackup,restoreBackup} from '../shared/midi.mjs';

const $=id=>document.getElementById(id);
const CAT=['KK','SD','HH','PR'],CAT_LONG=['kick','snare','hat','percussion'],BANKS=4,LENGTH=1;   // longest sound, s
const FM1_PORT=/stattic|fm.?1|sloop|felucca/i;
let midi=null,link=null,version='',busy=false;
const banks=Array.from({length:BANKS},()=>null);        // the zones on the FM-1 (null: not read)
const choice=Array(BANKS).fill('auto');                 // the category a bank's next files get

let toastTimer=0;
const status=(text,error=false)=>{
  $('status').textContent=text;$('status').classList.toggle('error',error);
  const t=$('toast');t.textContent=text;t.classList.toggle('error',error);t.classList.add('show');
  clearTimeout(toastTimer);toastTimer=setTimeout(()=>t.classList.remove('show'),error?12000:6000);
};
const bytesOf=zones=>zones.reduce((n,z)=>n+z.data.length,0);
const live=zones=>zones.filter(z=>!z.deleted);
const places=zones=>16-zones.length+zones.filter(z=>z.deleted).length;
/* the category from the file's name: kick / bd / kk, snare / sd / sn / clap, hat / hh / cymbal; anything else percussion */
function guess(name){
  const n=name.toLowerCase();
  if(/kick|(^|[^a-z])(bd|kk)|bassdrum/.test(n))return 0;
  if(/snare|clap|rim|(^|[^a-z])(sd|sn|cp)/.test(n))return 1;
  if(/hat|(^|[^a-z])(hh|oh|ch)|cymb|ride|crash/.test(n))return 2;
  return 3;
}

async function action(fn){
  if(busy)return;busy=true;render();
  try{await fn();}catch(e){status(e.message,true);}
  finally{busy=false;$('progress').value=0;render();}
}
function render(){
  for(const id of ['backup','restore','disconnect'])$(id).disabled=busy||!link;
  $('connect').disabled=busy||!!link;$('open').disabled=busy||!!link||!midi;
  const box=$('banks');box.replaceChildren();
  if(!link)return;
  for(let s=0;s<BANKS;s++){
    const zones=banks[s]||[],free=SMP.MAX_DATA-bytesOf(zones),card=document.createElement('div');card.className='bank';
    const top=document.createElement('div');top.className='bank-top';
    top.innerHTML=`<span class="bank-name">USR ${s+1}</span><span class="bank-info">${live(zones).length} / 16 sounds · ${(Math.max(0,free)/1024).toFixed(1)} KiB free</span>`;
    const meter=document.createElement('div');meter.className='meter';meter.innerHTML=`<i style="width:${Math.min(100,(SMP.MAX_DATA-free)/SMP.MAX_DATA*100)}%"></i>`;
    const list=document.createElement('div');list.className='shots';
    if(!live(zones).length)list.innerHTML='<div class="empty">Empty</div>';
    zones.forEach((z,i)=>{
      if(z.deleted)return;
      const row=document.createElement('div');row.className='shot';
      row.innerHTML=`<span class="cat c${z.category}" title="${CAT_LONG[z.category]}">${CAT[z.category]}</span><span>${z.name} · ${(z.n/SMP.RATE).toFixed(2)} s</span>`;
      const play=document.createElement('button');play.textContent='▶';play.title='Listen';play.onclick=()=>preview(z);
      const del=document.createElement('button');del.textContent='✕';del.title='Delete from the FM-1';del.disabled=busy;
      del.onclick=()=>action(()=>removeShot(s,i));
      row.append(play,del);list.append(row);
    });
    const add=document.createElement('div');add.className='add-row';
    const full=places(zones)<=0||free<1500;
    const pick=document.createElement('label');pick.className='button primary';pick.textContent=full?'Bank full':'+ Add sounds';
    if(full||busy){pick.style.opacity=.4;pick.style.pointerEvents='none';}
    const input=document.createElement('input');input.type='file';input.multiple=true;input.accept='audio/*,.wav,.aif,.aiff,.flac,.mp3';
    input.onchange=()=>{const files=[...input.files];input.value='';if(files.length)action(()=>addShots(s,files));};
    pick.append(input);
    const as=document.createElement('select');as.title='The category the added sounds get';
    as.innerHTML='<option value="auto">category from the file name</option>'+CAT.map((c,i)=>`<option value="${i}">all as ${c} (${CAT_LONG[i]})</option>`).join('');
    as.value=choice[s];as.onchange=()=>{choice[s]=as.value;};as.disabled=busy||full;
    add.append(pick,as);
    card.append(top,meter,list,add);box.append(card);
  }
}

/* ---------------------------------------------------------------- connect */
function fillPorts(){
  const ins=[...midi.inputs.values()],outs=[...midi.outputs.values()];
  for(const [id,ports] of [['input',ins],['output',outs]]){
    $(id).replaceChildren();for(const p of ports){const o=document.createElement('option');o.value=p.id;o.textContent=p.name;$(id).append(o);}
  }
  const pairs=[];
  for(const i of ins.filter(p=>FM1_PORT.test(p.name)))for(const o of outs.filter(p=>FM1_PORT.test(p.name)))
    pairs.push([i,o,i.name===o.name||o.name.includes(i.name)||i.name.includes(o.name)?0:1]);
  pairs.sort((a,b)=>a[2]-b[2]);
  if(pairs[0]){$('input').value=pairs[0][0].id;$('output').value=pairs[0][1].id;}
  return pairs;
}
async function openLink(input,output){
  const next=new MidiLink(input,output);
  try{const c=await capabilities(next);if(!c.flash)throw new Error('The FM-1 memory is not available');link=next;version=c.version;}
  catch(e){next.close();throw e;}
}
async function stopped(){const c=await capabilities(link);if(c.playing)throw new Error('Press STOP on the FM-1 first.');}
async function readBank(s,list){
  const entry=(list||await listObjects(link)).find(e=>e.id===32+s);
  banks[s]=readSlot(entry&&entry.length?await readObject(link,entry):new Uint8Array());
}
async function readAll(){
  await stopped();const list=await listObjects(link);
  for(let s=0;s<BANKS;s++){status(`Reading USR ${s+1}…`);await readBank(s,list);$('progress').value=(s+1)/BANKS*100;}
  status(`Connected (${version}).`);
}
$('connect').onclick=()=>action(async()=>{
  if(!navigator.requestMIDIAccess)throw new Error('This browser has no Web MIDI. Use desktop Chrome or Edge.');
  midi=await navigator.requestMIDIAccess({sysex:true});
  const pairs=fillPorts();let last=null;
  for(const [i,o] of pairs){try{await openLink(i,o);break;}catch(e){last=e;}}
  if(!link){$('ports').open=true;throw new Error(pairs.length?`The FM-1 did not answer (${last?.message||'no reply'}). Is STATTIC installed? Or choose its ports under “FM-1 not found?”.`:'No FM-1 found. Connect it by USB, or choose its ports under “FM-1 not found?”.');}
  await readAll();
});
$('open').onclick=()=>action(async()=>{
  const i=midi.inputs.get($('input').value),o=midi.outputs.get($('output').value);
  if(!i||!o)throw new Error('Choose an input and an output.');
  await openLink(i,o);$('ports').open=false;await readAll();
});
$('disconnect').onclick=()=>{link?.close();link=null;banks.fill(null);status('Disconnected.');render();};

/* ---------------------------------------------------------------- add / delete: written at once */
async function write(s,zones,what){
  await stopped();
  status(`${what} Writing USR ${s+1} to the FM-1, keep USB connected…`);
  await uploadSlot(link,s,live(zones).length?buildEncodedSlot('STATTIC',zones):new Uint8Array(),(n,t)=>$('progress').value=t?n/t*100:100);
  await readBank(s);
}
async function addShots(s,files){
  await stopped();await readBank(s);                     // (what is on the FM-1 now)
  const next=banks[s].map(z=>({...z})),skipped=[];let added=0;
  for(const [k,f] of files.entries()){
    status(`Converting ${f.name} (${k+1} of ${files.length})…`);$('progress').value=k/files.length*100;
    const cat=choice[s]==='auto'?guess(f.name):+choice[s];
    let z;try{z=await convertShot(f,cat,LENGTH);}catch(e){skipped.push(`${f.name}: ${e.message}`);continue;}
    const at=next.findIndex(o=>o.deleted),trial=next.map(x=>x);
    if(at<0)trial.push(z);else trial[at]=z;
    if(trial.length>16||bytesOf(trial)>SMP.MAX_DATA){skipped.push(`${f.name}: no room left in USR ${s+1}`);continue;}
    next.splice(0,next.length,...trial);added++;
  }
  if(added)await write(s,next,'');
  const counts=CAT.map((c,i)=>live(banks[s]).filter(z=>z.category===i).length);
  status(added?`Done: ${added} sound${added>1?'s':''} added to USR ${s+1} on the FM-1 (now ${CAT.map((c,i)=>counts[i]?`${counts[i]} ${c}`:'').filter(Boolean).join(', ')}).`+
      (skipped.length?` Not added: ${skipped.join('; ')}.`:''):`Nothing added. ${skipped.join('; ')}.`,!added);
}
async function removeShot(s,i){
  await stopped();await readBank(s);
  const z=banks[s][i];if(!z||z.deleted)return;
  const next=banks[s].map(x=>({...x}));
  next[i]={category:z.category,n:2,data:Uint8Array.of(0),deleted:true,name:'Empty'};   // (the place stays: the others keep theirs)
  while(next.at(-1)?.deleted)next.pop();
  await write(s,next,'');
  status(`Done: ${z.name} deleted from USR ${s+1} on the FM-1.`);
}

/* ---------------------------------------------------------------- listen (IMA ADPCM, as the FM-1 plays it) */
const STEP=[7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,15289,16818,18500,20350,22385,24623,27086,29794,32767];
const IDX=[-1,-1,-1,-1,2,4,6,8];
let ctx=null;
function preview(z){
  const out=new Float32Array(z.n);let pred=0,idx=0;
  for(let n=0;n<z.n;n++){
    const code=(z.data[n>>1]>>((n&1)*4))&15,step=STEP[idx];let vd=step>>3;
    if(code&4)vd+=step;if(code&2)vd+=step>>1;if(code&1)vd+=step>>2;
    pred=Math.max(-32768,Math.min(32767,code&8?pred-vd:pred+vd));idx=Math.max(0,Math.min(88,idx+IDX[code&7]));out[n]=pred/32768;
  }
  ctx=ctx||new AudioContext();const buf=ctx.createBuffer(1,out.length,SMP.RATE);buf.copyToChannel(out,0);
  const src=ctx.createBufferSource();src.buffer=buf;src.connect(ctx.destination);src.start();
}

/* ---------------------------------------------------------------- full backup */
$('backup').onclick=()=>action(async()=>{
  await stopped();status('Reading the backup…');
  const backup=await captureNativeBackup(link,version,(n,t)=>$('progress').value=n/Math.max(1,t)*100);
  download(`STATTIC-backup-${Date.now()}.json`,JSON.stringify(backup,null,2),'application/json');
  status('Full backup downloaded. Keep the JSON file.');
});
$('restore').onchange=()=>action(async()=>{
  const b=JSON.parse(await $('restore').files[0].text());
  status('Restoring the backup…');
  await restoreBackup(link,b,(n,t)=>$('progress').value=n/t*100);
  await readAll();status('Backup restored. Check the patterns and samples on the FM-1.');
});

render();
