// The phone UI. One self-contained page (no internet on the boat), served
// from flash. Talks to the JSON endpoints in web.h.
#pragma once

static const char PAGE_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="format-detection" content="telephone=no,email=no,address=no">
<title>GPSSpeed</title>
<style>
:root{--bg:#0f1419;--card:#1a2129;--line:#2a333d;--fg:#e8edf2;--dim:#8b97a3;--acc:#3fb6ff;--ok:#39c96b;--warn:#f0b43c;--bad:#f0565c}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.4 -apple-system,system-ui,sans-serif;padding:12px 12px 40px}
h1{font-size:18px;margin:4px 0 10px;display:flex;justify-content:space-between;align-items:center}
h2{font-size:13px;text-transform:uppercase;letter-spacing:.06em;color:var(--dim);margin:0 0 10px}
.card{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:14px;margin-bottom:12px}
.big{font-size:56px;font-weight:700;line-height:1;font-variant-numeric:tabular-nums}.unit{font-size:18px;color:var(--dim);margin-left:4px}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:8px 14px;margin-top:12px}
.k{color:var(--dim);font-size:12px}.v{font-variant-numeric:tabular-nums;font-weight:600}
.pill{display:inline-block;padding:2px 9px;border-radius:99px;font-size:12px;font-weight:600;background:var(--line)}
.ok{background:#173d26;color:var(--ok)}.warn{background:#3d3217;color:var(--warn)}.bad{background:#3d1719;color:var(--bad)}
.row{display:flex;gap:8px;align-items:center;flex-wrap:wrap;margin:8px 0}
button{background:var(--acc);color:#001a2b;border:0;border-radius:9px;padding:10px 14px;font-weight:700;font-size:15px}
button.sec{background:var(--line);color:var(--fg)}button.bad{background:var(--bad);color:#fff}
input,select{background:#0b0f13;color:var(--fg);border:1px solid var(--line);border-radius:8px;padding:9px;font-size:16px;width:100%}
label{display:block;font-size:12px;color:var(--dim);margin:8px 0 3px}
.two{display:grid;grid-template-columns:1fr 1fr;gap:10px}
table{width:100%;border-collapse:collapse;font-variant-numeric:tabular-nums}td,th{padding:5px 4px;border-bottom:1px solid var(--line);text-align:left;font-size:14px}
.note{color:var(--dim);font-size:13px;margin:6px 0 0}#msg{position:fixed;left:12px;right:12px;bottom:12px;background:#223;border:1px solid var(--line);border-radius:10px;padding:10px;display:none}
progress{width:100%;height:10px}
.open{display:block;text-align:center;background:var(--acc);color:#001a2b;border-radius:9px;padding:8px;font-weight:700;font-size:15px;text-decoration:none}
.open span{display:block;font-weight:500;font-size:12px;opacity:.7}
ul.tips{margin:10px 0 0;padding-left:18px;color:var(--dim);font-size:13px}ul.tips li{margin:3px 0}ul.tips b{color:var(--fg);font-weight:600}
button.mini{padding:3px 10px;font-size:12px;border-radius:7px;background:var(--line);color:var(--fg);margin-left:4px;vertical-align:baseline}
</style></head><body>
<h1><span>GPSSpeed <span id="ver" class="k"></span></span><span id="wifi" class="pill"></span></h1>
<div class="card">
 <a class="open" href="http://gpsspeed.local/" target="_blank" rel="noopener">Open in browser<span>gpsspeed.local</span></a>
 <ul class="tips">
  <li>Tap the button to open the app in your browser.</li>
  <li>Once there: Share &rarr; <b>Add to Home Screen</b> for a one-tap icon.</li>
  <li>Need the link? <b>gpsspeed.local</b><button class="mini" onclick="copyUrl()">Copy</button></li>
 </ul>
</div>

<div class="card">
 <h2>Live</h2>
 <div><span class="big" id="out">--</span><span class="unit">MPH to dash</span></div>
 <div class="row"><span id="fix" class="pill">--</span><span id="mode" class="pill">--</span><span id="ant" class="pill">antenna ?</span></div>
 <div class="grid">
  <div><div class="k">GPS speed</div><div class="v" id="raw">--</div></div>
  <div><div class="k">Output</div><div class="v" id="hz">--</div></div>
  <div><div class="k">Satellites / HDOP</div><div class="v" id="sats">--</div></div>
  <div><div class="k">Fix age</div><div class="v" id="age">--</div></div>
  <div><div class="k">GPS link</div><div class="v" id="link">--</div></div>
  <div><div class="k">Battery</div><div class="v" id="vin">--</div></div>
  <div><div class="k">Uptime</div><div class="v" id="up">--</div></div>
  <div><div class="k">Last reset</div><div class="v" id="rst">--</div></div>
 </div>
</div>

<div class="card">
 <h2>Test the dash</h2>
 <div class="row"><button onclick="mode('gps')">Normal (GPS)</button><button class="sec" onclick="mode('selftest')">Self-test</button><button class="sec" onclick="mode('next')">Next step</button></div>
 <label>Hold a fixed speed (bench / dock)</label>
 <div class="row"><input id="man" type="number" step="0.1" min="0" max="60" placeholder="MPH" style="flex:1"><button class="sec" onclick="mode('manual',v('man'))">Set</button></div>
 <p class="note">Manual and self-test never persist: a reboot always comes back in Normal.</p>
</div>

<div class="card">
 <h2>Calibrate</h2>
 <p class="note" style="margin-top:0">Hold a speed (self-test or manual), read the dash, enter what it shows. The board corrects that speed and interpolates between points.</p>
 <div class="row"><div style="flex:1"><label>Commanded</label><div class="v" id="cmd">--</div></div>
  <div style="flex:1"><label>Dash shows</label><input id="dash" type="number" step="0.1" placeholder="MPH"></div></div>
 <div class="row"><button onclick="calAdd()">Save point</button><button class="sec" onclick="post('/api/cal/clear').then(load)">Clear all</button></div>
 <table id="cal"></table>
</div>

<div class="card">
 <h2>Settings</h2>
 <div class="two">
  <div><label>Hz per MPH</label><input id="k" type="number" step="0.001"></div>
  <div><label>No-fix speed (MPH)</label><input id="nofix" type="number" step="0.1"></div>
  <div><label>Below this, output 0 (MPH)</label><input id="min" type="number" step="0.1"></div>
  <div><label>GPS rate</label><select id="rate"><option>1</option><option>5</option><option>10</option></select></div>
  <div><label>Predictor</label><select id="pred"><option value="1">on</option><option value="0">off</option></select></div>
  <div><label>Look-ahead (s)</label><input id="lead" type="number" step="0.05"></div>
  <div><label>Accel smoothing (0-1)</label><input id="alpha" type="number" step="0.05"></div>
  <div><label>WiFi window (s)</label><input id="win" type="number" step="10"></div>
 </div>
 <label>WiFi password (optional, 8+ chars; blank = no change)</label><input id="pass" type="password" autocomplete="new-password">
 <div class="row"><button class="sec" onclick="post('/api/config',{open:1}).then(()=>toast('WiFi is open (no password) from next power-up'))">Remove password</button></div>
 <div class="row"><button onclick="save()">Save</button></div>
 <p class="note">GPS rate and WiFi changes apply on the next power cycle.</p>
</div>

<div class="card">
 <h2>Firmware</h2>
 <p class="note" style="margin-top:0">Upload a .bin built for ESP32-C3. A bad image can't brick it: after 3 crashes it rolls back to the previous firmware.</p>
 <input id="fw" type="file" accept=".bin">
 <div class="row"><button onclick="ota()">Upload</button><button class="sec" onclick="keep()">Keep WiFi on</button></div>
 <progress id="prog" value="0" max="100" style="display:none"></progress>
 <div class="row"><button class="sec" onclick="confirm('Reboot?')&&post('/api/reboot')">Reboot</button><button class="bad" onclick="confirm('Erase all settings and calibration?')&&post('/api/factory')">Factory reset</button></div>
</div>
<div id="msg"></div>

<script>
const $=id=>document.getElementById(id),v=id=>$(id).value;
function toast(t){const m=$('msg');m.textContent=t;m.style.display='block';clearTimeout(m._t);m._t=setTimeout(()=>m.style.display='none',2500)}
function post(u,d){return fetch(u,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams(d||{})}).then(r=>r.text().then(t=>{if(!r.ok)throw t;return t})).catch(e=>{toast('Error: '+e);throw e})}
function mode(m,mph){post('/api/mode',{mode:m,mph:mph||''}).then(()=>toast('Mode: '+m))}
function calAdd(){post('/api/cal',{dash:v('dash')}).then(()=>{toast('Point saved');$('dash').value='';load()})}
function keep(){post('/api/wifi/keep').then(()=>toast('WiFi stays on until reboot'))}
function pill(el,t,c){el.textContent=t;el.className='pill '+(c||'')}
function fmtUp(s){const h=Math.floor(s/3600),m=Math.floor(s/60)%60;return (h?h+'h ':'')+m+'m '+(s%60)+'s'}
async function tick(){try{const s=await (await fetch('/api/status')).json();
 $('ver').textContent='v'+s.fw;$('out').textContent=s.out.toFixed(1);$('raw').textContent=s.fix?s.raw.toFixed(2)+' MPH':'--';
 $('hz').textContent=s.hz?s.hz.toFixed(2)+' Hz':'idle';$('sats').textContent=s.sats+' / '+(s.hdop?s.hdop.toFixed(1):'--');
 $('age').textContent=s.age<0?'never':(s.age/1000).toFixed(1)+' s';$('link').textContent=s.baud?s.baud+' baud, '+s.rate+' Hz':'no data';
 $('vin').textContent=s.vin.toFixed(1)+' V';$('up').textContent=fmtUp(s.up);$('rst').textContent=s.reset;$('cmd').textContent=s.cmd.toFixed(1)+' MPH';
 pill($('fix'),s.fix?'GPS fix':(s.talking?'searching':'no GPS data'),s.fix?'ok':(s.talking?'warn':'bad'));
 pill($('mode'),s.mode,s.mode=='normal'?'':'warn');
 pill($('ant'),'antenna '+s.ant,s.ant=='OK'?'ok':(s.ant=='unknown'?'':'bad'));
 pill($('wifi'),s.wifi_keep?'WiFi: kept on':('WiFi: '+s.wifi_left+'s'),s.wifi_left<20&&!s.wifi_keep?'warn':'');
}catch(e){pill($('wifi'),'offline','bad')}}
async function load(){const c=await (await fetch('/api/config')).json();
 for(const k of ['k','nofix','min','lead','alpha','win'])$(k).value=c[k];$('rate').value=c.rate;$('pred').value=c.pred?1:0;
 $('cal').innerHTML='<tr><th>Speed</th><th>Correction</th></tr>'+(c.cal.length?c.cal.map(p=>'<tr><td>'+p.mph.toFixed(1)+' MPH</td><td>'+((p.f-1)*100).toFixed(2)+' %</td></tr>').join(''):'<tr><td colspan=2 class=k>none yet</td></tr>')}
function save(){const d={};for(const k of ['k','nofix','min','lead','alpha','win','rate','pred','pass'])d[k]=v(k);post('/api/config',d).then(()=>{toast('Saved');$('pass').value='';load()})}
function ota(){const f=$('fw').files[0];if(!f)return toast('Pick a .bin first');const x=new XMLHttpRequest(),p=$('prog');p.style.display='block';
 x.upload.onprogress=e=>p.value=e.loaded/e.total*100;x.onload=()=>toast(x.status==200?'Updated - rebooting':'Failed: '+x.responseText);
 x.onerror=()=>toast('Upload failed');const fd=new FormData();fd.append('fw',f);x.open('POST','/update');x.send(fd)}
function copyUrl(){const u='http://gpsspeed.local';(navigator.clipboard?navigator.clipboard.writeText(u):Promise.reject()).then(()=>toast('Copied '+u)).catch(()=>toast(u))}
// Tell the board a real page is open; on the first hello reload once so the
// phone re-checks the network and the pop-up's Cancel turns into Done.
fetch('/api/hello',{method:'POST'}).then(r=>r.text()).then(t=>{if(t==='new')location.reload()}).catch(()=>{});
load();tick();setInterval(tick,700);
</script></body></html>)HTML";
