#include "web_server.h"
#include "wifi_manager.h"
#include "version.h"
#include "logger.h"
#include "gui.h"
#include "can.h"
#include "config_manager.h"
#include "alarm_manager.h"
#include "trip_manager.h"
#include "time_manager.h"
#include "vehicle_data.h"
#include "history.h"
#include "diagnostic_profile.h"
#include "discovery.h"
#include "display_settings.h"
#include "power_manager.h"

#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <SD_MMC.h>
#include <Preferences.h>

static WebServer server(80);

static const char *WEB_USER = "admin";
static Preferences webPrefs;
static bool webAuthEnabled = false;
static String webPassword;

static bool rebootPending = false;
static uint32_t rebootAt = 0;

static bool otaStarted = false;
static bool otaFinished = false;
static bool otaSuccess = false;
static String otaError;
static size_t otaBytesWritten = 0;

static const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="it">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>BMW 520xd Monitor</title>
<style>
:root{--bg:#050607;--panel:#121518;--line:#30363d;--txt:#f4f6f8;--mut:#9aa4ae;--yel:#ffc400;--blu:#1f8cff;--red:#ff5b64;--green:#4dd17a}
*{box-sizing:border-box} body{margin:0;background:var(--bg);color:var(--txt);font-family:Arial,sans-serif}
header{position:sticky;top:0;background:#070809;border-bottom:1px solid var(--line);padding:16px 20px;display:flex;justify-content:space-between;gap:15px;align-items:center;z-index:2}
.brand b{font-size:24px}.brand span{display:block;color:var(--yel);font-size:12px;font-weight:bold;margin-top:3px}
.status{font-size:13px;color:var(--mut);text-align:right}
main{max-width:1100px;margin:auto;padding:18px}
nav{display:flex;flex-wrap:wrap;gap:8px;margin-bottom:18px}
nav button,.btn{background:var(--panel);border:1px solid var(--line);color:var(--txt);padding:10px 14px;border-radius:8px;cursor:pointer}
nav button.active{border-color:var(--yel);color:var(--yel)}
.grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:12px}
.card{background:var(--panel);border:1px solid var(--line);border-radius:10px;padding:16px}
.card .name{font-size:14px;color:var(--mut);margin-bottom:8px}.card .value{font-size:28px;font-weight:bold}
.blue{color:var(--blu)}.yellow{color:var(--yel)} .green{color:var(--green)} .red{color:var(--red)}
section{display:none} section.active{display:block}
h2{font-size:18px;margin:0 0 12px;color:var(--yel)}
table{width:100%;border-collapse:collapse}.row{display:flex;justify-content:space-between;border-bottom:1px solid #222;padding:12px 0}
.controls{display:flex;flex-wrap:wrap;gap:10px;margin:12px 0}.btn.danger{border-color:var(--red);color:var(--red)}.btn.ok{border-color:var(--green);color:var(--green)}
input,select{background:#0b0d0f;color:var(--txt);border:1px solid var(--line);border-radius:6px;padding:10px}
progress{width:100%;height:18px}
small{color:var(--mut)}
@media(max-width:700px){.grid{grid-template-columns:1fr}.status{display:none}}
</style>
</head>
<body>
<header><div class="brand"><b>BMW 520xd</b><span>PERFORMANCE MONITOR</span></div><div class="status" id="topstatus">Caricamento...</div></header>
<main>
<nav>
<button data-tab="home" class="active">HOME</button>
<button data-tab="motore">MOTORE</button>
<button data-tab="dpf">DPF</button>
<button data-tab="zf8">ZF8</button>
<button data-tab="diag">DIAG</button>
<button data-tab="canraw">CAN RAW</button>
<button data-tab="discovery">DISCOVERY</button>
<button data-tab="logger">LOGGER</button>
<button data-tab="wifi">WI-FI</button>
<button data-tab="firmware">FIRMWARE</button>
<button data-tab="grafici">GRAFICI 24H</button>
<button data-tab="trip">TRIP</button>
<button data-tab="allarmi">ALLARMI</button>
<button data-tab="system">SISTEMA</button>
</nav>

<section id="home" class="active"><h2>HOME</h2><div class="grid">
<div class="card"><div class="name">ACQUA</div><div class="value blue" id="homeCoolant">-- °C</div></div>
<div class="card"><div class="name">ARIA ASPIRATA</div><div class="value blue" id="homeIntake">-- °C</div></div>
<div class="card"><div class="name">OLIO MOTORE</div><div class="value yellow" id="homeOil">-- °C</div></div>
<div class="card"><div class="name">DPF TRIGGER</div><div class="value yellow" id="homeDpf">-- %</div></div>
<div class="card"><div class="name">TURBO</div><div class="value" id="homeTurbo">-- bar</div></div>
<div class="card"><div class="name">CAMBIO</div><div class="value" id="homeGearbox">-- °C</div></div>
</div></section>

<section id="motore"><h2>MOTORE / DDE B47</h2><div class="card" id="engineRows"></div></section>
<section id="dpf"><h2>FILTRO ANTIPARTICOLATO</h2><div class="card" id="dpfRows"></div></section>
<section id="zf8"><h2>CAMBIO ZF8</h2><div class="card" id="gearRows"></div></section>

<section id="diag"><h2>DIAGNOSTICA</h2><div class="card">
<div class="row"><span>Display</span><b class="green">OK</b></div>
<div class="row"><span>Touch</span><b class="green">OK</b></div>
<div class="row"><span>Interfaccia CAN</span><b id="diagCan">da testare</b></div>
<div class="row"><span>DDE / motore</span><b id="diagDde">non interrogata</b></div>
<div class="row"><span>EGS / cambio</span><b id="diagEgs">profilo BMW da caricare</b></div>
</div></section>


<section id="canraw"><h2>OBD-II ATTIVO / CATALOGO CAN</h2>
<div class="card">
<div id="canStats"></div>
<div class="controls">
<button class="btn" onclick="refreshCan()">AGGIORNA</button>
<button class="btn danger" onclick="clearCanCatalog()">AZZERA STATISTICHE</button>
</div>
<small>ISO 15765-4 CAN 11 bit / 500 kbit/s. Discovery 0x7DF, polling DDE fisico 0x7E0, ISO-TP multiframe con Flow Control.</small>
</div>
<h2 style="margin-top:18px">ID RILEVATI</h2>
<div class="card" style="overflow:auto">
<table>
<thead><tr><th>ID</th><th>Count</th><th>FPS stim.</th><th>DLC</th><th>Ultimi byte</th></tr></thead>
<tbody id="canCatalog"></tbody>
</table>
</div>
</section>


<section id="discovery"><h2>BMW DISCOVERY / EVENT MARKERS</h2>
<div class="card">
<div id="discoveryStats"></div>
<div class="controls">
<button class="btn" onclick="refreshDiscovery()">AGGIORNA</button>
<button class="btn" id="scanBtn" onclick="scanEcus()">SCAN OBD READ-ONLY</button>
<button class="btn" id="bmwScanBtn" onclick="scanBmwExt()">BMW EXT 6F1 READ-ONLY</button>
<span id="scanMsg"><small>Pronto</small></span>
</div>
<small>OBD: TesterPresent 7E0..7E7. BMW EXT: source F1 / CAN 6F1 verso 12 DDE, 18 EGS, 5E GWS, 60 KOMBI. Solo UDS 3E 00: nessun coding, reset, routine o comando attuatore.</small>
</div>
<h2 style="margin-top:18px">MARCA EVENTI NEL LOG</h2>
<div class="card">
<div class="controls">
<button class="btn" onclick="markEvent('P')">P</button>
<button class="btn" onclick="markEvent('R')">R</button>
<button class="btn" onclick="markEvent('N')">N</button>
<button class="btn" onclick="markEvent('D')">D</button>
<button class="btn" onclick="markEvent('SHIFT_UP')">SHIFT +</button>
<button class="btn" onclick="markEvent('SHIFT_DOWN')">SHIFT -</button>
<button class="btn" onclick="markEvent('ACCEL')">ACCEL</button>
<button class="btn" onclick="markEvent('COAST')">RILASCIO</button>
<button class="btn" onclick="markEvent('BRAKE')">FRENO</button>
<button class="btn" onclick="markEvent('STOP')">STOP</button>
</div>
<div class="controls"><input id="customMarker" placeholder="Evento personalizzato"><button class="btn" onclick="markCustom()">MARCA</button></div>
<small>I marker finiscono nel file *_events.csv insieme a RPM, velocità e ai valori BMW disponibili in quel momento.</small>
</div>
<h2 style="margin-top:18px">ID / BYTE CHE CAMBIANO</h2>
<div class="card" style="overflow:auto">
<table><thead><tr><th>ID</th><th>Count</th><th>Periodo ms</th><th>Ultimi byte</th><th>Cambi byte 0..7</th></tr></thead><tbody id="discoveryTable"></tbody></table>
</div></section>

<section id="logger"><h2>DATA LOGGER CAN</h2>
<div class="card">
<div id="logState" class="row"></div>
<div class="controls"><button class="btn ok" onclick="logStart()">AVVIA</button><button class="btn" onclick="logStop()">FERMA</button><button class="btn danger" onclick="delAllLogs()">ELIMINA TUTTO</button></div>
<small>Registra RAW TX/RX, catalogo ID e un CSV decoded a 2 Hz alimentato dallo stesso modello dati di display e web.</small>
</div>
<h2 style="margin-top:18px">FILE</h2><div class="card" id="logFiles">Caricamento...</div></section>

<section id="wifi"><h2>WI-FI</h2><div class="card">
<div id="wifiState"></div>
<div class="controls"><button class="btn" onclick="scanWifi()">SCANSIONA</button><button class="btn danger" onclick="forgetWifi()">DIMENTICA RETE</button></div>
<div id="wifiList"></div>
</div></section>

<section id="firmware"><h2>AGGIORNAMENTO OTA</h2><div class="card">
<p>Apri la pagina OTA dedicata. Durante il trasferimento non vengono eseguiti aggiornamenti periodici della dashboard.</p>
<p><a class="btn ok" href="/ota">APRI AGGIORNAMENTO OTA</a></p>
<small>Usa esclusivamente il file BIN prodotto dalla build BMW 520xd Monitor.</small>
</div></section>


<section id="grafici"><h2>GRAFICI - ULTIME 24 ORE</h2>
<div class="card">
<div class="controls">
<select id="chartMetric">
<option value="coolant">Temperatura acqua</option>
<option value="oil">Temperatura olio</option>
<option value="intake">Aria aspirata</option>
<option value="turbo">Pressione turbo</option>
<option value="dpf">DPF trigger rigenerazione</option>
<option value="gearbox">Temperatura cambio</option>
<option value="rail">Pressione rail</option>
<option value="dpfDiff">Pressione differenziale DPF</option>
<option value="egt1">EGT sensore 1</option>
<option value="egt2">EGT sensore 2</option>
<option value="rpm">RPM</option>
<option value="speed">Velocita</option>
</select>
<button class="btn" id="refreshChart">AGGIORNA</button>
</div>
<canvas id="historyCanvas" width="1000" height="360" style="width:100%;height:auto;background:#090b0d;border:1px solid #30363d;border-radius:8px"></canvas>
<small>Buffer RAM circolare: massimo 1440 campioni, uno al minuto. I punti piu vecchi vengono sovrascritti automaticamente.</small>
</div></section>

<section id="trip"><h2>TRIP / PICCHI</h2><div class="card" id="tripBox"></div><div class="controls"><button class="btn" id="resetPeaks">RESET PICCHI</button></div></section>
<section id="allarmi"><h2>ALLARMI</h2><div class="card">
<div class="row"><span>Acqua °C</span><input id="limCool" type="number" value="110"></div>
<div class="row"><span>Olio °C</span><input id="limOil" type="number" value="130"></div>
<div class="row"><span>Cambio °C</span><input id="limGear" type="number" value="115"></div>
<div class="row"><span>EGT/DPF °C</span><input id="limDpf" type="number" value="750"></div>
<div class="controls"><button class="btn ok" id="saveAlarms">SALVA SOGLIE</button></div>
</div></section>
<section id="system"><h2>SISTEMA</h2><div class="card" id="sys"></div>
<h2 style="margin-top:18px">ACCESSO WEB</h2><div class="card">
<div id="securityState" class="row"></div>
<div class="row"><span>Nuova password</span><input id="webPassword" type="password" minlength="8" maxlength="63" autocomplete="new-password" placeholder="minimo 8 caratteri"></div>
<div class="controls"><button class="btn ok" id="enableAuth">ABILITA / CAMBIA PASSWORD</button><button class="btn danger" id="disableAuth">DISATTIVA PASSWORD</button></div>
<small>Utente: admin. Senza password chiunque sia collegato alla rete del dispositivo può controllarlo.</small>
</div>
<div class="controls">
<button class="btn" onclick="reboot()">RIAVVIA</button>
<button class="btn danger" onclick="factoryReset()">FACTORY RESET</button>
<a class="btn" href="/api/config/export">ESPORTA CONFIG</a>
<input id="cfgFile" type="file" accept=".json">
<button class="btn" id="cfgImport">IMPORTA CONFIG</button>
</div></section>
</main>
<script>
const row=(n,v)=>`<div class="row"><span>${n}</span><b>${v}</b></div>`;
let lastStatusJson=null;
let statusBusy=false, canBusy=false, discoveryBusy=false, scanUiActive=false;
engineRows.innerHTML=row('Regime motore','-- rpm')+row('Temperatura acqua','-- °C')+row('Temperatura olio','-- °C')+row('Aria aspirata','-- °C')+row('Pressione turbo','-- bar')+row('Boost assoluto','-- kPa')+row('Pressione rail','-- bar')+row('Tensione ECU','-- V')+row('EGR reale','-- %');
dpfRows.innerHTML=row('Rigenerazione','--')+row('Temp. ingresso','-- °C')+row('Temp. uscita','-- °C')+row('Press. differenziale','--')+row('Massa fuliggine','--')+row('Massa cenere','--');
gearRows.innerHTML=row('Marcia','--')+row('Olio cambio','-- °C')+row('Slittamento','-- rpm')+row('Lock-up','--');

document.querySelectorAll('nav button').forEach(b=>b.onclick=()=>{document.querySelectorAll('nav button').forEach(x=>x.classList.remove('active'));document.querySelectorAll('section').forEach(x=>x.classList.remove('active'));b.classList.add('active');document.getElementById(b.dataset.tab).classList.add('active')});

async function status(){
if(statusBusy)return; statusBusy=true;
try{
let r=await fetch('/api/status');let j=await r.json(); lastStatusJson=j;
topstatus.textContent=`${j.wifi_connected?'Wi-Fi '+j.ssid:'AP '+j.ap_ssid} | ${j.ip} | FW ${j.firmware}`;
wifiState.innerHTML=row('Stato',j.wifi_connected?'CONNESSO':'NON CONNESSO')+row('SSID',j.ssid||'-')+row('IP LAN',j.ip)+row('AP',j.ap_ssid)+row('IP AP',j.ap_ip)+row('RSSI',j.rssi?j.rssi+' dBm':'-');
 logState.innerHTML=row('Registrazione',j.logger?'ATTIVA':'FERMA')+row('File corrente',j.log_file||'-')+row('Dimensione',j.log_size+' byte')+row('Frame scritti',j.log_frames)+row('Frame persi logger',j.log_dropped)+row('Stalli SD >250 ms',j.log_stalls)+row('Write massimo',j.log_max_write_ms+' ms')+row('Flush massimo',j.log_max_flush_ms+' ms');
sys.innerHTML=row('Firmware',j.firmware)+row('Uptime',j.uptime+' s')+row('Heap libero',j.free_heap+' byte')+row('PSRAM libera',j.free_psram+' byte')+row('Pagina display',j.page)+row('Tema display',j.display_theme_status)+row('Luminosità',j.display_theme_night?j.display_night_brightness+'%':j.display_day_brightness+'%')+row('CAN',j.can_online?'ONLINE':'OFFLINE')+row('CAN FPS',j.can_fps)+row('CAN frame totali',j.can_total)+row('DDE',j.dde_detected?'7E8 OK':'non rilevata')+row('Profilo BMW',j.bmw_profile);
const fmt=(v,unit,dec)=>v==null?'--':Number(v).toFixed(dec===undefined?0:dec)+(unit?' '+unit:'');
const yesno=(known,v)=>!known?'--':(v?'SI':'NO');
homeCoolant.textContent=fmt(j.coolant,'°C');
homeIntake.textContent=fmt(j.intake,'°C');
homeOil.textContent=fmt(j.oil,'°C');
homeDpf.textContent=fmt(j.dpf_trigger,'% ',1).trim();
homeTurbo.textContent=fmt(j.turbo,'bar',2);
homeGearbox.textContent=fmt(j.gearbox,'°C');
engineRows.innerHTML=
 row('Regime motore',fmt(j.rpm,'rpm'))+
 row('Temperatura acqua',fmt(j.coolant,'°C'))+
 row('Temperatura olio',fmt(j.oil,'°C'))+
 row('Aria aspirata',fmt(j.intake,'°C'))+
 row('Pressione turbo',fmt(j.turbo,'bar',2))+
 row('Boost assoluto',fmt(j.boost_abs,'kPa',1))+
 row('Boost target',fmt(j.boost_target,'kPa',1))+
 row('Pressione rail',fmt(j.rail,'bar',1))+
 row('Rail target',fmt(j.rail_target,'bar',1))+
 row('Tensione ECU',fmt(j.voltage,'V',2))+
 row('MAF',fmt(j.maf,'g/s',1))+
 row('Carico motore',fmt(j.engine_load,'%',1))+
 row('Acceleratore',fmt(j.accelerator,'%',1))+
 row('EGR comandata',fmt(j.egr_commanded,'%',1))+
 row('EGR reale',fmt(j.egr_actual,'%',1));
dpfRows.innerHTML=
 row('Rigenerazione',yesno(j.dpf_regen_known,j.dpf_regen))+
 row('Tipo rigenerazione',!j.dpf_regen_type_known?'--':(j.dpf_regen_active_type?'ATTIVA':'PASSIVA'))+
 row('EGT sensore 1',fmt(j.egt1,'°C',1))+
 row('EGT sensore 2',fmt(j.egt2,'°C',1))+
 row('EGT sensore 3',fmt(j.egt3,'°C',1))+
 row('Press. differenziale',fmt(j.dpf_diff,'hPa',1))+
 row('Press. ingresso',fmt(j.dpf_inlet,'kPa',2))+
 row('Press. uscita',fmt(j.dpf_outlet,'kPa',2))+
 row('Trigger rigenerazione',fmt(j.dpf_trigger,'%',1))+
 row('Media tempo rig.',fmt(j.dpf_avg_time,'min',0))+
 row('Media distanza rig.',fmt(j.dpf_avg_distance,'km',0))+
 row('Massa fuliggine',fmt(j.dpf_soot,'g',1))+
 row('Massa cenere',fmt(j.dpf_ash,'g',1))+
 row('NOx sensore 1',fmt(j.nox1,'ppm',0))+
 row('NOx sensore 2',fmt(j.nox2,'ppm',0))+
 row('Lambda 1',fmt(j.lambda1,'',3))+
 row('Lambda 2',fmt(j.lambda2,'',3));
gearRows.innerHTML=
 row('Marcia',j.gear<0?'--':j.gear)+
 row('Olio cambio',fmt(j.gearbox,'°C'))+
 row('Input RPM',fmt(j.gear_input_rpm,'rpm'))+
 row('Output RPM',fmt(j.gear_output_rpm,'rpm'))+
 row('Slittamento',fmt(j.converter_slip,'rpm'))+
 row('Lock-up',!j.lockup_known?'--':(j.lockup?'ON':'OFF'))+
 row('Stato mapping','BMW EGS / PRG ancora da validare');
diagCan.textContent=j.can_driver?'attiva':'errore driver';
diagDde.textContent=j.dde_detected?'DDE 7E8 risponde':'nessuna risposta';
diagEgs.textContent=j.egs_detected?'risponde':'mapping BMW da caricare';
}catch(e){console.debug('status',e)}finally{statusBusy=false}
}
async function scanWifi(){wifiList.textContent='Scansione...';let r=await fetch('/api/wifi/scan');let a=await r.json();wifiList.innerHTML=a.map((n,i)=>`<div class="row"><span>${n.ssid} <small>${n.rssi} dBm ${n.secure?'🔒':''}</small></span><span><input id="p${i}" type="password" placeholder="password"><button class="btn" onclick="connectWifi(${i},'${encodeURIComponent(n.ssid)}')">CONNETTI</button></span></div>`).join('')||'Nessuna rete trovata';}
async function connectWifi(i,s){let p=document.getElementById('p'+i).value;await fetch('/api/wifi/connect',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'ssid='+s+'&password='+encodeURIComponent(p)});setTimeout(status,1500)}
async function forgetWifi(){await fetch('/api/wifi/forget',{method:'POST'});setTimeout(status,800)}
async function logStart(){await fetch('/api/logger/start',{method:'POST'});await status();await logs()}
async function logStop(){await fetch('/api/logger/stop',{method:'POST'});await status();await logs()}
async function logs(){let r=await fetch('/api/logs');let a=await r.json();logFiles.innerHTML=a.map(f=>`<div class="row"><span>${f.name} <small>${f.size} byte</small></span><span><a class="btn" href="/api/log/download?name=${encodeURIComponent(f.name)}">SCARICA</a><button class="btn danger" onclick="delLog('${f.name.replace(/'/g,"\\'")}')">ELIMINA</button></span></div>`).join('')||'Nessun log';}
async function delLog(name){
 if(!confirm('Eliminare '+name+'?'))return;
 const r=await fetch('/api/log/delete?name='+encodeURIComponent(name),{method:'POST'});
 const t=await r.text();
 if(!r.ok) alert(t);
 await logs();
}
async function delAllLogs(){
 if(!confirm('Eliminare tutte le acquisizioni dalla microSD?'))return;
 const r=await fetch('/api/log/delete-all',{method:'POST'});
 const t=await r.text();
 if(!r.ok) alert(t);
 await status();
 await logs();
}
async function reboot(){if(confirm('Riavviare la board?')) await fetch('/api/reboot',{method:'POST'})}
async function factoryReset(){if(confirm('Cancellare configurazione Wi-Fi e riavviare?')) await fetch('/api/factory-reset',{method:'POST'})}



async function refreshDiscovery(){
 if(discoveryBusy||scanUiActive)return; discoveryBusy=true;
 try{
 const r=await fetch('/api/discovery'); const j=await r.json();
 discoveryStats.innerHTML=
   row('ID osservati',j.ids)+row('ID passivi',j.passive_ids)+row('Frame passivi',j.passive_frames)+
   row('OBD scan',j.scan_active?'IN CORSO':j.scan_result)+row('OBD mask',j.scan_mask)+row('BMW EXT scan',j.bmw_scan_active?'IN CORSO':j.bmw_scan_result)+row('BMW EXT mask',j.bmw_scan_mask);
 discoveryTable.innerHTML=j.entries.map(e=>
   `<tr><td>${e.id}</td><td>${e.count}</td><td>${e.avg_ms.toFixed(2)}</td><td><code>${e.last}</code></td><td>${e.changes.join(' / ')}</td></tr>`
 ).join('')||'<tr><td colspan="5">Nessun frame</td></tr>';
 }catch(e){console.debug('discovery',e)}finally{discoveryBusy=false}
}
async function scanEcus(){
 const b=document.getElementById('scanBtn'),m=document.getElementById('scanMsg');
 if(scanUiActive)return;
 scanUiActive=true;
 if(b)b.disabled=true; if(m)m.innerHTML='<small>Scan in corso... polling pesante sospeso</small>';
 try{
   const r=await fetch('/api/diag/scan',{method:'POST'});
   const t=await r.text();
   if(!r.ok) throw new Error(t);
   if(m)m.innerHTML='<small>'+t+'</small>';
 }catch(e){
   if(m)m.innerHTML='<small>'+String(e.message||'Errore rete durante avvio scan')+'</small>';
   scanUiActive=false;
   if(b)b.disabled=false;
   return;
 }
 // The ECU scan itself is a cooperative ~3 s state machine. During that short
 // window suppress catalog/discovery downloads; /api/status remains alive.
 setTimeout(async()=>{
   scanUiActive=false;
   if(b)b.disabled=false;
   await status();
   await refreshDiscovery();
   await refreshCan();
 },3600);
}
async function scanBmwExt(){
 const b=document.getElementById('bmwScanBtn'),m=document.getElementById('scanMsg');
 if(scanUiActive)return;
 scanUiActive=true;
 if(b)b.disabled=true; if(m)m.innerHTML='<small>BMW Extended scan 6F1 in corso...</small>';
 try{
   const r=await fetch('/api/diag/bmw-ext-scan',{method:'POST'});
   const t=await r.text();
   if(!r.ok) throw new Error(t);
   if(m)m.innerHTML='<small>'+t+'</small>';
 }catch(e){
   if(m)m.innerHTML='<small>'+String(e.message||'Errore BMW scan')+'</small>';
   scanUiActive=false; if(b)b.disabled=false; return;
 }
 setTimeout(async()=>{
   scanUiActive=false; if(b)b.disabled=false;
   await status(); await refreshDiscovery(); await refreshCan();
 },2600);
}
async function markEvent(label){
 const r=await fetch('/api/logger/marker?label='+encodeURIComponent(label),{method:'POST'});
 if(!r.ok) alert(await r.text());
}
async function markCustom(){const x=customMarker.value.trim();if(x){await markEvent(x);customMarker.value='';}}
setInterval(refreshDiscovery,2500);

async function refreshCan(){
 if(canBusy||scanUiActive)return; canBusy=true;
 try{
 let j=lastStatusJson;
 if(!j){let s=await fetch('/api/status');j=await s.json();lastStatusJson=j;}
 canStats.innerHTML=
   row('Driver TWAI',j.can_driver?'READY':'ERRORE')+
   row('Bus',j.can_online?'ONLINE':'OFFLINE')+
   row('Frame/s',j.can_fps)+
   row('Frame totali',j.can_total)+
   row('ID unici',j.can_ids)+
   row('RX missed',j.can_missed)+
   row('Bus errors',j.can_errors)+row('Protocollo','ISO15765-4 11bit 500k')+row('Richieste TX',j.can_tx_requests)+row('Richieste/s',j.can_request_rate)+row('Risposte/s',j.can_reply_rate)+row('TX fallite',j.can_tx_failed)+row('Risposte OBD',j.can_obd_replies)+row('Stato TWAI',j.can_state)+row('Bus-off',j.can_bus_off_count)+row('Stato OBD',j.obd_status);

 let r=await fetch('/api/can/catalog');let a=await r.json();
 a.sort((x,y)=>y.count-x.count);
 canCatalog.innerHTML=a.map(x=>
   `<tr><td>${x.id}${x.extended?' EXT':''}</td><td>${x.count}</td><td>-</td><td>${x.dlc}</td><td><code>${x.data}</code></td></tr>`
 ).join('')||'<tr><td colspan="5">Nessun frame ricevuto</td></tr>';
 }catch(e){console.debug('can',e)}finally{canBusy=false}
}
async function clearCanCatalog(){
 await fetch('/api/can/clear',{method:'POST'});
 await refreshCan();
}
setInterval(refreshCan,2500);

const metricNames={
coolant:'Temperatura acqua',
oil:'Temperatura olio',
intake:'Aria aspirata',
turbo:'Pressione turbo',
dpf:'DPF trigger rigenerazione',
gearbox:'Temperatura cambio',
rail:'Pressione rail',
dpfDiff:'Pressione differenziale DPF',
egt1:'EGT sensore 1',
egt2:'EGT sensore 2',
rpm:'RPM',
speed:'Velocita'
};

async function drawHistory(){
 const r=await fetch('/api/history');
 const a=await r.json();
 const metric=chartMetric.value;
 const c=historyCanvas,ctx=c.getContext('2d');
 const W=c.width,H=c.height;
 ctx.clearRect(0,0,W,H);
 ctx.fillStyle='#090b0d';ctx.fillRect(0,0,W,H);
 ctx.strokeStyle='#30363d';ctx.lineWidth=1;
 for(let i=0;i<=6;i++){let y=30+i*(H-60)/6;ctx.beginPath();ctx.moveTo(55,y);ctx.lineTo(W-20,y);ctx.stroke()}
 for(let i=0;i<=8;i++){let x=55+i*(W-75)/8;ctx.beginPath();ctx.moveTo(x,30);ctx.lineTo(x,H-30);ctx.stroke()}

 const vals=a.map(p=>p[metric]).filter(v=>v!==null&&Number.isFinite(v));
 ctx.fillStyle='#f4f6f8';ctx.font='18px Arial';ctx.fillText(metricNames[metric]||metric,55,22);
 if(!vals.length){ctx.fillStyle='#9aa4ae';ctx.fillText('Nessun dato disponibile',W/2-90,H/2);return}

 let min=Math.min(...vals),max=Math.max(...vals);
 if(min===max){min-=1;max+=1}
 const pad=(max-min)*0.12;min-=pad;max+=pad;

 ctx.fillStyle='#9aa4ae';ctx.font='14px Arial';
 ctx.fillText(max.toFixed(1),8,36);ctx.fillText(min.toFixed(1),8,H-28);
 ctx.fillText('-24h',55,H-7);ctx.fillText('ora',W-45,H-7);

 ctx.strokeStyle='#ffc400';ctx.lineWidth=3;ctx.beginPath();
 let started=false;
 a.forEach((p,i)=>{
   const v=p[metric];
   if(v===null||!Number.isFinite(v)){started=false;return}
   const x=55+(i/Math.max(1,a.length-1))*(W-75);
   const y=30+(1-(v-min)/(max-min))*(H-60);
   if(!started){ctx.moveTo(x,y);started=true}else ctx.lineTo(x,y)
 });
 ctx.stroke();
}
refreshChart.addEventListener('click',drawHistory);
chartMetric.addEventListener('change',drawHistory);

async function loadAlarms(){let r=await fetch('/api/alarms');let j=await r.json();limCool.value=j.coolant;limOil.value=j.oil;limGear.value=j.gearbox;limDpf.value=j.dpf;}
saveAlarms.addEventListener('click',async()=>{let body='coolant='+encodeURIComponent(limCool.value)+'&oil='+encodeURIComponent(limOil.value)+'&gearbox='+encodeURIComponent(limGear.value)+'&dpf='+encodeURIComponent(limDpf.value);await fetch('/api/alarms',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});});
resetPeaks.addEventListener('click',async()=>{await fetch('/api/trip/reset-peaks',{method:'POST'});});
cfgImport.addEventListener('click',async()=>{const f=cfgFile.files[0];if(!f)return;const t=await f.text();let r=await fetch('/api/config/import',{method:'POST',headers:{'Content-Type':'application/json'},body:t});alert(await r.text());});
async function loadSecurity(){try{let r=await fetch('/api/security');let j=await r.json();securityState.innerHTML=row('Protezione',j.enabled?'ATTIVA':'DISATTIVATA');}catch(e){securityState.textContent='Stato non disponibile';}}
enableAuth.addEventListener('click',async()=>{const p=webPassword.value;if(p.length<8){alert('La password deve contenere almeno 8 caratteri');return}let b='enabled=1&password='+encodeURIComponent(p);let r=await fetch('/api/security',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:b});alert(await r.text());webPassword.value='';loadSecurity();});
disableAuth.addEventListener('click',async()=>{if(!confirm('Disattivare la protezione web?'))return;let r=await fetch('/api/security',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'enabled=0'});alert(await r.text());loadSecurity();});
async function tripRefresh(){let r=await fetch('/api/status');let j=await r.json();tripBox.innerHTML=row('Stato',j.trip_active?'ATTIVO':'FERMO')+row('Durata',j.trip_duration+' s')+row('Picco acqua',j.peak_coolant==null?'--':j.peak_coolant+' °C')+row('Picco olio',j.peak_oil==null?'--':j.peak_oil+' °C')+row('Picco turbo',j.peak_turbo==null?'--':j.peak_turbo+' bar');}
loadAlarms();loadSecurity();tripRefresh();setInterval(tripRefresh,1500);
status();logs();setInterval(status,750);
setInterval(drawHistory,10000);
</script>
</body></html>
)HTML";

