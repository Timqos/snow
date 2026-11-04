let snowSelected=0, snowSchema=null, snowControls=[], snowDescriptorKey='';
const snowById=(id)=>document.getElementById(id);
const snowPost=(type,data={})=>send({type,track:snowSelected,...data});
for(let k=0;k<6;k++){
  const card=document.createElement('div');card.className='lab-channel';
  const b=document.createElement('button');b.dataset.track=k;b.disabled=true;b.textContent=`CH ${k+1}`;
  b.onclick=()=>snowPost('snow-track',{track:k});card.append(b);
  const label=document.createElement('label');label.textContent='PATTERN';
  const select=document.createElement('select');select.dataset.pattern=k;select.disabled=true;select.setAttribute('aria-label',`CH ${k+1} pattern`);
  for(let p=0;p<16;p++)select.add(new Option(String.fromCharCode(65+p),p));
  select.onchange=()=>snowPost('snow-pattern',{track:k,slot:+select.value});label.append(select);card.append(label);
  const all=document.createElement('label'),check=document.createElement('input');check.type='checkbox';check.dataset.mask=k;check.checked=true;check.disabled=true;
  check.onchange=()=>{let mask=0;for(const c of document.querySelectorAll('[data-mask]'))if(c.checked)mask|=1<<+c.dataset.mask;snowPost('snow-mask',{mask});};
  all.append(check,document.createTextNode(' ALL'));card.append(all);snowById('snow-channels').append(card);
}
snowById('snow-power').onclick=()=>snowById('go').click();
snowById('snow-play').onclick=()=>{snowPost('buttons',{mask:btnMask|(1<<10)});snowPost('buttons',{mask:btnMask});};
snowById('snow-demo').onclick=()=>{snowPost('snow-demo');snowById('snow-notice').textContent='Демо завантажено — натисни PLAY нижче.';};
snowById('snow-save').onclick=()=>{snowPost('snow-save');snowById('snow-notice').textContent='Тимчасовий стан збережено в RAM.';};
snowById('snow-reload').onclick=()=>snowPost('snow-reload',{mask:63});
snowById('snow-reload-track').onclick=()=>snowPost('snow-reload',{mask:1<<snowSelected});
snowById('snow-generate').onclick=()=>{snowPost('snow-generate',{vary:false});snowById('snow-notice').textContent=`CH ${snowSelected+1}: генерація з поточного seed.`;};
snowById('snow-vary').onclick=()=>{snowPost('snow-generate',{vary:true});snowById('snow-notice').textContent=`CH ${snowSelected+1}: нова варіація.`;};
snowById('snow-preset').onchange=(e)=>{if(e.target.value!==''){snowPost('snow-preset',{preset:+e.target.value});snowById('snow-notice').textContent=`CH ${snowSelected+1}: ${e.target.selectedOptions[0].textContent}`;e.target.value='';}};
function snowDesigner(schema){
  if(!schema)return;
  snowSchema=schema;snowDescriptorKey='';
  const host=snowById('snow-designer');
  const open=new Set([...host.querySelectorAll('details[open]')].map(d=>d.dataset.group));
  const initial=!snowControls.length;host.replaceChildren();snowControls=[];
  let group='',grid;
  schema.params.forEach((p,index)=>{
    if(p.group!==group){
      group=p.group;const details=document.createElement('details');details.dataset.group=group;
      details.open=open.has(group)||(initial&&['MACROS','OSCILLATORS / ALGORITHM','AMP ADSR','GENERATIVE SEQUENCER'].includes(group));
      const summary=document.createElement('summary');summary.textContent=group;details.append(summary);
      grid=document.createElement('div');grid.className='lab-grid';details.append(grid);host.append(details);
    }
    const label=document.createElement('label');label.className='lab-control';
    const name=document.createElement('span');name.textContent=p.name;label.append(name);
    const value=document.createElement('output');label.append(value);
    const input=document.createElement(p.options?'select':'input');input.setAttribute('aria-label',`${p.group}: ${p.name}`);
    if(p.options)p.options.forEach((text,v)=>input.add(new Option(text,p.min+v)));
    else {input.type='range';input.min=p.min;input.max=p.max;input.step=1;}
    input.oninput=()=>{value.textContent=p.options?p.options[+input.value-p.min]:input.value;snowPost('snow-set',{index,value:+input.value,schema:p.group==='INSERT / SEND FX'&&p.name==='TYPE'});};
    label.append(input);grid.append(label);snowControls.push({input,value});
  });
  const presets=snowById('snow-preset');presets.replaceChildren(new Option('Обрати звук…',''));
  schema.presets.forEach((name,i)=>presets.add(new Option(name,i)));
}
function snowFrame(state){
  if(state.selected!==snowSelected){snowSelected=state.selected;snowPost('snow-schema');}
  // Insert A/B/C labels and ranges also follow changes made on the hardware panel.
  if(state.values&&snowSchema){
    const key=snowSchema.params.map((p,i)=>p.group==='INSERT / SEND FX'&&p.name==='TYPE'?state.values[i]:'').join(',');
    if(snowDescriptorKey&&key!==snowDescriptorKey)snowPost('snow-schema');
    snowDescriptorKey=key;
  }
  snowById('snow-selection').textContent=`CH ${snowSelected+1} / SOUND DESIGN`;
  for(const b of document.querySelectorAll('[data-track]'))b.setAttribute('aria-pressed',String(+b.dataset.track===snowSelected));
  for(const c of document.querySelectorAll('[data-mask]'))c.checked=!!(state.mask&(1<<+c.dataset.mask));
  if(state.patterns)for(const p of document.querySelectorAll('[data-pattern]'))if(document.activeElement!==p)p.value=state.patterns[+p.dataset.pattern];
  if(state.values)snowControls.forEach((c,i)=>{if(document.activeElement!==c.input){c.input.value=state.values[i];c.value.textContent=state.text?.[i]??state.values[i];}});
}
function snowReady(schema){
  snowDesigner(schema);
  for(const input of document.querySelectorAll('#snow-lab button,#snow-lab select,#snow-lab input'))input.disabled=false;
  snowById('snow-power').disabled=true;snowById('snow-power').textContent='Звук увімкнено';
}
