// SPDX-License-Identifier: GPL-3.0-only
// The STATTIC simulator page: the FM-1 drawn as the unit (panel.js, window.PANEL), its controls sent to the firmware
// running in the audio worklet (processor.mjs: buttons by panel.c id, encoders 0 SELECT 1 ALGORITHM 2 PRESETS 3..6 K1..K4,
// keys 0..26 = semitones from F3).
const $=id=>document.getElementById(id),ctx2=$('screen').getContext('2d'),pixels=ctx2.createImageData(240,240);
let audio,node,gain,ready=false,layout=null;
const status=text=>$('status').textContent=text;
const send=data=>{if(ready)node.port.postMessage(data);};
function draw(buffer,state){const image=new Uint16Array(buffer);for(let i=0;i<image.length;i++){const raw=image[i],v=(raw>>8|raw<<8)&65535,j=i*4;pixels.data[j]=(v>>11)*255/31;pixels.data[j+1]=(v>>5&63)*255/63;pixels.data[j+2]=(v&31)*255/31;pixels.data[j+3]=255;}ctx2.putImageData(pixels,0,0);window.statticSim.state=state;}
window.statticSim={state:null,send};

$('start').onclick=async()=>{
  $('start').disabled=true;status('Loading the interface and audio…');
  try{
    audio=new AudioContext({sampleRate:44100,latencyHint:'interactive'});await audio.resume();
    let build={};try{build=await (await fetch('./build.json?fresh='+Date.now(),{cache:'no-store'})).json();}catch{}
    const v=encodeURIComponent(build.wasm_sha256||Date.now());
    const response=await fetch('./stattic.wasm?v='+v,{cache:'no-cache'});if(!response.ok)throw new Error('The simulator could not be loaded');const module=await WebAssembly.compile(await response.arrayBuffer());
    if(WebAssembly.Module.imports(module).length)throw new Error('Unexpected firmware imports');
    await audio.audioWorklet.addModule('./processor.mjs?v='+v);node=new AudioWorkletNode(audio,'stattic',{numberOfInputs:0,numberOfOutputs:1,outputChannelCount:[2],processorOptions:{module}});
    gain=audio.createGain();gain.gain.value=$('volume').value/100;node.connect(gain).connect(audio.destination);
    node.port.onmessage=({data})=>{if(data.type==='screen')draw(data.screen,data.state);if(data.type==='error')status(data.message);};
    ready=true;$('silence').disabled=false;$('start').textContent='Simulator running';status((build.version?build.version+'. ':'')+'HOME: SELECT changes pages; ALGORITHM changes channel. PLAY starts the sequence.');
  }catch(error){status(error.message);$('start').disabled=false;await audio?.close();}
};
const setVolume=v=>{$('volume').value=v;if(gain)gain.gain.setTargetAtTime(v/100,audio.currentTime,.02);};
$('volume').oninput=()=>setVolume(+$('volume').value);

/* ---------------------------------------------------------------- the panel */
const device=$('device'),svg=$('panel'),canvas=$('screen');
const held=new Map();                                  // pointer / keyboard id -> {kind, id, g}
function press(tag,kind,id,g){
  if(held.has(tag))return;held.set(tag,{kind,id,g});g?.classList.add('held');
  send({type:kind,id,down:true});
}
function lift(tag){
  const h=held.get(tag);if(!h)return;held.delete(tag);
  if(![...held.values()].some(o=>o.kind===h.kind&&o.id===h.id)){h.g?.classList.remove('held');send({type:h.kind,id:h.id,down:false});}
}
function releaseAll(){for(const tag of [...held.keys()])lift(tag);if(ready)send({type:'panic'});}
$('silence').onclick=releaseAll;window.addEventListener('blur',releaseAll);document.addEventListener('visibilitychange',()=>{if(document.hidden)releaseAll();});