static bool auth()
{
    if (!webAuthEnabled) return true;
    if (webPassword.length() >= 8 && server.authenticate(WEB_USER, webPassword.c_str())) return true;
    server.requestAuthentication();
    return false;
}

static const char OTA_HTML[] PROGMEM = R"HTML(
<!doctype html><html lang="it"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>BMW 520xd OTA</title><style>body{margin:0;background:#050607;color:#f4f6f8;font-family:Arial,sans-serif}main{max-width:560px;margin:auto;padding:24px}.card{background:#121518;border:1px solid #30363d;border-radius:10px;padding:20px}h1{color:#ffc400;font-size:22px}input,button,a{display:block;margin:16px 0;padding:12px;font-size:16px}button,a{background:#121518;color:#4dd17a;border:1px solid #4dd17a;border-radius:8px;text-decoration:none}small{color:#9aa4ae}</style></head>
<body><main><h1>BMW 520xd — OTA RECOVERY</h1><div class="card"><p>Seleziona il firmware BIN e premi AGGIORNA. Non chiudere Chrome e non cambiare rete durante il trasferimento.</p>
<form method="POST" action="/update" enctype="multipart/form-data"><input name="firmware" type="file" accept=".bin,application/octet-stream" required><button type="submit">AGGIORNA FIRMWARE</button></form>
<small>La pagina resterà in attesa fino alla verifica completa. Il monitor si riavvierà automaticamente solo dopo un aggiornamento valido.</small></div><a href="/">Torna alla dashboard</a></main></body></html>
)HTML";

