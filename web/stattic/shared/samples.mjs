// SPDX-License-Identifier: GPL-3.0-only
// SLOOP editor conversion, adapted for STATTIC category libraries.
const SMP = { SLOT_SIZE: 0xF000, DATA_OFF: 512, RATE: 22050, HDR_LEN: 32 + 16 * 28, MAX_ZONES: 16 };
SMP.MAX_DATA = SMP.SLOT_SIZE - SMP.DATA_OFF;

/* groups of up to 7 bytes, each preceded by their top bits */
function pack7(b) {
  const out = [];
  for (let i = 0; i < b.length; i += 7) {
    const g = Array.from(b.slice(i, i + 7));
    out.push(g.reduce((m, x, j) => m | (((x >> 7) & 1) << j), 0));
    for (const x of g) out.push(x & 0x7F);
  }
  return out;
}
function unpack7(a) {
  const out = [];
  for (let i = 0; i < a.length; i += 8) {
    const top = a[i];
    for (let j = 0; j < 7 && i + 1 + j < a.length; j++) out.push(a[i + 1 + j] | (((top >> j) & 1) << 7));
  }
  return Uint8Array.from(out);
}

const CRC_TABLE = (() => {
  const t = new Uint32Array(256);
  for (let n = 0; n < 256; n++) { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320 ^ (c >>> 1) : c >>> 1; t[n] = c >>> 0; }
  return t;
})();
function crc32(b) { let c = 0xFFFFFFFF; for (let i = 0; i < b.length; i++) c = CRC_TABLE[(c ^ b[i]) & 0xFF] ^ (c >>> 8); return (c ^ 0xFFFFFFFF) >>> 0; }

const IMA_STEP = [7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88,
  97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658,
  724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660,
  4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818,
  18500, 20350, 22385, 24623, 27086, 29794, 32767];
const IMA_IDX = [-1, -1, -1, -1, 2, 4, 6, 8];
/* IMA ADPCM, 4 bit, low nibble first, from predictor 0 / index 0 -> {data, pred, idx} (state at loopStart) */
function imaEncode(s, loopStart = 0) {
  let pred = 0, idx = 0, atLoop = [0, 0];
  const nib = new Uint8Array(s.length + (s.length & 1));
  for (let n = 0; n < s.length; n++) {
    if (n === loopStart) atLoop = [pred, idx];
    const step = IMA_STEP[idx];
    let diff = s[n] - pred, code = 0;
    if (diff < 0) { code = 8; diff = -diff; }
    let vd = step >> 3;
    if (diff >= step) { code |= 4; diff -= step; vd += step; }
    if (diff >= step >> 1) { code |= 2; diff -= step >> 1; vd += step >> 1; }
    if (diff >= step >> 2) { code |= 1; vd += step >> 2; }
    pred = Math.max(-32768, Math.min(32767, code & 8 ? pred - vd : pred + vd));
    idx = Math.max(0, Math.min(88, idx + IMA_IDX[code & 7]));
    nib[n] = code;
  }
  const out = new Uint8Array(nib.length >> 1);
  for (let k = 0; k < nib.length; k += 2) out[k >> 1] = nib[k] | (nib[k + 1] << 4);
  return { data: out, pred: atLoop[0], idx: atLoop[1] };
}

