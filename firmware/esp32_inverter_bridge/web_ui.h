#ifndef WEB_UI_H
#define WEB_UI_H

#include <pgmspace.h>

// Self-contained HTML settings page served at GET /.
// All assets inline (CSS + JS) so the ENC28J60 only serves one request.
// Talks to the existing /api/* JSON endpoints.
// PROGMEM places blob in flash; served via chunked writes to avoid heap copy.

static const char WEB_UI_HTML[] PROGMEM = R"HTML(<!doctype html><html lang="en"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Mastervolt Bridge</title>
<style>
:root{--bg:#111;--card:#1c1c1c;--fg:#eee;--muted:#888;--accent:#4ea8ff;--ok:#3ecf6c;--bad:#ff5a5a;--warn:#f5b342}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--fg);font-family:system-ui,sans-serif;font-size:15px;line-height:1.4}
header{padding:14px 20px;background:#000;border-bottom:1px solid #222;display:flex;justify-content:space-between;align-items:center}
header h1{margin:0;font-size:18px;font-weight:600}header .live{font-size:12px;color:var(--muted)}
main{max-width:780px;margin:0 auto;padding:14px}
section{background:var(--card);border:1px solid #2a2a2a;border-radius:8px;padding:14px;margin-bottom:14px}
section h2{margin:0 0 10px;font-size:14px;color:var(--muted);text-transform:uppercase;letter-spacing:.05em}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:6px 16px}
.grid i{color:var(--muted);font-size:13px;font-style:normal}.grid b{font-weight:normal;font-variant-numeric:tabular-nums}
p{display:flex;gap:8px;align-items:center;margin:8px 0;flex-wrap:wrap}p label{flex:0 0 140px;color:var(--muted);font-size:13px}
p input[type=number]{background:#0c0c0c;border:1px solid #333;color:var(--fg);padding:6px 8px;border-radius:4px;font:inherit}
p input[type=checkbox]{width:18px;height:18px}
.n{flex:0 0 5ch;min-width:unset}
button{background:var(--accent);border:1px solid var(--accent);color:#fff;padding:6px 12px;border-radius:4px;font:inherit;cursor:pointer}
button:hover{opacity:.85}
.pill{display:inline-block;padding:2px 8px;border-radius:10px;font-size:12px;font-weight:600}
.pill.ok{background:#13351f;color:var(--ok)}.pill.bad{background:#391616;color:var(--bad)}.pill.warn{background:#3a2a10;color:var(--warn)}
.msg{margin-top:8px;font-size:13px;min-height:18px}.msg.ok{color:var(--ok)}.msg.bad{color:var(--bad)}
.unk{color:var(--muted);font-style:italic}[hidden]{display:none}
#fwVersion{font-size:12px;color:var(--muted);margin-top:2px}
footer{padding:14px 20px;text-align:center;color:var(--muted);font-size:12px}
</style></head><body>
<header><div><h1>Mastervolt Bridge</h1><div id="fwVersion">-</div></div><span class="live" id="lastFetch">connecting...</span></header>
<main>
<section><h2>Inverter Status</h2><div class="grid">
<i>Power</i><b id="sPower">-</b>
<i>Daily yield</i><b id="sDaily">-</b>
<i>Total yield</i><b id="sTotal">-</b>
<i>Operating</i><b id="sOp">-</b>
<i>Link state</i><b id="sLink">-</b>
<i>Failure streak</i><b id="sStreak">-</b>
<i>WiFi</i><b id="sWifi">-</b>
<i>Ethernet</i><b id="sEth">-</b>
</div></section>
<section><h2>Polling</h2>
<p><label for="iInterval">Interval (s)</label><input type="number" id="iInterval" min=1 max=300 class=n><button id="bInterval">Apply</button></p>
<p class=msg id="mInterval"></p></section>
<section><h2>Power Limit</h2>
<p id="rowPowerUnknown"><label>Status</label><span class="unk" id="curPower">unknown</span></p>
<p id="rowPowerForm" hidden><label for="iPower">Power limit (W)</label><input type="number" id="iPower" min=0 max=1575 class=n><button id="bPower">Apply</button></p>
<p class=msg id="mPower"></p></section>
<section><h2>Shadow Function</h2>
<p id="rowShadowUnknown"><label>Status</label><span class="unk" id="curShadow">unknown</span></p>
<p id="rowShadowForm" hidden><label for="iShadow">Enabled</label><input type="checkbox" id="iShadow"><button id="bShadow">Apply</button></p>
<p class=msg id="mShadow"></p></section>
<section><h2>Actions</h2>
<p><label for="iDebug">Debug logs</label><input type="checkbox" id="iDebug"><button id="bDebug">Apply</button></p>
<p class=msg id="mAction"></p></section>
<section><h2>MQTT Settings</h2>
<div class="grid"><i>Status</i><b id="sMqtt">-</b></div>
<p><label for="iMqttIp">Broker IP</label><input type="text" id="iMqttIp" style="background:#0c0c0c;border:1px solid #333;color:var(--fg);padding:6px 8px;border-radius:4px;font:inherit;width:140px"></p>
<p><label for="iMqttPort">Port</label><input type="number" id="iMqttPort" min=1 max=65535 class=n></p>
<p><label for="iMqttUser">Username</label><input type="text" id="iMqttUser" style="background:#0c0c0c;border:1px solid #333;color:var(--fg);padding:6px 8px;border-radius:4px;font:inherit;width:140px" autocomplete="off"></p>
<p><label for="iMqttPass">Password</label><input type="password" id="iMqttPass" style="background:#0c0c0c;border:1px solid #333;color:var(--fg);padding:6px 8px;border-radius:4px;font:inherit;width:140px" autocomplete="off" placeholder="(unchanged)"></p>
<p><label for="iMqttPrefix">Topic prefix</label><input type="text" id="iMqttPrefix" style="background:#0c0c0c;border:1px solid #333;color:var(--fg);padding:6px 8px;border-radius:4px;font:inherit;width:180px"></p>
<p><label for="iMqttOn">Enabled</label><input type="checkbox" id="iMqttOn"></p>
<p><button id="bMqtt">Save MQTT</button></p>
<p class=msg id="mMqtt"></p></section>
</main>
<footer>API at <a href="/api" style="color:var(--accent)">/api</a></footer>
<script>
const $=id=>document.getElementById(id);
const UH='<span class="unk">-</span>';
function pill(t,c){return'<span class="pill '+c+'">'+t+'</span>';}
function flash(el,ok,msg){el.textContent=msg;el.className='msg '+(ok?'ok':'bad');setTimeout(()=>{if(el.textContent===msg)el.textContent='';},4000);}
function fin(v){return typeof v==='number'&&isFinite(v);}
function focused(id){return document.activeElement===$(id);}
function fmtKwh(v){return fin(v)?(v.toFixed(3)+' kWh'):'-';}
function syncToggle(known,unknownRow,formRow,statusEl,inputId,val,isBool){
  $(unknownRow).hidden=known;$(formRow).hidden=!known;
  if(known){statusEl.textContent='';statusEl.className='';if(!focused(inputId)){if(isBool)$(inputId).checked=val;else $(inputId).value=val;}}
  else{statusEl.textContent='unknown';statusEl.className='unk';if(isBool)$(inputId).checked=false;else $(inputId).value='';}
}
function syncPower(i){syncToggle(fin(i.power_limit_watts),'rowPowerUnknown','rowPowerForm',$('curPower'),'iPower',i.power_limit_watts,false);}
function syncShadow(i){syncToggle(typeof i.shadow_enabled==='boolean','rowShadowUnknown','rowShadowForm',$('curShadow'),'iShadow',i.shadow_enabled,true);}
async function jget(u){var r=await fetch(u,{cache:'no-store'});if(!r.ok)throw new Error('HTTP '+r.status);return r.json();}
async function jpost(u,b){var r=await fetch(u,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(b)});var d=null;try{d=await r.json();}catch(e){}if(!r.ok)throw new Error((d&&d.error)||'HTTP '+r.status);return d||{};}
async function loadDevice(){
  try{var d=await jget('/api/device');
    if(d.firmware_version)$('fwVersion').textContent='Firmware: '+d.firmware_version;
    $('sEth').textContent=d.ethernet_ip||'-';
  }catch(e){}}
async function refreshHealth(){
  try{var h=await jget('/api/health');
    $('sWifi').innerHTML=h.wifi_connected?pill('connected','ok'):pill('disconnected','bad');
    $('iDebug').checked=!!h.debug_mode;
    $('sOp').textContent=h.operating_status?(h.operating_status==='1'?'normal':'code '+h.operating_status):'-';
    $('sLink').innerHTML=pill(h.inverter_link_state||'?',h.inverter_link_state==='ONLINE'?'ok':h.inverter_link_state==='DORMANT'?'bad':'warn');
  }catch(e){}}
async function refreshInfo(){
  try{var i=await jget('/api/info');
    $('sPower').textContent=i.power?(i.power+' W'):'-';
    $('sDaily').textContent=fmtKwh(i.daily_yield);$('sTotal').textContent=fmtKwh(i.total_yield);
    $('sStreak').textContent=(i.failure_streak_s||0)+' s';
    if(!focused('iInterval'))$('iInterval').value=Math.round(i.poll_interval_ms/1000);
    syncPower(i);syncShadow(i);
    $('lastFetch').textContent='updated '+new Date().toLocaleTimeString();
  }catch(e){
    $('sPower').innerHTML='<span class="unk">offline</span>';
    $('sDaily').innerHTML=UH;$('sTotal').innerHTML=UH;
    $('sStreak').textContent='-';
    syncPower({});syncShadow({});
    $('lastFetch').textContent='error: '+e.message;
  }}
$('bPower').onclick=async()=>{var v=parseInt($('iPower').value,10);
  try{var r=await jpost('/api/power',{power:v});flash($('mPower'),true,r.deferred?'Deferred: '+r.reason:'Power set to '+v+' W');refreshInfo();}catch(e){flash($('mPower'),false,e.message);}};
$('bShadow').onclick=async()=>{var v=$('iShadow').checked;
  try{var r=await jpost('/api/shadow',{enabled:v});flash($('mShadow'),true,r.deferred?'Deferred: '+r.reason:'Shadow '+(v?'enabled':'disabled'));refreshInfo();}catch(e){flash($('mShadow'),false,e.message);}};
$('bDebug').onclick=async()=>{var v=$('iDebug').checked;
  try{await jpost('/api/debug',{debug:v});flash($('mAction'),true,'Debug '+(v?'on':'off'));refreshHealth();}catch(e){flash($('mAction'),false,e.message);}};
$('bInterval').onclick=async()=>{var v=parseInt($('iInterval').value,10);
  if(isNaN(v)||v<1||v>300){flash($('mInterval'),false,'Enter 1-300 seconds');return;}
  try{var r=await jpost('/api/interval',{interval:v*1000});flash($('mInterval'),true,'Interval set to '+(r.poll_interval_ms/1000)+' s');refreshInfo();}catch(e){flash($('mInterval'),false,e.message);}};
$('bMqtt').onclick=async()=>{
  var b={broker_ip:$('iMqttIp').value,broker_port:parseInt($('iMqttPort').value,10),enabled:$('iMqttOn').checked,topic_prefix:$('iMqttPrefix').value,username:$('iMqttUser').value};
  var pw=$('iMqttPass').value;if(pw.length>0)b.password=pw;
  try{var r=await jpost('/api/mqtt',b);flash($('mMqtt'),true,'Saved');refreshMqtt();}catch(e){flash($('mMqtt'),false,e.message);}};
async function refreshMqtt(){
  try{var m=await jget('/api/mqtt');
    $('sMqtt').innerHTML=m.connected?pill('connected','ok'):pill('disconnected','bad');
    if(!focused('iMqttIp'))$('iMqttIp').value=m.broker_ip||'';
    if(!focused('iMqttPort'))$('iMqttPort').value=m.broker_port||1883;
    if(!focused('iMqttUser'))$('iMqttUser').value=m.username||'';
    if(!focused('iMqttPrefix'))$('iMqttPrefix').value=m.topic_prefix||'';
    $('iMqttOn').checked=!!m.enabled;
    $('iMqttPass').placeholder=m.has_password?'(saved)':'(none)';
  }catch(e){$('sMqtt').innerHTML=pill('error','bad');}}
loadDevice();refreshHealth();refreshInfo();refreshMqtt();setInterval(refreshInfo,5000);setInterval(refreshHealth,60000);setInterval(refreshMqtt,30000);
</script></body></html>)HTML";

// Compile-time length (excludes null terminator)
static constexpr size_t WEB_UI_HTML_LEN = sizeof(WEB_UI_HTML) - 1;

// Safety guard: fail compilation if HTML blob exceeds 16 KB
static_assert(WEB_UI_HTML_LEN < 16384, "Web UI HTML too large for stable ENC28J60 delivery");

#endif // WEB_UI_H
