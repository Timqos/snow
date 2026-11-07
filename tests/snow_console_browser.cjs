// SPDX-License-Identifier: GPL-3.0-only
// Run locally with Playwright + Chromium. The serial port is a deterministic
// CDC model; this does not claim a physical FM-1 USB test.
const fs=require('fs'),path=require('path'),http=require('http'),assert=require('node:assert/strict');
const {chromium}=require('playwright');
const ROOT=path.resolve(__dirname,'../build/snow-preview'),OUT=path.resolve(__dirname,'../build/stattic-usb-review');
const map=JSON.parse(fs.readFileSync(path.join(ROOT,'console/maps.json'))).maps[0];
const mime={'.html':'text/html','.mjs':'text/javascript','.js':'text/javascript','.json':'application/json','.css':'text/css','.svg':'image/svg+xml'};
(async()=>{
 fs.mkdirSync(OUT,{recursive:true});
 const server=http.createServer((req,res)=>{
  let rel=decodeURIComponent(new URL(req.url,'http://localhost').pathname);if(rel.endsWith('/'))rel+='index.html';
  const base=rel.startsWith('/snow/')?ROOT:path.resolve(__dirname,'../../snow-publish');if(base===ROOT)rel=rel.slice(5);
  const p=path.resolve(base,'.'+rel);if(!p.startsWith(base+'/')){res.writeHead(403);return res.end();}
  fs.readFile(p,(e,b)=>{if(e){res.writeHead(404);res.end();}else{res.setHeader('Content-Type',mime[path.extname(p)]||'application/octet-stream');res.end(b);}});
 });
 await new Promise(r=>server.listen(0,'127.0.0.1',r));let browser;
 try{
  browser=await chromium.launch({executablePath:process.env.CHROMIUM||'/usr/bin/chromium',headless:true,args:['--no-sandbox','--disable-dev-shm-usage'],env:{...process.env,XDG_CONFIG_HOME:'/tmp/stattic-usb-config',XDG_CACHE_HOME:'/tmp/stattic-usb-cache'}});
  const context=await browser.newContext({viewport:{width:1440,height:1100},locale:'en-US',acceptDownloads:true});
  await context.addInitScript(map=>{
   const S={requests:0,commands:[],late:1050,uptime:10000,midi:0};window.__usb=S;
   navigator.requestMIDIAccess=()=>{S.midi++;throw new Error('Unexpected MIDI access');};
   Object.defineProperty(navigator,'serial',{configurable:true,value:{async requestPort(options){
    S.requests++;if(options.filters[0].usbVendorId!==0x1209||options.filters[0].usbProductId!==1)throw new Error('Wrong USB filter');
    let controller;const enc=new TextEncoder();
    const push=text=>{for(let i=0;i<text.length;i+=7)controller.enqueue(enc.encode(text.slice(i,i+7)));};
    const words=values=>{const b=new Uint8Array(values.length*4),v=new DataView(b.buffer);values.forEach((x,i)=>v.setUint32(i*4,x,true));return b;};
    const port={readable:new ReadableStream({start(c){controller=c;},cancel(){S.cancelled=true;}}),
     writable:new WritableStream({write(bytes){
      const cmd=new TextDecoder().decode(bytes).trim();S.commands.push(cmd);
      if(cmd==='status'){S.uptime+=2000;push(`status\r\nfelucca ${map.version}\r\nuptime_ms ${S.uptime}\r\ncpu_pct 80\r\naudio_late ${S.late}\r\nboots 4\r\ncpu2 1\r\ncpu2_bad 0\r\ncpu_mhz 360\r\nengine CHORD\r\npreset <img src=x onerror=bad()>\r\n> `);return;}
      if(cmd==='crash'){push('crash\r\nno crash record\r\n> ');return;}
      const m=/^memr 0x([0-9a-f]+) (\d+)$/.exec(cmd);if(!m)throw new Error('Unexpected command: '+cmd);
      const entry=Object.entries(map.regions).find(([,r])=>r.address===parseInt(m[1],16)&&r.size===Number(m[2]));if(!entry)throw new Error('Unmapped read');
      const [name,r]=entry;
      const data={late_base:words([800]),late_counter:words([S.late]),peak:words([190000]),had_late:Uint8Array.of(1),event:words([167155,70000,30000,42000,7000]),rendered_voices:Uint8Array.of(1,1,1,1,1,4),part_cost:new Uint8Array(new Uint16Array([500,600,700,800,900,5000]).buffer)}[name];
      let reply=cmd+'\r\n';for(let i=0;i<data.length;i+=16)reply+=(r.address+i).toString(16).padStart(8,'0')+': '+Array.from(data.slice(i,i+16),x=>x.toString(16).padStart(2,'0')).join(' ')+'\r\n';push(reply+'> ');
     }}),async open(){S.open=true;},async setSignals(signals){if(signals.dataTerminalReady)push(`\r\nFelucca ${map.version} console - 'help'\r\n> `);},async close(){S.open=false;},getInfo(){return {usbVendorId:0x1209,usbProductId:1};}};
    S.drop=()=>controller.close();return port;
   }}});
  },map);
  const page=await context.newPage(),errors=[];page.on('pageerror',e=>errors.push(e.message));
  const url=`http://127.0.0.1:${server.address().port}/snow/console/`;
  await page.goto(url,{waitUntil:'networkidle'});
  assert.equal(await page.evaluate(()=>__usb.requests),0);assert(await page.locator('#export-json').isDisabled());
  await page.locator('#connect').click();await page.waitForFunction(()=>document.querySelector('#count').textContent.startsWith('1 sample'));
  await page.locator('#pause').click();
  assert.equal(await page.locator('#cpu').innerText(),'80');assert.equal(await page.locator('#late').innerText(),'250');
  assert.equal(await page.locator('#synth').innerText(),'2,917');assert((await page.locator('#tracks').innerText()).includes('CH: 4 voices'));
  assert.equal(await page.locator('img[src="x"]').count(),0);
  await page.evaluate(()=>{__usb.late+=3;});await page.locator('#snapshot').click();
  await page.waitForFunction(()=>document.querySelector('#count').textContent.startsWith('2 samples'));
  assert.equal(await page.locator('#misses').innerText(),'3');assert.equal(await page.locator('#late').innerText(),'253');
  const sent=await page.evaluate(()=>__usb.commands.length);
  await page.locator('#note').fill('CH + KK + BS <svg onload=bad()>');await page.locator('#mark').click();
  assert.equal(await page.evaluate(()=>__usb.commands.length),sent);
  const downloadPromise=page.waitForEvent('download');await page.locator('#export-json').click();const download=await downloadPromise;
  const file=path.join(OUT,'console-capture.json');await download.saveAs(file);const report=JSON.parse(fs.readFileSync(file));
  assert.equal(report.samples.length,2);assert.equal(report.samples[1].delta_late,3);
  assert.equal(report.samples[1].stages.ch_synth_us,70000/24);
  assert(report.events.some(e=>e.type==='rasp-marker'&&e.note.includes('CH + KK + BS')));assert(report.raw.includes('memr'));
  await page.locator('#crash').click();await page.waitForFunction(()=>document.querySelector('#message').textContent.includes('Crash report saved'));
  assert.equal(await page.evaluate(()=>__usb.midi),0);
  await page.screenshot({path:path.join(OUT,'console-desktop.png'),fullPage:true});
  await page.setViewportSize({width:390,height:844});await page.screenshot({path:path.join(OUT,'console-mobile.png'),fullPage:true});
  assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1));
  await page.locator('#disconnect').click();await page.waitForFunction(()=>document.querySelector('#connection').textContent==='Disconnected');
  assert(!(await page.locator('#export-json').isDisabled()));assert(await page.locator('#mark').isDisabled());
  await page.locator('#connect').click();await page.waitForFunction(()=>document.querySelector('#count').textContent.startsWith('3 samples'));
  await page.evaluate(()=>__usb.drop());await page.waitForFunction(()=>document.querySelector('#connection').textContent==='Disconnected');
  assert(!(await page.locator('#export-json').isDisabled()));assert.deepEqual(errors,[]);
  const unsupported=await browser.newPage();await unsupported.addInitScript(()=>Object.defineProperty(navigator,'serial',{value:undefined}));
  await unsupported.goto(url,{waitUntil:'networkidle'});assert(await unsupported.locator('#connect').isDisabled());await unsupported.locator('#clear').click();assert(await unsupported.locator('#connect').isDisabled());
  const main=await context.newPage();await main.setViewportSize({width:390,height:844});
  await main.goto(url.replace('console/',''),{waitUntil:'networkidle'});
  assert.equal(await main.locator('.snow-header nav').getByRole('link',{name:'USB console',exact:true}).count(),1);
  assert((await main.locator('.hardware-fault').innerText()).includes('Known hardware audio fault'));
  assert(await main.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1));
  await main.screenshot({path:path.join(OUT,'main-console-link-mobile.png'),fullPage:true});
  console.log('USB BROWSER PASSED: explicit port permission only, fragmented CDC, real map decoding, growing L, local markers, JSON download, no MIDI/settings/reset writes, disconnect/reconnect/unplug, unsupported browser and mobile layout. Physical USB remains untested.');
 }finally{if(browser)await browser.close();await new Promise(r=>server.close(r));}
})().catch(e=>{console.error(e.stack);process.exit(1);});
