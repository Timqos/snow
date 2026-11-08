// SPDX-License-Identifier: GPL-3.0-only
// Serve build/stattic-site locally, then run this with Playwright and Chromium.
const {chromium}=require('playwright'),assert=require('node:assert/strict'),fs=require('node:fs');
(async()=>{
 const base=process.argv[2]||'http://127.0.0.1:8765/',out='build/stattic-browser';fs.mkdirSync(out,{recursive:true});
 const browser=await chromium.launch({headless:true,executablePath:process.env.CHROMIUM||'/usr/bin/chromium',args:['--no-sandbox','--autoplay-policy=no-user-gesture-required']});
 try{
  const page=await browser.newPage({viewport:{width:1250,height:1080}}),errors=[];
  page.on('pageerror',e=>errors.push(e.message));page.on('response',r=>{if(r.status()>=400)errors.push(r.status()+' '+r.url());});
  await page.goto(base+'sim/');await page.locator('#start').click();await page.waitForFunction(()=>window.statticSim?.state?.length===10);
  const state=()=>page.evaluate(()=>window.statticSim.state),send=data=>page.evaluate(d=>window.statticSim.send(d),data);
  const encoder=async(id,value)=>{await send({type:'encoder',id,value});await page.waitForTimeout(120);};
  const button=async(id,ms=80)=>{await send({type:'button',id,down:true});await page.waitForTimeout(ms);await send({type:'button',id,down:false});await page.waitForTimeout(120);};
  for(let p=0;p<5;p++){assert.equal((await state())[1],p);await page.locator('.device').screenshot({path:`${out}/home-${p+1}.png`});if(p<4)await encoder(0,1);}
  await encoder(0,-4);let art=-1,group=new Set();
  for(let i=0;i<12;i++){await encoder(3+i%4,1);const current=(await state())[8];assert.notEqual(current,art);art=current;if(!(i%3))group=new Set();assert(!group.has(art));group.add(art);}
  const before=await state();await encoder(6,-1);const after=await state();assert.equal(after[8],before[8]);assert.notEqual(after[9],before[9]);
  for(let i=0;i<3;i++)await encoder(1,1);const ch=await state();assert.equal(ch[0],2);await button(13,720);const held=await state();assert.notEqual(held[6],ch[6]);assert.equal(held[7],ch[7]);await button(13);assert.equal((await state())[7],ch[7]+1);
  await button(10);assert.equal((await state())[2],1);await page.locator('#silence').click();await page.waitForTimeout(150);assert.equal((await state())[2],0);
  await button(2);assert.equal((await state())[4],0);await page.locator('.device').screenshot({path:`${out}/envelope.png`});await button(6);await button(6,740);assert.equal((await state())[5],1);await page.locator('.device').screenshot({path:`${out}/system-menu.png`});
  for(const route of ['','manual/','samples/','install/','install/factory.html','console/']){
   await page.goto(base+route);await page.waitForTimeout(200);
   await page.setViewportSize({width:390,height:900});
   assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1),'mobile overflow '+route);
   if(route==='samples/'){
    const wav=Buffer.alloc(44+22050);wav.write('RIFF');wav.writeUInt32LE(wav.length-8,4);wav.write('WAVEfmt ',8);wav.writeUInt32LE(16,16);wav.writeUInt16LE(1,20);wav.writeUInt16LE(1,22);wav.writeUInt32LE(22050,24);wav.writeUInt32LE(44100,28);wav.writeUInt16LE(2,32);wav.writeUInt16LE(16,34);wav.write('data',36);wav.writeUInt32LE(22050,40);for(let i=0;i<11025;i++)wav.writeInt16LE(Math.round(12000*Math.sin(i*.07)*Math.exp(-i/1800)),44+i*2);
    await page.locator('#files').setInputFiles({name:'kick.wav',mimeType:'audio/wav',buffer:wav});await page.waitForFunction(()=>document.querySelectorAll('.list-row').length===1);assert.equal(await page.locator('.list-row strong').textContent(),'KK');
    await page.locator('.list-row button').first().click();await page.locator('#files').setInputFiles({name:'kick-new.wav',mimeType:'audio/wav',buffer:wav});await page.waitForFunction(()=>document.getElementById('status').textContent.includes('конвертовано'));assert.equal(await page.locator('.list-row').count(),1);
    const download=page.waitForEvent('download');await page.locator('#export').click();await (await download).saveAs(out+'/converted.stbank');assert(fs.statSync(out+'/converted.stbank').size>512);await page.screenshot({path:out+'/samples-mobile.png',fullPage:true});
   }
   if(route==='install/')assert.match(await page.locator('#install-status').textContent(),/Local preview|verified/);
   if(route==='install/factory.html'){
    assert.equal(await page.locator('#factory-file').count(),1);
    assert(await page.evaluate(()=>typeof document.getElementById('factory-file').onchange==='function'));
   }
  }
  assert.deepEqual(errors,[]);console.log('Browser PASS: real AudioWorklet, five HOME pages, art bag/upward wrap, CH hold, PLAY/panic, native windows, mobile layouts, sample conversion/replacement/export, installer and console.');
 }finally{await browser.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