static String jsonEscape(const String &s)
{
    String out;
    out.reserve(s.length() + 8);
    for (size_t i = 0; i < s.length(); ++i) {
        char c = s[i];
        if (c == '"' || c == '\\') out += '\\';
        out += c;
    }
    return out;
}

static String jsonFloat(float v);

static void sendStatus()
{
    String j = "{";
    j += "\"firmware\":\"" + String(FW_VERSION) + "\",";
    j += "\"wifi_connected\":" + String(wifi_connected() ? "true" : "false") + ",";
    j += "\"ssid\":\"" + jsonEscape(wifi_ssid()) + "\",";
    j += "\"ip\":\"" + jsonEscape(wifi_ip()) + "\",";
    j += "\"rssi\":" + String(wifi_rssi()) + ",";
    j += "\"ap_ssid\":\"" + jsonEscape(wifi_ap_ssid()) + "\",";
    j += "\"ap_ip\":\"" + jsonEscape(wifi_ap_ip()) + "\",";
    j += "\"logger\":" + String(logger_active() ? "true" : "false") + ",";
    j += "\"log_file\":\"" + jsonEscape(logger_current_file()) + "\",";
    j += "\"log_size\":" + String((unsigned long)logger_current_size()) + ",";
    j += "\"uptime\":" + String((unsigned long)(millis()/1000)) + ",";
    j += "\"free_heap\":" + String((unsigned long)ESP.getFreeHeap()) + ",";
    j += "\"free_psram\":" + String((unsigned long)ESP.getFreePsram()) + ",";
    j += "\"storage_ready\":" + String(logger_storage_ready() ? "true" : "false") + ",";
    j += "\"storage_name\":\"" + logger_storage_name() + "\",";
    j += "\"storage_total\":" + String((unsigned long)(logger_storage_total()/1024/1024)) + ",";
    j += "\"storage_used\":" + String((unsigned long)(logger_storage_used()/1024/1024)) + ",";
    j += "\"page\":\"" + String(gui_current_page_name()) + "\",";
    j += "\"display_day_brightness\":" + String(display_day_brightness()) + ",";
    j += "\"display_night_brightness\":" + String(display_night_brightness()) + ",";
    j += "\"display_theme_mode\":" + String((int)display_theme_mode()) + ",";
    j += "\"display_theme_night\":" + String(display_theme_is_night() ? "true" : "false") + ",";
    j += "\"display_theme_status\":\"" + jsonEscape(display_theme_status()) + "\",";
    j += "\"display_theme_source\":\"" + jsonEscape(display_theme_source()) + "\",";
    j += "\"power_state\":\"" + jsonEscape(power_manager_state()) + "\",";
    j += "\"ap_clients\":" + String(wifi_ap_client_count());
    j += ",\"can_online\":" + String(can_is_online() ? "true" : "false");
    j += ",\"can_driver\":" + String(can_driver_ready() ? "true" : "false");
    j += ",\"can_total\":" + String((unsigned long)can_total_frames());
    j += ",\"can_fps\":" + String((unsigned long)can_frames_per_second());
    j += ",\"can_missed\":" + String((unsigned long)can_rx_missed());
    j += ",\"can_errors\":" + String((unsigned long)can_bus_errors());
    j += ",\"can_tx_requests\":" + String((unsigned long)can_tx_requests());
    j += ",\"can_tx_failed\":" + String((unsigned long)can_tx_failed());
    j += ",\"can_bus_off_count\":" + String((unsigned long)can_bus_off_count());
    j += ",\"can_request_rate\":" + String((unsigned long)can_request_rate());
    j += ",\"can_reply_rate\":" + String((unsigned long)can_reply_rate());
    j += ",\"can_state\":\"" + jsonEscape(can_state_text()) + "\"";
    j += ",\"can_obd_replies\":" + String((unsigned long)can_obd_replies());
    j += ",\"obd_active\":" + String(obd_is_active() ? "true" : "false");
    j += ",\"obd_status\":\"" + jsonEscape(obd_last_status()) + "\"";
    j += ",\"can_ids\":" + String(can_catalog_count());
    j += ",\"discovery_ids\":" + String(discovery_count());
    j += ",\"discovery_passive_ids\":" + String((unsigned long)discovery_passive_ids());
    j += ",\"discovery_passive_frames\":" + String((unsigned long)discovery_passive_frames());
    j += ",\"diag_scan_active\":" + String(can_readonly_scan_active() ? "true" : "false");
    j += ",\"diag_scan_mask\":\"0x" + String(can_readonly_scan_response_mask(), HEX) + "\"";
    j += ",\"bmw_ext_scan_active\":" + String(can_bmw_extended_scan_active() ? "true" : "false");
    j += ",\"bmw_ext_scan_mask\":\"0x" + String(can_bmw_extended_scan_response_mask(), HEX) + "\"";
    VehicleData &vd = vehicle_data();
    j += ",\"coolant\":" + jsonFloat(vd.coolant);
    j += ",\"intake\":" + jsonFloat(vd.intake);
    j += ",\"rpm\":" + jsonFloat(vd.rpm);
    j += ",\"speed\":" + jsonFloat(vd.speed);
    j += ",\"maf\":" + jsonFloat(vd.maf);
    j += ",\"engine_load\":" + jsonFloat(vd.engineLoad);
    j += ",\"baro\":" + jsonFloat(vd.baro);
    j += ",\"ambient\":" + jsonFloat(vd.ambient);
    j += ",\"oil\":" + jsonFloat(vd.oil);
    j += ",\"turbo\":" + jsonFloat(vd.turbo);
    j += ",\"dpf_trigger\":" + jsonFloat(vd.dpfNormalizedTrigger);
    j += ",\"gearbox\":" + jsonFloat(vd.gearbox);
    j += ",\"voltage\":" + jsonFloat(vd.voltage);
    j += ",\"throttle\":" + jsonFloat(vd.throttle);
    j += ",\"accelerator\":" + jsonFloat(vd.accelerator);
    j += ",\"boost_abs\":" + jsonFloat(vd.boostAbsKpa);
    j += ",\"boost_target\":" + jsonFloat(vd.boostTargetKpa);
    j += ",\"rail\":" + jsonFloat(vd.railBar);
    j += ",\"rail_target\":" + jsonFloat(vd.railTargetBar);
    j += ",\"fuel_temp\":" + jsonFloat(vd.fuelTemp);
    j += ",\"egr_commanded\":" + jsonFloat(vd.egrCommanded);
    j += ",\"egr_actual\":" + jsonFloat(vd.egrActual);
    j += ",\"egr_error\":" + jsonFloat(vd.egrError);
    j += ",\"egt1\":" + jsonFloat(vd.egt1);
    j += ",\"egt2\":" + jsonFloat(vd.egt2);
    j += ",\"egt3\":" + jsonFloat(vd.egt3);
    j += ",\"egt4\":" + jsonFloat(vd.egt4);
    j += ",\"nox1\":" + jsonFloat(vd.nox1Ppm);
    j += ",\"nox2\":" + jsonFloat(vd.nox2Ppm);
    j += ",\"lambda1\":" + jsonFloat(vd.lambda1);
    j += ",\"lambda2\":" + jsonFloat(vd.lambda2);
    j += ",\"fast_age_ms\":" + String(vd.lastFastDataMs ? (unsigned long)(millis()-vd.lastFastDataMs) : 0);
    j += ",\"dpf_age_ms\":" + String(vd.lastDpfDataMs ? (unsigned long)(millis()-vd.lastDpfDataMs) : 0);
    j += ",\"dpf_diff\":" + jsonFloat(vd.dpfDiffPressureHpa);
    j += ",\"dpf_inlet\":" + jsonFloat(vd.dpfInletPressureKpa);
    j += ",\"dpf_outlet\":" + jsonFloat(vd.dpfOutletPressureKpa);
    j += ",\"dpf_avg_time\":" + jsonFloat(vd.dpfAvgRegenTimeMin);
    j += ",\"dpf_avg_distance\":" + jsonFloat(vd.dpfAvgRegenDistanceKm);
    j += ",\"dpf_soot\":" + jsonFloat(vd.dpfSootMassG);
    j += ",\"dpf_ash\":" + jsonFloat(vd.dpfAshMassG);
    j += ",\"dpf_regen\":" + String(vd.dpfRegen ? "true" : "false");
    j += ",\"dpf_regen_known\":" + String(vd.dpfRegenKnown ? "true" : "false");
    j += ",\"dpf_regen_active_type\":" + String(vd.dpfRegenActiveType ? "true" : "false");
    j += ",\"dpf_regen_type_known\":" + String(vd.dpfRegenTypeKnown ? "true" : "false");
    j += ",\"gear\":" + String(vd.gear);
    j += ",\"gear_input_rpm\":" + jsonFloat(vd.gearboxInputRpm);
    j += ",\"gear_output_rpm\":" + jsonFloat(vd.gearboxOutputRpm);
    j += ",\"converter_slip\":" + jsonFloat(vd.converterSlipRpm);
    j += ",\"lockup\":" + String(vd.lockup ? "true" : "false");
    j += ",\"lockup_known\":" + String(vd.lockupKnown ? "true" : "false");
    j += ",\"dde_detected\":" + String(vd.ddeDetected ? "true" : "false");
    j += ",\"egs_detected\":" + String(vd.egsDetected ? "true" : "false");
    j += ",\"bmw_profile\":\"D70BX7A0 metadata ready / proprietary mappings pending\"";
    j += ",\"log_frames\":" + String((unsigned long)logger_frames_written());
    j += ",\"log_dropped\":" + String((unsigned long)logger_frames_dropped());
    j += ",\"log_stalls\":" + String((unsigned long)logger_stall_count());
    j += ",\"log_max_write_ms\":" + String((unsigned long)logger_max_write_ms());
    j += ",\"log_max_flush_ms\":" + String((unsigned long)logger_max_flush_ms());
    j += ",\"log_last_io_ms\":" + String((unsigned long)logger_last_io_ms());
    j += ",\"time\":\"" + jsonEscape(time_iso()) + "\"";
    const TripStats &ts = trip_stats();
    j += ",\"trip_active\":" + String(ts.active ? "true" : "false");
    j += ",\"trip_duration\":" + String((unsigned long)ts.durationSec);
    j += ",\"peak_coolant\":" + jsonFloat(ts.maxCoolant);
    j += ",\"peak_oil\":" + jsonFloat(ts.maxOil);
    j += ",\"peak_turbo\":" + jsonFloat(ts.maxTurbo);
    j += "}";

    server.send(200, "application/json", j);
}

