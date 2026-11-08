// SPDX-License-Identifier: GPL-3.0-only
// STATTIC sample manager: connect, see all four user banks at once, add / replace / delete sounds, save a bank.
import {SMP,readSlot,buildEncodedSlot,convertShot} from '../shared/samples.mjs';
import {MidiLink,capabilities,listObjects,readObject,download,uploadSlot,captureNativeBackup,restoreBackup} from '../shared/midi.mjs';

const $=id=>document.getElementById(id);
const CAT=['KK','SD','HH','PR'],CAT_NAME=['kick','snare','hat','percussion'],BANKS=4;
const FM1_PORT=/stattic|fm.?1|sloop|felucca/i;
let midi=null,link=null,version='',busy=false,replacing=null;
const device=Array.from({length:BANKS},()=>null);       // what the FM-1 holds (null: not read)
const work=Array.from({length:BANKS},()=>[]);           // what this page shows and would save
const dirty=Array(BANKS).fill(false);

const status=(text,error=false)=>{$('status').textContent=text;$('status').classList.toggle('error',error);};
const clone=zones=>zones.map(z=>({...z}));
const bytesOf=zones=>zones.reduce((n,z)=>n+z.data.length,0);
const live=zones=>zones.filter(z=>!z.deleted);

async function action(fn){
  if(busy)return;busy=true;update();
  try{await fn();}catch(e){status(e.message,true);}
  finally{busy=false;$('progress').value=0;update();}
}
function update(){
  $('connect').disabled=busy||!!link;$('reload').disabled=busy||!link;$('disconnect').disabled=busy||!link;
  $('open').disabled=busy||!!link||!midi;$('backup').disabled=busy||!link;$('restore').disabled=busy||!link;
  $('files').disabled=busy||!link;$('pick-label').classList.toggle('disabled',busy||!link);$('pick-label').style.opacity=busy||!link?.4:1;
  $('target').disabled=busy||!link||!!replacing;$('category').disabled=busy||!link||!!replacing;
  render();
}

/* ---------------------------------------------------------------- the four banks */
function render(){
  const box=$('banks');box.replaceChildren();
  for(let s=0;s<BANKS;s++){
    const zones=work[s],used=zones.length?512+bytesOf(zones):0,count=zones.length,card=document.createElement('div');
    card.className='bank'+(dirty[s]?' changed':'')+(+$('target').value===s&&link?' target':'');
    const top=document.createElement('div');top.className='bank-top';
    const name=document.createElement('span');name.className='bank-name';name.textContent=`USR ${s+1}`;
    const tag=document.createElement('span');tag.className='tag'+(dirty[s]?' dirty':device[s]?' ok':'');
    tag.textContent=!link&&!device[s]?'not read':dirty[s]?'changed · not saved':live(zones).length?'on the FM-1':'empty';
    top.append(name,tag);card.append(top);
    const meter=document.createElement('div');meter.className='meter'+(used>SMP.SLOT_SIZE||count>16?' full':'');
    const fill=document.createElement('i');fill.style.width=`${Math.min(100,used/SMP.SLOT_SIZE*100)}%`;meter.append(fill);card.append(meter);
    const counts=document.createElement('div');counts.className='counts';
    const per=CAT.map((c,i)=>live(zones).filter(z=>z.category===i).length);
    counts.innerHTML=`<span><b>${(used/1024).toFixed(1)}</b> / ${(SMP.SLOT_SIZE/1024).toFixed(0)} KiB</span><span><b>${live(zones).length}</b> / 16 sounds</span>`+
      CAT.map((c,i)=>`<span>${c} <b>${per[i]}</b></span>`).join('');
    card.append(counts);
    const list=document.createElement('div');list.className='shots';
    if(!live(zones).length){const e=document.createElement('div');e.className='empty';e.textContent=device[s]||!link?'Empty.':'';list.append(e);}
    zones.forEach((z,i)=>{
      if(z.deleted)return;
      const row=document.createElement('div');row.className='shot'+(z.isNew?' new':'');
      const cat=document.createElement('span');cat.className=`cat c${z.category}`;cat.textContent=CAT[z.category];cat.title=CAT_NAME[z.category];
      const label=document.createElement('span');label.className='name';label.textContent=`${z.name} · ${(z.n/SMP.RATE).toFixed(2)} s`;
      const play=document.createElement('button');play.textContent='▶';play.title='Listen';play.onclick=()=>preview(z);
      const rep=document.createElement('button');rep.textContent='Replace';rep.disabled=busy;rep.onclick=()=>startReplace(s,i);
      const del=document.createElement('button');del.textContent='✕';del.title='Delete';del.disabled=busy;del.onclick=()=>remove(s,i);
      row.append(cat,label,play,rep,del);list.append(row);
    });
    card.append(list);
    const acts=document.createElement('div');acts.className='bank-actions';
    const add=document.createElement('button');add.textContent='Add sounds here';add.disabled=busy||!link;
    add.onclick=()=>{$('target').value=s;cancelReplace();render();$('add-step').scrollIntoView({behavior:'smooth'});};
    const save=document.createElement('button');save.className='primary';save.textContent='Save to FM-1';save.disabled=busy||!link||!dirty[s];save.onclick=()=>action(()=>saveBank(s));
    const undo=document.createElement('button');undo.textContent='Undo changes';undo.disabled=busy||!dirty[s];
    undo.onclick=()=>{work[s]=clone(device[s]||[]);dirty[s]=false;cancelReplace();render();status(`USR ${s+1}: changes undone.`);};
    acts.append(add,save,undo);card.append(acts);
    const file=document.createElement('div');file.className='bank-file';
    const dl=document.createElement('a');dl.textContent='Download as a bank file';
    dl.onclick=()=>{if(live(work[s]).length)download(`STATTIC-USR${s+1}.stbank`,buildEncodedSlot('STATTIC',work[s]));else status(`USR ${s+1} is empty.`,true);};
    const open=document.createElement('label');open.textContent='Open a bank file';const input=document.createElement('input');input.type='file';input.accept='.stbank';
    input.onchange=()=>action(async()=>{const zones=readSlot(new Uint8Array(await input.files[0].arrayBuffer()));work[s]=zones.map(z=>({...z,isNew:true}));dirty[s]=true;
      status(`USR ${s+1}: bank file opened (${live(zones).length} sounds). Press “Save to FM-1” to put it on the device.`);});
    open.append(input);file.append(dl,open);card.append(file);
    box.append(card);
  }
}

