// SPDX-License-Identifier: GPL-3.0-only
// Real shipped page with a fragmented Web Serial model. Its S1 banner is
// compiled from cdc_task() by stattic_console_test.mjs. No physical USB.
const fs=require('node:fs'),path=require('node:path'),http=require('node:http'),assert=require('node:assert/strict');
const {execFileSync}=require('node:child_process'),{chromium}=require('playwright');
const ROOT=path.resolve(__dirname,'../web/stattic'),OUT=path.resolve(__dirname,'../build/stattic-console-browser');
(async()=>{
 execFileSync(process.execPath,['tests/stattic_console_test.mjs'],{cwd:path.resolve(__dirname,'..'),stdio:'pipe'});
 const wire=JSON.parse(fs.readFileSync(path.resolve(__dirname,'../build/stattic-console-wire.json')));
 fs.mkdirSync(OUT,{recursive:true});
 const mime={'.html':'text/html','.mjs':'text/javascript','.json':'application/json','.css':'text/css','.svg':'image/svg+xml'};
 const server=http.createServer((req,res)=>{let name=new URL(req.url,'http://localhost').pathname;if(name.endsWith('/'))name+='index.html';const p=path.resolve(ROOT,'.'+name);if(!p.startsWith(ROOT+path.sep)){res.writeHead(403);return res.end();}fs.readFile(p,(e,data)=>{if(e){res.writeHead(404);return res.end();}res.setHeader('Content-Type',mime[path.extname(p)]||'application/octet-stream');res.end(data);});});
 await new Promise(r=>server.listen(0,'127.0.0.1',r));let browser;
 try{
  browser=await chromium.launch({executablePath:process.env.CHROMIUM||'/usr/bin/chromium',headless:true,args:['--no-sandbox']});
  const context=await browser.newContext({viewport:{width:1250,height:1100},locale:'en-US',acceptDownloads:true});
  await context.addInitScript(wire=>{
   const state={requests:0,commands:[],late:7,uptime:10000,badBanner:false};window.__usb=state;
   Object.defineProperty(navigator,'serial',{configurable:true,value:{async requestPort(options){
    state.requests++;if(options.filters[0].usbVendorId!==0x1209||options.filters[0].usbProductId!==1)throw new Error('Wrong USB filter');
    let controller;const encode=new TextEncoder(),push=text=>{for(let i=0;i<text.length;i+=3)controller.enqueue(encode.encode(text.slice(i,i+3)));};
    return {readable:new ReadableStream({start(c){controller=c;},cancel(){state.cancelled=true;}}),writable:new WritableStream({write(bytes){const cmd=new TextDecoder().decode(bytes).trim();state.commands.push(cmd);if(cmd==='status'){state.uptime+=2000;push(wire.status.replace(/audio_late \d+/,`audio_late ${state.late}`).replace(/uptime_ms \d+/,`uptime_ms ${state.uptime}`));}else if(cmd==='crash')push('crash\r\nno crash record\r\n> ');else throw new Error('Unexpected console command: '+cmd);}}),async open(){state.open=true;},async setSignals(s){if(s.dataTerminalReady)push(state.badBanner?'Other device\r\n> ':wire.greeting);},async close(){state.open=false;},getInfo(){return {usbVendorId:0x1209,usbProductId:1};}};
   }}});
  },wire);
  const page=await context.newPage(),errors=[];page.on('pageerror',e=>errors.push(e.message));
  await page.goto(`http://127.0.0.1:${server.address().port}/console/`);
  assert.equal(await page.evaluate(()=>__usb.requests),0);await page.locator('#connect').click();
  await page.waitForFunction(()=>document.getElementById('count').textContent.startsWith('1 sample'));
  await page.locator('#pause').click();assert.equal(await page.locator('#cpu').innerText(),'60');assert.equal(await page.locator('#late').innerText(),'7');assert.equal(await page.locator('#synth').innerText(),'2,000');
  assert.match(await page.locator('#firmware').innerText(),/STATTIC 0\.9\.0-S1.*2 cores/);assert.equal(await page.locator('#tracks span').count(),4);
  await page.evaluate(()=>__usb.late+=3);await page.locator('#snapshot').click();await page.waitForFunction(()=>document.getElementById('count').textContent.startsWith('2 samples'));
  assert.equal(await page.locator('#misses').innerText(),'3');await page.locator('#note').fill('CH + BS + DR');await page.locator('#mark').click();
  const download=page.waitForEvent('download');await page.locator('#export-json').click();const file=path.join(OUT,'capture.json');await (await download).saveAs(file);const log=JSON.parse(fs.readFileSync(file));
  assert.equal(log.tool_version,'5');assert.equal(log.samples[1].delta_late,3);assert(log.raw.includes(wire.greeting));assert(log.events.some(e=>e.type==='rasp-marker'));assert.deepEqual(await page.evaluate(()=>__usb.commands),['status','status']);
  await page.setViewportSize({width:390,height:900});assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1));await page.screenshot({path:path.join(OUT,'mobile.png'),fullPage:true});
  await page.locator('#disconnect').click();await page.waitForFunction(()=>document.getElementById('connection').textContent==='Disconnected');assert(await page.evaluate(()=>!__usb.open));
  await page.evaluate(()=>__usb.badBanner=true);await page.locator('#connect').click();await page.waitForFunction(()=>document.getElementById('message').textContent.includes('did not identify'));
  assert(await page.evaluate(()=>!__usb.open));assert.deepEqual(errors,[]);
  console.log('S1 console browser PASS: real C banner without v, fragmented CDC, direct status, four channel timings, L delta, markers/export, mobile, disconnect and unrelated-device rejection. No device writes.');
 }finally{if(browser)await browser.close();await new Promise(r=>server.close(r));}
})().catch(e=>{console.error(e);process.exitCode=1;});