static void sendWifiScan()
{
    int n = wifi_scan();
    if (wifi_scan_running() && n==0) { server.send(202,"application/json","[]"); return; }

    String j = "[";
    for (int i = 0; i < n; ++i) {
        WifiNetworkInfo w = wifi_scan_get(i);
        if (i) j += ",";
        j += "{\"ssid\":\"" + jsonEscape(w.ssid) + "\",";
        j += "\"rssi\":" + String(w.rssi) + ",";
        j += "\"secure\":" + String(w.secure ? "true" : "false") + "}";
    }
    j += "]";

    server.send(200, "application/json", j);
}

static void sendLogs()
{
    int n = logger_file_count();

    String j = "[";
    for (int i = 0; i < n; ++i) {
        if (i) j += ",";

        String name = logger_file_name(i);

        j += "{\"name\":\"" + jsonEscape(name) + "\",";
        j += "\"size\":" + String((unsigned long)logger_file_size(i)) + "}";
    }
    j += "]";

    server.send(200, "application/json", j);
}

static void downloadLog()
{
    if (!auth()) return;

    if (!logger_storage_ready()) {
        server.send(503, "text/plain", "microSD non disponibile");
        return;
    }

    // SD_MMC is shared with the acquisition logger. Serving an open session
    // would serialize long reads with writes/flushes and starve CAN + Wi-Fi.
    if (logger_active()) {
        server.send(409, "text/plain",
                    "Ferma il logger prima di scaricare i file");
        return;
    }

    String name = server.arg("name");
    if (!name.startsWith("/")) name = "/" + name;

    if (!SD_MMC.exists(name)) {
        server.send(404, "text/plain", "File non trovato");
        return;
    }

    File f = SD_MMC.open(name, FILE_READ);
    if (!f) {
        server.send(500, "text/plain", "Impossibile aprire il file");
        return;
    }

    String downloadName = name.substring(name.lastIndexOf('/') + 1);
    server.sendHeader(
        "Content-Disposition",
        "attachment; filename=\"" + downloadName + "\""
    );
    server.setContentLength(f.size());
    server.send(200, "text/csv", "");

    WiFiClient client = server.client();
    uint8_t buffer[2048];
    while (f.available() && client.connected()) {
        const size_t count = f.read(buffer, sizeof(buffer));
        if (!count) break;
        size_t sent = 0;
        while (sent < count && client.connected()) {
            const size_t n = client.write(buffer + sent, count - sent);
            if (!n) { delay(1); continue; }
            sent += n;
            can_update();
            yield();
        }
    }
    f.close();
}


