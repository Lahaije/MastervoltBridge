#ifndef WEB_UI_H
#define WEB_UI_H

#include <pgmspace.h>

// Self-contained HTML settings page served at GET /.
// All assets inline (CSS + JS) so the ENC28J60 only serves one request.
// Talks to the existing /api/* JSON endpoints.
// PROGMEM places blob in flash; served via chunked writes to avoid heap copy.

static const char WEB_UI_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Mastervolt Bridge</title>
<style>
:root{--bg:#111;--card:#1c1c1c;--fg:#eee;--muted:#888;--accent:#4ea8ff;--ok:#3ecf6c;--bad:#ff5a5a;--warn:#f5b342}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--fg);font-family:system-ui,sans-serif;font-size:15px;line-height:1.4}
header{padding:14px 20px;background:#000;border-bottom:1px solid #222;display:flex;justify-content:space-between;align-items:center}
header h1{margin:0;font-size:18px;font-weight:600}
header .live{font-size:12px;color:var(--muted)}
main{max-width:780px;margin:0 auto;padding:14px}
.card{background:var(--card);border:1px solid #2a2a2a;border-radius:8px;padding:14px;margin-bottom:14px}
.card h2{margin:0 0 10px;font-size:14px;color:var(--muted);text-transform:uppercase;letter-spacing:.05em}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:6px 16px}
.grid .k{color:var(--muted);font-size:13px}
.grid .v{font-variant-numeric:tabular-nums}
.row{display:flex;gap:8px;align-items:center;margin:8px 0;flex-wrap:wrap}
.row label{flex:0 0 140px;color:var(--muted);font-size:13px}
.row input[type=number]{flex:1;min-width:100px;background:#0c0c0c;border:1px solid #333;color:var(--fg);padding:6px 8px;border-radius:4px;font:inherit}
.row input[type=checkbox]{width:18px;height:18px}
button{background:#2a2a2a;border:1px solid #3a3a3a;color:var(--fg);padding:6px 12px;border-radius:4px;font:inherit;cursor:pointer}
button:hover{background:#333}
button.primary{background:var(--accent);border-color:var(--accent);color:#fff}
button.warn{background:var(--warn);border-color:var(--warn);color:#000}
.pill{display:inline-block;padding:2px 8px;border-radius:10px;font-size:12px;font-weight:600}
.pill.ok{background:#13351f;color:var(--ok)}
.pill.bad{background:#391616;color:var(--bad)}
.pill.warn{background:#3a2a10;color:var(--warn)}
.pill.muted{background:#222;color:var(--muted)}
.msg{margin-top:8px;font-size:13px;min-height:18px}
.msg.ok{color:var(--ok)}
.msg.bad{color:var(--bad)}
.unknown{color:var(--muted);font-style:italic}
footer{padding:14px 20px;text-align:center;color:var(--muted);font-size:12px}
</style>
</head>
<body>
<header>
<h1>Mastervolt Bridge</h1>
<span class="live" id="lastFetch">connecting...</span>
</header>
<main>

<div class="card">
<h2>Inverter Status</h2>
<div class="grid">
<div class="k">Power</div><div class="v" id="sPower">-</div>
<div class="k">Daily yield</div><div class="v" id="sDaily">-</div>
<div class="k">Total yield</div><div class="v" id="sTotal">-</div>
<div class="k">Operating</div><div class="v" id="sOp">-</div>
<div class="k">Link state</div><div class="v" id="sLink">-</div>
<div class="k">Failure streak</div><div class="v" id="sStreak">-</div>
<div class="k">WiFi</div><div class="v" id="sWifi">-</div>
<div class="k">Ethernet</div><div class="v" id="sEth">-</div>
</div>
</div>

<div class="card">
<h2>Power Limit</h2>
<div class="row">
<label>Current</label>
<span class="v" id="curPower">unknown</span>
</div>
<div class="row">
<label for="iPower">New (W)</label>
<input type="number" id="iPower" min="0" max="1575" step="1">
<button class="primary" id="bPower">Apply</button>
</div>
<div class="msg" id="mPower"></div>
</div>

<div class="card">
<h2>Shadow Function</h2>
<div class="row">
<label>Current</label>
<span class="v" id="curShadow">unknown</span>
</div>
<div class="row">
<label for="iShadow">Enabled</label>
<input type="checkbox" id="iShadow">
<button class="primary" id="bShadow">Apply</button>
</div>
<div class="msg" id="mShadow"></div>
</div>

<div class="card">
<h2>Actions</h2>
<div class="row">
<label for="iDebug">Debug logs</label>
<input type="checkbox" id="iDebug">
<button id="bDebug">Apply</button>
</div>
<div class="row">
<label></label>
<button id="bPulse">Wake Pulse</button>
<button id="bWifiOff" class="warn">Inverter WiFi Off</button>
</div>
<div class="msg" id="mAction"></div>
</div>

<div class="card">
<h2>Polling</h2>
<div class="grid">
<div class="k">Interval</div><div class="v" id="sPollInt">-</div>
<div class="k">State</div><div class="v" id="sPollState">-</div>
</div>
<div class="row">
<label for="iInterval">Interval (s)</label>
<input type="number" id="iInterval" min="1" max="300" step="1">
<button class="primary" id="bInterval">Apply</button>
</div>
<div class="msg" id="mInterval"></div>
</div>

</main>
<footer>API at <a href="/api" style="color:var(--accent)">/api</a></footer>

<script>
const $=id=>document.getElementById(id);

function pill(text,cls){return '<span class="pill '+cls+'">'+text+'</span>';}
function flash(el,ok,msg){el.textContent=msg;el.className='msg '+(ok?'ok':'bad');setTimeout(()=>{if(el.textContent===msg)el.textContent='';},4000);}

async function jget(url){const r=await fetch(url,{cache:'no-store'});if(!r.ok)throw new Error('HTTP '+r.status);return r.json();}
async function jpost(url,body){const r=await fetch(url,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});let d=null;try{d=await r.json();}catch(e){}if(!r.ok){const m=(d&&d.error)?d.error:('HTTP '+r.status);throw new Error(m);}return d||{};}

async function refresh(){
  try{
    const health=await jget('/api/health');
    $('sWifi').innerHTML=health.wifi_connected?pill('connected '+health.wifi_ip,'ok'):pill('disconnected','bad');
    $('sEth').textContent=health.ethernet_ip||'-';
    $('iDebug').checked=!!health.debug_mode;

    try{
      const info=await jget('/api/info');
      $('sPower').textContent=info.power?(info.power+' W'):'-';
      $('sDaily').textContent=info.daily_yield?(info.daily_yield+' kWh'):'-';
      $('sTotal').textContent=info.total_yield?(info.total_yield+' kWh'):'-';
      $('sOp').textContent=info.operating_status?(info.operating_status==='1'?'normal':'code '+info.operating_status):'-';
      $('sLink').innerHTML=pill(info.inverter_link_state||'?',info.inverter_link_state==='ONLINE'?'ok':info.inverter_link_state==='DORMANT'?'bad':'warn');
      $('sStreak').textContent=(info.failure_streak_s||0)+' s';
      $('sPollInt').textContent=(info.poll_interval_ms/1000)+' s';
      $('sPollState').textContent=info.inverter_link_state||'-';
    }catch(e){
      $('sPower').innerHTML='<span class="unknown">offline</span>';
      $('sDaily').innerHTML='<span class="unknown">-</span>';
      $('sTotal').innerHTML='<span class="unknown">-</span>';
      $('sOp').innerHTML='<span class="unknown">-</span>';
      $('sLink').innerHTML=pill('offline','bad');
      $('sStreak').textContent='-';
    }
    $('lastFetch').textContent='updated '+new Date().toLocaleTimeString();
  }catch(e){
    $('lastFetch').textContent='error: '+e.message;
  }
}

$('bPower').onclick=async()=>{
  const v=parseInt($('iPower').value,10);
  try{const r=await jpost('/api/power',{power:v});flash($('mPower'),true,r.deferred?'Deferred: '+r.reason:'Power set to '+v+' W');refresh();}
  catch(e){flash($('mPower'),false,e.message);}
};

$('bShadow').onclick=async()=>{
  const v=$('iShadow').checked;
  try{const r=await jpost('/api/shadow',{enabled:v});flash($('mShadow'),true,r.deferred?'Deferred: '+r.reason:'Shadow '+(v?'enabled':'disabled'));refresh();}
  catch(e){flash($('mShadow'),false,e.message);}
};

$('bDebug').onclick=async()=>{
  const v=$('iDebug').checked;
  try{await jpost('/api/debug',{debug:v});flash($('mAction'),true,'Debug '+(v?'on':'off'));refresh();}
  catch(e){flash($('mAction'),false,e.message);}
};

$('bPulse').onclick=async()=>{
  try{const r=await jget('/pulse');flash($('mAction'),r.reconnected,'Pulse: '+(r.reconnected?'reconnected':'no connection'));refresh();}
  catch(e){flash($('mAction'),false,e.message);}
};

$('bWifiOff').onclick=async()=>{
  if(!confirm('Turn inverter WiFi off?'))return;
  try{const r=await jpost('/wifi/off',{});flash($('mAction'),true,r.pressed?'WiFi-off sent':'already disconnected');}
  catch(e){flash($('mAction'),false,e.message);}
};

$('bInterval').onclick=async()=>{
  const v=parseInt($('iInterval').value,10);
  if(isNaN(v)||v<1||v>300){flash($('mInterval'),false,'Enter 1-300 seconds');return;}
  try{const r=await jpost('/api/interval',{interval:v*1000});flash($('mInterval'),true,'Interval set to '+(r.effective_interval_ms/1000)+' s');refresh();}
  catch(e){flash($('mInterval'),false,e.message);}
};

refresh();
setInterval(refresh,5000);
</script>
</body>
</html>
)HTML";

// Compile-time length (excludes null terminator)
static constexpr size_t WEB_UI_HTML_LEN = sizeof(WEB_UI_HTML) - 1;

// Safety guard: fail compilation if HTML blob exceeds 16 KB
static_assert(WEB_UI_HTML_LEN < 16384, "Web UI HTML too large for stable ENC28J60 delivery");

#endif // WEB_UI_H
