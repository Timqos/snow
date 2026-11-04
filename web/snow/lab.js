let snowSelected=0,snowSchema=null,snowControls=[],snowDescriptorKey='',snowView='sound',snowHalf=0;
let snowUpgradeFrom04=false;
const snowRoles=['KICK','SNARE','HAT','PERC','BASS','CHORD'];
const snowById=(id)=>document.getElementById(id);
const snowPost=(type,data={})=>send({type,track:snowSelected,...data});
const snowNotice=(text)=>{snowById('snow-notice').textContent=text;};
function snowShow(view){snowView=view;for(const v of ['sound','fx','patterns','gen'])snowById({sound:'snow-sound-panel',fx:'snow-fx-panel',patterns:'snow-pattern-panel',gen:'snow-generator-panel'}[v]).hidden=v!==view;for(const b of document.querySelectorAll('[data-view]'))b.setAttribute('aria-pressed',String(b.dataset.view===view));}
for(const b of document.querySelectorAll('[data-view]'))b.onclick=()=>snowShow(b.dataset.view);
for(let k=0;k<6;k++){
 const card=document.createElement('div');card.className='lab-channel';
 const b=document.createElement('button');b.dataset.track=k;b.disabled=true;b.textContent=`${snowRoles[k]} / ${k+1}`;
 b.onclick=()=>snowPost('snow-track',{track:k});card.append(b);
 const label=document.createElement('label');label.textContent='PATTERN';
 const select=document.createElement('select');select.dataset.pattern=k;select.disabled=true;select.setAttribute('aria-label',`${snowRoles[k]} pattern`);
 for(let p=0;p<16;p++)select.add(new Option(String.fromCharCode(65+p),p));
 select.onchange=()=>{snowPost('snow-pattern',{track:k,slot:+select.value});snowSetHalf(Math.floor(+select.value/8));};label.append(select);card.append(label);
 const all=document.createElement('label'),check=document.createElement('input');check.type='checkbox';check.dataset.mask=k;check.checked=true;check.disabled=true;
 check.onchange=()=>{let mask=0;for(const c of document.querySelectorAll('[data-mask]'))if(c.checked)mask|=1<<+c.dataset.mask;snowPost('snow-mask',{mask});};
 all.append(check,document.createTextNode(' ALL'));card.append(all);snowById('snow-channels').append(card);
 const column=document.createElement('div');column.className='lab-pattern-column';
 const title=document.createElement('strong');title.textContent=snowRoles[k];column.append(title);
 for(let slot=0;slot<16;slot++){
  const p=document.createElement('button');p.dataset.slot=slot;p.dataset.channel=k;p.textContent=String.fromCharCode(65+slot);p.disabled=true;p.hidden=slot>=8;
  p.setAttribute('aria-label',`${snowRoles[k]} pattern ${p.textContent}`);p.onclick=()=>snowPost('snow-pattern',{track:k,slot});column.append(p);
 }
 const knob=document.createElement('input');knob.type='range';knob.min=0;knob.max=15;knob.step=1;knob.value=0;knob.disabled=true;knob.dataset.patternKnob=k;knob.setAttribute('aria-label',`${snowRoles[k]} pattern knob`);
 knob.oninput=()=>{snowPost('snow-pattern',{track:k,slot:+knob.value});snowSetHalf(Math.floor(+knob.value/8));};column.append(knob);snowById('snow-pattern-grid').append(column);
}
function snowSetHalf(half){snowHalf=half;for(const b of document.querySelectorAll('[data-slot]'))b.hidden=Math.floor(+b.dataset.slot/8)!==half;snowById('snow-pattern-half').textContent=half?'I–P → A–H':'A–H → I–P';}
snowById('snow-pattern-half').onclick=()=>snowSetHalf(1-snowHalf);
snowById('snow-power').onclick=()=>snowById('go').click();
snowById('snow-play').onclick=()=>{snowPost('buttons',{mask:btnMask|(1<<10)});snowPost('buttons',{mask:btnMask});};
snowById('snow-tempo').onchange=(e)=>snowPost('snow-tempo',{value:+e.target.value});
snowById('snow-demo').onclick=()=>{snowPost('snow-demo');snowNotice('Демо завантажено — натисни PLAY.');};
snowById('snow-save').onclick=()=>{snowPost('snow-save');snowNotice('Тимчасовий стан збережено в RAM.');};
snowById('snow-reload').onclick=()=>snowPost('snow-reload',{mask:63});
snowById('snow-reload-track').onclick=()=>snowPost('snow-reload',{mask:1<<snowSelected});
const snowAll=()=>snowById('snow-target').value==='all';
snowById('snow-rnd').onclick=()=>{snowPost(snowAll()?'snow-scene':'snow-generate',{vary:true});snowNotice('GEN RND: новий рисунок.');};
snowById('snow-parm').onclick=()=>{snowPost('snow-genparm',{all:snowAll()});snowNotice('GEN PARM: нові локи поверх поточних нот.');};
snowById('snow-clear').onclick=()=>{snowPost('snow-clear',{all:snowAll()});snowNotice('Кроки та локи очищено.');};
snowById('snow-parm-amt').oninput=e=>{snowById('snow-amt-value').textContent=e.target.value+'%';const index=snowSchema?.params.findIndex(p=>p.name==='PARM AMT');if(index>=0){for(const track of snowAll()?[0,1,2,3,4,5]:[snowSelected])snowPost('snow-set',{track,index,value:+e.target.value});}};
snowById('snow-target').onchange=()=>{const index=snowSchema?.params.findIndex(p=>p.name==='TARGET');if(index>=0)snowPost('snow-set',{index,value:snowAll()?1:0});};
snowById('snow-random-sound').onclick=()=>{snowPost('snow-random-sound');snowNotice('RANDOM EDIT: усі параметри EDIT поточного каналу. SAVE held — UNDO.');};
snowById('snow-color').onchange=e=>snowPost('snow-color',{value:+e.target.value});
snowById('snow-preset').onchange=(e)=>{if(e.target.value!==''){snowPost('snow-preset',{preset:+e.target.value});snowNotice(`${snowRoles[snowSelected]}: ${e.target.selectedOptions[0].textContent}`);e.target.value='';}};
function snowDesigner(schema){
 if(!schema)return;snowSchema=schema;snowDescriptorKey='';
 const hosts=['snow-sound-controls','snow-fx-controls','snow-gen-controls'].map(snowById);
 const open=new Set(hosts.flatMap(h=>[...h.querySelectorAll('details[open]')].map(d=>d.dataset.group)));
 const initial=!snowControls.length;for(const host of hosts)host.replaceChildren();snowControls=[];
 let group='',grid;
 schema.params.forEach((p,index)=>{
  if(p.group!==group){group=p.group;
   const isGen=['GENERATIVE SEQUENCER','SEQUENCE'].includes(group),isFx=['DRIVE / INSERT / DUCK','CHANNEL DELAY','CHANNEL REVERB','CHANNEL CHORUS','TAPE','BITCRUSH','FX RACK'].includes(group);
   const details=document.createElement('details');details.dataset.group=group;
   details.open=open.has(group)||(initial&&(['MACROS','OSCILLATORS / ALGORITHM','AMP ADSR'].includes(group)||isFx||isGen));
   const summary=document.createElement('summary');summary.textContent=group;details.append(summary);
   grid=document.createElement('div');grid.className='lab-grid';details.append(grid);hosts[isGen?2:isFx?1:0].append(details);
  }
  const label=document.createElement('label');label.className='lab-control';
  const name=document.createElement('span');name.textContent=p.name;label.append(name);
  const value=document.createElement('output');label.append(value);
  const input=document.createElement(p.options?'select':'input');input.setAttribute('aria-label',`${schema.role}: ${p.group}: ${p.name}`);
  if(p.options)p.options.forEach((text,v)=>input.add(new Option(text,p.min+v)));
  else{input.type='range';input.min=p.min;input.max=p.max;input.step=1;}
  input.oninput=()=>{value.textContent=p.options?p.options[+input.value-p.min]:input.value;snowPost('snow-set',{index,value:+input.value,schema:p.group==='DRIVE / INSERT / DUCK'&&p.name==='TYPE'});};
  label.append(input);grid.append(label);snowControls.push({input,value});
 });
 const presets=snowById('snow-preset');presets.replaceChildren(new Option(`${schema.role}: 10 пресетів…`,''));schema.presets.forEach((name,i)=>presets.add(new Option(name,i)));
 const colors=snowById('snow-color');colors.replaceChildren();schema.colors.forEach((name,i)=>colors.add(new Option(name,i)));
}
function snowFrame(state){
 if(state.steps){const host=snowById('snow-step-grid');while(host.children.length<state.length){const cell=document.createElement('div');cell.className='lab-step';host.append(cell);}while(host.children.length>state.length)host.lastChild.remove();[...host.children].forEach((cell,i)=>{const step=state.steps[i],on=!!(step&256),hits=((step>>10)&3)+1;cell.classList.toggle('on',on);cell.classList.toggle('current',i===state.position);cell.style.setProperty('--vel',(step&255)/127);cell.textContent=`${i+1}${step&512?' •':''}${on&&hits>1?' ×'+hits:''}`;cell.title=on?`Note ${state.notes[i]}, velocity ${step&255}`:'Пауза';});}
 if(state.color!==undefined&&document.activeElement!==snowById('snow-color')){snowById('snow-color').value=state.color;const colors=['#39dbf5','#709bff','#ff9850','#71e8a0','#eeeeee','#ebc75f','#f594c2','#ba96ff','#ff6a76','#7ce7ce'];document.documentElement.style.setProperty('--snow-accent',colors[state.color]||colors[0]);}
 if(state.values&&snowSchema){const i=snowSchema.params.findIndex(p=>p.name==='PARM AMT');if(i>=0&&document.activeElement!==snowById('snow-parm-amt')){snowById('snow-parm-amt').value=state.values[i];snowById('snow-amt-value').textContent=state.values[i]+'%';}}

 if(state.selected!==snowSelected){snowSelected=state.selected;snowPost('snow-schema');}
 if(state.values&&snowSchema){const key=snowSchema.params.map((p,i)=>p.group==='DRIVE / INSERT / DUCK'&&p.name==='TYPE'?state.values[i]:'').join(',');if(snowDescriptorKey&&key!==snowDescriptorKey)snowPost('snow-schema');snowDescriptorKey=key;}
 if(snowSchema&&state.values){const target=snowById('snow-target'),index=snowSchema.params.findIndex(p=>p.name==='TARGET');target.options[0].textContent=['KK','SN','HH','PR','BS','CH'][snowSelected];if(index>=0&&document.activeElement!==target)target.value=state.values[index]?'all':'channel';}
 snowById('snow-selection').textContent=`${snowRoles[snowSelected]} / CH ${snowSelected+1}`;
 for(const b of document.querySelectorAll('[data-track]'))b.setAttribute('aria-pressed',String(+b.dataset.track===snowSelected));
 for(const c of document.querySelectorAll('[data-mask]'))c.checked=!!(state.mask&(1<<+c.dataset.mask));
 if(state.patterns){
  for(const p of document.querySelectorAll('[data-pattern]'))if(document.activeElement!==p)p.value=state.patterns[+p.dataset.pattern];
  for(const p of document.querySelectorAll('[data-pattern-knob]'))if(document.activeElement!==p)p.value=state.patterns[+p.dataset.patternKnob];
  for(const p of document.querySelectorAll('[data-slot]')){const ch=+p.dataset.channel,slot=+p.dataset.slot;p.setAttribute('aria-pressed',String(state.active?.[ch]===slot));p.classList.toggle('pending',state.pending?.[ch]===slot);p.classList.toggle('used',!!state.used?.[ch]?.[slot]);}
 }
 if(state.tempo&&document.activeElement!==snowById('snow-tempo'))snowById('snow-tempo').value=state.tempo;
 if(state.values)snowControls.forEach((c,i)=>{if(document.activeElement!==c.input){c.input.value=state.values[i];c.value.textContent=state.text?.[i]??state.values[i];}});
}
function snowReady(schema){snowDesigner(schema);if(snowUpgradeFrom04){const index=schema.params.findIndex(p=>p.name==='TARGET');if(index>=0)for(let track=0;track<6;track++)snowPost('snow-set',{track,index,value:0});snowUpgradeFrom04=false;}for(const input of document.querySelectorAll('#snow-lab button,#snow-lab select,#snow-lab input'))input.disabled=false;snowById('snow-power').disabled=true;snowById('snow-power').textContent='Звук увімкнено';}