static String jsonFloat(float v)
{
    if (isnan(v)) return "null";
    return String(v, 3);
}

static void sendHistory()
{
    int n = history_count();
    String j = "[";
    for (int i = 0; i < n; ++i) {
        HistoryPoint p;
        if (!history_get(i, p)) continue;
        if (i) j += ",";
        j += "{";
        j += "\"m\":" + String((int)p.ageMinutes) + ",";
        j += "\"coolant\":" + jsonFloat(p.coolant) + ",";
        j += "\"oil\":" + jsonFloat(p.oil) + ",";
        j += "\"intake\":" + jsonFloat(p.intake) + ",";
        j += "\"turbo\":" + jsonFloat(p.turbo) + ",";
        j += "\"dpf\":" + jsonFloat(p.dpf) + ",";
        j += "\"gearbox\":" + jsonFloat(p.gearbox) + ",";
        j += "\"rpm\":" + jsonFloat(p.rpm) + ",";
        j += "\"speed\":" + jsonFloat(p.speed) + ",";
        j += "\"rail\":" + jsonFloat(p.rail) + ",";
        j += "\"dpfDiff\":" + jsonFloat(p.dpfDiff) + ",";
        j += "\"egt1\":" + jsonFloat(p.egt1) + ",";
        j += "\"egt2\":" + jsonFloat(p.egt2);
        j += "}";
    }
    j += "]";
    server.send(200, "application/json", j);
}


