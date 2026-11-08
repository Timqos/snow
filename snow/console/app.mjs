// SPDX-License-Identifier: GPL-3.0-only
import {SerialConsole,CaptureLog,captureSample} from './core.mjs?v=3';
const $=id=>document.getElementById(id),names=['KK','SD','HH','PR','BS','CH'];
let log=new CaptureLog(),link=null,timer=null,busy=false,recording=false,last=null,misses=0,rawTimer=null;
const catalog=fetch('maps.json',{cache:'no-store'}).then(r=>{if(!r.ok)throw new Error('Diagnostic map unavailable.');return r.json();});
const text=(id,value)=>$(id).textContent=value;
const num=(value,decimals=0)=>Number.isFinite(value)?value.toLocaleString(undefined,{maximumFractionDigits:decimals}):'—';
function message(value,error=false){text('message',value);$('message').classList.toggle('error',error);}
function updateControls(){
  $('connect').disabled=!navigator.serial||!!link||busy;$('disconnect').disabled=!link;
  $('pause').disabled=!link;$('pause').textContent=recording?'Pause capture':'Resume capture';
  $('snapshot').disabled=!link||busy;$('crash').disabled=!link||busy;$('mark').disabled=!link;
  $('export-json').disabled=$('export-text').disabled=!(log.raw||log.events.length||log.samples.length);
  const samples=log.samples.length,marks=log.events.filter(e=>e.type==='rasp-marker').length;
  text('count',`${samples} sample${samples===1?'':'s'} · ${marks} mark${marks===1?'':'s'}`);
}
function renderRaw(){
  rawTimer=null;const pane=$('raw'),position=pane.scrollTop,follow=pane.scrollHeight-pane.clientHeight-position<36;
  text('raw',log.raw.slice(-100000)||'No USB data received.');pane.scrollTop=follow?pane.scrollHeight:position;updateControls();
}
function raw(chunk){log.addRaw(chunk);if(!rawTimer)rawTimer=setTimeout(renderRaw,100);}
function render(sample){
  text('cpu',num(sample.status.cpu_pct));text('late',num(sample.late));text('late-scope',sample.late_scope);
  text('misses',num(misses));text('delta',`${num(sample.delta_late)} since previous sample`);
  $('miss-card').classList.toggle('detected',misses>0);
  const clock=sample.status.cpu_mhz>0?`${sample.status.cpu_mhz} MHz`:'SLOOP clock configuration';
  text('firmware',`${sample.status.version} · ${sample.status.cpu2?'2 cores':'1 core'} · ${clock}`);
  text('shed-status',`Voice protection: ${num(sample.status.voices_shed)} forced voice fades since boot, across all channels.`);
  const p=sample.stages?.consistent?sample.stages:null;
  for(const [id,key] of [['peak','peak_pct'],['last','last_pct'],['synth','ch_synth_us'],['fx','ch_fx_mix_us'],['wait','worker_wait_us'],['irq','irq_us']])
    text(id,num(p?.[key],id==='peak'||id==='last'?1:0));
  text('stage-mode',p?p.mode:sample.map?'Snapshot changed while reading; raw replies saved.':'Basic status only: no matching official firmware map.');
  $('failure-detail').hidden=!p?.track_wall_us;
  $('failure-tracks').replaceChildren();
  p?.track_wall_us?.forEach((us,i)=>{
    const node=document.createElement('span'),bit=1<<i;
    const owner=(p.worker_tracks&bit)?(p.primary_tracks&bit)?'both cores in this half':'core 1':'core 0';
    node.textContent=`${names[i]}: ${num(us)} µs · ${owner}`;$('failure-tracks').append(node);
  });
  text('critical-path',p?.track_wall_us?`Serial events / mix / master: ${num(p.serial_us)} µs · Core 0 work: ${num(p.primary_work_us)} µs · Core 1 work: ${num(p.worker_work_us)} µs · Primary IRQ overhead model: ${num(p.primary_irq_overhead_pct,1)}%`:'');
  $('tracks').replaceChildren();
  p?.parts?.forEach(part=>{const node=document.createElement('span');node.textContent=`${part.role.toUpperCase()}: synthesis/player ${num(part.synth_us)} µs · FX ${num(part.fx_us)} µs · max ${num(part.total_max_us)} µs/half`;$('tracks').append(node);});
  sample.current_rendered_voices?.forEach((voices,i)=>{
    const node=document.createElement('span');node.textContent=`${names[i]}: ${voices} voice${voices===1?'':'s'} · ~${num(sample.current_track_estimate_us?.[i])} µs/block`;$('tracks').append(node);
  });
  $('timeline').replaceChildren();for(const s of log.samples.slice(-20).reverse()){
    const row=document.createElement('tr');if(s.delta_late)row.className='missed';
    const stage=s.stages?.consistent?s.stages:null;
    const values=[new Date(s.finished).toLocaleTimeString(),num(s.status.cpu_pct),num(s.late),num(s.delta_late),num(stage?.ch_synth_us),num(stage?.ch_fx_mix_us),num(stage?.worker_wait_us)];
    for(const value of values){const cell=document.createElement('td');cell.textContent=value;row.append(cell);}$('timeline').append(row);
  }
  updateControls();
}
function schedule(){clearTimeout(timer);if(recording&&link)timer=setTimeout(poll,Math.max(2000,Math.min(5000,Number($('interval').value)||2000)));}
async function poll(){
  if(!link||busy)return;busy=true;updateControls();const current=link;
  try{
    const {maps=[]}=await catalog.catch(()=>({maps:[]}));const sample=await captureSample(current,maps,last);
    if(current!==link)return;
    log.sample(sample);last=sample;misses+=sample.delta_late;render(sample);
    message(sample.delta_late?`Captured ${sample.delta_late} new missed buffer(s). Failure snapshot saved.`:`Snapshot saved at ${new Date(sample.finished).toLocaleTimeString()}.`);
  }catch(error){
    if(current===link){log.event('capture-error',{message:error.message});message(error.message,true);await disconnect(error.message);}
  }finally{busy=false;updateControls();schedule();}
}
async function disconnect(reason='Disconnected'){
  recording=false;clearTimeout(timer);const current=link;link=null;updateControls();
  if(current){log.event('disconnected',{reason});await current.close();}
  text('connection','Disconnected');updateControls();
}
$('connect').addEventListener('click',async()=>{
  if(!navigator.serial){message('Web Serial requires desktop Chrome or Edge on HTTPS.',true);return;}
  busy=true;updateControls();let current;
  try{
    const port=await navigator.serial.requestPort({filters:[{usbVendorId:0x1209,usbProductId:0x0001}]});
    current=new SerialConsole(port,{onRaw:raw,onDisconnect:error=>{message(error.message,true);void disconnect(error.message);}});
    link=current;text('connection','Connecting…');await current.open();
    if(link!==current){await current.close();return;}
    log.event('connected',{usb:port.getInfo?.()||{},user_agent:navigator.userAgent});last=null;
    recording=true;text('connection','Connected · capturing');message('Connected. Reading the existing firmware console.');
  }catch(error){message(error.name==='NotFoundError'?'USB port selection cancelled.':error.message,true);await disconnect(error.message);}
  finally{busy=false;updateControls();}
  if(recording)await poll();
});
$('disconnect').addEventListener('click',()=>void disconnect());
$('pause').addEventListener('click',()=>{
  recording=!recording;clearTimeout(timer);log.event(recording?'capture-resumed':'capture-paused');
  text('connection',recording?'Connected · capturing':'Connected · capture paused');updateControls();
  if(recording&&!busy)void poll();
});
$('snapshot').addEventListener('click',()=>void poll());
$('interval').addEventListener('change',()=>{log.event('poll-interval',{milliseconds:Number($('interval').value)});schedule();});
$('mark').addEventListener('click',()=>{
  log.event('rasp-marker',{note:$('note').value.trim(),nearest_sample_time:last?.finished||null,late:last?.late??null});
  message('Rasp marker saved. The next normal snapshot will follow; no extra USB burst.');updateControls();
});
$('crash').addEventListener('click',async()=>{
  if(!link||busy)return;busy=true;clearTimeout(timer);updateControls();const current=link;
  try{const reply=await current.request('crash');log.event('crash-report',{reply});message('Crash report saved in the raw log.');}
  catch(error){message(error.message,true);await disconnect(error.message);}
  finally{busy=false;updateControls();schedule();}
});
function download(extension,value,type){
  const blob=new Blob([value],{type}),url=URL.createObjectURL(blob),a=document.createElement('a');
  a.href=url;a.download=`stattic-usb-log-${new Date().toISOString().replace(/[:.]/g,'-')}.${extension}`;a.click();setTimeout(()=>URL.revokeObjectURL(url),10000);
}
$('export-json').addEventListener('click',()=>download('json',JSON.stringify(log.export(),null,2),'application/json'));
$('export-text').addEventListener('click',()=>{
  const report=log.export(),events=report.events.map(e=>JSON.stringify(e)).join('\n');
  download('txt',`STATTIC USB diagnostic log\nStarted: ${report.started}\nExported: ${report.exported}\nTrimmed: ${report.raw_dropped_chars} raw characters, ${report.samples_dropped} samples, ${report.events_dropped} events\n\nEVENTS\n${events}\n\nSAMPLES\n${report.samples.map(s=>JSON.stringify(s)).join('\n')}\n\nRAW USB\n${report.raw}\n`,'text/plain');
});
$('clear').addEventListener('click',()=>{
  log=new CaptureLog();misses=0;text('raw','Local log cleared.');text('timeline','');
  if(last)render(last);updateControls();message('Local log cleared. USB connection unchanged.');
});
if(!navigator.serial){
  $('connect').disabled=true;text('support','Open this page in desktop Chrome or Edge. Web Serial is unavailable in this browser.');
}
document.addEventListener('visibilitychange',()=>log.event(document.hidden?'tab-hidden':'tab-visible'));
window.addEventListener('beforeunload',()=>{if(link)void link.close();});
catalog.catch(error=>{message(error.message+' Basic status logging still works.',true);});
