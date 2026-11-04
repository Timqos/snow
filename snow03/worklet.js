// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
// Felucca in an AudioWorklet: felucca.wasm is the whole FM-1 firmware against a simulated FM-1
// (web/emu/felucca_web.c). Each render quantum asks it for 128 frames, which runs the device's clock forward;
// between quanta the page's input goes in and, about 30 times a second (4 when the page is hidden), the screen,
// the lights and any changed flash go out. After X0X's web/emu/worklet.js (charlesvestal/fm1-x0x, GPL-3.0).

// The flash the page keeps: storage.c's areas (projects, user presets and their FM6 patches, the user sample
// slots 0x97000..0xDFFFF; the autosave 0xE5000..0xE6FFF (1.2); the settings and the second FM6 copy
// 0xFC000..0xFFFFF; Skiff's pattern bank 0xE7000..0xE8FFF and 0xEA000..0xFBFFF), only the sectors that hold something
// (an erased sector is all 0xFF). {offset: bytes}
export const STORE_AREAS = [[0x97000, 0xE0000], [0xE5000, 0xE9000], [0xEA000, 0x100000]];
export function storeSectors(nor) {
  const out = {};
  for (const [a, b] of STORE_AREAS)
    for (let off = a; off < b; off += 4096) {
      const s = nor.subarray(off, off + 4096);
      if (s.some((v) => v !== 0xFF)) out[off] = s.slice();
    }
  return out;
}
export function restoreSectors(nor, sectors) {
  for (const [k, v] of Object.entries(sectors || {})) {
    const off = +k, b = v instanceof Uint8Array ? v : new Uint8Array(v);
    if (STORE_AREAS.some(([a, e]) => off >= a && off + 4096 <= e && off % 4096 === 0) && b.length === 4096)
      nor.set(b, off);
  }
}

export const HOLD_MS = 24;