static void sendDiagnosticProfile()
{
    String j = "[";
    for (int i = 0; i < diagnostic_parameter_count(); ++i) {
        const DiagnosticParameterInfo &p = diagnostic_parameter(i);
        if (i) j += ",";
        j += "{\"key\":\"" + String(p.key) + "\",";
        j += "\"ecu\":\"" + String(p.ecu) + "\",";
        j += "\"protocol\":\"" + String(p.protocol) + "\",";
        j += "\"service\":" + String(p.service) + ",";
        j += "\"identifier\":" + String(p.identifier) + ",";
        j += "\"unit\":\"" + String(p.unit) + "\",";
        j += "\"confidence\":\"" + String(diagnostic_confidence_name(p.confidence)) + "\",";
        j += "\"description\":\"" + jsonEscape(String(p.description)) + "\"}";
    }
    j += "]";
    server.send(200, "application/json", j);
}


static void sendDiscovery()
{
    String j = "{";
    j += "\"ids\":" + String(discovery_count());
    j += ",\"passive_ids\":" + String((unsigned long)discovery_passive_ids());
    j += ",\"passive_frames\":" + String((unsigned long)discovery_passive_frames());
    j += ",\"scan_active\":" + String(can_readonly_scan_active() ? "true" : "false");
    j += ",\"scan_mask\":\"0x" + String(can_readonly_scan_response_mask(), HEX) + "\"";
    j += ",\"scan_result\":\"" + jsonEscape(can_readonly_scan_result()) + "\"";
    j += ",\"bmw_scan_active\":" + String(can_bmw_extended_scan_active() ? "true" : "false");
    j += ",\"bmw_scan_mask\":\"0x" + String(can_bmw_extended_scan_response_mask(), HEX) + "\"";
    j += ",\"bmw_scan_result\":\"" + jsonEscape(can_bmw_extended_scan_result()) + "\"";
    j += ",\"entries\":[";

    for (int i = 0; i < discovery_count(); ++i) {
        DiscoveryCanEntry e;
        if (!discovery_get(i, e)) continue;
        if (i) j += ",";
        float avg = e.count > 1 ? (float)(e.lastMs-e.firstMs)/(float)(e.count-1) : 0.0f;
        String data;
        char b[4];
        for (uint8_t k=0;k<e.dlc && k<8;++k){if(k)data+=" ";snprintf(b,sizeof(b),"%02X",e.last[k]);data+=b;}
        j += "{\"id\":\"0x" + String(e.id,HEX) + "\",\"count\":" + String((unsigned long)e.count);
        j += ",\"avg_ms\":" + String(avg,3) + ",\"last\":\"" + data + "\",\"changes\":[";
        for(int k=0;k<8;++k){if(k)j+=",";j+=String((unsigned long)e.changes[k]);}
        j += "]}";
    }
    j += "]}";
    server.send(200,"application/json",j);
}

