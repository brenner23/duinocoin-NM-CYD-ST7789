#ifndef DASHBOARD_H
#define DASHBOARD_H

const char WEBSITE[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="de">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Duino-Coin @@DEVICE@@</title>
<style>
  *{box-sizing:border-box}
  body{
    margin:0;background:#0d0f0f;color:#f3f3f3;
    font-family:Arial,Helvetica,sans-serif;
  }
  .wrap{max-width:760px;margin:0 auto;padding:28px 18px 48px}
  h1{margin:0 0 6px;color:#00ff2a;font-size:30px}
  .sub{color:#8d9696;margin:0 0 22px;font-size:14px}
  .card{
    background:#181b1b;border-radius:10px;padding:18px;margin:0 0 14px;
    box-shadow:0 0 0 1px rgba(255,255,255,.02) inset;
  }
  .title{
    color:#00eaff;font-weight:700;letter-spacing:2px;font-size:13px;
    margin-bottom:14px;
  }
  .grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:10px}
  .stat{
    background:#101212;border:1px solid #2a2f2f;border-radius:7px;padding:12px;
  }
  .value{font-size:25px;font-weight:700;color:#fff}
  .label{font-size:11px;color:#7f8a8a;letter-spacing:1.2px;margin-top:4px}
  .range-head{display:flex;justify-content:space-between;align-items:center;margin-bottom:7px}
  .percent{color:#00ff2a;font-weight:700}
  input[type=range]{width:100%;accent-color:#00ff2a}
  input[type=number]{width:100%;background:#101212;color:#fff;border:1px solid #2a2f2f;border-radius:6px;padding:12px;font-size:18px}
  .wallet-balance{font-size:28px;font-weight:700;color:#00ff2a;margin-bottom:5px;overflow-wrap:anywhere}
  .wallet-usd{font-size:16px;color:#00eaff;margin:0 0 7px}
  .wallet-meta{font-size:12px;color:#8d9696;margin-bottom:14px}
  .btnrow{display:grid;grid-template-columns:1fr 1fr;gap:10px}
  .miner-summary{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:8px;margin:14px 0}
  .mini{background:#101212;border:1px solid #2a2f2f;border-radius:7px;padding:10px;text-align:center}
  .mini b{display:block;font-size:18px;color:#fff}.mini span{font-size:10px;color:#7f8a8a;letter-spacing:.8px}
  .tablewrap{overflow-x:auto;margin-top:12px}
  table{width:100%;border-collapse:collapse;font-size:12px;white-space:nowrap}
  th,td{padding:8px 7px;border-bottom:1px solid #2a2f2f;text-align:left}
  th{color:#8d9696;font-size:10px;letter-spacing:.7px}
  .refresh{background:#00eaff;color:#001417}
  .hint{font-size:11px;color:#6d7474;margin-top:8px}
  .btn{
    width:100%;border:0;border-radius:6px;padding:13px 14px;
    font-weight:700;font-size:15px;cursor:pointer;margin-top:10px;
  }
  .save{background:#00ff19;color:#001500}
  .restart{background:#232727;color:#ff6262;border:1px solid #4b2a2a}
  .status{min-height:18px;text-align:center;margin-top:10px;color:#00eaff;font-size:13px}
  .crash-entry{background:#101212;border:1px solid #4b2a2a;border-radius:7px;padding:12px;margin-top:10px}
  .crash-head{color:#ff6262;font-weight:700;letter-spacing:1px;margin-bottom:9px}
  .crash-grid{display:grid;grid-template-columns:130px 1fr;gap:5px 10px;font-size:12px}
  .crash-grid span{color:#7f8a8a}.crash-grid b{color:#f3f3f3;overflow-wrap:anywhere}
  .crash-empty{color:#8d9696;font-size:13px;padding:4px 0}
  .clearlog{background:#232727;color:#ffb762;border:1px solid #51402a}
  .footer{text-align:center;color:#596161;font-size:11px;margin-top:20px}
  @media(max-width:560px){.grid{grid-template-columns:1fr}.wrap{padding:18px 12px}}
</style>
</head>
<body>
<div class="wrap">
  <h1>⚙ Duino-Coin Miner</h1>
  <p class="sub">@@DEVICE@@ · @@ID@@ · <span id="ip">@@IP_ADDR@@</span></p>
  <div class="card" style="padding:12px 18px"><div style="display:flex;justify-content:space-between;gap:12px;align-items:center;flex-wrap:wrap"><div><span id="onlineLed" style="display:inline-block;width:10px;height:10px;border-radius:50%;background:#00ff2a;margin-right:7px"></span><b id="onlineText">ONLINE</b></div><div><span class="label">MINING TIME</span> <b id="miningTime">0Y 0M 0W 0D 00:00:00</b></div><div class="label">LAST UPDATE <span id="lastUpdate">--:--:--</span></div></div></div>

  <div class="card">
    <div class="title">MINING</div>
    <div class="grid">
      <div class="stat"><div class="value"><span id="hashrate">@@HASHRATE@@</span> <small>kH/s</small></div><div class="label">HASHRATE</div></div>
      <div class="stat"><div class="value" id="difficulty">@@DIFF@@</div><div class="label">DIFFICULTY</div></div>
      <div class="stat"><div class="value" id="shares">@@SHARES@@</div><div class="label">SHARES</div></div>
      <div class="stat"><div class="value" id="node">@@NODE@@</div><div class="label">NODE</div></div>
    </div>
  </div>

  <div class="card">
    <div class="title">GERÄT</div>
    <div class="grid">
      <div class="stat"><div class="value" id="freeHeap">@@MEMORY@@</div><div class="label">FREIER SPEICHER</div></div>
      <div class="stat"><div class="value">@@VERSION@@</div><div class="label">MINER VERSION</div></div>
      @@DEVICE_DIAG_FIELDS@@
    </div>
    @@TEMP_CONTROLS@@
  </div>

  <div class="card">
    <div class="title">WALLET</div>
    <div class="wallet-balance"><span id="walletBalance">@@WALLET_BALANCE@@</span> DUCO</div>
    <div class="wallet-usd">≈ $<span id="walletUsd">@@WALLET_USD@@</span> USD</div>
    <div class="wallet-meta">Letztes Update: <span id="walletLastUpdate">@@WALLET_LAST_UPDATE@@</span></div>
    <div class="miner-summary">
      <div class="mini"><b id="activeDevices">@@ACTIVE_DEVICES@@</b><span>GERÄTE</span></div>
      <div class="mini"><b id="activeThreads">@@ACTIVE_THREADS@@</b><span>THREADS</span></div>
      <div class="mini"><b id="totalHashrate">@@TOTAL_HASHRATE@@</b><span>GESAMT</span></div>
    </div>
    <div class="wallet-meta">Gesamt: <span id="totalAccepted">@@TOTAL_ACCEPTED@@</span> Accepted · <span id="totalRejected">@@TOTAL_REJECTED@@</span> Rejected</div>
    <div class="tablewrap"><table>
      <thead><tr><th>MINER</th><th>IP</th><th>HASHRATE</th><th>DIFF</th><th>A / R</th><th>SHARETIME</th><th>POOL</th></tr></thead>
      <tbody id="minerRows">@@MINER_ROWS@@</tbody>
    </table></div>
    <div class="label" style="margin-bottom:6px">AUTOMATISCHE AKTUALISIERUNG IN SEKUNDEN</div>
    <input id="walletInterval" type="number" min="60" max="86400" step="1" value="@@WALLET_INTERVAL@@">
    <div class="hint">Standard: 300 Sekunden = 5 Minuten. Erlaubt sind 60 bis 86400 Sekunden.</div>
    <div class="btnrow">
      <button class="btn save" onclick="saveWalletInterval()">SPEICHERN</button>
      <button class="btn refresh" onclick="refreshWallet()">JETZT AKTUALISIEREN</button>
    </div>
    <div id="walletStatus" class="status"></div>
  </div>

  <div class="card">
    <div class="title">HELLIGKEIT</div>
    <div class="range-head">
      <span>Display-Backlight</span>
      <span class="percent"><span id="brightnessValue">@@BRIGHTNESS@@</span>%</span>
    </div>
    <input id="brightness" type="range" min="0" max="100" step="1" value="@@BRIGHTNESS@@">
    <div class="hint">Die Helligkeit wird beim Schieben sofort live übernommen und ist bei LDR-Automatik die Mindesthelligkeit.</div>
    <label style="display:flex;align-items:center;gap:10px;margin:14px 0 8px">
      <input id="autoBrightness" type="checkbox" @@AUTO_BRIGHTNESS@@ style="width:20px;height:20px">
      <span>Auto brightness (LDR)</span>
    </label>
    <div class="hint">LDR GPIO34 · Rohwert beim Laden: @@LDR_RAW@@ · Mehr Umgebungslicht hebt das Backlight automatisch bis 100 % an.</div>
    <button class="btn save" onclick="saveBrightness()">SPEICHERN</button>
    <div id="status" class="status"></div>
  </div>

  <div class="card">
    <div class="title">DISPLAY-AUSRICHTUNG</div>
    <div class="label" style="margin-bottom:6px">ROTATION</div>
    <select id="rotation" style="width:100%;padding:12px;border-radius:10px;background:#101a22;color:#e9f2f7;border:1px solid #294552;font-size:16px">
      <option value="0">0°</option>
      <option value="90">90°</option>
      <option value="180">180°</option>
      <option value="270">270°</option>
    </select>
    <div class="hint">90° und 270° sind Querformat. 0° und 180° sind Hochformat. Die Rotation wird sofort angewendet und gespeichert.</div>
    <button class="btn save" onclick="saveRotation()">SPEICHERN &amp; ANWENDEN</button>
    <div id="rotationStatus" class="status"></div>
  </div>

@@TFT_DIAGNOSTICS_CARD@@


@@CRASH_CARD@@
@@COLLISION_CARD@@

  <div class="card">
    <div class="title">SYSTEM</div>
    <button class="btn restart" onclick="restartMiner()">ESP32 NEU STARTEN</button>
  </div>

  <div class="footer">Duino-Coin · lokales Miner-Dashboard</div>
</div>

<script>
const slider = document.getElementById('brightness');
const value = document.getElementById('brightnessValue');
const statusBox = document.getElementById('status');
const rotationBox = document.getElementById('rotation');
const autoBrightness = document.getElementById('autoBrightness');
rotationBox.value = '@@ROTATION@@';
let liveTimer = 0;
let brightnessRequestRunning = false;
let pendingBrightness = null;

function sendBrightness(v) {
  if (brightnessRequestRunning) {
    pendingBrightness = v;
    return;
  }

  brightnessRequestRunning = true;
  fetch('/brightness?value=' + v)
    .then(r => r.text())
    .then(() => { statusBox.textContent = 'Live: ' + v + '%'; })
    .catch(() => { statusBox.textContent = 'Verbindung fehlgeschlagen'; })
    .finally(() => {
      brightnessRequestRunning = false;
      if (pendingBrightness !== null) {
        const next = pendingBrightness;
        pendingBrightness = null;
        sendBrightness(next);
      }
    });
}

slider.addEventListener('input', () => {
  value.textContent = slider.value;
  clearTimeout(liveTimer);
  liveTimer = setTimeout(() => sendBrightness(slider.value), 300);
});

function saveBrightness(){
  fetch('/brightness/save?value=' + slider.value)
    .then(r => r.text())
    .then(t => { statusBox.textContent = t; })
    .catch(() => { statusBox.textContent = 'Speichern fehlgeschlagen'; });
}


autoBrightness.addEventListener('change', () => {
  fetch('/brightness/auto?enabled=' + (autoBrightness.checked ? '1' : '0'))
    .then(r => r.text().then(t => { if(!r.ok) throw new Error(t); return t; }))
    .then(t => { statusBox.textContent = t; })
    .catch(e => { statusBox.textContent = e.message || 'LDR-Einstellung fehlgeschlagen'; });
});

function saveRotation(){
  const status = document.getElementById('rotationStatus');
  const v = rotationBox.value;
  status.textContent = 'Speichere ' + v + '° ...';
  fetch('/rotation/save?value=' + v)
    .then(r => r.text().then(t => { if(!r.ok) throw new Error(t); return t; }))
    .then(t => { status.textContent = t; })
    .catch(e => { status.textContent = e.message || 'Speichern fehlgeschlagen'; });
}

function tftTest(){
  const status = document.getElementById('tftStatus');
  status.textContent = 'Sende Testmuster ...';
  fetch('/tft/test', {method:'POST'})
    .then(r => r.text().then(t => { if(!r.ok) throw new Error(t); return t; }))
    .then(t => { status.textContent = t; })
    .catch(e => { status.textContent = e.message || 'TFT-Test fehlgeschlagen'; });
}

function tftRecover(){
  const status = document.getElementById('tftStatus');
  status.textContent = 'Initialisiere TFT neu ...';
  fetch('/tft/recover', {method:'POST'})
    .then(r => r.text().then(t => { if(!r.ok) throw new Error(t); return t; }))
    .then(t => { status.textContent = t; })
    .catch(e => { status.textContent = e.message || 'TFT-Recovery fehlgeschlagen'; });
}

function clearTftLog(){
  const status = document.getElementById('tftStatus');
  fetch('/tft/log/clear',{method:'POST'})
    .then(r=>r.text()).then(t=>{status.textContent=t; refreshLive(true);})
    .catch(e=>status.textContent=e.message);
}

function saveWalletInterval(){
  const box = document.getElementById('walletInterval');
  const status = document.getElementById('walletStatus');
  const v = parseInt(box.value, 10);
  if (!Number.isFinite(v) || v < 60 || v > 86400) {
    status.textContent = 'Bitte 60 bis 86400 Sekunden eingeben';
    return;
  }
  fetch('/wallet/interval?value=' + v)
    .then(r => r.text().then(t => { if(!r.ok) throw new Error(t); return t; }))
    .then(t => { status.textContent = t; })
    .catch(e => { status.textContent = e.message || 'Speichern fehlgeschlagen'; });
}

function refreshWallet(){
  const status = document.getElementById('walletStatus');
  status.textContent = 'Wallet wird aktualisiert...';
  fetch('/wallet/refresh', {method:'POST'})
    .then(r => r.text().then(t => { if(!r.ok) throw new Error(t); return t; }))
    .then(() => {
      status.textContent = 'Wallet aktualisiert';
      refreshLive(true);
    })
    .catch(e => { status.textContent = e.message || 'Aktualisierung fehlgeschlagen'; });
}


function saveTempInterval(){
  const v=document.getElementById('tempInterval').value;
  const st=document.getElementById('tempStatus');
  fetch('/diag/tempinterval?value='+encodeURIComponent(v)).then(r=>r.text().then(t=>{if(!r.ok)throw new Error(t);return t;})).then(t=>st.textContent=t).catch(e=>st.textContent=e.message);
}


function restartMiner(){
  if(!confirm('ESP32 wirklich neu starten?')) return;
  statusBox.textContent = 'Neustart...';
  fetch('/restart', {method:'POST'}).catch(()=>{});
}

function clearCrashlog(){
  const box=document.getElementById('crashStatus');
  fetch('/crashlog/clear',{method:'POST'}).then(r=>r.text()).then(t=>{box.textContent=t; refreshLive(true);}).catch(e=>box.textContent=e.message);
}
function clearCollisionStats(){
  const box=document.getElementById('collisionStatus');
  fetch('/collision/clear',{method:'POST'}).then(r=>r.text()).then(t=>{box.textContent=t; refreshLive(true);}).catch(e=>box.textContent=e.message);
}

let miningBaseSeconds = 0;
let miningBaseAt = Date.now();
let failedLiveRequests = 0;
let lastCrashCount = -1;
let lastTftErrorCount = -1;

function formatMiningTime(totalSeconds){
  let n=Math.max(0,Math.floor(totalSeconds));
  const Y=Math.floor(n/31536000); n%=31536000;
  const M=Math.floor(n/2592000); n%=2592000;
  const W=Math.floor(n/604800); n%=604800;
  const D=Math.floor(n/86400); n%=86400;
  const H=Math.floor(n/3600); n%=3600;
  const m=Math.floor(n/60), sec=n%60;
  return `${Y}Y ${M}M ${W}W ${D}D ${String(H).padStart(2,'0')}:${String(m).padStart(2,'0')}:${String(sec).padStart(2,'0')}`;
}
function tickMiningTime(){
  const elapsed=Math.floor((Date.now()-miningBaseAt)/1000);
  document.getElementById('miningTime').textContent=formatMiningTime(miningBaseSeconds+elapsed);
}
function setOnline(ok){
  if(ok){ failedLiveRequests=0; document.getElementById('onlineLed').style.background='#00ff2a'; document.getElementById('onlineText').textContent='ONLINE'; }
  else if(++failedLiveRequests>=2){ document.getElementById('onlineLed').style.background='#ff3b3b'; document.getElementById('onlineText').textContent='OFFLINE'; }
}
function putText(id,v){const e=document.getElementById(id); if(e&&v!==undefined)e.textContent=v;}
function refreshDiagnostics(){
  fetch('/api/diag?t='+Date.now()).then(r=>{if(!r.ok)throw 0;return r.json()}).then(d=>{
    if(d.crashlog_html!==undefined){const e=document.getElementById('crashlog');if(e)e.innerHTML=d.crashlog_html;}
    if(d.collision_html!==undefined){const e=document.getElementById('collisionStats');if(e)e.innerHTML=d.collision_html;}
    if(d.tftlog_html!==undefined){const e=document.getElementById('tftLog');if(e)e.innerHTML=d.tftlog_html;}
  }).catch(()=>{});
}
function refreshLive(forceDiag=false){
  fetch('/api/status?t='+Date.now(),{cache:'no-store'}).then(r=>{if(!r.ok)throw 0;return r.json()}).then(d=>{
    setOnline(true);
    miningBaseSeconds=d.uptime_sec||0; miningBaseAt=Date.now(); tickMiningTime();
    putText('hashrate',d.hashrate); putText('difficulty',d.difficulty); putText('shares',d.shares); putText('node',d.node); putText('freeHeap',d.free_heap);
    if(d.cpu_temp!==undefined) putText('cpuTemp',Number(d.cpu_temp).toFixed(1)); if(d.cpu_temp_max!==undefined) putText('cpuTempMax',Number(d.cpu_temp_max).toFixed(1));
    putText('walletBalance',d.wallet_balance); putText('walletUsd',d.wallet_usd); putText('walletLastUpdate',d.wallet_last_update);
    putText('activeDevices',d.active_devices); putText('activeThreads',d.active_threads); putText('totalHashrate',d.total_hashrate); putText('totalAccepted',d.total_accepted); putText('totalRejected',d.total_rejected);
    if(d.miner_rows_html!==undefined){const e=document.getElementById('minerRows');if(e)e.innerHTML=d.miner_rows_html;}
    document.getElementById('lastUpdate').textContent=new Date().toLocaleTimeString();
    if(forceDiag || (d.crash_count!==undefined && d.crash_count!==lastCrashCount) || (d.tft_error_count!==undefined && d.tft_error_count!==lastTftErrorCount)) refreshDiagnostics();
    if(d.crash_count!==undefined)lastCrashCount=d.crash_count;
    if(d.tft_error_count!==undefined)lastTftErrorCount=d.tft_error_count;
  }).catch(()=>setOnline(false));
}
setInterval(tickMiningTime,1000);
setInterval(()=>refreshLive(false),60000);
refreshLive(false);

</script>
</body>
</html>
)=====";

#endif
