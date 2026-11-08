// SPDX-License-Identifier: GPL-3.0-only
const $=id=>document.getElementById(id),ctx2=$('screen').getContext('2d'),pixels=ctx2.createImageData(240,240);
let audio,node,gain,ready=false;const active=new Set();
const status=text=>$('status').textContent=text;
const send=data=>{if(ready)node.port.postMessage(data);};
function draw(buffer,state){const image=new Uint16Array(buffer);for(let i=0;i<image.length;i++){const raw=image[i],v=(raw>>8|raw<<8)&65535,j=i*4;pixels.data[j]=(v>>11)*255/31;pixels.data[j+1]=(v>>5&63)*255/63;pixels.data[j+2]=(v&31)*255/31;pixels.data[j+3]=255;}ctx2.putImageData(pixels,0,0);window.statticSim.state=state;}
window.statticSim={state:null,send};
$('start').onclick=async()=>{
  $('start').disabled=true;status('Loading the interface and audio…');
  try{
    audio=new AudioContext({sampleRate:44100,latencyHint:'interactive'});await audio.resume();
    const response=await fetch('./stattic.wasm');if(!response.ok)throw new Error('The simulator could not be loaded');const module=await WebAssembly.compile(await response.arrayBuffer());
    if(WebAssembly.Module.imports(module).length)throw new Error('Unexpected firmware imports');
    await audio.audioWorklet.addModule('./processor.mjs');node=new AudioWorkletNode(audio,'stattic',{numberOfInputs:0,numberOfOutputs:1,outputChannelCount:[2],processorOptions:{module}});
    gain=audio.createGain();gain.gain.value=$('volume').value/100;node.connect(gain).connect(audio.destination);
    node.port.onmessage=({data})=>{if(data.type==='screen')draw(data.screen,data.state);if(data.type==='error')status(data.message);};
    ready=true;$('silence').disabled=false;$('start').textContent='Simulator running';status('HOME: SELECT changes pages; ALGORITHM changes channel. PLAY starts the sequence.');
  }catch(error){status(error.message);$('start').disabled=false;await audio?.close();}
};
$('volume').oninput=()=>{if(gain)gain.gain.setTargetAtTime($('volume').value/100,audio.currentTime,.02);};
function releaseAll(){for(const item of document.querySelectorAll('.on'))item.classList.remove('on');active.clear();if(ready)send({type:'panic'});}
$('silence').onclick=releaseAll;window.addEventListener('blur',releaseAll);document.addEventListener('visibilitychange',()=>{if(document.hidden)releaseAll();});
const labels=['FX','SEL','ENV','LFO','EDIT','GLO','HOME','SAVE','ARP','SEQ','PLAY','REC','OCT−','OCT+'];
function held(element,down,up){element.onpointerdown=e=>{e.preventDefault();element.setPointerCapture(e.pointerId);element.classList.add('on');down();};const end=e=>{if(!element.classList.contains('on'))return;element.classList.remove('on');up();};element.onpointerup=end;element.onpointercancel=end;element.onlostpointercapture=end;}
labels.forEach((name,id)=>{const button=document.createElement('button');button.textContent=name;button.dataset.id=id;button.setAttribute('aria-label',name);$('buttons').append(button);held(button,()=>send({type:'button',id,down:true}),()=>send({type:'button',id,down:false}));});
['SELECT','ALGORITHM','PRESETS','K1','K2','K3','K4'].forEach((name,id)=>{
  const div=document.createElement('div');div.className='knob';div.dataset.id=id;div.innerHTML=`<span>${name}</span><div class="dial" role="spinbutton" tabindex="0" aria-label="${name}"></div><div class="nudge"><button aria-label="${name} minus">−</button><button aria-label="${name} plus">+</button></div>`;
  (id<3?$('nav-knobs'):$('macros')).append(div);const move=value=>send({type:'encoder',id,value});
  div.onwheel=e=>{e.preventDefault();move(e.deltaY<0?1:-1);};div.querySelectorAll('button').forEach((b,i)=>b.onclick=()=>move(i?1:-1));
  const dial=div.querySelector('.dial');let y=null;dial.onpointerdown=e=>{y=e.clientY;dial.setPointerCapture(e.pointerId);};dial.onpointermove=e=>{if(y===null)return;const n=Math.trunc((y-e.clientY)/7);if(n){move(n);y-=n*7;}};dial.onpointerup=dial.onpointercancel=()=>y=null;dial.onkeydown=e=>{if(['ArrowUp','ArrowRight','ArrowDown','ArrowLeft'].includes(e.key)){e.preventDefault();move(['ArrowUp','ArrowRight'].includes(e.key)?1:-1);}};
});
for(let id=0;id<27;id++){const button=document.createElement('button');button.className='key'+([1,3,5,8,10].includes(id%12)?' black':'');button.setAttribute('aria-label',`Note ${id+1}`);button.dataset.note=id;$('keys').append(button);held(button,()=>send({type:'key',id,down:true}),()=>send({type:'key',id,down:false}));}
const codes=['KeyZ','KeyS','KeyX','KeyD','KeyC','KeyV','KeyG','KeyB','KeyH','KeyN','KeyJ','KeyM','Comma','KeyL','Period','Semicolon','Slash','KeyQ','Digit2','KeyW','Digit3','KeyE','KeyR','Digit5','KeyT','Digit6','KeyY'];
window.addEventListener('keydown',e=>{if(e.repeat||e.target.matches('input,select,textarea'))return;const id=codes.indexOf(e.code);if(id>=0){e.preventDefault();active.add(id);send({type:'key',id,down:true});}if(e.code==='Space'){e.preventDefault();send({type:'button',id:10,down:true});}});
window.addEventListener('keyup',e=>{const id=codes.indexOf(e.code);if(active.delete(id))send({type:'key',id,down:false});if(e.code==='Space')send({type:'button',id:10,down:false});});