/* ---------------------------------------------------------------- connect and read */
function pickPorts(){
  const ins=[...midi.inputs.values()],outs=[...midi.outputs.values()];
  for(const [id,ports] of [['input',ins],['output',outs]]){
    $(id).replaceChildren();
    for(const p of ports){const o=document.createElement('option');o.value=p.id;o.textContent=p.name;$(id).append(o);}
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
async function readAll(){
  const c=await capabilities(link);if(c.playing)throw new Error('Press STOP on the FM-1, then “Read again from the FM-1”.');
  const list=await listObjects(link);
  for(let s=0;s<BANKS;s++){
    status(`Reading USR ${s+1}…`);
    const entry=list.find(e=>e.id===32+s),bytes=entry&&entry.length?await readObject(link,entry,(n,t)=>$('progress').value=(s+n/t)/BANKS*100):new Uint8Array();
    device[s]=readSlot(bytes);work[s]=clone(device[s]);dirty[s]=false;
  }
  cancelReplace();
  status(`Connected: ${version}. All four banks read from the FM-1.`);
}
$('connect').onclick=()=>action(async()=>{
  if(!navigator.requestMIDIAccess)throw new Error('This browser has no Web MIDI. Use desktop Chrome or Edge.');
  midi=await navigator.requestMIDIAccess({sysex:true});
  const pairs=pickPorts();let last=null;
  for(const [i,o] of pairs){try{status(`Trying ${i.name}…`);await openLink(i,o);break;}catch(e){last=e;}}
  if(!link){$('ports').open=true;throw new Error(pairs.length?`The FM-1 did not answer (${last?.message||'no reply'}). Check that STATTIC is installed, or choose its ports below.`:'No FM-1 found. Connect it by USB, or choose its ports below.');}
  await readAll();
});
$('open').onclick=()=>action(async()=>{
  const i=midi.inputs.get($('input').value),o=midi.outputs.get($('output').value);
  if(!i||!o)throw new Error('Choose an input and an output.');
  await openLink(i,o);$('ports').open=false;await readAll();
});
$('reload').onclick=()=>action(async()=>{
  if(dirty.some(Boolean)&&!confirm('Reading again drops the changes you have not saved. Continue?'))return;
  await readAll();
});
$('disconnect').onclick=()=>{link?.close();link=null;device.fill(null);for(let s=0;s<BANKS;s++){work[s]=[];dirty[s]=false;}cancelReplace();status('Disconnected.');update();};

/* ---------------------------------------------------------------- add, replace, delete */
function startReplace(s,i){
  replacing={s,i};const z=work[s][i];
  $('target').value=s;$('category').value=z.category;$('files').multiple=false;
  $('replacing-text').textContent=`Replacing ${z.name} in USR ${s+1}: choose one file (it stays a ${CAT_NAME[z.category]}, in the same place).`;
  $('replacing').classList.remove('hidden');update();$('add-step').scrollIntoView({behavior:'smooth'});
}
function cancelReplace(){replacing=null;$('files').multiple=true;$('replacing').classList.add('hidden');}
$('cancel-replace').onclick=()=>{cancelReplace();update();};
$('target').onchange=()=>render();
function remove(s,i){
  const z=work[s][i];
  work[s][i]={category:z.category,n:2,data:Uint8Array.of(0),deleted:true,name:'Empty'};   // (the place stays: later sounds keep theirs)
  while(work[s].at(-1)?.deleted)work[s].pop();
  dirty[s]=true;cancelReplace();render();status(`USR ${s+1}: ${z.name} deleted here. Press “Save to FM-1” to delete it on the device.`);
}
$('files').onchange=()=>action(async()=>{
  const files=[...$('files').files];$('files').value='';if(!files.length)return;
  const s=+$('target').value,duration=+$('duration').value;
  if(replacing){
    if(files.length!==1)throw new Error('Choose one file to replace a sound.');
    const {s:rs,i}=replacing,old=work[rs][i],z=await convertShot(files[0],old.category,duration),next=clone(work[rs]);
    next[i]={...z,isNew:true,name:short(files[0].name)};
    if(bytesOf(next)>SMP.MAX_DATA)throw new Error(`That file does not fit in USR ${rs+1}. Choose a shorter one, or “Longest sound” 0.5 s.`);
    work[rs]=next;dirty[rs]=true;cancelReplace();
    status(`USR ${rs+1}: ${old.name} replaced by ${short(files[0].name)} here. Press “Save to FM-1” to put it on the device.`);return;
  }
  const cat=+$('category').value,next=clone(work[s]),skipped=[];let added=0;
  for(const [k,f] of files.entries()){
    $('progress').value=k/files.length*100;status(`Converting ${f.name}…`);
    let z;try{z={...await convertShot(f,cat,duration),isNew:true,name:short(f.name)};}catch(e){skipped.push(`${f.name} (${e.message})`);continue;}
    const at=next.findIndex(o=>o.deleted&&o.category===cat),trial=clone(next);
    if(at<0)trial.push(z);else trial[at]=z;
    if(trial.length>16||bytesOf(trial)>SMP.MAX_DATA){skipped.push(`${f.name} (USR ${s+1} is full)`);continue;}
    next.splice(0,next.length,...trial);added++;
  }
  if(added){work[s]=next;dirty[s]=true;}
  const left=SMP.MAX_DATA-bytesOf(work[s]);
  status((added?`USR ${s+1}: ${added} ${CAT[cat]} sound${added>1?'s':''} added here, ${(left/1024).toFixed(1)} KiB and ${16-work[s].length} places left. Press “Save to FM-1” on USR ${s+1} to put ${added>1?'them':'it'} on the device.`:'Nothing was added.')+
    (skipped.length?` Not added: ${skipped.join('; ')}.`:''),!added);
});
const short=name=>name.replace(/\.[^.]+$/,'').slice(0,24);

/* ---------------------------------------------------------------- save */
async function saveBank(s){
  const c=await capabilities(link);if(c.playing)throw new Error('Press STOP on the FM-1 before saving.');
  const zones=work[s],bytes=live(zones).length?buildEncodedSlot('STATTIC',zones):new Uint8Array();
  const list=await listObjects(link),entry=list.find(e=>e.id===32+s);
  const had=!!(entry&&entry.length);
  if(had){const old=await readObject(link,entry);download(`STATTIC-USR${s+1}-before-${Date.now()}.stbank`,old);}
  status(`Saving USR ${s+1}. Keep USB connected…`);
  await uploadSlot(link,s,bytes,(n,t)=>$('progress').value=t?n/t*100:100);
  device[s]=readSlot(bytes);work[s]=clone(device[s]);dirty[s]=false;
  status(`USR ${s+1} saved on the FM-1 and checked by reading it back.${had?' A copy of the old bank was downloaded first (STATTIC-USR'+(s+1)+'-before-….stbank).':''}`);
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
  const c=await capabilities(link);if(c.playing)throw new Error('Press STOP before the backup.');
  status('Reading the backup…');
  const backup=await captureNativeBackup(link,c.version,(n,t)=>$('progress').value=n/Math.max(1,t)*100);
  download(`STATTIC-backup-${Date.now()}.json`,JSON.stringify(backup,null,2),'application/json');
  status('Full backup downloaded. Keep the JSON file.');
});
$('restore').onchange=()=>action(async()=>{
  const b=JSON.parse(await $('restore').files[0].text());
  status('Restoring the backup…');
  await restoreBackup(link,b,(n,t)=>$('progress').value=n/t*100);
  await readAll();status('Backup restored. Check the patterns and samples on the FM-1.');
});

update();
