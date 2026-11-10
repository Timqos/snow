// SPDX-License-Identifier: GPL-3.0-only
// USB CDC diagnostics for the existing STATTIC console. No device writes or resets.
export const VERSION = '5';
export const MAX_RAW = 2 * 1024 * 1024;
export const MAX_SAMPLES = 6000;
const encoder = new TextEncoder();
const PROMPT = /(?:^|\r?\n)> $/;
// Native SLOOP and S1 omit the legacy "v" prefix; S1 also includes its
// product name in FELUCCA_VERSION. Match a complete known-console banner.
const CONSOLE_GREETING = /^(?:Felucca|STATTIC)[ \t]+(?:(?:SLOOP|STATTIC)[ \t]+)?v?\d+(?:\.\d+)+(?:[-+][\w.-]+)?(?:[ \t]+BETA)?[ \t]+console(?:[ \t]+-[ \t]+'help')?\r?$/im;
export const u32 = n => Number(n) >>> 0;

export function parseStatus(text) {
  const result = {};
  for (const line of text.split(/\r?\n/)) {
    const version = /^(?:felucca|stattic)\s+(.+?)\s*$/i.exec(line.trim());
    if (version) { result.version = version[1]; continue; }
    const match = /^([a-z][a-z0-9_]*)\s+(.+)$/.exec(line.trim());
    if (!match || match[1] === 'status') continue;
    result[match[1]] = /^-?\d+$/.test(match[2]) ? Number(match[2]) : match[2];
  }
  if (!result.version || !Number.isFinite(result.uptime_ms) || !Number.isFinite(result.audio_late))
    throw new Error('Incomplete firmware status. Capture paused; the raw reply is in the log.');
  if(result.diag_schema===3)result.cpu2=result.cpu1===3?1:0;
  return result;
}

export function parseMemory(text, region) {
  const bytes = new Map();
  for (const line of text.split(/\r?\n/)) {
    const match = /^([0-9a-f]{8}):((?: [0-9a-f]{2})+)\s*$/i.exec(line.trim());
    if (!match) continue;
    const address = parseInt(match[1], 16);
    match[2].trim().split(/ +/).forEach((v, i) => bytes.set(address + i, parseInt(v, 16)));
  }
  return Uint8Array.from({length: region.size}, (_, i) => {
    const value = bytes.get(region.address + i);
    if (value === undefined) throw new Error('Incomplete diagnostic memory reply.');
    return value;
  });
}

export function validateMap(map) {
  if (!map || typeof map.version !== 'string' || !/^[a-f0-9]{64}$/.test(map.package_sha256 || '') ||
      map.ticks_per_us !== 24 || map.half_frames !== 256 || map.sample_rate !== 44100)
    throw new Error('Unsupported diagnostic map.');
  const sizes = {late_base:4, peak:4, had_late:1, event:20, late_counter:4, rendered_voices:6, part_cost:12};
  if(map.detail_schema!==undefined){
    if(map.detail_schema!==1)throw new Error('Unsupported diagnostic detail schema.');
    sizes.detail=48;
  }else if(map.regions?.detail)throw new Error('Unversioned diagnostic detail region.');
  for (const [name, size] of Object.entries(sizes)) {
    const r = map.regions?.[name];
    if (!r || !Number.isInteger(r.address) || r.size !== size || r.address < 0x1c00000 || r.address + size > 0x1c80000)
      throw new Error('Invalid diagnostic region: ' + name);
  }
  return map;
}