/* WAV: PCM 8/16/24/32 or float, any channel count, mixed to mono -> {sr, x} (gen_samples.read_any_wav) */
function parseWav(buf) {
  const d = new Uint8Array(buf), v = new DataView(d.buffer, d.byteOffset, d.byteLength);
  const id = (i) => String.fromCharCode(d[i], d[i + 1], d[i + 2], d[i + 3]);
  if (d.length < 12 || id(0) !== "RIFF" || id(8) !== "WAVE") throw new Error("not a WAV file");
  let i = 12, fmt = null, data = null, loop = null;
  while (i + 8 <= d.length) {
    const cid = id(i), n = v.getUint32(i + 4, true);
    if (cid === "fmt ") {
      let tag = v.getUint16(i + 8, true);
      const ch = v.getUint16(i + 10, true), sr = v.getUint32(i + 12, true), bits = v.getUint16(i + 22, true);
      if (tag === 0xFFFE) tag = v.getUint16(i + 32, true);
      fmt = { tag, ch, sr, bits };
    } else if (cid === "data") {
      data = [i + 8, Math.min(d.length, i + 8 + n)];
    } else if (cid === "smpl" && n >= 60 && i + 8 + 60 <= d.length && v.getUint32(i + 8 + 28, true)) {
      loop = [v.getUint32(i + 8 + 44, true), v.getUint32(i + 8 + 48, true)];   /* (2.7) its first loop, in frames */
    }
    i += 8 + n + (n & 1);
  }
  if (!fmt || !data) throw new Error("WAV without fmt/data");
  const { tag, ch, sr, bits } = fmt, bps = bits >> 3, frame = bps * ch;
  if (!(tag === 1 || tag === 3) || !bps || !ch) throw new Error("unsupported WAV format");
  const nfr = Math.floor((data[1] - data[0]) / frame), x = new Float64Array(nfr);
  for (let f = 0; f < nfr; f++) {
    let acc = 0;
    for (let c = 0; c < ch; c++) {
      const o = data[0] + f * frame + c * bps;
      let s;
      if (tag === 3) s = bps === 4 ? v.getFloat32(o, true) : v.getFloat64(o, true);
      else if (bps === 2) s = v.getInt16(o, true) / 32768;
      else if (bps === 3) s = (((d[o] | (d[o + 1] << 8) | (d[o + 2] << 16)) << 8) >> 8) / 8388608;
      else if (bps === 4) s = v.getInt32(o, true) / 2147483648;
      else s = (d[o] - 128) / 128;
      acc += s;
    }
    x[f] = acc / ch;
  }
  return { sr, x, loop };
}

/* gen_samples.resample: moving average over the ratio, then linear interpolation */
function resample(x, sr, to) {
  if (sr === to) return Array.from(x);
  if (sr > to) {
    const k = Math.max(1, pyRound(sr / to));
    if (k > 1) {
      const y = new Array(x.length);
      for (let i = 0; i < x.length; i++) { let s = 0; for (let j = i; j < Math.min(x.length, i + k); j++) s += x[j]; y[i] = s / k; }
      x = y;
    }
  }
  const step = sr / to, out = [];
  let p = 0;
  while (p < x.length - 1) { const i = Math.floor(p), f = p - i; out.push(x[i] * (1 - f) + x[i + 1] * f); p += step; }
  return out;
}
function pyRound(v) { const f = Math.floor(v), r = v - f; return r > 0.5 ? f + 1 : r < 0.5 ? f : (f % 2 ? f + 1 : f); }

/* mono float -> int16, peak normalised to 30000 */
function normalize(x) {
  let pk = 1e-9;
  for (const v of x) pk = Math.max(pk, Math.abs(v));
  return Int16Array.from(x, (v) => Math.max(-32768, Math.min(32767, Math.trunc(v / pk * 30000))));
}

/* SLOOP 2.6: how loud a sound is heard, in dB: K-weighted (ITU-R BS.1770, the ear's curve: the lows count less, the
   presence more), the mean square over 100 ms windows, the loudest window (a hit is heard by its loudest moment, not
   by its average with the silence after; a click shorter than a window counts over the whole window, as the ear does) */
function loudDb(s, sr = SMP.RATE) {
  const bq = (G, Q, fc, shelf) => {                 /* the two stages' coefficients at this rate (pyloudnorm's) */
    const A = Math.pow(10, G / 40), w = 2 * Math.PI * fc / sr, c = Math.cos(w), al = Math.sin(w) / (2 * Q), r = 2 * Math.sqrt(A) * al;
    const k = shelf
      ? [A * ((A + 1) + (A - 1) * c + r), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - r),
         (A + 1) - (A - 1) * c + r, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - r]
      : [(1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al];
    return k.map((v) => v / k[3]);
  };
  const st = [bq(3.99984385397, 0.7071752369554193, 1681.9744509555319, true), bq(0, 0.5003270373253953, 38.13547087613982, false)];
  const y = new Float64Array(s.length);
  for (let i = 0; i < s.length; i++) y[i] = s[i] / 32768;
  for (const [b0, b1, b2, , a1, a2] of st) {
    let x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    for (let i = 0; i < y.length; i++) { const x = y[i], v = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2; x2 = x1; x1 = x; y2 = y1; y1 = v; y[i] = v; }
  }
  const W = Math.round(sr * 0.1), hop = Math.max(1, W >> 2);
  let best = 1e-12;
  for (let a = 0; a < Math.max(1, y.length - W + hop); a += hop) {
    let e = 0;
    for (let i = a; i < Math.min(y.length, a + W); i++) e += y[i] * y[i];
    best = Math.max(best, e / W);
  }
  return 10 * Math.log10(best) - 0.691;
}
/* gains (dB, -24..0) that bring sounds to the same heard level: the louder ones turned down to the quietest (every
   sound comes in peak-normalised, so turning up would clip); null entries stay null */