static void sendCanCatalog()
{
    String j = "[";
    uint32_t now = millis();

    for (int i = 0; i < can_catalog_count(); ++i) {
        CanCatalogEntry e;
        if (!can_catalog_get(i, e)) continue;

        if (j.length() > 1) j += ",";

        String data;
        char b[4];

        for (uint8_t k = 0; k < e.dlc && k < 8; ++k) {
            if (k) data += " ";
            snprintf(b, sizeof(b), "%02X", e.data[k]);
            data += b;
        }

        j += "{";
        j += "\"id\":\"0x" + String(e.id, HEX) + "\",";
        j += "\"id_dec\":" + String((unsigned long)e.id) + ",";
        j += "\"extended\":" + String(e.extended ? "true" : "false") + ",";
        j += "\"count\":" + String((unsigned long)e.count) + ",";
        j += "\"age_ms\":" + String((unsigned long)(now - e.lastMillis)) + ",";
        j += "\"dlc\":" + String(e.dlc) + ",";
        j += "\"data\":\"" + data + "\"";
        j += "}";
    }

    j += "]";
    server.send(200, "application/json", j);
}

void web_server_begin()
{
    webPrefs.begin("bmwweb", false);
    webAuthEnabled = webPrefs.getBool("auth", false);
    webPassword = webPrefs.getString("pass", "");
    if (webAuthEnabled && webPassword.length() < 8) {
        webAuthEnabled = false;
        webPrefs.putBool("auth", false);
    }

    server.on("/", HTTP_GET, []() {
        if (!auth()) return;
        server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
    });

    server.on("/ota", HTTP_GET, []() {
        if (!auth()) return;
        server.send_P(200, "text/html; charset=utf-8", OTA_HTML);
    });

    server.on("/api/security", HTTP_GET, []() {
        if (!auth()) return;
        server.send(200, "application/json", String("{\"enabled\":") + (webAuthEnabled ? "true" : "false") + "}");
    });

    server.on("/api/security", HTTP_POST, []() {
        if (!auth()) return;
        bool enabled = server.arg("enabled") == "1";
        if (enabled) {
            String password = server.arg("password");
            if (password.length() < 8 || password.length() > 63) {
                server.send(400, "text/plain", "Password non valida: usare da 8 a 63 caratteri");
                return;
            }
            webPassword = password;
            webPrefs.putString("pass", webPassword);
            webAuthEnabled = true;
            webPrefs.putBool("auth", true);
            server.send(200, "text/plain", "Protezione attivata. Utente: admin");
        } else {
            webAuthEnabled = false;
            webPassword = "";
            webPrefs.putBool("auth", false);
            webPrefs.remove("pass");
            server.send(200, "text/plain", "Protezione web disattivata");
        }
    });

    server.on("/api/discovery", HTTP_GET, []() {
        if (!auth()) return;
        sendDiscovery();
    });

    server.on("/api/diag/scan", HTTP_POST, []() {
        if (!auth()) return;
        if (can_readonly_scan_active() || can_bmw_extended_scan_active()) {
            server.send(409, "text/plain", "Una scansione diagnostica e gia in corso");
            return;
        }
        can_start_readonly_scan();
        server.send(200, "text/plain", "Scan ECU avviato: 8 TesterPresent cooperativi, nessun blocco del web server");
    });

    server.on("/api/diag/bmw-ext-scan", HTTP_POST, []() {
        if (!auth()) return;
        if (can_readonly_scan_active() || can_bmw_extended_scan_active()) { server.send(409,"text/plain","Una scansione diagnostica e gia in corso"); return; }
        can_start_bmw_extended_scan();
        server.send(200,"text/plain","BMW EXT avviato: 6F1 -> 12,18,5E,60; TesterPresent read-only");
    });

    server.on("/api/logger/marker", HTTP_POST, []() {
        if (!auth()) return;
        if (!logger_active()) { server.send(409,"text/plain","Avvia prima il logger"); return; }
        String label=server.arg("label");
        if (!label.length()) { server.send(400,"text/plain","Etichetta mancante"); return; }
        logger_mark_event(label);
        server.send(200,"text/plain","Marker registrato");
    });

    server.on("/api/history", HTTP_GET, []() {
        if (!auth()) return;
        sendHistory();
    });

    server.on("/api/diag/profile", HTTP_GET, []() {
        if (!auth()) return;
        sendDiagnosticProfile();
    });

    server.on("/api/can/catalog", HTTP_GET, []() {
        if (!auth()) return;
        sendCanCatalog();
    });

    server.on("/api/can/clear", HTTP_POST, []() {
        if (!auth()) return;
        can_stats_reset();
        server.send(200, "text/plain", "Statistiche CAN e catalogo azzerati");
    });

    server.on("/api/status", HTTP_GET, []() {
        if (!auth()) return;
        sendStatus();
    });

    server.on("/api/wifi/scan", HTTP_GET, []() {
        if (!auth()) return;
        sendWifiScan();
    });

    server.on("/api/wifi/connect", HTTP_POST, []() {
        if (!auth()) return;

        String ssid = server.arg("ssid");
        String password = server.arg("password");

        if (!ssid.length()) {
            server.send(400, "text/plain", "SSID mancante");
            return;
        }

        wifi_connect(ssid, password);
        server.send(200, "text/plain", "Connessione avviata");
    });

    server.on("/api/wifi/forget", HTTP_POST, []() {
        if (!auth()) return;
        wifi_forget();
        server.send(200, "text/plain", "Rete dimenticata");
    });

    server.on("/api/logger/start", HTTP_POST, []() {
        if (!auth()) return;
        server.send(logger_start() ? 200 : 500, "text/plain",
                    logger_active() ? "Logger avviato" : "Errore logger");
    });

    server.on("/api/logger/stop", HTTP_POST, []() {
        if (!auth()) return;
        logger_stop();
        server.send(200, "text/plain", "Logger fermato");
    });

    server.on("/api/logs", HTTP_GET, []() {
        if (!auth()) return;
        sendLogs();
    });

    server.on("/api/log/download", HTTP_GET, downloadLog);

    server.on("/api/log/delete", HTTP_POST, []() {
        if (!auth()) return;

        String name = server.arg("name");
        if (!name.length()) {
            server.send(400, "text/plain", "Nome file mancante");
            return;
        }

        if (logger_delete(name)) {
            server.send(200, "text/plain", "Log eliminato");
        } else {
            server.send(
                409,
                "text/plain",
                logger_active()
                    ? "Impossibile eliminare il file: potrebbe essere il log attivo"
                    : "Impossibile eliminare il file dalla microSD"
            );
        }
    });

    server.on("/api/log/delete-all", HTTP_POST, []() {
        if (!auth()) return;
        logger_delete_all();
        server.send(200, "text/plain", "Acquisizioni eliminate");
    });

    server.on("/api/trip/reset-peaks", HTTP_POST, []() {
        if (!auth()) return;
        trip_reset_peaks();
        server.send(200,"text/plain","Picchi azzerati");
    });

    server.on("/api/alarms", HTTP_GET, []() {
        if (!auth()) return;
        String j="{";
        j += "\"coolant\":" + String(alarm_limit_coolant(),1) + ",";
        j += "\"oil\":" + String(alarm_limit_oil(),1) + ",";
        j += "\"gearbox\":" + String(alarm_limit_gearbox(),1) + ",";
        j += "\"dpf\":" + String(alarm_limit_dpf(),1);
        j += "}";
        server.send(200,"application/json",j);
    });

    server.on("/api/alarms", HTTP_POST, []() {
        if (!auth()) return;
        alarm_set_limits(server.arg("coolant").toFloat(),server.arg("oil").toFloat(),server.arg("gearbox").toFloat(),server.arg("dpf").toFloat());
        server.send(200,"text/plain","Soglie salvate");
    });

    server.on("/api/config/export", HTTP_GET, []() {
        if (!auth()) return;
        server.sendHeader("Content-Disposition", "attachment; filename=\"bmw520xd_config.json\"");
        server.send(200,"application/json",config_export_json());
    });

    server.on("/api/config/import", HTTP_POST, []() {
        if (!auth()) return;
        String err;
        if(config_import_json(server.arg("plain"),err)) server.send(200,"text/plain","Configurazione importata");
        else server.send(400,"text/plain",err);
    });

    server.on("/api/reboot", HTTP_POST, []() {
        if (!auth()) return;
        if (logger_active()) logger_stop();
        server.send(200, "text/plain", "Riavvio...");
        rebootPending = true;
        rebootAt = millis() + 600;
    });

    server.on("/api/factory-reset", HTTP_POST, []() {
        if (!auth()) return;
        wifi_factory_reset();
        webAuthEnabled = false;
        webPassword = "";
        webPrefs.clear();
        server.send(200, "text/plain", "Reset configurazione. Riavvio...");
        rebootPending = true;
        rebootAt = millis() + 600;
    });

    server.on("/update", HTTP_POST,
        []() {
            if (!auth()) return;

            if (!otaStarted) {
                server.send(
                    400,
                    "text/plain",
                    "OTA non avviata: il browser non ha inviato un upload multipart valido."
                );
                return;
            }

            if (!otaFinished) {
                server.send(
                    500,
                    "text/plain",
                    "OTA incompleta: upload terminato senza evento finale."
                );
                return;
            }

            if (!otaSuccess || Update.hasError()) {
                String msg = "OTA fallita";
                if (otaError.length()) {
                    msg += ": ";
                    msg += otaError;
                }
                server.send(500, "text/plain", msg);
                return;
            }

            server.send(
                200,
                "text/plain",
                "Aggiornamento completato: " +
                String((unsigned long)otaBytesWritten) +
                " byte. Riavvio..."
            );

            // Reboot ONLY after a fully successful OTA.
            rebootPending = true;
            rebootAt = millis() + 1500;
        },
        []() {
            if (webAuthEnabled && !server.authenticate(WEB_USER, webPassword.c_str())) return;

            HTTPUpload &upload = server.upload();

            if (upload.status == UPLOAD_FILE_START) {
                // OTA and SD_MMC must never contend for long synchronous I/O.
                // Closing here also guarantees all CSV data is committed first.
                if (logger_active()) logger_stop();
                otaStarted = true;
                otaFinished = false;
                otaSuccess = false;
                otaError = "";
                otaBytesWritten = 0;

                Serial.printf(
                    "OTA START: %s, content-length=%u\n",
                    upload.filename.c_str(),
                    server.client().available()
                );

                if (!upload.filename.endsWith(".bin")) {
                    otaError = "estensione file non valida";
                    Serial.println("OTA ERROR: file non .bin");
                    return;
                }

                // UPDATE_SIZE_UNKNOWN lets the updater use the inactive OTA slot.
                if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
                    otaError = "Update.begin() fallito";
                    Update.printError(Serial);
                    return;
                }

                Serial.printf(
                    "OTA target partition capacity: %u bytes\n",
                    (unsigned int)Update.size()
                );
            }
            else if (upload.status == UPLOAD_FILE_WRITE) {
                if (otaError.length()) return;

                size_t written = Update.write(
                    upload.buf,
                    upload.currentSize
                );

                otaBytesWritten += written;

                if (written != upload.currentSize) {
                    otaError = "errore scrittura flash";
                    Update.printError(Serial);
                    return;
                }

                static uint32_t lastPrint = 0;
                uint32_t now = millis();

                if (now - lastPrint > 500) {
                    lastPrint = now;
                    Serial.printf(
                        "OTA WRITE: %u bytes total\n",
                        (unsigned int)otaBytesWritten
                    );
                }
            }
            else if (upload.status == UPLOAD_FILE_END) {
                otaFinished = true;

                if (otaError.length()) {
                    Serial.printf(
                        "OTA END with error: %s\n",
                        otaError.c_str()
                    );
                    Update.abort();
                    return;
                }

                if (!Update.end(true)) {
                    otaError = "Update.end() fallito";
                    otaSuccess = false;
                    Update.printError(Serial);
                    return;
                }

                otaSuccess = !Update.hasError();

                Serial.printf(
                    "OTA END: %u bytes, success=%s\n",
                    upload.totalSize,
                    otaSuccess ? "YES" : "NO"
                );
            }
            else if (upload.status == UPLOAD_FILE_ABORTED) {
                otaFinished = true;
                otaSuccess = false;
                otaError = "upload interrotto dal client";
                Update.abort();
                Serial.println("OTA ABORTED");
            }
        }
    );

    server.begin();
    Serial.println("Web server started on port 80");
}

void web_server_loop()
{
    server.handleClient();

    if (rebootPending && (int32_t)(millis() - rebootAt) >= 0) {
        ESP.restart();
    }
}