export class SerialConsole {
  constructor(port, {onRaw=()=>{}, onDisconnect=()=>{}, timeout=4000}={}) {
    this.port=port;this.onRaw=onRaw;this.onDisconnect=onDisconnect;this.timeout=timeout;
    this.buffer='';this.pending=null;this.map=null;this.closed=true;this.reader=null;this.writer=null;
  }
  async open() {
    await this.port.open({baudRate:115200, bufferSize:16384, flowControl:'none'});
    this.closed=false;this.writer=this.port.writable.getWriter();
    const greeting=this.waitPrompt().then(value=>({value}),error=>({error}));
    this.readTask=this.readLoop();
    try {
      // The FM-1 CDC handler only records DTR; it does not reset the device.
      await this.port.setSignals({dataTerminalReady:true});
      const result=await greeting;if(result.error)throw result.error;const text=result.value;
      if (!CONSOLE_GREETING.test(text))
        throw new Error('This port did not identify itself as the FM-1 firmware console.');
    } catch (error) { await this.close();throw error; }
  }
  waitPrompt() {
    if (this.pending) throw new Error('A console request is already running.');
    return new Promise((resolve,reject) => {
      const timer=setTimeout(() => {
        this.pending=null;
        reject(new Error('USB console timed out. Disconnect and reconnect before continuing.'));
      },this.timeout);
      this.pending={resolve,reject,timer};this.deliver();
    });
  }
  deliver() {
    if (!this.pending || !PROMPT.test(this.buffer)) return;
    const reply=this.buffer;this.buffer='';const p=this.pending;this.pending=null;
    clearTimeout(p.timer);p.resolve(reply);
  }
  fail(error) {
    if (this.pending) {clearTimeout(this.pending.timer);this.pending.reject(error);this.pending=null;}
  }
  async readLoop() {
    const decoder=new TextDecoder();
    try {
      this.reader=this.port.readable.getReader();
      while (!this.closed) {
        const {value,done}=await this.reader.read();if(done)break;
        const text=decoder.decode(value,{stream:true});this.onRaw(text);
        this.buffer+=text;
        if (this.buffer.length>262144) throw new Error('Unexpectedly large console reply.');
        this.deliver();
      }
      if(!this.closed)throw new Error('USB device disconnected.');
    } catch(error) {
      if(!this.closed){this.fail(error);this.onDisconnect(error);}
    } finally {this.reader?.releaseLock();this.reader=null;}
  }
  setMap(map) {this.map=map ? validateMap(map) : null;}
  async request(command) {
    if(this.closed)throw new Error('USB console is disconnected.');
    const memory=/^memr 0x([0-9a-f]+) (\d+)$/.exec(command);
    const allowed=command==='status'||command==='crash'||(memory&&this.map&&
      Object.values(this.map.regions).some(r=>r.address===parseInt(memory[1],16)&&r.size===Number(memory[2])));
    if(!allowed)throw new Error('Only status, crash and mapped diagnostic reads are permitted.');
    if(this.pending)throw new Error('A console request is already running.');
    this.buffer='';const reply=this.waitPrompt();
    // Install the rejection handler before a potentially slow USB write.
    const settled=reply.then(value=>({value}),error=>({error}));
    try {this.writer.write(encoder.encode(command+'\r')).catch(error=>this.fail(error));}
    catch(error){this.fail(error);}
    const result=await settled;if(result.error)throw result.error;return result.value;
  }
  async memory(name) {
    const r=this.map?.regions[name];if(!r)throw new Error('No matching firmware diagnostic map.');
    return parseMemory(await this.request(`memr 0x${r.address.toString(16)} ${r.size}`),r);
  }
  async close() {
    if(this.closed)return;
    this.closed=true;this.fail(new Error('USB console closed.'));
    try{await this.reader?.cancel();}catch{}
    try{await this.readTask;}catch{}
    try{await this.port.setSignals({dataTerminalReady:false});}catch{}
    try{this.writer?.releaseLock();}catch{}
    this.writer=null;try{await this.port.close();}catch{}
  }
}