function build(name){
  layout=name;for(const d of device.querySelectorAll('.hit'))d.remove();
  const P=window.PANEL.draw(svg,name,{}),L=P.L;
  device.style.aspectRatio=`${L.w} / ${L.h}`;device.style.maxWidth=name==='tall'?'560px':'1000px';
  const [sx,sy,ss]=P.screen;
  Object.assign(canvas.style,{left:`${sx/L.w*100}%`,top:`${sy/L.h*100}%`,width:`${ss/L.w*100}%`,height:`${ss/L.h*100}%`});
  const hit=(c,cls,role)=>{
    const b=c.g.getBBox(),d=document.createElement('div');d.className='hit'+(cls?' '+cls:'');
    Object.assign(d.style,{left:`${b.x/L.w*100}%`,top:`${b.y/L.h*100}%`,width:`${b.width/L.w*100}%`,height:`${b.height/L.h*100}%`});
    d.setAttribute('role',role);d.setAttribute('aria-label',c.name);d.title=c.name;d.tabIndex=0;
    d.addEventListener('touchstart',e=>e.preventDefault(),{passive:false});d.addEventListener('contextmenu',e=>e.preventDefault());
    device.append(d);return d;
  };
  for(const b of P.buttons){
    const d=hit(b,'','button');
    d.onpointerdown=e=>{e.preventDefault();d.setPointerCapture(e.pointerId);press('p'+e.pointerId,'button',b.bit,b.g);};
    d.onkeydown=e=>{if((e.key==='Enter'||e.key===' ')&&!e.repeat){e.preventDefault();e.stopPropagation();press('f'+b.bit,'button',b.bit,b.g);}};
    d.onkeyup=()=>lift('f'+b.bit);
  }
  P.keys.forEach((k,i)=>{
    const d=hit(k,'','button');
    d.onpointerdown=e=>{e.preventDefault();d.setPointerCapture(e.pointerId);press('p'+e.pointerId,'key',i,k.g);};
  });
  for(const k of P.knobs){
    const d=hit(k,'knob','slider');let drag=null,wheel=0,angle=0;
    const turn=n=>{
      if(!n)return;
      if(k.role==='master'){setVolume(Math.max(0,Math.min(100,+$('volume').value+n*2)));angle=-135+270*$('volume').value/100;}
      else{send({type:'encoder',id:k.role,value:n});angle=(angle+n*18)%360;}
      if(k.ind)k.ind.setAttribute('transform',`rotate(${angle} ${k.x} ${k.y})`);
    };
    if(k.role==='master'){angle=-135+270*$('volume').value/100;k.ind?.setAttribute('transform',`rotate(${angle} ${k.x} ${k.y})`);}
    d.onpointerdown=e=>{e.preventDefault();d.setPointerCapture(e.pointerId);drag={y:e.clientY,acc:0};};
    d.onpointermove=e=>{if(!drag)return;drag.acc+=drag.y-e.clientY;drag.y=e.clientY;const step=k.role==='master'?3:8,n=Math.trunc(drag.acc/step);if(n){drag.acc-=n*step;turn(n);}};
    d.onpointerup=d.onpointercancel=()=>{drag=null;};
    d.addEventListener('wheel',e=>{e.preventDefault();const dy=e.deltaMode===1?e.deltaY*33:e.deltaY;
      if(Math.abs(dy)>=50){turn(dy<0?1:-1);wheel=0;return;}wheel-=dy;const n=Math.trunc(wheel/25);if(n){wheel-=n*25;turn(n);}},{passive:false});
    d.onkeydown=e=>{const v={ArrowUp:1,ArrowRight:1,ArrowDown:-1,ArrowLeft:-1}[e.key];if(v){e.preventDefault();e.stopPropagation();turn(v);}};
  }
}
for(const t of ['pointerup','pointercancel','lostpointercapture'])device.addEventListener(t,e=>lift('p'+e.pointerId));
const tall=matchMedia('(orientation: portrait) and (max-width: 700px)');
build(tall.matches?'tall':'wide');tall.addEventListener('change',()=>build(tall.matches?'tall':'wide'));

/* ---------------------------------------------------------------- the computer keyboard */
const codes=['KeyZ','KeyS','KeyX','KeyD','KeyC','KeyV','KeyG','KeyB','KeyH','KeyN','KeyJ','KeyM','Comma','KeyL','Period','Semicolon','Slash','KeyQ','Digit2','KeyW','Digit3','KeyE','KeyR','Digit5','KeyT','Digit6','KeyY'];
const keyG=i=>svg.querySelector(`[data-ctrl="KEY${i}"]`),btnG=name=>svg.querySelector(`[data-ctrl="${name}"]`);
window.addEventListener('keydown',e=>{
  if(e.repeat||e.metaKey||e.ctrlKey||e.altKey||e.target.matches('input,select,textarea')||e.target.closest?.('.hit'))return;
  const id=codes.indexOf(e.code);
  if(id>=0){e.preventDefault();press('k'+e.code,'key',id,keyG(id));}
  if(e.code==='Space'){e.preventDefault();press('kSpace','button',window.PANEL.B.PLAY,btnG('PLAY'));}
});
window.addEventListener('keyup',e=>lift('k'+e.code));
