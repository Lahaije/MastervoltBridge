#pragma once

// Self-contained HTML settings page served at GET /.
// All assets inline (CSS + JS) so the ENC28J60 only serves one request.
// Talks to the existing /api/* JSON endpoints.

static const char WEB_UI_HTML[] = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Mastervolt Bridge</title>
<style>
:root{--bg:#111;--card:#1c1c1c;--fg:#eee;--muted:#888;--accent:#4ea8ff;--ok:#3ecf6c;--bad:#ff5a5a;--warn:#f5b342;}
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
.row input[type=number],.row input[type=text],.row input[type=password]{flex:1;min-width:120px;background:#0c0c0c;border:1px solid #333;color:var(--fg);padding:6px 8px;border-radius:4px;font:inherit}
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
  <span class="live" id="lastFetch">never updated</span>
</header>
<main>

<div class="card">
  <h2>Status</h2>
  <div class="grid">
    <div class="k">Power</div>          <div class="v" id="sPower">-</div>
    <div class="k">Daily yield</div>    <div class="v" id="sDaily">-</div>
    <div class="k">Total yield</div>    <div class="v" id="sTotal">-</div>
    <div class="k">Operating</div>      <div class="v" id="sOp">-</div>
    <div class="k">Last poll</div>      <div class="v" id="sLast">-</div>
    <div class="k">WiFi</div>           <div class="v" id="sWifi">-</div>
    <div class="k">Ethernet</div>       <div class="v" id="sEth">-</div>
    <div class="k">MQTT</div>           <div class="v" id="sMqtt">-</div>
  </div>
</div>

<div class="card">
  <h2>Power Limit</h2>
  <div class="row">
    <label for="iPower">Current</label>
    <span class="v" id="curPower">-</span>
  </div>
  <div class="row">
    <label for="iPower">New (W)</label>
    <input type="number" id="iPower" min="0" max="1575" step="1">
    <button class="primary" id="bPower">Apply</button>
  </div>
  <div class="msg" id="mPower"></div>
</div>

<div class="card">
  <h2>Polling Interval</h2>
  <div class="row">
    <label>Current</label>
    <span class="v" id="curPoll">-</span>
  </div>
  <div class="row">
    <label for="iPoll">New (s)</label>
    <input type="number" id="iPoll" min="1" max="3600" step="1">
    <button class="primary" id="bPoll">Apply</button>
  </div>
  <div class="msg" id="mPoll"></div>
</div>

<div class="card">
  <h2>Shadow Function</h2>
  <div class="row">
    <label>Current</label>
    <span class="v" id="curShadow">-</span>
  </div>
  <div class="row">
    <label for="iShadow">Enabled</label>
    <input type="checkbox" id="iShadow">
    <button class="primary" id="bShadow">Apply</button>
  </div>
  <div class="msg" id="mShadow"></div>
</div>

<div class="card">
  <h2>Home Assistant (MQTT)</h2>
  <div class="row">
    <label for="iHa">HA enabled</label>
    <input type="checkbox" id="iHa">
  </div>
  <div class="row">
    <label for="iBroker">Broker IP</label>
    <input type="text" id="iBroker" placeholder="192.168.1.x">
  </div>
  <div class="row">
    <label for="iUser">Username</label>
    <input type="text" id="iUser" autocomplete="username">
  </div>
  <div class="row">
    <label for="iPass">Password</label>
    <input type="password" id="iPass" placeholder="(unchanged)" autocomplete="new-password">
  </div>
  <div class="row">
    <label></label>
    <button class="primary" id="bMqtt">Save</button>
    <span class="v" id="curMqtt"></span>
  </div>
  <div class="msg" id="mMqtt"></div>
</div>

<div class="card">
  <h2>Debug &amp; Actions</h2>
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

</main>
<footer>API at <a href="/api" style="color:var(--accent)">/api</a> &middot; bridge IP <span id="ip"></span></footer>

<script>
const $ = (id) => document.getElementById(id);
let mqttCache = null;  // /api/mqtt is fetched once and re-read only after a save

function setUnknown(el){ el.innerHTML = '<span class="unknown">unknown</span>'; }

function fmtPill(text, cls){ return '<span class="pill '+cls+'">'+text+'</span>'; }

function flash(el, ok, msg){
  el.textContent = msg;
  el.className = 'msg ' + (ok ? 'ok' : 'bad');
  setTimeout(() => { if(el.textContent === msg) el.textContent=''; }, 4000);
}

async function jget(url){
  const r = await fetch(url, {cache:'no-store'});
  if(!r.ok) throw new Error('HTTP '+r.status);
  return r.json();
}

async function jpost(url, body){
  const r = await fetch(url, {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify(body)});
  let data = null;
  try { data = await r.json(); } catch(e){}
  if(!r.ok){
    const msg = (data && data.error) ? data.error : ('HTTP '+r.status);
    throw new Error(msg);
  }
  return data || {};
}

async function refresh(){
  try {
    // Serialize requests: ENC28J60/UIPEthernet has very few concurrent sockets
    // and the API server accepts one client per loop() iteration, so parallel
    // fetches queue up at the server anyway. Doing them in sequence avoids
    // half-open sockets and lets each response paint before the next request.
    const info   = await jget('/api/info');
    const health = await jget('/api/health');
    const mqtt   = mqttCache || (mqttCache = await jget('/api/mqtt'));

    // Telemetry
    if(info.telemetry_valid){
      $('sPower').textContent = (info.power||'?') + ' W';
      $('sDaily').textContent = (info.daily_yield||'?') + ' kWh';
      $('sTotal').textContent = (info.total_yield||'?') + ' kWh';
      $('sOp').textContent = info.operating_status === '1' ? 'normal' : ('code '+info.operating_status);
      const age = Math.floor((Date.now() - performance.timing.navigationStart - (info.last_update_ms||0))/1000);
      $('sLast').textContent = info.last_update_ms ? (info.last_update_ms+' ms (bridge time)') : '-';
    } else {
      setUnknown($('sPower')); setUnknown($('sDaily')); setUnknown($('sTotal'));
      setUnknown($('sOp')); setUnknown($('sLast'));
    }

    // Connectivity
    $('sWifi').innerHTML = health.wifi_connected
      ? fmtPill('connected '+health.wifi_ip, 'ok')
      : fmtPill('disconnected', 'bad');
    $('sEth').innerHTML = fmtPill(health.ethernet_ip||'-', 'ok');
    $('ip').textContent = health.ethernet_ip||'';
    if(health.ha_mqtt_enabled){
      $('sMqtt').innerHTML = health.mqtt_connected
        ? fmtPill('connected '+(health.mqtt_broker||'?'), 'ok')
        : (health.mqtt_scanning ? fmtPill('scanning','warn') : fmtPill('disconnected','bad'));
    } else {
      $('sMqtt').innerHTML = fmtPill('disabled','muted');
    }

    // Cached settings
    if(info.power_limit_known){
      $('curPower').textContent = info.power_limit_watts + ' W';
      if(document.activeElement !== $('iPower')) $('iPower').value = info.power_limit_watts;
    } else { setUnknown($('curPower')); }

    if(typeof info.polling_interval_s === 'number'){
      $('curPoll').textContent = info.polling_interval_s + ' s';
      if(document.activeElement !== $('iPoll')) $('iPoll').value = info.polling_interval_s;
    } else { setUnknown($('curPoll')); }

    if(info.shadow_known){
      $('curShadow').textContent = info.shadow ? 'ON' : 'OFF';
      if(document.activeElement !== $('iShadow')) $('iShadow').checked = !!info.shadow;
    } else { setUnknown($('curShadow')); }

    // MQTT
    $('iHa').checked = !!mqtt.ha_enabled;
    if(document.activeElement !== $('iBroker')) $('iBroker').value = mqtt.broker || '';
    if(document.activeElement !== $('iUser')) $('iUser').value = mqtt.user || '';
    $('curMqtt').textContent = mqtt.has_credentials ? 'creds saved' : 'no creds';
    $('iDebug').checked = !!health.debug_mode;

    $('lastFetch').textContent = 'updated ' + new Date().toLocaleTimeString();
  } catch(e){
    $('lastFetch').textContent = 'error: ' + e.message;
  }
}

$('bPower').onclick = async () => {
  const v = parseInt($('iPower').value, 10);
  try { await jpost('/api/power', {power: v}); flash($('mPower'), true, 'Power set to '+v+' W'); refresh(); }
  catch(e){ flash($('mPower'), false, e.message); }
};

$('bPoll').onclick = async () => {
  const v = parseInt($('iPoll').value, 10);
  try { const r = await jpost('/api/polling', {seconds: v}); flash($('mPoll'), true, 'Polling set to '+r.poll_interval_seconds+' s'); refresh(); }
  catch(e){ flash($('mPoll'), false, e.message); }
};

$('bShadow').onclick = async () => {
  const v = $('iShadow').checked;
  try { await jpost('/api/shadow', {enabled: v}); flash($('mShadow'), true, 'Shadow '+(v?'enabled':'disabled')); refresh(); }
  catch(e){ flash($('mShadow'), false, e.message); }
};

$('bMqtt').onclick = async () => {
  const body = {
    ha_enabled: $('iHa').checked,
    broker: $('iBroker').value.trim(),
    user: $('iUser').value.trim(),
  };
  const pass = $('iPass').value;
  if(pass) body.password = pass;
  try { await jpost('/api/mqtt', body); $('iPass').value=''; mqttCache=null; flash($('mMqtt'), true, 'MQTT settings saved'); refresh(); }
  catch(e){ flash($('mMqtt'), false, e.message); }
};

$('bDebug').onclick = async () => {
  const v = $('iDebug').checked;
  try { await jpost('/api/debug', {debug: v}); flash($('mAction'), true, 'Debug '+(v?'on':'off')); refresh(); }
  catch(e){ flash($('mAction'), false, e.message); }
};

$('bPulse').onclick = async () => {
  try { const r = await fetch('/pulse'); const d = await r.json(); flash($('mAction'), d.reconnected, d.reconnected?'WiFi reconnected':'pulse sent, no connection'); refresh(); }
  catch(e){ flash($('mAction'), false, e.message); }
};

$('bWifiOff').onclick = async () => {
  if(!confirm('Send a button press to turn the inverter WiFi off?')) return;
  try { await jpost('/wifi/off', {}); flash($('mAction'), true, 'WiFi-off press sent'); }
  catch(e){ flash($('mAction'), false, e.message); }
};

refresh();
setInterval(refresh, 5000);
</script>
</body>
</html>
)HTML";
