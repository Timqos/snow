// SPDX-License-Identifier: GPL-3.0-only
// One owner for the real DSP, UI, sequencer, panel input and simulated flash.
class StatticProcessor extends AudioWorkletProcessor{
  constructor(options){
    super();this.w=new WebAssembly.Instance(options.processorOptions.module,{}).exports;this.w._initialize?.();this.w.stattic_sim_init();
    this.block=new Float32Array(64);this.pos=32;this.phase=0;this.count=0;this.a=[0,0];this.b=[0,0];
    this.port.onmessage=({data})=>{try{
      if(data.type==='encoder')this.w.stattic_sim_encoder(data.id,data.value);
      if(data.type==='button')this.w.stattic_sim_button(data.id,+data.down);
      if(data.type==='key')this.w.stattic_sim_key(data.id,+data.down);
      if(data.type==='panic'){this.w.stattic_sim_panic();}
      this.w.stattic_sim_frame();this.display();
    }catch(error){this.port.postMessage({type:'error',message:error.message});}};
    this.w.stattic_sim_frame();this.display();
  }
  display(){const screen=new Uint16Array(this.w.memory.buffer,this.w.stattic_sim_screen(),240*240).slice();const state=Array.from({length:10},(_,i)=>this.w.stattic_sim_state(i));this.port.postMessage({type:'screen',screen:screen.buffer,state},[screen.buffer]);}
  next(){
    if(this.pos>=32){const p=this.w.stattic_sim_render();this.block.set(new Float32Array(this.w.memory.buffer,p,64));this.pos=0;this.count+=32;if(this.count>=2205){this.count-=2205;this.w.stattic_sim_frame();this.display();}}
    const i=this.pos++*2;return [this.block[i],this.block[i+1]];
  }
  process(inputs,outputs){
    const channels=outputs[0];if(!channels.length)return true;
    for(let i=0;i<channels[0].length;i++){
      this.phase+=44100/sampleRate;while(this.phase>=1){this.a=this.b;this.b=this.next();this.phase--;}
      channels[0][i]=this.a[0]+(this.b[0]-this.a[0])*this.phase;
      if(channels[1])channels[1][i]=this.a[1]+(this.b[1]-this.a[1])*this.phase;
    }return true;
  }
}
registerProcessor('stattic',StatticProcessor);