const word = bytes => new DataView(bytes.buffer,bytes.byteOffset,bytes.byteLength).getUint32(0,true);
const same = (a,b) => a.length===b.length&&a.every((x,i)=>x===b[i]);
export async function captureSample(link, maps, previous=null) {
  const started=new Date().toISOString(),status=parseStatus(await link.request('status'));
  const map=maps.find(m=>m.version===status.version)||null;link.setMap(map);
  status.uptime_ms=u32(status.uptime_ms);
  const reboot=!!previous&&(status.boots!==previous.status.boots||u32(status.uptime_ms-previous.status.uptime_ms)>0x80000000);
  const sample={started,finished:null,status,reboot,raw_late:u32(status.audio_late),late_scope:'since connection',stages:null};
  if(status.diag_schema===3){
    sample.late_base=0;sample.late_scope='since cold boot';sample.late=sample.raw_late;
    sample.stages={consistent:true,peak_pct:status.audio_max_us/status.audio_half_us*100,last_pct:status.audio_last_us/status.audio_half_us*100,irq_us:status.late_nested_us,mode:status.late_snapshot_count?'latest missed DMA half':'last completed DMA half',half_us:status.audio_half_us,
      ch_synth_us:status.late_snapshot_count?status.late_ch_synth_us:status.ch_synth_us,ch_fx_mix_us:status.late_snapshot_count?status.late_ch_fx_us:status.ch_fx_us,
      pre_worker_events_us:status.late_snapshot_count?status.late_events_us:status.events_last_us,shared_mix_us:status.late_snapshot_count?status.late_buses_us:status.buses_last_us,
      wall_us:status.late_wall_us,nested_us:status.late_nested_us,late_snapshot_count:status.late_snapshot_count,
      worker_wait_us:status.cpu1_wait_pct*status.audio_half_us/100};
    sample.stages.parts=['dr','bs','sy','ch'].map(role=>({role,synth_us:status[role==="dr"?'dr_player_us':role+'_synth_us'],fx_us:status[role+'_fx_us'],total_max_us:status[role+'_total_max_us']}));
  }else if(map){
    sample.map={version:map.version,package_sha256:map.package_sha256,source_commit:map.source_commit,
      selected_by:'reported version; device does not report a package hash'};
    sample.late_base=word(await link.memory('late_base'));sample.late_scope='since boot';
    for(let attempt=0;attempt<2;attempt++){
      const before=word(await link.memory('late_counter'));
      const peak=word(await link.memory('peak')),had=(await link.memory('had_late'))[0];
      const first=await link.memory('event'),detail=map.detail_schema?await link.memory('detail'):null;
      const second=await link.memory('event'),detail2=map.detail_schema?await link.memory('detail'):null;
      const after=word(await link.memory('late_counter'));sample.raw_late=after;
      if(before===after&&same(first,second)&&(!detail||same(detail,detail2))&&had<=1&&peak>=word(first)){
        const view=new DataView(first.buffer,first.byteOffset,first.byteLength),us=Math.floor(map.half_frames*1e6/map.sample_rate);
        sample.stages={consistent:true,mode:had?'latest missed half':'busiest half before first miss',
          peak_ticks:peak,total_ticks:view.getUint32(0,true),
          peak_pct:peak/(us*map.ticks_per_us)*100,last_pct:view.getUint32(0,true)/(us*map.ticks_per_us)*100,
          ch_synth_us:view.getUint32(4,true)/map.ticks_per_us,ch_fx_mix_us:view.getUint32(8,true)/map.ticks_per_us,
          worker_wait_us:view.getUint32(12,true)/map.ticks_per_us,irq_us:view.getUint32(16,true)/map.ticks_per_us};
        if(detail){
          const extra=new DataView(detail.buffer,detail.byteOffset,detail.byteLength),ticks=map.ticks_per_us;
          sample.stages.track_wall_us=Array.from({length:6},(_,i)=>extra.getUint32(i*4,true)/ticks);
          sample.stages.serial_us=extra.getUint32(24,true)/ticks;
          sample.stages.primary_work_us=extra.getUint32(28,true)/ticks;
          sample.stages.worker_work_us=extra.getUint32(32,true)/ticks;
          sample.stages.worker_tracks=extra.getUint32(36,true);
          sample.stages.primary_tracks=extra.getUint32(40,true);
          sample.stages.primary_irq_overhead_pct=extra.getUint32(44,true)*100/256;
        }
        break;
      }
    }
    if(!sample.stages)sample.stages={consistent:false,reason:'Snapshot was unstable or invalid in both reads; raw replies retained.'};
    sample.current_rendered_voices=Array.from(await link.memory('rendered_voices'));
    const cost=await link.memory('part_cost'),v=new DataView(cost.buffer,cost.byteOffset,cost.byteLength);
    sample.current_track_estimate_us=Array.from({length:6},(_,i)=>v.getUint16(i*2,true)/map.ticks_per_us);
    sample.late=u32(sample.raw_late-sample.late_base);
  }else if(status.diag_schema!==3){
    sample.connection_late_base=reboot?sample.raw_late:previous?.connection_late_base??sample.raw_late;
    sample.late=u32(sample.raw_late-sample.connection_late_base);
  }
  sample.delta_late=previous&&!reboot?u32(sample.raw_late-previous.raw_late):0;
  sample.finished=new Date().toISOString();return sample;
}

export class CaptureLog {
  constructor(){this.started=new Date().toISOString();this.raw='';this.raw_dropped=0;this.samples=[];this.events=[];this.samples_dropped=0;this.events_dropped=0;}
  addRaw(text){this.raw+=text;if(this.raw.length>MAX_RAW){const n=this.raw.length-MAX_RAW;this.raw=this.raw.slice(n);this.raw_dropped+=n;}}
  event(type,detail={}){this.events.push({time:new Date().toISOString(),type,...detail});if(this.events.length>MAX_SAMPLES){this.events.shift();this.events_dropped++;}}
  sample(sample){this.samples.push(sample);if(this.samples.length>MAX_SAMPLES){this.samples.shift();this.samples_dropped++;}
    if(sample.reboot)this.event('device-restarted');if(sample.delta_late)this.event('missed-audio-deadline',{count:sample.delta_late,late:sample.late,stages:sample.stages});}
  export(){return {schema:1,tool:'STATTIC USB diagnostics',tool_version:VERSION,started:this.started,exported:new Date().toISOString(),
    raw_dropped_chars:this.raw_dropped,samples_dropped:this.samples_dropped,events_dropped:this.events_dropped,samples:this.samples,events:this.events,raw:this.raw};}
}