if (typeof registerProcessor === "function") {
  const clock = globalThis.performance ? () => globalThis.performance.now() : () => Date.now();
  class Felucca extends AudioWorkletProcessor {
    constructor() {
      super();
      this.ex = null;
      this.sentDraws = -1;
      this.sentWrites = -1;
      this.sentLeds = "";
      this.lastFrame = 0;
      this.period = 1 / 30;
      this.busy = 0;
      this.frames = 0;
      this.queue = [];                                // button / key changes, each held >= HOLD_MS of device time
      this.nextOk = 0;
      this.port.onmessage = (e) => this.onMessage(e.data);
    }

    async onMessage(m) {
      if (m.type === "load") {
        try {
          const { instance } = await WebAssembly.instantiate(m.wasm, {});
          const ex = instance.exports;
          ex._initialize();
          ex.web_nor_erase();
          restoreSectors(new Uint8Array(ex.memory.buffer, ex.web_nor(), ex.web_nor_size()), m.sectors);
          ex.web_master(m.master ?? 700);
          ex.web_boot();
          this.ex = ex;
          this.sentWrites = ex.web_flash_writes_count();
          this.port.postMessage({ type: "ready", rate: ex.web_sample_rate(), snowSchema: this.snowSchema() });
        } catch (err) {
          this.port.postMessage({ type: "error", message: String(err && err.message || err) });
        }
        return;
      }
      const ex = this.ex;
      if (!ex) return;
      if (m.type === "buttons" || m.type === "keys") this.queue.push(m);
      else if (m.type === "enc") ex.web_enc(m.role, m.n | 0);
      else if (m.type === "master") ex.web_master(m.value | 0);
      else if (m.type === "midi") ex.web_midi(m.data[0] | 0, m.data[1] | 0, m.data[2] | 0);
      else if (m.type === "snow-demo" && ex.web_snow_demo) {
        ex.web_snow_demo();
        this.port.postMessage({type:"snow-schema",schema:this.snowSchema()});
      }
      else if (m.type === "snow-save" && ex.web_snow_save) ex.web_snow_save();
      else if (m.type === "snow-reload" && ex.web_snow_reload) {
        ex.web_snow_reload(m.mask ?? 63);
        this.port.postMessage({type:"snow-schema",schema:this.snowSchema()});
      }
      else if (m.type === "snow-track" && ex.web_snow_select) ex.web_snow_select(m.track >>> 0);
      else if (m.type === "snow-mask" && ex.web_snow_mask) ex.web_snow_mask(m.mask >>> 0);
      else if (m.type === "snow-schema" && ex.web_snow_param_count) this.port.postMessage({type:"snow-schema",schema:this.snowSchema()});
      else if (m.type === "snow-set" && ex.web_snow_set) {
        ex.web_snow_set(m.track >>> 0, m.index >>> 0, m.value | 0);
        if (m.schema) this.port.postMessage({type:"snow-schema", schema:this.snowSchema()});
      }
      else if (m.type === "snow-preset" && ex.web_snow_preset) {
        ex.web_snow_preset(m.track >>> 0, m.preset >>> 0);
        this.port.postMessage({type:"snow-schema", schema:this.snowSchema()});
      }
      else if (m.type === "snow-scene" && ex.web_snow_scene) {
        if(ex.web_snow_scene(m.style>>>0,m.vary?1:0,m.tempo?1:0))this.port.postMessage({type:"snow-notice",text:"Дочекайся запуску запланованого патерна."});
      }
      else if (m.type === "snow-tempo" && ex.web_snow_tempo) ex.web_snow_tempo(m.value>>>0);
      else if (m.type === "snow-generate" && ex.web_snow_generate) {
        if(ex.web_snow_generate(m.track>>>0,m.vary?1:0))this.port.postMessage({type:"snow-notice",text:"Дочекайся запуску запланованого патерна."});
      }
      else if (m.type === "snow-pattern" && ex.web_snow_pattern) {
        const rc=ex.web_snow_pattern(m.track >>> 0, m.slot >>> 0);
        if(rc) this.port.postMessage({type:"snow-notice",text:"Pattern unavailable: bank or automation capacity reached."});
      }
      else if (m.type === "visible") this.period = m.on ? 1 / 30 : 1 / 4;
    }

    string(ptr) {
      if (!ptr) return "";
      const mem = new Uint8Array(this.ex.memory.buffer);
      let end = ptr;
      while (mem[end] && end-ptr < 128) end++;
      // Firmware labels are ASCII; TextDecoder is unavailable in some worklets.
      return String.fromCharCode(...mem.subarray(ptr,end));
    }

    snowSchema() {
      const x=this.ex;
      if (!x?.web_snow_param_count) return null;
      const channel=x.web_snow_selected();
      return {channel,role:this.string(x.web_snow_role(channel)),styles:Array.from({length:x.web_snow_style_count()},(_,i)=>this.string(x.web_snow_style_name(i))),params:Array.from({length:x.web_snow_param_count()},(_,i)=> {
        const min=x.web_snow_param_min(channel,i),max=x.web_snow_param_max(channel,i);
        const first=x.web_snow_param_enum(channel,i,min);
        return {id:x.web_snow_param_id(i),name:this.string(x.web_snow_param_name(channel,i)),
          group:this.string(x.web_snow_param_group(i)),min,max,
          options:first?Array.from({length:max-min+1},(_,v)=>this.string(x.web_snow_param_enum(channel,i,min+v))):null};
      }),presets:Array.from({length:x.web_snow_preset_count(channel)},(_,i)=>this.string(x.web_snow_preset_name(channel,i)))};
    }

    publish() {
      const ex = this.ex, mem = ex.memory.buffer;
      const msg = { type: "frame" };
      if (ex.web_snow_selected) {
        const selected=ex.web_snow_selected();
        msg.snow = {selected, mask:ex.web_snow_mask_get()};
        if(ex.web_snow_param_count) {
          msg.snow.values=Array.from({length:ex.web_snow_param_count()},(_,i)=>ex.web_snow_param_value(selected,i));
          msg.snow.text=Array.from({length:ex.web_snow_param_count()},(_,i)=>this.string(ex.web_snow_param_text(selected,i)));
          msg.snow.patterns=Array.from({length:6},(_,i)=>ex.web_snow_pattern_get(i));
          msg.snow.active=Array.from({length:6},(_,i)=>ex.web_snow_pattern_active(i));
          msg.snow.pending=Array.from({length:6},(_,i)=>ex.web_snow_pattern_pending(i));
          msg.snow.used=Array.from({length:6},(_,k)=>Array.from({length:16},(_,i)=>!!ex.web_snow_pattern_used(k,i)));
          msg.snow.tempo=ex.web_snow_tempo_get();
        }
      }
      const leds = [ex.web_lit_buttons(), ex.web_lit_keys(), ex.web_dim_buttons(), ex.web_dim_keys(), ex.web_dim_level(),
                    ex.web_breath_buttons(), ex.web_breath_keys(), ex.web_mid_keys ? ex.web_mid_keys() : 0];
      const an = ex.web_anim_levels ? ex.web_anim_levels() : 0;   // the power-on sweep: each LED's level
      const anim = an ? Array.from(new Uint8Array(mem, an, 41)) : null;
      const key = leds.join(",") + (anim ? ";" + anim.join(",") : "");
      if (key !== this.sentLeds) {
        this.sentLeds = key;
        msg.leds = leds;
        msg.anim = anim;
      }
      if (this.frames >= 44100) {                     // the share of real time spent running the device
        msg.load = this.busy / (this.frames / 44.1);
        this.busy = this.frames = 0;
      }
      const transfer = [];
      const draws = ex.web_screen_draws();
      if (draws !== this.sentDraws) {
        this.sentDraws = draws;
        msg.fb = new Uint16Array(mem, ex.web_screen(), 240 * 240).slice();
        transfer.push(msg.fb.buffer);
      }
      const writes = ex.web_flash_writes_count();
      if (writes !== this.sentWrites) {               // a save is done within one main-loop pass: whole here
        this.sentWrites = writes;
        msg.sectors = storeSectors(new Uint8Array(mem, ex.web_nor(), ex.web_nor_size()));
      }
      if (msg.snow || msg.leds || msg.fb || msg.sectors || msg.load !== undefined) this.port.postMessage(msg, transfer);
    }

    // A click's down and up can arrive within one render quantum, but the FM-1's contacts never close for less than
    // its debounce, and the UI reads them once a frame: each change of the buttons or the keys stays HOLD_MS of the
    // device's time before the next one goes in (a tap: down for a frame and more, then up).
    input() {
      const ex = this.ex, now = ex.web_now_ms();
      while (this.queue.length && now - this.nextOk >= 0) {
        const m = this.queue.shift();
        if (m.type === "buttons") ex.web_buttons(m.mask >>> 0);
        else ex.web_keys(m.mask >>> 0);
        this.nextOk = now + HOLD_MS;
      }
    }

    process(inputs, outputs) {
      const ex = this.ex;
      if (!ex) return true;
      if (this.queue.length) this.input();
      const out = outputs[0], n = out[0].length;
      const t0 = clock();
      ex.web_render(n);
      this.busy += clock() - t0;
      this.frames += n;
      out[0].set(new Float32Array(ex.memory.buffer, ex.web_out_l(), n));
      if (out[1]) out[1].set(new Float32Array(ex.memory.buffer, ex.web_out_r(), n));
      if (currentTime - this.lastFrame >= this.period) {
        this.lastFrame = currentTime;
        this.publish();
      }
      return true;
    }
  }
  registerProcessor("felucca", Felucca);
}