const LEVEL_MIN = -24;
function matchGains(sounds) {
  const L = sounds.map((x) => (x ? loudDb(x) : null)), q = L.filter((v) => v != null);
  if (!q.length) return L;
  const target = Math.max(Math.min(...q), Math.max(...q) + LEVEL_MIN);
  return L.map((v) => (v == null ? null : Math.max(LEVEL_MIN, Math.min(0, Math.round(target - v)))));
}
/* a sound with its gain (dB, 0 = as it came) */
function withGain(s, db) {
  if (!db) return s;
  const g = Math.pow(10, Math.max(LEVEL_MIN, Math.min(0, db)) / 20);
  return Int16Array.from(s, (v) => Math.round(v * g));
}

/* root from a file name like "piano_C4" or "F#3" (C4 = 60), else 60 (sampleio.note_from_name) */
function rootFromName(name) {
  const m = /(?<![A-Za-z])([A-G])([#b]?)(-?\d)(?!\d)/.exec(name);
  if (!m) return 60;
  const n = { C: 0, D: 2, E: 4, F: 5, G: 7, A: 9, B: 11 }[m[1]] + (m[2] === "#" ? 1 : m[2] === "b" ? -1 : 0) + (parseInt(m[3], 10) + 1) * 12;
  return Math.max(0, Math.min(127, n));
}

/* 2.7: a file's loop (the Samples page): 0 ONE SHOT; 1 LOOP, the whole file again while the key is held, let go it ends
   its round; 2 LOOP+TAIL, the start, then [ls, le] again while held, let go the rest to the end. Both are the zone's
   looped = 2 (SMP_LOOP_HELD, firmware eng_sample.c; SLOOP 2.6 loops them until the release ends the note) */
const LOOP={range:z=>[0,z.s.length-1]};
function buildSlot(name, zonesIn) {
  const zones = [], parts = [];
  let len = 0;
  for (const z of zonesIn) {
    const [ls, le] = z.loop ? LOOP.range(z) : [0, z.s.length - 1];
    const e = imaEncode(z.s, ls);                   /* (the decoder's state at the loop start) */
    zones.push({ off: len, n: z.s.length, ls, le, looped: z.loop ? 2 : 0, root: z.root, lo: z.lo ?? null, hi: z.hi ?? null,
      pred: e.pred, idx: e.idx });
    parts.push(e.data);
    len += e.data.length;
  }
  const data = new Uint8Array(len);
  let o = 0;
  for (const p of parts) { data.set(p, o); o += p.length; }
  if (len > SMP.MAX_DATA) throw new Error(`too long: ${len} B (max ${SMP.MAX_DATA} B)`);
  if (!zones.length || zones.length > SMP.MAX_ZONES) throw new Error("1..16 files per slot");
  zones.sort((a, b) => a.root - b.root);
  zones.forEach((z, j) => {
    if (z.lo == null) {
      z.lo = j === 0 ? 0 : Math.floor((zones[j - 1].root + z.root) / 2) + 1;
      z.hi = j === zones.length - 1 ? 127 : Math.floor((z.root + zones[j + 1].root) / 2);
    }
  });
  const hdr = new Uint8Array(SMP.HDR_LEN), v = new DataView(hdr.buffer);
  v.setUint32(0, 0x504D5346, true);
  v.setUint16(4, 1, true);
  hdr[6] = zones.length;hdr[7]=0x53;
  const nm = String(name).toUpperCase().replace(/[^\x20-\x7E]/g, "").slice(0, 8);
  for (let i = 0; i < nm.length; i++) hdr[8 + i] = nm.charCodeAt(i);
  v.setUint32(16, len, true);
  v.setUint32(20, crc32(data), true);
  const rate = pyRound(SMP.RATE / 44100 * 65536);
  zones.forEach((z, j) => {
    const b = 32 + j * 28;
    [z.off, z.n, z.ls, z.le, rate].forEach((x, k) => v.setUint32(b + k * 4, x >>> 0, true));
    v.setInt16(b + 20, z.root * 16, true);
    v.setInt16(b + 22, z.pred, true);
    hdr[b + 24] = z.idx; hdr[b + 25] = z.lo; hdr[b + 26] = z.hi; hdr[b + 27] = z.looped || 0;
  });
  return { hdr, data, zones };
}
export {SMP,pack7,unpack7,crc32,imaEncode,parseWav,resample,normalize,buildSlot};
export const CATEGORY_NOTES=[36,38,42,60];
export function readSlot(bytes){
  if(!bytes.length)return [];
  if(bytes.length<512)throw new Error('Incomplete sample bank');
  const v=new DataView(bytes.buffer,bytes.byteOffset,bytes.byteLength),count=bytes[6],length=v.getUint32(16,true);
  if(v.getUint32(0,true)!==0x504D5346||v.getUint16(4,true)!==1||bytes[7]!==0x53||!count||count>16||length>SMP.MAX_DATA||512+length!==bytes.length)throw new Error('This bank is not a STATTIC drum library');
  const data=bytes.slice(512);if(crc32(data)!==v.getUint32(20,true))throw new Error('Sample bank CRC mismatch');
  const zones=[];
  for(let i=0;i<count;i++){
    const b=32+i*28,off=v.getUint32(b,true),n=v.getUint32(b+4,true),cat=CATEGORY_NOTES.indexOf(bytes[b+25]);
    if(cat<0||bytes[b+26]!==bytes[b+25]||bytes[b+27]||n<2||off+Math.ceil(n/2)>data.length||v.getUint32(b+16,true)!==32768)throw new Error('Invalid category or sample range');
    zones.push({category:cat,n,deleted:n===2,data:data.slice(off,off+Math.ceil(n/2)),name:`${['KK','SD','HH','PR'][cat]} ${i+1}`});
  }return zones;
}
export function buildEncodedSlot(name,zones){
  if(!zones.length||zones.length>16)throw new Error('A bank needs 1–16 shots');
  const length=zones.reduce((n,z)=>n+z.data.length,0);
  if(length>SMP.MAX_DATA)throw new Error('The selected bank is full');
  const raw=new Uint8Array(512+length),v=new DataView(raw.buffer);raw.fill(255,480,512);
  v.setUint32(0,0x504D5346,true);v.setUint16(4,1,true);raw[6]=zones.length;raw[7]=0x53;
  [...name.toUpperCase().replace(/[^\x20-\x7e]/g,'').slice(0,8)].forEach((c,i)=>raw[8+i]=c.charCodeAt(0));v.setUint32(16,length,true);
  let off=0;zones.forEach((z,i)=>{
    if(!Number.isInteger(z.category)||z.category<0||z.category>3||z.data.length!==Math.ceil(z.n/2))throw new Error('Invalid shot');
    const b=32+i*28;[off,z.n,0,z.n-1,32768].forEach((x,j)=>v.setUint32(b+j*4,x,true));
    v.setInt16(b+20,60*16,true);raw[b+25]=raw[b+26]=CATEGORY_NOTES[z.category];
    raw.set(z.data,512+off);off+=z.data.length;
  });v.setUint32(20,crc32(raw.subarray(512)),true);return raw;
}
export async function convertShot(file,category,duration=.5){
  if(file.size>32*1024*1024)throw new Error('Use an audio file smaller than 32 MiB');
  const buffer=await file.arrayBuffer();let pcm;
  try{pcm=parseWav(buffer);}catch(error){
    const ctx=new AudioContext();try{
      const audio=await ctx.decodeAudioData(buffer.slice(0)),mono=new Float64Array(audio.length);
      for(let c=0;c<audio.numberOfChannels;c++){const data=audio.getChannelData(c);for(let i=0;i<data.length;i++)mono[i]+=data[i]/audio.numberOfChannels;}
      pcm={x:mono,sr:audio.sampleRate};
    }finally{await ctx.close();}
  }
  const s=normalize(resample(pcm.x,pcm.sr,SMP.RATE).slice(0,Math.round(SMP.RATE*duration)));
  if(s.length<22)throw new Error('The file contains too little audio');
  const fade=Math.min(110,s.length>>2);for(let i=0;i<fade;i++)s[s.length-1-i]=Math.round(s[s.length-1-i]*i/fade);
  return {name:file.name,category,n:s.length,data:imaEncode(s).data};
}
