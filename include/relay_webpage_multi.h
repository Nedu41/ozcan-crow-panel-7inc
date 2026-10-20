// ===================== YEREL HTML (PROGMEM) =====================
const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="tr">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>Merkezi Hub</title>

<style>
@keyframes pulse { 0% { opacity: 1; } 50% { opacity: 0.7; } 100% { opacity: 1; } }
#alertbar { position: sticky; top: 0; z-index: 50; }
.alert { display: flex; align-items: center; justify-content: space-between; gap: 10px; margin: 6px 8px; padding: 10px 12px; border-radius: 8px; border: 2px solid #FF9800; color: #fff; animation: pulse 1.2s infinite; }
.alert-eq { background: #B71C1C; }
.alert-wx { background: #B23A00; }
.alert small { opacity: 0.9; }
.alert button { flex: none; background: #fff; color: #222; border: 0; border-radius: 6px; padding: 8px 14px; font-weight: bold; cursor: pointer; }
.btn.pulsing { animation: pulse 1s infinite; }
.btn.success { background: #4CAF50 !important; color: white !important; }
.btn.error { background: #cf6679 !important; color: white !important; }
.btn.warning { background: #ff9800 !important; color: black !important; }

body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: #121212; color: #eee; margin: 0; padding-bottom: 20px; }
.header { background: #222; color: #03dac6; padding: 12px; text-align: center; font-size: 1.1em; font-weight: bold; border-bottom: 2px solid #333; display: flex; justify-content: space-between; align-items: center; }
.header span { font-size: 0.8em; color: #888; }
.fwver { font-size: 0.6em; color: #9a9a9a; font-weight: normal; margin-left: 4px; }
.nav { background: #1a1a1a; padding: 8px; display: flex; flex-wrap: wrap; justify-content: center; gap: 6px; border-bottom: 1px solid #333; }
.nav a { color: #888; text-decoration: none; font-size: 0.9em; padding: 5px 10px; border-radius: 4px; transition: 0.3s; }
.nav a.active { background: #3700b3; color: #fff; }

.grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(160px, 1fr)); gap: 10px; padding: 10px; max-width: 800px; margin: 0 auto; }
.card { background: #1e1e1e; border: 1px solid #333; border-radius: 10px; padding: 10px; box-shadow: 0 2px 5px rgba(0,0,0,0.5); position: relative; display: flex; flex-direction: column; transition: transform 0.2s; }
.card-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 8px; border-bottom: 1px solid #333; padding-bottom: 5px; }
.dev-name { font-weight: bold; font-size: 0.95em; color: #fff; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; max-width: 100px; }
.status-dot { width: 10px; height: 10px; border-radius: 50%; background: #555; display: inline-block; margin-right: 5px; }
.online-dot { background: #03dac6; box-shadow: 0 0 5px #03dac6; }
.offline-dot { background: #cf6679; }
.timer-icon-btn { background: transparent; border: none; color: #bb86fc; cursor: pointer; font-size: 1.2em; padding: 0 5px; }

.relay-row { display: flex; align-items: center; justify-content: space-between; margin-bottom: 8px; background: #252525; padding: 5px; border-radius: 6px; }
.relay-label { font-size: 0.8em; color: #aaa; font-weight: bold; width: 25px; }
.btn { flex-grow: 1; padding: 8px 5px; border: none; border-radius: 6px; font-weight: bold; font-size: 0.85em; cursor: pointer; transition: 0.2s; text-transform: uppercase; letter-spacing: 0.5px; }
.on { background: #03dac6; color: #000; }
.off { background: #333; color: #ccc; border: 1px solid #444; }
.off:hover { background: #444; }
.sched-info { font-size: 0.65em; color: #bb86fc; text-align: center; margin-top: -3px; margin-bottom: 5px; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }

.section { margin-top: 5px; background: #2a2a2a; padding: 8px; border-radius: 8px; display: none; border: 1px solid #444; font-size: 0.85em; }
.days { display: flex; justify-content: space-between; gap: 2px; margin: 10px 0; }
.days label { width: 25px; height: 25px; line-height: 25px; background: #444; color: #eee; border-radius: 4px; text-align: center; cursor: pointer; font-size: 0.75em; font-weight: bold; }
.days input[type="checkbox"]:checked + label { background: #bb86fc; color: #000; }
.days input[type="checkbox"] { display: none; }

input[type="time"], input[type="text"], input[type="number"], select { 
    background: #111; border: 1px solid #444; color: #fff; padding: 5px; border-radius: 4px; width: 100%; box-sizing: border-box; 
}
.action-btns { display: flex; gap: 5px; margin-top: 10px; }

.settings-container { max-width: 600px; margin: 10px auto; padding: 15px; background: #1f1f1f; border-radius: 8px; }
table { width: 100%; border-collapse: collapse; }
td { padding: 8px; border-bottom: 1px solid #333; }

.toast { position: fixed; left: 50%; bottom: 20px; transform: translateX(-50%); background: #1f1f1f; color: #03dac6; border: 1px solid #03dac6; border-radius: 20px; padding: 8px 18px; font-size: 0.9em; opacity: 0; pointer-events: none; transition: opacity 0.3s; z-index: 1000; }
.toast.show { opacity: 1; }
.toast.err { color: #cf6679; border-color: #cf6679; }
input.invalid { border-color: #cf6679 !important; background: #3a1f24 !important; }
.scan-box { background: #1a1a1a; border: 1px solid #2196F3; border-radius: 6px; padding: 10px; margin: 10px 0; font-size: 0.85em; }
.scan-row { display: flex; flex-wrap: wrap; gap: 8px; align-items: center; padding: 6px 0; border-top: 1px solid #333; }
.scan-row span { flex: 1 1 200px; color: #03dac6; }
.scan-row input { flex: 1 1 120px; background: #222; color: #eee; border: 1px solid #444; border-radius: 4px; padding: 4px; }

/* WiFi bilgi kutusu */
.wifi-info {
    font-size: 0.7em;
    color: #888;
    margin-bottom: 8px;
    padding: 4px;
    background: #1a1a1a;
    border-radius: 4px;
    text-align: center;
    border: 1px solid #333;
    transition: all 0.3s ease;
}

.wifi-info:hover {
    background: #252525;
    border-color: #555;
}

/* Tablo stilleri */
#devicesTable {
    width: 100%;
    border-collapse: collapse;
    margin-top: 10px;
}

#devicesTable th {
    background: #2a2a2a;
    color: #bb86fc;
    padding: 12px 8px;
    text-align: left;
    font-size: 0.9em;
    border-bottom: 2px solid #333;
}

#devicesTable td {
    padding: 10px 8px;
    border-bottom: 1px solid #333;
    vertical-align: middle;
}

#devicesTable tr:hover {
    background: #252525;
}

/* MAC ID kutusu */
.mac-box {
    background: #1a1a1a;
    padding: 4px 8px;
    border-radius: 4px;
    border: 1px solid #333;
    font-family: 'Courier New', monospace;
    font-size: 0.85em;
    color: #bb86fc;
    letter-spacing: 0.5px;
}

/* Cihaz işlem butonları */
.device-action-btns {
    display: flex;
    gap: 5px;
    margin-top: 5px;
    justify-content: center;
}

.device-action-btn {
    padding: 4px 8px;
    font-size: 0.75em;
    border-radius: 4px;
    border: none;
    cursor: pointer;
    transition: 0.2s;
}

.device-action-btn.eeprom {
    background: #ff9800;
    color: black;
}

.device-action-btn.wifi {
    background: #2196F3;
    color: white;
}

.device-action-btn:hover {
    opacity: 0.9;
}

/* RSSI Sütunu Stilleri */
.rssi-value {
    font-family: 'Courier New', monospace;
    font-weight: bold;
    font-size: 0.85em;
    text-align: center;
    padding: 3px 8px;
    border-radius: 4px;
    display: inline-block;
    min-width: 70px;
}

.rssi-excellent { 
    background: rgba(76, 175, 80, 0.2); 
    color: #4CAF50;
    border: 1px solid #4CAF50;
}
.rssi-good { 
    background: rgba(255, 193, 7, 0.2); 
    color: #FFC107;
    border: 1px solid #FFC107;
}
.rssi-fair { 
    background: rgba(255, 152, 0, 0.2); 
    color: #FF9800;
    border: 1px solid #FF9800;
}
.rssi-poor { 
    background: rgba(244, 67, 54, 0.2); 
    color: #f44336;
    border: 1px solid #f44336;
}
.rssi-offline { 
    background: rgba(117, 117, 117, 0.2); 
    color: #757575;
    border: 1px solid #757575;
}

/* RSSI İkonları */
.rssi-icon {
    font-size: 1.1em;
    margin-right: 5px;
}

/* CİHAZLAR SAYFASI İÇİN YENİ STİLLER */
.devices-container {
    max-width: 100%;
    margin: 10px auto;
    padding: 15px;
    background: #1f1f1f;
    border-radius: 8px;
    overflow-x: auto;
}

.devices-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    margin-bottom: 15px;
    flex-wrap: wrap;
    gap: 10px;
}

.devices-header h3 {
    margin: 0;
    color: #bb86fc;
}

.devices-header-buttons {
    display: flex;
    gap: 8px;
    flex-wrap: wrap;
}

/* Cihazlar Tablosu için responsive tasarım */
.devices-table-wrapper {
    overflow-x: auto;
    border-radius: 8px;
    border: 1px solid #333;
    background: #222;
}

#devicesTable {
    width: 100%;
    min-width: 800px;
    border-collapse: collapse;
    margin: 0;
}

#devicesTable th {
    background: #2a2a2a;
    color: #bb86fc;
    padding: 12px 8px;
    text-align: left;
    font-size: 0.85em;
    border-bottom: 2px solid #333;
    white-space: nowrap;
    position: sticky;
    top: 0;
    z-index: 10;
}

#devicesTable td {
    padding: 10px 8px;
    border-bottom: 1px solid #333;
    vertical-align: middle;
    font-size: 0.85em;
}

#devicesTable tr:hover {
    background: #252525;
}

/* Hücre özel stilleri */
.cell-compact {
    max-width: 120px;
    min-width: 100px;
}

.cell-ip {
    min-width: 120px;
    max-width: 150px;
    font-family: 'Courier New', monospace;
    font-size: 0.9em;
}

.cell-name {
    min-width: 120px;
    max-width: 150px;
}

.cell-mac {
    min-width: 140px;
    max-width: 160px;
}

.cell-rssi {
    min-width: 100px;
    max-width: 120px;
}

.cell-status {
    min-width: 90px;
    max-width: 100px;
}

.cell-actions {
    min-width: 140px;
    max-width: 160px;
}

.cell-delete {
    min-width: 60px;
    max-width: 70px;
    text-align: center;
}

/* Input alanları için optimize stil */
.compact-input {
    width: 100%;
    min-width: 100px;
    padding: 6px 8px;
    background: #111;
    border: 1px solid #444;
    color: #fff;
    border-radius: 4px;
    font-size: 0.85em;
    box-sizing: border-box;
}

.compact-input:focus {
    outline: none;
    border-color: #bb86fc;
}

/* MAC ID kutusu */
.mac-box-compact {
    background: #1a1a1a;
    padding: 4px 8px;
    border-radius: 4px;
    border: 1px solid #333;
    font-family: 'Courier New', monospace;
    font-size: 0.8em;
    color: #bb86fc;
    letter-spacing: 0.5px;
    display: inline-block;
    max-width: 140px;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
}

/* Cihaz işlem butonları */
.device-action-btns-compact {
    display: flex;
    flex-wrap: wrap;
    gap: 4px;
    justify-content: flex-start;
}

.device-action-btn-compact {
    padding: 4px 6px;
    font-size: 0.7em;
    border-radius: 4px;
    border: none;
    cursor: pointer;
    transition: 0.2s;
    white-space: nowrap;
}

.device-action-btn-compact.eeprom {
    background: #ff9800;
    color: black;
}

.device-action-btn-compact.wifi {
    background: #2196F3;
    color: white;
}

.device-action-btn-compact:hover {
    opacity: 0.9;
    transform: translateY(-1px);
}

/* RSSI göstergesi */
.rssi-value-compact {
    font-family: 'Courier New', monospace;
    font-weight: bold;
    font-size: 0.8em;
    text-align: center;
    padding: 4px 6px;
    border-radius: 4px;
    display: inline-block;
    min-width: 70px;
    white-space: nowrap;
}

/* Durum göstergesi */
.status-indicator {
    display: flex;
    align-items: center;
    gap: 6px;
    white-space: nowrap;
}

.status-dot-indicator {
    width: 8px;
    height: 8px;
    border-radius: 50%;
    display: inline-block;
}

.status-online {
    background: #03dac6;
    box-shadow: 0 0 5px #03dac6;
}

.status-offline {
    background: #cf6679;
}

/* Ekleme/Silme butonları */
.add-device-btn {
    background: #4CAF50 !important;
    color: white !important;
    padding: 8px 12px !important;
}

.delete-device-btn {
    background: #cf6679 !important;
    color: white !important;
    padding: 6px 10px !important;
    font-size: 0.9em;
}

.save-devices-btn {
    background: #bb86fc !important;
    color: black !important;
    padding: 8px 16px !important;
    font-weight: bold;
}

/* Mobil için responsive düzenlemeler */
@media (max-width: 768px) {
    .devices-container {
        padding: 10px;
        margin: 5px;
    }
    
    .devices-header {
        flex-direction: column;
        align-items: stretch;
    }
    
    .devices-header-buttons {
        justify-content: center;
    }
    
    #devicesTable th,
    #devicesTable td {
        padding: 8px 6px;
        font-size: 0.8em;
    }
    
    .cell-compact {
        min-width: 90px;
        max-width: 110px;
    }
    
    .compact-input {
        font-size: 0.8em;
        padding: 4px 6px;
    }
    
    .device-action-btns-compact {
        flex-direction: column;
    }
    
    .device-action-btn-compact {
        width: 100%;
        text-align: center;
    }
    
    .mac-box-compact {
        max-width: 120px;
        font-size: 0.75em;
    }
    
    .rssi-value-compact {
        min-width: 60px;
        font-size: 0.75em;
    }
}

@media (max-width: 480px) {
    #devicesTable {
        min-width: 600px;
    }
    
    .devices-header h3 {
        font-size: 1.1em;
    }
    
    .compact-input {
        font-size: 0.75em;
    }
}

/* Scrollbar stil */
.devices-table-wrapper::-webkit-scrollbar {
    height: 8px;
}

.devices-table-wrapper::-webkit-scrollbar-track {
    background: #1a1a1a;
    border-radius: 4px;
}

.devices-table-wrapper::-webkit-scrollbar-thumb {
    background: #444;
    border-radius: 4px;
}

.devices-table-wrapper::-webkit-scrollbar-thumb:hover {
    background: #555;
}

/* Hücre araç ipuçları */
[title] {
    position: relative;
}

[title]:hover::after {
    content: attr(title);
    position: absolute;
    bottom: 100%;
    left: 50%;
    transform: translateX(-50%);
    background: #333;
    color: #fff;
    padding: 5px 10px;
    border-radius: 4px;
    font-size: 0.8em;
    white-space: nowrap;
    z-index: 100;
    pointer-events: none;
    opacity: 0.9;
}

/* İkonlar */
.icon-small {
    font-size: 0.9em;
    margin-right: 3px;
}

/* Responsive için */
@media (max-width: 768px) {
    .settings-container {
        padding: 10px;
        margin: 5px;
    }
    
    #devicesTable th, 
    #devicesTable td {
        padding: 8px 5px;
        font-size: 0.85em;
    }
    
    #devicesTable .btn {
        padding: 4px 8px;
        font-size: 0.75em;
    }
    
    .device-action-btns {
        flex-direction: column;
        gap: 3px;
    }
    
    .wifi-info {
        font-size: 0.65em;
        padding: 3px;
    }
    
    .card {
        min-width: 140px;
    }
    
    .rssi-value {
        min-width: 60px;
        font-size: 0.8em;
        padding: 2px 6px;
    }
}

/* Cihaz kartlari - telefonda tek sutun, genis ekranda yan yana */
.dev-list { display: grid; grid-template-columns: repeat(auto-fill, minmax(260px, 1fr)); gap: 10px; }
.dev-list { gap: 14px; }
.dev-card { background: #181818; border: 2px solid #555; border-radius: 12px; padding: 12px; display: flex; flex-direction: column; gap: 4px; box-shadow: 0 0 0 1px #000, 0 4px 12px rgba(0,0,0,0.7); }
.dev-card.is-on { border-color: #00c853; }
.dev-card.is-off { border-color: #d32f2f; }
.dev-card-head { display: flex; align-items: center; gap: 10px; padding-bottom: 10px; margin-bottom: 4px; border-bottom: 2px solid #3a3a3a; }
.dev-card-head .compact-input { flex: 1; font-weight: bold; }
.dev-card-row { display: flex; align-items: center; gap: 8px; font-size: 0.85em; padding: 6px 0; border-bottom: 1px solid #2c2c2c; }
.dev-card-row > span { width: 55px; color: #888; flex-shrink: 0; }
.dev-card-row > input, .dev-card-row > select { flex: 1; }
.dev-card-row .mac-box-compact, .dev-card-row .rssi-value-compact { flex: 1; max-width: none; text-align: left; }
/* LED'ler: belirgin parlaklik. Yesil = bagli, kirmizi = yok, sonuk gri = kapali */
.dev-card .status-dot { width: 14px; height: 14px; margin-right: 0; border: 2px solid rgba(0,0,0,0.55); flex-shrink: 0; }
.dev-card .online-dot { background: #00e676; box-shadow: 0 0 3px #00e676, 0 0 9px #00e676, 0 0 16px #00e676; }
.dev-card .offline-dot { background: #ff1744; box-shadow: 0 0 3px #ff1744, 0 0 9px #ff1744; }
.mini-btn { background: #333; color: #eee; border: 1px solid #444; border-radius: 4px; padding: 3px 8px; cursor: pointer; }
.feat-icons { font-size: 0.85em; margin-left: 4px; }
.note-row { display: flex; align-items: center; gap: 10px; padding: 10px 6px; border-bottom: 1px solid #333; }
.note-row input[type="checkbox"] { width: 20px; height: 20px; flex-shrink: 0; }
.note-text { flex: 1; word-break: break-word; }
.note-row.done .note-text { color: #666; text-decoration: line-through; }
.devices-container { max-width: 900px; margin: 10px auto; padding: 10px; }
.devices-header { display: flex; justify-content: space-between; align-items: center; flex-wrap: wrap; gap: 8px; }
.devices-header-buttons { display: flex; gap: 6px; flex-wrap: wrap; }
@media (min-width: 769px) {
    .grid { max-width: 1000px; grid-template-columns: repeat(auto-fill, minmax(200px, 1fr)); }
    .dev-name { max-width: 150px; }
}

</style>
</head>
<body>

<div class="header">
    <div>MERKEZİ HUB <small id="fwver" class="fwver"></small></div>
    <span id="clock">00:00</span>
</div>

<div class="nav">
  <a href="/" id="nav1" onclick="route('/');return false;">Ana</a>
  <a href="/lamps" id="nav7" onclick="route('/lamps');return false;">Lambalar</a>
  <a href="/weather" id="nav3" onclick="route('/weather');return false;">Oto-Hava</a>
  <a href="/settings" id="nav2" onclick="route('/settings');return false;">Cihazlar</a>
  <a href="/sistem" id="nav9" onclick="route('/sistem');return false;">Sistem</a>
  <a href="/wifi" id="nav6" onclick="route('/wifi');return false;">WiFi</a>
  <a href="/log" id="nav4" onclick="route('/log');return false;">Kayıt</a>
  <a href="/notes" id="nav5" onclick="route('/notes');return false;">Notlar</a>
  <a href="/info" id="nav10" onclick="route('/info');return false;">Donanım</a>
</div>

<div id="alertbar"></div>

<div id="content">Yükleniyor...</div>

<script>
let DEVICES = [], STATUS = {}, CURRENT_WEATHER = {}, WEATHER_CONFIG = {};
const RELAY_COUNT = {};  // cihaz adi -> role sayisi (son bilinen)
// Zamanlayici ayarlarinin TEK kaynagi HUB (eskiden tarayicinin localStorage'i -
// baska telefonda varsayilan degerler gorunuyordu)
let SCHEDULE = {};
function schedFor(name, r) {
    const sc = SCHEDULE[name] || {}, k = 'r' + r;
    const start = sc[k + '_start'], end = sc[k + '_end'];
    return {
        start: (start && start.length === 5) ? start : null,
        end: (end && end.length === 5) ? end : null,
        days: (typeof sc[k + '_days'] === 'number') ? sc[k + '_days'] : null,
        rise: sc[k + '_sunrise_act'] || null,
        set: sc[k + '_sunset_act'] || null
    };
}
function escHtml(s) {
    return String(s).replace(/[&<>"']/g, ch => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[ch]));
}
let lastToggleTime = 0;
const daysName = ["Pt","Sa","Ça","Pe","Cu","Ct","Pz"];

// ===== WebSocket - buton gecikmesini onlemek icin HTTP polling/proxy yerine =====
let ws = null;
let wsReqCounter = 1;
const pendingControlRequests = {};
// ip_role -> {until, state}: komut bittikten sonra kisa sure eski durumu yoksay
const controlHold = {};

function connectWS() {
    const proto = location.protocol === "https:" ? "wss:" : "ws:";
    ws = new WebSocket(`${proto}//${location.host}/ws`);
    ws.onclose = () => setTimeout(connectWS, 2000);
    ws.onerror = () => { try { ws.close(); } catch(e) {} };
    ws.onmessage = (evt) => {
        let msg;
        try { msg = JSON.parse(evt.data); } catch(e) { return; }
        if (msg.type === "status") {
            applyStatusPayload(msg);
            applyDomUpdates();
        } else if (msg.type === "control_result") {
            const p = pendingControlRequests[msg.reqId];
            if (p) {
                clearTimeout(p.timeoutHandle);
                p.resolve(msg);
                delete pendingControlRequests[msg.reqId];
            }
        } else if (msg.type === "scan_result") {
            renderScanResults(msg.devices || []);
        } else if (msg.type === "alerts") {
            renderAlerts(msg.list || []);
        }
    };
}

// Hava/deprem uyarilari: onaylanana kadar kalir (HUB ekranindaki banner da kalkar)
let ALERTS = [];
function renderAlerts(list) {
    ALERTS = list;
    const el = document.getElementById('alertbar');
    if (!el) return;
    el.innerHTML = list.map(a =>
        `<div class="alert ${a.kind === 1 ? 'alert-eq' : 'alert-wx'}">` +
        `<div><b>${a.kind === 1 ? '🌍' : '⛈️'} ${escHtml(a.title)}</b><br><small>${escHtml(a.detail)}</small></div>` +
        `<button onclick="ackAlert('${escHtml(a.id)}')">Onayla</button></div>`).join('');
}
function ackAlert(id) {
    fetch('/alert_ack?id=' + encodeURIComponent(id)).then(() => renderAlerts(ALERTS.filter(a => a.id !== id)));
}

// 5sn: sunucu zayif sinyalli cihaza bir kez yeniden deniyor (2 x ~1.8sn)
function sendControl(ip, relay, action, timeoutMs = 5000) {
    return new Promise((resolve, reject) => {
        if (!ws || ws.readyState !== WebSocket.OPEN) {
            reject(new Error("WS bağlı değil"));
            return;
        }
        const reqId = wsReqCounter++;
        const t = setTimeout(() => {
            delete pendingControlRequests[reqId];
            reject(new Error("zaman aşımı"));
        }, timeoutMs);
        pendingControlRequests[reqId] = { resolve, reject, timeoutHandle: t };
        ws.send(JSON.stringify({ type: "control", reqId, ip, relay, action }));
    });
}

// Saat Göstergesi
setInterval(() => {
    const d = new Date();
    document.getElementById('clock').innerText = d.toLocaleTimeString([], {hour: '2-digit', minute:'2-digit'});
}, 1000);

// Navigasyon
function route(path) { 
    history.pushState(null, '', path); 
    navigate(); 
}

// API Fonksiyonları
function fetchDevices(){ 
    return fetch("/get_devices").then(r=>r.json()).then(d=>DEVICES=d); 
}

function applyStatusPayload(d) {
    STATUS = d.devices || {};
    CURRENT_WEATHER = d.weather || {};
    WEATHER_CONFIG = d.weather_config || {};
    SCHEDULE = d.schedule || {};
    if (d.fw) document.getElementById("fwver").textContent = "v" + d.fw;
    return d;
}

function fetchStatus(){
    return fetch("/fetch_status").then(r=>r.json()).then(applyStatusPayload);
}

// Otomatik kayit bildirimi (kaydet butonlari kaldirildi)
function toast(msg, isError) {
    let el = document.getElementById("toast");
    if (!el) {
        el = document.createElement("div");
        el.id = "toast";
        document.body.appendChild(el);
    }
    el.textContent = msg;
    el.className = "toast show" + (isError ? " err" : "");
    clearTimeout(el._t);
    el._t = setTimeout(() => { el.className = "toast"; }, 1800);
}

// Ayni alanda art arda degisiklikleri (gun kutucuklari vb.) tek kayda indirir
const _saveTimers = {};
function debounceSave(key, fn, ms = 700) {
    clearTimeout(_saveTimers[key]);
    _saveTimers[key] = setTimeout(fn, ms);
}

// Geri bildirim
function feedbackSuccess(btnId) {
    const btn = document.getElementById(btnId); 
    if(!btn) return;
    const t = btn.textContent; 
    const c = btn.className;
    btn.textContent = '✓ OK'; 
    btn.className = 'btn success';
    setTimeout(() => { 
        btn.textContent = t; 
        btn.className = c; 
    }, 2000);
}

// RSSI değerine göre sınıf belirleme fonksiyonu
function getRSSIClass(rssi) {
    if (rssi === undefined || rssi === null || rssi === 0 || rssi === "OFFLINE") {
        return "rssi-offline";
    }
    
    const rssiValue = parseInt(rssi);
    
    if (rssiValue >= -50) {
        return "rssi-excellent";
    } else if (rssiValue >= -60) {
        return "rssi-good";
    } else if (rssiValue >= -70) {
        return "rssi-fair";
    } else {
        return "rssi-poor";
    }
}

// RSSI değerine göre ikon belirleme fonksiyonu
function getRSSIIcon(rssi) {
    if (rssi === undefined || rssi === null || rssi === 0 || rssi === "OFFLINE") {
        return "📴";
    }
    
    const rssiValue = parseInt(rssi);
    
    if (rssiValue >= -50) {
        return "📶"; // Mükemmel sinyal
    } else if (rssiValue >= -60) {
        return "📶"; // İyi sinyal
    } else if (rssiValue >= -70) {
        return "📶"; // Orta sinyal
    } else if (rssiValue >= -80) {
        return "📶"; // Zayıf sinyal
    } else {
        return "📶"; // Çok zayıf sinyal
    }
}

// RSSI değerini formatlama fonksiyonu
function formatRSSI(rssi) {
    if (rssi === undefined || rssi === null || rssi === 0) {
        return "OFFLINE";
    }
    if (rssi === "OFFLINE") {
        return "Çevrimdışı";
    }
    return `${rssi} dBm`;
}

// --- ANA PANEL RENDER ---
function renderControl() {
    let html = `<div class="grid">`;
    
    if(!DEVICES.length) {
        html += `<div style="text-align:center;width:100%;padding:20px;color:#cf6679">
                    <h4>❌ Cihaz Yok</h4>
                    <p>Ayarlar > Cihazlar sekmesinden cihaz ekleyin.</p>
                    <button class="btn" onclick="route('/settings')" style="margin-top:10px">Cihaz Ekle</button>
                 </div>`;
    }
    
    DEVICES.forEach((dev, i) => {
        html += `
        <div class="card">
            <div class="card-header">
                <div style="display:flex;align-items:center">
                    <span class="status-dot" id="dot_${i}"></span>
                    <span class="dev-name" style="cursor:pointer" title="${escHtml(dev.ip)} — ayarlar" onclick="route('/device?i=${i}')">${escHtml(dev.name)}</span>
                    <span class="feat-icons">${featIcons(dev)}</span>
                </div>
                <button class="timer-icon-btn" onclick="openSect(${i})" title="Zamanlayıcı Ayarları">🕒</button>
            </div>

            <!-- WiFi Sinyal Bilgisi -->
            <div class="wifi-info" id="wifi_${i}" style="font-size:0.7em;color:#888;margin-bottom:8px;padding:4px;background:#1a1a1a;border-radius:4px;text-align:center">
                📶 WiFi: Yükleniyor...
            </div>

            ${[1, 2, 3, 4].map(r => `<div id="rw${r}_${i}" style="${r > 2 ? 'display:none' : ''}">
            <div class="relay-row">
                <div class="relay-label">R${r}</div>
                <button class="btn off" id="b${r}_${i}" onclick="toggle(${i},${r})">...</button>
            </div>
            <div class="sched-info" id="p${r}_${i}"></div>
            </div>`).join('')}

            <div class="section" id="sec_${i}">
                <div style="display:flex;gap:10px;justify-content:center;margin-bottom:10px;">
                    ${[1, 2, 3, 4].map(r => `<label id="rl${r}_${i}" style="${r > 2 ? 'display:none' : ''}"><input type="radio" name="r${i}" value="${r}" ${r === 1 ? 'checked' : ''} onchange="loadProgValues(${i})"> Röle ${r}</label>`).join('')}
                </div>
                
                <div style="display:flex;gap:5px;align-items:center">
                    <input type="time" id="t1_${i}" value="08:00" onchange="autoSaveProg(${i})">
                    <span>-</span>
                    <input type="time" id="t2_${i}" value="18:00" onchange="autoSaveProg(${i})">
                </div>

                <div class="days">
                    ${daysName.map((d,j)=>`<input type="checkbox" id="d${i}_${j}" onchange="autoSaveProg(${i})"><label for="d${i}_${j}">${d}</label>`).join("")}
                </div>
                
                <div style="margin:8px 0; padding:6px; background:#1a1a1a; border-radius:6px; font-size:0.75em">

<div style="margin:8px 0; padding:6px; background:#1a1a1a; border-radius:6px; font-size:0.75em">

  <div style="margin-bottom:6px">
    ☀️ Gündoğumu:
    <select id="sunrise_act_${i}" onchange="autoSaveProg(${i})">
      <option value="0">Pasif</option>
      <option value="ON">AÇ</option>
      <option value="OFF">KAPAT</option>
    </select>
  </div>

  <div>
    🌙 Günbatımı:
    <select id="sunset_act_${i}" onchange="autoSaveProg(${i})">
      <option value="0">Pasif</option>
      <option value="ON">AÇ</option>
      <option value="OFF">KAPAT</option>
    </select>
  </div>

</div>

                 </div>

                <div class="action-btns">
                    <button class="btn off" onclick="clearProg(${i})">SİL</button>
                </div>
            </div>
        </div>`;
    });
    
    html += `</div>`;

    document.getElementById("content").innerHTML = html;
}

// Durum Güncelleme
function update() {
    if(location.pathname !== "/") return;
    fetchStatus().then(applyDomUpdates);
}

function applyDomUpdates() {
        DEVICES.forEach((dev, i) => {
            const s = STATUS[dev.name] || {r1_state:"OFFLINE", r2_state:"OFFLINE"};
            
            // Online/Offline Durumu
            const online = s.r1_state !== "OFFLINE";
            const dot = document.getElementById("dot_"+i);
            if(dot) {
                dot.className = "status-dot " + (online ? "online-dot" : "offline-dot");
            }

            // WiFi Sinyal Bilgisi Güncelleme
            const wifiInfo = document.getElementById("wifi_"+i);
            if(wifiInfo) {
                if(online && s.wifi_rssi) {
                    const rssi = s.wifi_rssi;

                    // Sinyal gücüne göre renk belirleme
                    let signalColor = "#cf6679"; // Kırmızı (zayıf)
                    if(rssi >= -50) signalColor = "#4CAF50"; // Yeşil (mükemmel)
                    else if(rssi >= -60) signalColor = "#8BC34A"; // Açık yeşil (çok iyi)
                    else if(rssi >= -70) signalColor = "#FF9800"; // Turuncu (iyi)
                    else if(rssi >= -80) signalColor = "#FFC107"; // Sarı (zayıf)

                    wifiInfo.innerHTML = `<span style="color:${signalColor}">📶 ${rssi}dBm</span>`;
                    wifiInfo.title = `SSID: ${s.wifi_ssid || 'Bilinmiyor'}\nSinyal Gücü: ${rssi}dBm`;
                } else {
                    wifiInfo.innerHTML = '<span style="color:#cf6679">📶 Çevrimdışı</span>';
                    wifiInfo.title = 'Cihaz çevrimdışı';
                }
            }

            // Röle Durumları (cihazin role sayisi: status "relays", cevrimdisiyken son bilinen, yoksa 2)
            if (s.relays) RELAY_COUNT[dev.name] = s.relays;
            const rcnt = RELAY_COUNT[dev.name] || 2;
            for(let r = 1; r <= 4; r++) {
                const rw = document.getElementById("rw"+r+"_"+i);
                if (rw) rw.style.display = r <= rcnt ? "" : "none";
                const rlb = document.getElementById("rl"+r+"_"+i);
                if (rlb) rlb.style.display = r <= rcnt ? "" : "none";
                if (r > rcnt) continue;
                const state = s[`r${r}_state`] || "OFFLINE";
                const btn = document.getElementById("b"+r+"_"+i);
                const prog = document.getElementById("p"+r+"_"+i);
                // Komut suruyorsa ya da yeni bitti ve eski durum gelmeye devam
                // ediyorsa butona dokunma (geri donup tekrar degisme titremesi)
                const hold = controlHold[dev.ip + "_" + r];
                const skipBtn = btn && (btn.dataset.pending === "1" ||
                    (hold && Date.now() < hold.until && state !== hold.state));

                if (skipBtn) {
                    // buton oldugu gibi kalir
                } else if(state === "OFFLINE") {
                    if(btn) { 
                        btn.textContent = "❌ HATA"; 
                        btn.className = "btn error"; 
                        btn.disabled = true; 
                    }
                } else {
                    if(btn) {
                        btn.textContent = state === "ON" ? "✅ AÇIK" : "⭕ KAPALI";
                        btn.className = "btn " + (state === "ON" ? "on" : "off");
                        btn.disabled = false;
                    }
                }

                // Zamanlayici ozeti - HUB'daki kayittan (her telefonda ayni)
                const sc = schedFor(dev.name, r);
                let info = "";
                if (sc.days > 0 && sc.start && sc.end) info = `🕒 ${sc.start} - ${sc.end}`;
                if (sc.rise === "ON" || sc.rise === "OFF") info += ` ☀️${sc.rise === "ON" ? "AÇ" : "KAPAT"}`;
                if (sc.set === "ON" || sc.set === "OFF") info += ` 🌙${sc.set === "ON" ? "AÇ" : "KAPAT"}`;
                if (prog) prog.innerText = info.trim();

            }
        });
}

// Zamanlayıcı Bölümü Aç/Kapa
function openSect(i) { 
    const sec = document.getElementById("sec_"+i);
    const isVis = sec.style.display === "block";
    
    // Diğer bölümleri kapat
    document.querySelectorAll('.section').forEach(s => s.style.display = 'none');
    
    // İsteneni aç/kapa
    if(!isVis) { 
        sec.style.display = "block"; 
        loadProgValues(i); 
    }
}

// Seçili Röle
function getRelay(i) { 
    return document.querySelector(`input[name="r${i}"]:checked`).value; 
}

// Gün Maskesi
function getMask(i) { 
    let m = 0; 
    for(let j = 0; j < 7; j++) {
        if(document.getElementById("d"+i+"_"+j).checked) {
            m |= 1 << j;
        }
    }
    return m; 
}

// Röle Toggle - eskiden 2 ayrı fetch() (manual_control + proxy) atıyordu,
// senkron WebServer altında bu araya giren durum sorgulamalarıyla
// çakışıp "buton gecikmesi" hissi yaratıyordu. Artık tek WS round-trip.
function toggle(i, r) {
    const dev = DEVICES[i];
    const btn = document.getElementById("b"+r+"_"+i);
    if (btn.dataset.pending === "1") return;  // cift tiklama ikinci komut gondermesin
    const originalText = btn.textContent;
    const originalClass = btn.className;

    // Optimistik UI
    btn.textContent = "⏳";
    btn.disabled = true;
    btn.dataset.pending = "1";

    // Röle durumunu belirle
    const action = originalText.includes("AÇIK") ? "off" : "on";

    sendControl(dev.ip, parseInt(r), action)
        .then(res => {
            if (!res.ok) throw new Error("cihaz yanıt vermiyor");
            const state = action === "on" ? "ON" : "OFF";
            controlHold[dev.ip + "_" + r] = { until: Date.now() + 3000, state };
            if (STATUS[dev.name]) STATUS[dev.name][`r${r}_state`] = state;
            btn.textContent = action === "on" ? "✅ AÇIK" : "⭕ KAPALI";
            btn.className = action === "on" ? "btn on" : "btn off";
        })
        .catch(error => {
            btn.textContent = originalText;
            btn.className = originalClass;
            // alert() sayfayi kapatilana kadar donduruyordu - toast bloklamaz
            toast(`✗ ${dev.name}: ${error.message}`, true);
        })
        .finally(() => { btn.disabled = false; delete btn.dataset.pending; });
}

// Zamanlayıcı - alan degisince otomatik kaydedilir (KAYDET butonu yok)
function autoSaveProg(i) {
    debounceSave("prog_" + i, () => saveProg(i));
}

function saveProg(i) {
    const dev = DEVICES[i];
    const r = getRelay(i);
    const start = document.getElementById("t1_"+i).value;
    const end = document.getElementById("t2_"+i).value;
    const [sh, sm] = start.split(":");
    const [eh, em] = end.split(":");
    const mask = getMask(i);

const sunriseAct = document.getElementById(`sunrise_act_${i}`).value;
const sunsetAct  = document.getElementById(`sunset_act_${i}`).value;

    // Gun secilmemisse saatli zamanlayici pasif (active=0) kaydedilir -
    // gundogumu/batimi ayari gunlerden bagimsiz calismaya devam eder.
    const active = mask ? 1 : 0;

    // LocalStorage
    localStorage.setItem(`p_${dev.name}_r${r}_s`, start);
    localStorage.setItem(`p_${dev.name}_r${r}_e`, end);
    localStorage.setItem(`p_${dev.name}_r${r}_m`, mask);
localStorage.setItem(`sun_${dev.name}_r${r}_rise`, sunriseAct);
localStorage.setItem(`sun_${dev.name}_r${r}_set`, sunsetAct);


    // ✅ TEK url TANIMI
const url =
 `/save?relay_id=${r}` +
 `&start_h=${sh}&start_m=${sm}` +
 `&end_h=${eh}&end_m=${em}` +
 `&days=${mask}&active=${active}` +
 `&sunrise_act=${sunriseAct}` +
 `&sunset_act=${sunsetAct}`;


    fetch(`/proxy?ip=${encodeURIComponent(dev.ip)}&url=${encodeURIComponent(url)}`)
        .then(response => {
            if(response.ok) {
                toast(mask ? "✓ Zamanlayıcı kaydedildi" : "✓ Kaydedildi (gün seçilmedi, saatli program pasif)");
            } else {
                toast("✗ Zamanlayıcı kaydedilemedi", true);
            }
        })
        .catch(() => toast("✗ İletişim hatası", true));
}


// Zamanlayıcı Temizle
function clearProg(i) {
    const dev = DEVICES[i];
    const r = getRelay(i);

    // Local Storage'dan sil
    localStorage.removeItem(`p_${dev.name}_r${r}_s`);
    localStorage.removeItem(`p_${dev.name}_r${r}_e`);
    localStorage.removeItem(`p_${dev.name}_r${r}_m`);
localStorage.removeItem(`sun_${dev.name}_r${r}_rise`);
localStorage.removeItem(`sun_${dev.name}_r${r}_set`);


    fetch(`/proxy?ip=${encodeURIComponent(dev.ip)}&url=${encodeURIComponent(`/save?relay_id=${r}&days=0&active=0`)}`)
        .then(() => {
            alert("Zamanlayıcı iptal edildi!");
            setTimeout(update, 800);
        })
        .catch(() => alert("İletişim hatası!"));
}

// Zamanlayıcı Değerlerini Yükle
function loadProgValues(i) {
    const dev = DEVICES[i];
    const r = getRelay(i);
    // Once HUB'daki kayit; yoksa (hic kaydedilmemis) bu tarayicinin eski kaydi
    const sc = schedFor(dev.name, r);
    const s = sc.start || localStorage.getItem(`p_${dev.name}_r${r}_s`);
    const e = sc.end || localStorage.getItem(`p_${dev.name}_r${r}_e`);
    const m = sc.days !== null ? String(sc.days) : localStorage.getItem(`p_${dev.name}_r${r}_m`);
    const sr = sc.rise || localStorage.getItem(`sun_${dev.name}_r${r}_rise`) || "0";
    const ss = sc.set || localStorage.getItem(`sun_${dev.name}_r${r}_set`) || "0";

    document.getElementById(`sunrise_act_${i}`).value = sr;
    document.getElementById(`sunset_act_${i}`).value  = ss;

    document.getElementById(`t1_${i}`).value = s || "08:00";
    document.getElementById(`t2_${i}`).value = e || "18:00";

    const mask = m !== null ? parseInt(m) : 127; // Varsayılan: tüm günler
    for(let j = 0; j < 7; j++) {
        document.getElementById(`d${i}_${j}`).checked = (mask & (1 << j)) !== 0;
    }
}

// --- AYARLAR SAYFASI ---
// Tablo yerine kart listesi: telefonda ve bilgisayarda ayni gorunum
function renderSettings() {
    fetchStatus().then(() => {
        let html = `
        <div class="devices-container">
            <div class="devices-header">
                <h3>📱 Cihaz Yönetimi</h3>
                <div class="devices-header-buttons">
                    <button class="btn warning" onclick="restartHub()" title="Hub'ı yeniden başlat">🔄 Yeniden Başlat</button>
                    <button class="btn" onclick="scanDevices()" title="Ağdaki ESP01 cihazlarını mDNS ile tara" style="background:#2196F3">🔍 Cihaz Tara</button>
                    <button class="btn add-device-btn" onclick="add()" title="Yeni cihaz ekle">➕ Yeni Cihaz</button>
                    <a class="btn" href="https://nedu41.github.io/ev-lamba/yukleyici/" target="_blank" rel="noopener" title="Yeni cihaza USB ile firmware yükle (Chrome/Edge)" style="background:#7a4fd0;text-decoration:none;display:inline-block">⬇️ Firmware Yükle</a>
                </div>
            </div>

            <div id="scanResults"></div>
            <div class="dev-list">`;

        DEVICES.forEach((d, i) => {
            const status = STATUS[d.name] || {};
            const mac = status.mac || d.mac || "?";
            const online = status.r1_state !== undefined && status.r1_state !== "OFFLINE";
            const rssi = status.wifi_rssi;
            html += `
                <div class="dev-card ${online ? 'is-on' : 'is-off'}">
                    <div class="dev-card-head">
                        <span class="status-dot ${online ? 'online-dot' : 'offline-dot'}"></span>
                        <input type="text" id="n${i}" value="${escHtml(d.name)}" class="compact-input" onchange="saveDevices()" placeholder="Cihaz Adı">
                        <button class="delete-device-btn" onclick="del(${i})" title="Sil">🗑️</button>
                    </div>
                    <div class="dev-card-row"><span>IP</span>
                        <input type="text" id="i${i}" value="${escHtml(d.ip)}" class="compact-input" onchange="saveDevices()" placeholder="192.168.1.x">
                    </div>
                    <div class="dev-card-row"><span>MAC</span>
                        <div class="mac-box-compact" id="mac_${i}">${escHtml(mac)}</div>
                        <button class="mini-btn" onclick="refreshMac(${i})">🔄</button>
                    </div>
                    <div class="dev-card-row"><span>Sinyal</span>
                        <div class="rssi-value-compact ${getRSSIClass(rssi)}" id="rssi_${i}">${formatRSSI(rssi)}</div>
                        <button class="mini-btn" onclick="refreshRSSI(${i})">🔄</button>
                    </div>
                    <div class="dev-card-row"><span>Bağlantı</span>
                        ${linkRow(status, online)}
                    </div>
                    <div class="dev-card-row"><span>Güç</span>
                        ${txRow(i, status, online)}
                    </div>
                    <div class="dev-card-row"><span>MQTT</span>
                        ${mqttRow(i, status, online)}
                    </div>
                    <div class="dev-card-row"><span>Özellik</span>
                        <div class="feat-icons">${featIcons(d) || '<small style="color:#666">yok</small>'}</div>
                    </div>
                    <div class="device-action-btns-compact">
                        <button class="device-action-btn-compact" style="background:#bb86fc;color:#000" onclick="route('/device?i=${i}')">⚙️ Ayarlar</button>
                        <button class="device-action-btn-compact eeprom" onclick="deviceClear(${i}, this, 'eeprom')">💾 EEPROM</button>
                        <button class="device-action-btn-compact wifi" onclick="deviceClear(${i}, this, 'wifi')">📶 WiFi</button>
                    </div>
                </div>`;
        });

        html += `</div>
            <div style="margin-top:15px;padding:10px;background:#252525;border-radius:6px;font-size:0.8em;color:#888">
                📊 <b>${DEVICES.length}</b> cihaz kayıtlı
            </div>
        </div>`;

        document.getElementById("content").innerHTML = html;
    });
}

// Kopma teshisi (firmware v2.7): acik kalma suresi + WiFi kopma sayisi/son sebebi.
// Sebep 802.11 reason kodu; 200 = sinyal kaybi (mesafe/parazit), besleme
// sorununda ise kopma degil yeniden baslama olur (sure sifirlanir).
const WIFI_REASONS = {2:"kimlik geçersiz", 4:"hareketsizlik", 8:"cihaz ayrıldı", 15:"el sıkışma zaman aşımı",
    200:"sinyal kaybı", 201:"AP bulunamadı", 202:"kimlik hatası", 203:"ilişkilendirme hatası", 204:"el sıkışma hatası"};
function linkRow(status, online) {
    if (!online) return '<small style="color:#666">çevrimdışı</small>';
    if (status.wifi_drops === undefined) return '<small style="color:#888">firmware eski (v2.7 gerekli)</small>';
    const u = status.uptime || 0, d = Math.floor(u / 86400), h = Math.floor(u % 86400 / 3600), m = Math.floor(u % 3600 / 60);
    const up = d ? `${d}g ${h}s` : h ? `${h}s ${m}dk` : `${m}dk`;
    let txt = `${up} açık · ${status.wifi_drops} kopma`;
    if (status.wifi_drops > 0) txt += ` (son: ${WIFI_REASONS[status.wifi_reason] || status.wifi_reason})`;
    return `<small title="kod ${status.wifi_reason}">${txt}</small>`;
}

// WiFi gonderme gucu (firmware v2.9). Sadece cihazin gonderimini etkiler:
// uzak cihazda artirmak baglantiyi guclendirir, ama zayif beslemede yuksek guc
// yeniden baslamaya (brownout) yol acabilir - o zaman dusur.
const TX_STEPS = [8.5, 11, 13, 15, 17, 19.5, 20.5];
function txRow(i, status, online) {
    if (!online) return '<small style="color:#666">çevrimdışı</small>';
    if (status.tx_power === undefined) return '<small style="color:#888">firmware eski (v2.9 gerekli)</small>';
    const cur = status.tx_power, max = status.tx_max || 20.5;
    const steps = TX_STEPS.filter(v => v <= max);
    if (!steps.includes(cur)) steps.push(cur), steps.sort((a, b) => a - b);
    return `<select class="compact-input" onchange="setDeviceTx(${i}, this)">` +
        steps.map(v => `<option value="${v}" ${v === cur ? 'selected' : ''}>${v} dBm${v === max ? ' (en yüksek)' : ''}</option>`).join('') +
        `</select>`;
}

function setDeviceTx(i, sel) {
    const d = DEVICES[i], want = parseFloat(sel.value);
    const prev = STATUS[d.name] ? STATUS[d.name].tx_power : want;
    sel.disabled = true;
    fetch(`/proxy?ip=${encodeURIComponent(d.ip)}&url=${encodeURIComponent('/txpower?dbm=' + want)}`)
        .then(r => { if (!r.ok) throw new Error(); return r.json(); })
        .then(j => {
            if (Math.abs(j.tx_power - want) > 0.01) throw new Error();
            if (STATUS[d.name]) STATUS[d.name].tx_power = j.tx_power;
            toast(`✓ ${d.name}: güç ${j.tx_power} dBm`);
        })
        .catch(() => {
            sel.value = prev;
            toast(`✗ ${d.name}: güç ayarlanamadı (çevrimdışı ya da firmware eski)`, true);
        })
        .finally(() => { sel.disabled = false; });
}

// MQTT (genel broker uzerinden disaridan kontrol) - cihaz basina ac/kapat.
// Nokta = broker baglantisi (yesil bagli, kirmizi degil); kapaliyken gri.
function mqttRow(i, status, online) {
    if (!online) return '<small style="color:#666">çevrimdışı</small>';
    if (typeof status.mqtt_enabled !== 'boolean')
        return '<small style="color:#888">firmware eski (v2.6 gerekli)</small>';
    const on = status.mqtt_enabled, conn = !!status.mqtt_connected;
    const dot = !on ? 'background:#3a3a3a;box-shadow:none' : conn ? 'background:#00e676;box-shadow:0 0 3px #00e676,0 0 9px #00e676,0 0 16px #00e676' : 'background:#ff1744;box-shadow:0 0 3px #ff1744,0 0 9px #ff1744';
    const title = !on ? "kapalı" : conn ? "broker'a bağlı" : "broker'a bağlı değil";
    return `<label style="display:flex;align-items:center;gap:6px;cursor:pointer">
                <input type="checkbox" ${on ? 'checked' : ''} onchange="setDeviceMqtt(${i}, this)"> uzaktan kontrol
            </label>
            <span class="status-dot" style="${dot}" title="${title}"></span>`;
}

function setDeviceMqtt(i, cb) {
    const d = DEVICES[i];
    const want = cb.checked;
    if (!want && !confirm(`${d.name}: MQTT kapatılsın mı?
Cihaz genel broker'dan ayrılır, MQTT uygulamasıyla dışarıdan kontrol edilemez.
Web/Tailscale ile kontrol devam eder.`)) {
        cb.checked = true;
        return;
    }
    cb.disabled = true;
    fetch(`/proxy?ip=${encodeURIComponent(d.ip)}&url=${encodeURIComponent('/mqtt?enable=' + (want ? 1 : 0))}`)
        .then(r => { if (!r.ok) throw new Error(); return r.json(); })
        .then(j => {
            if (j.mqtt_enabled !== want) throw new Error();
            if (STATUS[d.name]) STATUS[d.name].mqtt_enabled = want;
            toast(`✓ ${d.name}: MQTT ${want ? 'açıldı' : 'kapatıldı'}`);
            setTimeout(() => fetchStatus().then(() => { if (location.pathname === '/settings') renderSettings(); }), want ? 6000 : 1500);
        })
        .catch(() => {
            cb.checked = !want;
            toast(`✗ ${d.name}: MQTT ayarı değiştirilemedi (çevrimdışı ya da firmware eski)`, true);
        })
        .finally(() => { cb.disabled = false; });
}

// RSSI Yenileme
function refreshRSSI(deviceIndex) {
    const device = DEVICES[deviceIndex];
    const el = document.getElementById(`rssi_${deviceIndex}`);
    if (!el) return;
    const originalText = el.textContent, originalClass = el.className;
    el.textContent = "⏳";
    fetch(`/proxy?ip=${encodeURIComponent(device.ip)}&url=${encodeURIComponent("/status")}`)
        .then(r => { if (!r.ok) throw new Error(); return r.json(); })
        .then(data => {
            el.textContent = formatRSSI(data.wifi_rssi);
            el.className = `rssi-value-compact ${getRSSIClass(data.wifi_rssi)}`;
        })
        .catch(() => {
            el.textContent = "❌ Hata";
            setTimeout(() => { el.textContent = originalText; el.className = originalClass; }, 2000);
        });
}

// MAC ID Yenileme
function refreshMac(deviceIndex) {
    const device = DEVICES[deviceIndex];
    const el = document.getElementById(`mac_${deviceIndex}`);
    if (!el) return;
    const originalText = el.textContent;
    el.textContent = "⏳";
    fetchStatus().then(() => {
        const status = STATUS[device.name] || {};
        el.textContent = status.mac || "Bulunamadı";
    }).catch(() => {
        el.textContent = "❌ Hata";
        setTimeout(() => { el.textContent = originalText; }, 2000);
    });
}

// --- CİHAZ ÖZELLİKLERİ + CİHAZ AYAR SAYFASI (/device?i=N) ---
const FEATURES = [
    {k: 'lamba',  icon: '💡', label: 'Lamba'},
    {k: 'priz',   icon: '🔌', label: 'Priz'},
    {k: 'dimmer', icon: '🎚️', label: 'Dimmer'}
];
const LIGHT_W_PER_PCT = 7;  // %1 aydinlik = 7 W/m2

function featIcons(d) {
    const f = Array.isArray(d.features) ? d.features : [];
    return FEATURES.filter(x => f.includes(x.k)).map(x => `<span title="${x.label}">${x.icon}</span>`).join(' ');
}

function renderDevice() {
    const i = parseInt(new URLSearchParams(location.search).get('i'));
    const d = DEVICES[i];
    if (!d) {
        document.getElementById("content").innerHTML = `<div class="settings-container">Cihaz bulunamadı. <button class="btn" onclick="route('/settings')">Cihazlar</button></div>`;
        return;
    }
    fetchStatus().then(() => {
        const c = WEATHER_CONFIG;
        const targets = Array.isArray(c.targets) ? c.targets : [];
        const feats = Array.isArray(d.features) ? d.features : [];
        const toPct = w => Math.max(1, Math.round(w / LIGHT_W_PER_PCT));
        const w = CURRENT_WEATHER;
        const radPct = (typeof w.radiation === 'number' && w.radiation >= 0) ? Math.min(100, Math.round(w.radiation / LIGHT_W_PER_PCT)) : null;

        let relays = '';
        for (let r = 1; r <= 2; r++) {
            const t = targets.find(t => t.device === d.name && t.relay === r);
            const pct = toPct((t && t.light) || c.light_threshold || 60);
            const act = (t && t.action) || c.action || 'ON';
            relays += `
            <div class="dev-card">
                <label style="font-weight:bold"><input type="checkbox" id="da_${r}" ${t ? 'checked' : ''}> Röle ${r} — aydınlığa göre otomatik</label>
                <div class="dev-card-row"><span>Eşik %</span><input type="number" min="1" max="100" step="1" id="dl_${r}" value="${pct}"></div>
                <div class="dev-card-row"><span>İşlem</span><select id="dx_${r}"><option value="ON" ${act === 'ON' ? 'selected' : ''}>AÇ</option><option value="OFF" ${act === 'OFF' ? 'selected' : ''}>KAPAT</option></select></div>
                <small style="color:#888">Eşiğin altında uygulanır, eşiğin 1.3 katı üstünde geri alınır.</small>
            </div>`;
        }

        document.getElementById("content").innerHTML = `
        <div class="settings-container" onchange="autoSaveDevice(${i})">
            <h3>⚙️ ${escHtml(d.name)} <small style="color:#888;font-weight:normal">${escHtml(d.ip)}</small></h3>
            <h4 style="margin-bottom:8px">Özellikler</h4>
            <div style="display:flex;gap:18px;flex-wrap:wrap;margin-bottom:18px">
                ${FEATURES.map(f => `<label><input type="checkbox" class="d_feat" value="${f.k}" ${feats.includes(f.k) ? 'checked' : ''}> ${f.icon} ${f.label}</label>`).join('')}
            </div>
            <h4 style="margin-bottom:8px">Aydınlık otomasyonu <small style="color:#888;font-weight:normal">şu an: %${radPct ?? '-'}</small></h4>
            <div class="dev-list">${relays}</div>
            <br>
            <button class="btn" onclick="route('/lamps')">← Panel</button>
        </div>`;
    });
}

function autoSaveDevice(i) {
    debounceSave("device", () => saveDeviceSettings(i), 600);
}

function saveDeviceSettings(i) {
    const d = DEVICES[i];
    if (!d) return;
    const feats = Array.from(document.querySelectorAll('.d_feat:checked')).map(e => e.value);
    const jobs = [];

    if (JSON.stringify(feats) !== JSON.stringify(d.features || [])) {
        const arr = DEVICES.map((x, j) => j === i ? Object.assign({}, x, {features: feats}) : x);
        jobs.push(fetch("/save_devices", {
            method: "POST",
            headers: {"Content-Type": "application/json"},
            body: JSON.stringify(arr)
        }).then(r => { if (!r.ok) throw new Error(); DEVICES = arr; }));
    }

    const mine = [];
    for (let r = 1; r <= 2; r++) {
        if (!document.getElementById('da_' + r).checked) continue;
        const pct = Math.min(100, Math.max(1, parseInt(document.getElementById('dl_' + r).value) || 1));
        mine.push({device: d.name, relay: r, light: pct * LIGHT_W_PER_PCT, action: document.getElementById('dx_' + r).value});
    }
    const old = WEATHER_CONFIG.targets || [];
    const targets = old.filter(t => t.device !== d.name).concat(mine);
    if (JSON.stringify(targets) !== JSON.stringify(old)) {
        const cfg = Object.assign({}, WEATHER_CONFIG, {targets});
        jobs.push(fetch("/save_weather_config", {
            method: "POST",
            headers: {"Content-Type": "application/json"},
            body: JSON.stringify(cfg)
        }).then(r => { if (!r.ok) throw new Error(); WEATHER_CONFIG.targets = targets; }));
    }

    if (!jobs.length) return;
    Promise.all(jobs)
        .then(() => toast("✓ Kaydedildi"))
        .catch(() => toast("✗ Kaydedilemedi", true));
}

// Cihaz EEPROM / WiFi temizleme - dugmenin kendisi sonucu gosterir
function deviceClear(deviceIndex, btn, kind) {
    const device = DEVICES[deviceIndex];
    const isWifi = kind === 'wifi';
    const what = isWifi ? 'WiFi ayarları' : 'EEPROM (zamanlayıcı/MQTT ayarları)';
    const after = isWifi ? 'Cihaz ağdan düşecek ve ESP01_SETUP ağını açacak.' : 'Cihaz yeniden başlayacak.';
    if (!confirm(`${device.name}: ${what} silinsin mi?\n${after}`)) return;
    const orig = btn.innerHTML, origBg = btn.style.background, origCol = btn.style.color;
    btn.disabled = true;
    btn.innerHTML = '⏳ Siliniyor...';
    const done = (ok) => {
        btn.innerHTML = ok ? '✓ Silindi' : '✗ Hata';
        btn.style.background = ok ? '#4CAF50' : '#cf6679';
        btn.style.color = '#fff';
        toast(ok ? `✓ ${device.name}: ${isWifi ? 'WiFi' : 'EEPROM'} silindi` : `✗ ${device.name}: silinemedi (çevrimdışı olabilir)`, !ok);
        setTimeout(() => {
            btn.innerHTML = orig; btn.style.background = origBg; btn.style.color = origCol; btn.disabled = false;
        }, 4000);
        if (ok) setTimeout(() => fetchStatus(), 5000);
    };
    fetch(`/proxy?ip=${encodeURIComponent(device.ip)}&url=${encodeURIComponent(isWifi ? '/wifi_clr' : '/eeprom_clr')}`)
        .then(r => done(r.ok))
        .catch(() => done(false));
}

// Cihaz listesi - isim/IP alanindan cikinca otomatik kaydedilir (Kaydet butonu yok)
const IPV4_RE = /^(25[0-5]|2[0-4]\d|1?\d?\d)(\.(25[0-5]|2[0-4]\d|1?\d?\d)){3}$/;

function readDeviceRows() {
    const arr = [];
    for (let j = 0; j < DEVICES.length; j++) {
        const n = document.getElementById("n"+j), ip = document.getElementById("i"+j);
        const row = { name: n ? n.value.trim() : DEVICES[j].name, ip: ip ? ip.value.trim() : DEVICES[j].ip };
        if (DEVICES[j].mac) row.mac = DEVICES[j].mac;  // HUB'in ogrendigi MAC - IP takibi icin korunur
        if (DEVICES[j].features) row.features = DEVICES[j].features;
        arr.push(row);
    }
    return arr;
}

function rowsValid(arr) {
    return arr.every(d => d.name.length > 0 && IPV4_RE.test(d.ip));
}

function postDevices(arr, okMsg) {
    return fetch("/save_devices", {
        method: "POST",
        headers: {"Content-Type": "application/json"},
        body: JSON.stringify(arr)
    }).then(r => {
        if (!r.ok) throw new Error();
        DEVICES = arr;
        toast(okMsg);
    }).catch(() => toast("✗ Cihazlar kaydedilemedi", true));
}

function add() {
    DEVICES = readDeviceRows();
    // Yarim IP ile kaydedilmez - IP tamamlaninca otomatik kaydolur
    DEVICES.push({name: `Cihaz_${DEVICES.length+1}`, ip: "192.168.1."});
    renderSettings();
}

function del(idx) {
    const arr = readDeviceRows();
    if (!confirm(`"${arr[idx].name}" silinsin mi?`)) return;
    arr.splice(idx, 1);
    if (!rowsValid(arr)) {
        DEVICES = arr;
        renderSettings();
        toast("⚠ Diğer satırda eksik IP var, liste o düzelince kaydedilecek", true);
        return;
    }
    postDevices(arr, "✓ Cihaz silindi").then(() => renderSettings());
}

function saveDevices() {
    const arr = readDeviceRows();
    arr.forEach((d, j) => {
        const nEl = document.getElementById("n"+j), ipEl = document.getElementById("i"+j);
        if (nEl) nEl.classList.toggle("invalid", d.name.length === 0);
        if (ipEl) ipEl.classList.toggle("invalid", !IPV4_RE.test(d.ip));
    });
    if (!rowsValid(arr)) {
        toast("⚠ Eksik/geçersiz alan var, kaydedilmedi", true);
        return;
    }
    postDevices(arr, "✓ Cihazlar kaydedildi");
}

// ===== mDNS Cihaz Tarama (yarı otomatik: bul -> listele -> isim ver -> ekle) =====
let SCAN_RESULTS = [];

function scanDevices() {
    const el = document.getElementById("scanResults");
    if (!ws || ws.readyState !== WebSocket.OPEN) {
        alert("Bağlantı yok (WS), birkaç saniye sonra tekrar deneyin.");
        return;
    }
    if (el) el.innerHTML = `<div class="scan-box">⏳ Ağ taranıyor (~15 sn)...</div>`;
    ws.send(JSON.stringify({ type: "scan_devices", reqId: wsReqCounter++ }));
}

function renderScanResults(devices) {
    SCAN_RESULTS = devices;
    const el = document.getElementById("scanResults");
    if (!el) return;
    if (!devices.length) {
        el.innerHTML = `<div class="scan-box">Yeni cihaz bulunamadı (kayıtlı olanlar listelenmez).</div>`;
        return;
    }
    el.innerHTML = `<div class="scan-box"><b>🔍 Bulunan cihazlar</b>` +
        devices.map((d, idx) => `
            <div class="scan-row">
                <span>${d.ip} · ${d.mac} · ${d.hostname}.local</span>
                <input type="text" id="scanName_${idx}" placeholder="Cihaz adı">
                <button class="btn on" onclick="confirmScanDevice(${idx})">Ekle</button>
            </div>`).join("") + `</div>`;
}

function confirmScanDevice(idx) {
    const d = SCAN_RESULTS[idx];
    const name = document.getElementById(`scanName_${idx}`).value.trim();
    if (!d) return;
    if (!name) { alert("Önce bir isim girin."); return; }

    // Mevcut tablo satırlarındaki düzenlemeleri kaybetmemek için DOM'dan oku
    const arr = readDeviceRows();
    arr.push({ name, ip: d.ip, mac: d.mac });
    if (!rowsValid(arr)) {
        toast("⚠ Listede eksik IP'li satır var, önce onu düzeltin", true);
        return;
    }
    postDevices(arr, `✓ ${name} eklendi`).then(() => renderSettings());
}

function restartHub() {

        fetch("/restart_hub");
        setTimeout(() => alert("Yeniden başlatılıyor..."), 500);

}

function resetWifi() { 
         fetch("/reset_wifi");
        setTimeout(() => alert("WiFi sıfırlanıyor..."), 500);
}

function handleTestTelegram() {
    fetch("/test_telegram")
        .then(() => alert("Test mesajı gönderildi! Telegram'ı kontrol edin."))
        .catch(() => alert("Test mesajı gönderilemedi!"));
}

// --- HAVA DURUMU SAYFASI ---
function renderWeather() {
    fetchStatus().then(() => {
        const c = WEATHER_CONFIG;
        const w = CURRENT_WEATHER;
        
        // Durum Mesajı
        let statusMessage = "✅ Sistem Aktif";
        let statusColor = "#03dac6";
        let statusIcon = "✅";
        
        if (w.description === "FETCH_ERROR") {
            statusMessage = "❌ Hava verisi alınamadı";
            statusColor = "#cf6679";
            statusIcon = "❌";
        } else if (!w.temp && !w.humidity) {
            statusMessage = "⏳ Hava durumu yükleniyor...";
            statusColor = "#bb86fc";
            statusIcon = "⏳";
        }
        
        // Rüzgar Bilgisi
        let windInfo = "-";
        if (w.wind_speed) {
            windInfo = `${w.wind_speed} Km/s`;
            if (w.wind_direction && w.wind_direction !== "-") {
                windInfo += ` ${w.wind_direction}`;
            }
        }
        
        // Gercek isik seviyesi (gunes isinimi, W/m2) - bulut yuzdesi tahmini yerine
        let scoreInfo = "";
        // Aydinlik %: %100 = acik ogle (~700 W/m2). HUB esigi W/m2 olarak tutar.
        const LIGHT_REF = 7;  // 1% = 7 W/m2
        const rad = (typeof w.radiation === 'number' && w.radiation >= 0) ? Math.round(w.radiation) : null;
        const radPct = rad !== null ? Math.min(100, Math.round(rad / LIGHT_REF)) : null;
        if (rad !== null) {
            scoreInfo = `
            <div style="background:#1a1a1a;padding:15px;border-radius:8px;margin-bottom:20px;border-left:5px solid #FF9800">
                <div style="font-size:1.2em;font-weight:bold;color:#FF9800;margin-bottom:5px">☀️ AYDINLIK: %${radPct} <small style="color:#888;font-weight:normal">(${rad} W/m²)</small></div>
                <div style="font-size:0.8em;color:#888;margin-top:8px">
                    Kıyas: açık öğle %100 · kapalı/yağmurlu %7-20 · alacakaranlık %3 altı
                </div>
            </div>`;
        }
        
        let html = `
        <div class="settings-container">
            <h3>🌤️ Oto Meteoroloji Kontrol</h3>
            
            <!-- Hava Durumu Bilgisi -->
            <div style="background:#222;padding:15px;border-radius:8px;margin-bottom:20px">
                <div style="font-size:1.1em;font-weight:bold;color:#03dac6;margin-bottom:10px">
                    ${w.city_name || c.city || '-'}
                </div>
                <div style="display:grid;grid-template-columns:1fr 1fr;gap:10px">
                    <div>🌡️ Sıcaklık: <b>${w.temp || '-'}°C</b></div>
                    <div>💧 Nem: <b>%${w.humidity || '-'}</b></div>
                    <div>☁️ Bulut: <b>%${w.clouds || '-'}</b></div>
                    <div>🌬️ Rüzgar: <b>${windInfo}</b></div>
                </div>
                <div style="margin-top:10px;padding-top:10px;border-top:1px solid #333">
                    🌅 Gün Doğumu: <b>${w.sunrise_time || '-'}</b><br>
                    🌇 Gün Batımı: <b>${w.sunset_time || '-'}</b>
                </div>
            </div>
            
            ${scoreInfo}
            
            ${statusIcon === "✅" ? '' : `
            <div style="background:#1a1a1a;padding:10px;border-radius:5px;margin-bottom:20px;border:2px solid ${statusColor};text-align:center">
                <span style="color:${statusColor};font-weight:bold;font-size:1.1em">${statusMessage}</span>
            </div>`}

            <!-- Ayarlar Formu - her alan degisince otomatik kaydedilir -->
            <table id="weatherForm" onchange="autoSaveWeather()">
                <tr><td>Karanlıkta yanma saat aralığı:<br><small style="color:#888">Açık: gün doğumu+1sa → gün batımı-1sa<br>Kapalı: gün doğumu → gün batımı</small></td><td><label><input type="checkbox" id="w_daylight_window" ${c.daylight_window === false ? '' : 'checked'}> Aktif</label></td></tr>
                <tr><td>Rüzgar Eşiği (Km/s):</td><td><input type="number" step="0.1" id="w_wind_threshold" placeholder="15.0" value="${c.wind_threshold||'15.0'}"></td></tr>
                <tr><td>Şehir:</td><td><input type="text" id="w_city" placeholder="İstanbul" value="${c.city||''}"></td></tr>
                <tr><td>İlçe:</td><td><input type="text" id="w_district" placeholder="Kadıköy" value="${c.district||''}"></td></tr>
                <tr><td>Mahalle:</td><td><input type="text" id="w_neigh" placeholder="Opsiyonel" value="${c.neighbourhood||''}"></td></tr>
            </table>
            
            <br>
            
            <!-- Butonlar -->
            <div style="display:flex;flex-direction:column;gap:10px">
                <button class="btn" id="refreshWeatherBtn" onclick="refreshWeather()" style="background:#2196F3">🔄 Hava Durumunu Güncelle</button>
                <button class="btn" onclick="skyDemo(this)" style="background:#FF9800;color:#000">🌅 Ekranda 24 Saat Demosu (~2 dk)</button>
                <button class="btn" onclick="weatherDemo(this)" style="background:#03dac6;color:#000">⛈️ Ekranda Hava Demosu (kar, fırtına... ~1 dk)</button>
                <button class="btn" onclick="alertDemo(this)" style="background:#f44336;color:#fff">🚨 Uyarı Demosu (banner + üst şerit + alt yazı, 20 sn)</button>
            </div>
        </div>`;
        
        document.getElementById("content").innerHTML = html;
    });
}

// HUB ekraninda gokyuzunu (tan, kizillik, gece, ay/yildiz) 24 saat hizli oynatir
function skyDemo(btn) {
    btn.disabled = true;
    fetch("/sky_demo")
        .then(r => { if (!r.ok) throw new Error(); toast("✓ HUB ekranında 24 saat demosu başladı (~2 dk)"); })
        .catch(() => toast("✗ Demo başlatılamadı", true))
        .finally(() => setTimeout(() => { btn.disabled = false; }, 120000));
}

// Ornek hava + deprem uyarisi: TFT banner/ust serit/alt yazi + bu sayfadaki cubuk (20 sn, sonra kendiliginden kalkar)
function alertDemo(btn) {
    btn.disabled = true;
    fetch("/alert_demo")
        .then(r => { if (!r.ok) throw new Error(); toast("✓ Örnek uyarılar 20 sn gösterilecek"); })
        .catch(() => toast("✗ Demo başlatılamadı", true))
        .finally(() => setTimeout(() => { btn.disabled = false; }, 21000));
}

// HUB ekraninda tum hava sahnelerini 8'er saniye sirayla gosterir
function weatherDemo(btn) {
    btn.disabled = true;
    fetch("/weather_demo")
        .then(r => { if (!r.ok) throw new Error(); toast("✓ HUB ekranında hava demosu başladı (~1 dk)"); })
        .catch(() => toast("✗ Demo başlatılamadı", true))
        .finally(() => setTimeout(() => { btn.disabled = false; }, 64000));
}

function autoSaveWeather() {
    debounceSave("weather", saveWeather, 600);
}

function saveWeather() {
    const addrBefore = [WEATHER_CONFIG.city || "", WEATHER_CONFIG.district || "", WEATHER_CONFIG.neighbourhood || ""].join("|");
    // Hedefler (role/esik) cihaz ayar sayfasinda - burada aynen korunur
    const cfg = {
        active: true,
        targets: WEATHER_CONFIG.targets || [],
        threshold: WEATHER_CONFIG.threshold || 3,  // isinim verisi yoksa yedek (eski bulut skoru)
        light_threshold: WEATHER_CONFIG.light_threshold || 60,
        action: WEATHER_CONFIG.action || "ON",
        daylight_window: document.getElementById('w_daylight_window').checked,
        wind_threshold: parseFloat(document.getElementById('w_wind_threshold').value),
        city: document.getElementById('w_city').value.trim(),
        district: document.getElementById('w_district').value.trim(),
        neighbourhood: document.getElementById('w_neigh').value.trim(),
        latitude: "",
        longitude: ""
    };
    const addrChanged = [cfg.city, cfg.district, cfg.neighbourhood].join("|") !== addrBefore;

    fetch("/save_weather_config", {
        method: "POST",
        headers: {"Content-Type": "application/json"},
        body: JSON.stringify(cfg)
    })
    .then(r => {
        if (!r.ok) throw new Error();
        Object.assign(WEATHER_CONFIG, cfg);
        if (addrChanged) {
            toast("✓ Konum kaydedildi, hava verisi yenileniyor...");
            // Konum/hava bilgisi sayfada gorunuyor - kullanici baska alana
            // yazmiyorsa birkac sn sonra tazele (odak kacirmamak icin)
            setTimeout(() => {
                const a = document.activeElement;
                if (location.pathname === "/weather" && !(a && (a.tagName === "INPUT" || a.tagName === "SELECT"))) navigate();
            }, 5000);
        } else {
            toast("✓ Ayarlar kaydedildi");
        }
    })
    .catch(() => toast("✗ Ayarlar kaydedilemedi", true));
}

function refreshWeather() {
    const refreshBtn = document.getElementById('refreshWeatherBtn');
    const originalText = refreshBtn.textContent;
    
    refreshBtn.disabled = true;
    refreshBtn.textContent = "⏳ Güncelleniyor...";
    
    fetch("/save_weather_config", {
        method: "POST",
        headers: {"Content-Type": "application/json"},
        body: JSON.stringify({refresh: true})
    })
    .then(r => {
        if(r.ok) {
            // Sunucu yenilemeyi hava gorevine devrediyor (HTTPS 1-3sn) -
            // yeni veri gelmeden sayfayi tazelememek icin biraz bekle
            refreshBtn.textContent = "⏳ Veri çekiliyor...";
            setTimeout(() => {
                refreshBtn.textContent = originalText;
                refreshBtn.disabled = false;
                location.reload();
            }, 5000);
        } else {
            refreshBtn.textContent = "❌ Hata";
            refreshBtn.style.background = "#cf6679";
            setTimeout(() => {
                refreshBtn.textContent = originalText;
                refreshBtn.style.background = "";
                refreshBtn.disabled = false;
            }, 3000);
        }
    })
    .catch(() => {
        refreshBtn.textContent = "❌ Bağlantı Hatası";
        refreshBtn.style.background = "#cf6679";
        setTimeout(() => {
            refreshBtn.textContent = originalText;
            refreshBtn.style.background = "";
            refreshBtn.disabled = false;
        }, 3000);
    });
}

// --- OLAY KAYDI ---
const EVENT_SRC = {
    zaman: '⏰ Zamanlayıcı', mqtt: '📱 MQTT', web: '🖥️ Web', http: '🔗 HTTP',
    gunbatimi: '🌇 Günbatımı', gundogumu: '🌅 Gündoğumu',
    bulut: '🌥️ Karanlık', 'bulut-geri': '☀️ Aydınlandı'
};
function renderEvents() {
    fetch('/events').then(r => r.json()).then(list => {
        if (location.pathname !== '/log') return;
        let html = `<div class="settings-container"><h3>📋 Olay Kaydı</h3>
            <small style="color:#888">Son 50 röle olayı, en yeni üstte. HUB yeniden başlayınca sıfırlanır.</small>`;
        if (!list.length) {
            html += `<p style="color:#888;margin-top:15px">Henüz olay yok.</p>`;
        } else {
            html += `<table style="margin-top:10px;font-size:0.85em"><tr><th align="left">Zaman</th><th align="left">Lamba</th><th>İşlem</th><th align="left">Kaynak</th></tr>` +
                list.map(e => `<tr>
                    <td style="white-space:nowrap">${escHtml(e.t)}</td>
                    <td>${escHtml(e.dev)} <small style="color:#888">R${e.relay}</small></td>
                    <td align="center" style="color:${e.on ? '#FF9800' : '#03dac6'};font-weight:bold">${e.on ? 'AÇ' : 'KAPAT'}${e.ok ? '' : ' <span style="color:#cf6679" title="Cihaza ulaşılamadı">✗</span>'}</td>
                    <td>${escHtml(EVENT_SRC[e.src] || e.src)}</td></tr>`).join('') + `</table>`;
        }
        html += `</div>`;
        document.getElementById("content").innerHTML = html;
    }).catch(() => {});
}

// --- NOTLAR SAYFASI (HUB'da kalici: /get_notes, /save_notes) ---
let NOTES = [];

// ===== WiFi aglari (SuDepoSis'teki bolum gibi) =====
// Kayitli aglar HUB'da listelenir; acilista sirayla denenir (en ustteki once).
// "Bağlan" o agi en uste alir ve HUB'i yeniden baslatir - tutmazsa sonraki
// aglar (ev agi dahil) denenir, HUB agdan kopuk kalmaz.
function renderWifi() {
    fetch("/wifi_info").then(r => r.json()).then(info => {
        const saved = info.saved || [];
        const cur = info.ssid
            ? `<span class="status-dot online-dot"></span> <b>${escHtml(info.ssid)}</b> · ${info.rssi} dBm · ${escHtml(info.ip)}`
            : '<span class="status-dot offline-dot"></span> bağlı değil';
        const rows = saved.length ? saved.map(n => `
            <div class="dev-card-row" style="align-items:center">
                <span style="flex:1">${n.idx === 0 ? '⭐ ' : ''}${escHtml(n.ssid)}${n.ssid === info.ssid ? ' <small style="color:#03dac6">(bağlı)</small>' : ''}</span>
                ${n.ssid === info.ssid ? '' : `<button class="mini-btn" onclick="wifiConnect(${n.idx}, '${escHtml(n.ssid).replace(/'/g, "\\'")}')">Bağlan</button>`}
                <button class="mini-btn" style="color:#cf6679" onclick="wifiDelete(${n.idx}, '${escHtml(n.ssid).replace(/'/g, "\\'")}', ${n.ssid === info.ssid})">Sil</button>
            </div>`).join('') : '<small style="color:#888">Kayıtlı ağ yok.</small>';

        document.getElementById("content").innerHTML = `
        <div class="settings-container">
            <h3>📶 WiFi</h3>
            <div style="background:#222;padding:12px;border-radius:8px;margin-bottom:15px">${cur}</div>

            <div style="font-size:0.85em;color:#888;margin-bottom:6px">Kayıtlı ağlar (⭐ önce denenir, tutmazsa sıradakiler)</div>
            <div class="dev-card" style="margin-bottom:15px">${rows}</div>

            <div style="font-size:0.85em;color:#888;margin-bottom:6px">Ağ ekle</div>
            <div class="dev-card">
                <div class="dev-card-row">
                    <select id="wifiSel" class="compact-input" onchange="if(this.value) document.getElementById('wifiSsid').value = this.value">
                        <option value="">Ağları tara...</option>
                    </select>
                    <button class="mini-btn" id="wifiScanBtn" onclick="wifiScan()">🔍 Tara</button>
                </div>
                <div class="dev-card-row"><span>SSID</span>
                    <input type="text" id="wifiSsid" class="compact-input" placeholder="Ağ adı" maxlength="32">
                </div>
                <div class="dev-card-row"><span>Şifre</span>
                    <input type="password" id="wifiPass" class="compact-input" placeholder="Şifre" maxlength="63">
                    <button class="mini-btn" onclick="const p=document.getElementById('wifiPass'); p.type = p.type === 'password' ? 'text' : 'password'">👁</button>
                </div>
                <button class="btn" style="margin-top:8px;width:100%" onclick="wifiAdd()">💾 Kaydet</button>
                <small style="color:#888;display:block;margin-top:6px">Kaydetmek bağlanmaz - bağlanmak için listede "Bağlan"a bas (HUB yeniden başlar).</small>
            </div>
        </div>`;
    }).catch(() => toast("✗ WiFi bilgisi alınamadı", true));
}

function wifiScan() {
    const btn = document.getElementById("wifiScanBtn");
    btn.disabled = true;
    btn.textContent = "⏳";
    let tries = 0;
    const poll = () => fetch("/wifi_scan").then(r => r.json()).then(j => {
        if (j.scanning && ++tries < 15) return setTimeout(poll, 1000);
        const sel = document.getElementById("wifiSel");
        const nets = (j.networks || []).sort((a, b) => b.rssi - a.rssi);
        sel.innerHTML = `<option value="">${nets.length} ağ bulundu - seç...</option>` +
            nets.map(n => `<option value="${escHtml(n.ssid)}">${escHtml(n.ssid)} (${n.rssi} dBm${n.secured ? ', 🔒' : ''})</option>`).join('');
        btn.disabled = false;
        btn.textContent = "🔍 Tara";
    }).catch(() => { btn.disabled = false; btn.textContent = "🔍 Tara"; toast("✗ Tarama başarısız", true); });
    poll();
}

function wifiAdd() {
    const ssid = document.getElementById("wifiSsid").value.trim();
    const pass = document.getElementById("wifiPass").value;
    if (!ssid) { toast("✗ SSID gerekli", true); return; }
    fetch("/wifi_add", { method: "POST", body: new URLSearchParams({ ssid, pass }) })
        .then(r => r.json()).then(j => {
            if (!j.ok) throw new Error(j.msg || "kaydedilemedi");
            toast(`✓ ${ssid} kaydedildi`);
            renderWifi();
        })
        .catch(e => toast(`✗ ${e.message}`, true));
}

function wifiConnect(idx, ssid) {
    if (!confirm(`${ssid} ağına bağlanmak için HUB yeniden başlatılsın mı?\n\nBağlanamazsa kayıtlı diğer ağlar denenir. HUB yeni ağda farklı bir IP alabilir (merkezihub.local ile de açılır).`)) return;
    fetch(`/wifi_connect?idx=${idx}`).then(r => r.json()).then(j => {
        if (!j.ok) throw new Error();
        toast(`✓ HUB yeniden başlıyor, ${ssid} denenecek...`);
    }).catch(() => toast("✗ İşlem başarısız", true));
}

function wifiDelete(idx, ssid, isCurrent) {
    const warn = isCurrent ? "\n\nŞu an bu ağa bağlısın - HUB şimdilik bağlı kalır, ama sonraki açılışta bu ağ denenmez." : "";
    if (!confirm(`${ssid} kayıtlı ağlardan silinsin mi?${warn}`)) return;
    fetch(`/wifi_delete?idx=${idx}`).then(r => r.json()).then(j => {
        if (!j.ok) throw new Error();
        toast(`✓ ${ssid} silindi`);
        renderWifi();
    }).catch(() => toast("✗ Silinemedi", true));
}

function renderNotes() {
    fetch("/get_notes").then(r => r.json()).then(list => {
        NOTES = Array.isArray(list) ? list : [];
        drawNotes();
    }).catch(() => {
        document.getElementById("content").innerHTML = `<div class="settings-container">✗ Notlar yüklenemedi</div>`;
    });
}

function drawNotes() {
    const fmt = c => c ? new Date(c * 1000).toLocaleDateString('tr-TR', {day: 'numeric', month: 'short'}) : '';
    // Yapilmamislar ustte, her grupta en yeni ustte
    const order = NOTES.map((n, i) => i).sort((a, b) => (NOTES[a].d - NOTES[b].d) || ((NOTES[b].c || 0) - (NOTES[a].c || 0)));
    const rows = order.map(i => {
        const n = NOTES[i];
        return `
        <div class="note-row${n.d ? ' done' : ''}">
            <input type="checkbox" ${n.d ? 'checked' : ''} onchange="toggleNote(${i}, this.checked)">
            <span class="note-text">${escHtml(n.t)}</span>
            <small style="color:#888;white-space:nowrap">${fmt(n.c)}</small>
            <button class="mini-btn" onclick="delNote(${i})" title="Sil">🗑️</button>
        </div>`;
    }).join('');
    document.getElementById("content").innerHTML = `
    <div class="settings-container">
        <h3>📝 Notlar</h3>
        <div style="display:flex;gap:8px;margin-bottom:12px">
            <input type="text" id="noteInput" placeholder="Yeni not..." onkeydown="if(event.key==='Enter')addNote()">
            <button class="btn" style="flex-grow:0;background:#03dac6;color:#000" onclick="addNote()">Ekle</button>
        </div>
        ${rows || '<div style="color:#888">Henüz not yok.</div>'}
    </div>`;
}

function postNotes(msg) {
    return fetch("/save_notes", {
        method: "POST",
        headers: {"Content-Type": "application/json"},
        body: JSON.stringify(NOTES)
    }).then(r => {
        if (!r.ok) throw new Error();
        if (msg) toast(msg);
    }).catch(() => toast("✗ Not kaydedilemedi", true));
}

function addNote() {
    const el = document.getElementById("noteInput");
    const t = el.value.trim();
    if (!t) return;
    NOTES.push({t, d: false, c: Math.floor(Date.now() / 1000)});
    drawNotes();
    postNotes("✓ Not eklendi");
}

function toggleNote(i, done) {
    NOTES[i].d = done;
    drawNotes();
    postNotes();
}

function delNote(i) {
    if (!confirm(`Silinsin mi?\n"${NOTES[i].t}"`)) return;
    NOTES.splice(i, 1);
    drawNotes();
    postNotes("✓ Not silindi");
}

// ===================== ANA SAYFA (ekrandaki Ana gibi) =====================
function wmoText(c) {
    if (c === 0) return "Açık"; if (c <= 2) return "Parçalı bulutlu"; if (c === 3) return "Bulutlu";
    if (c === 45 || c === 48) return "Sisli"; if (c >= 51 && c <= 57) return "Çisenti";
    if (c >= 61 && c <= 67) return "Yağmurlu"; if (c >= 71 && c <= 77) return "Karlı";
    if (c >= 80 && c <= 82) return "Sağanak"; if (c === 85 || c === 86) return "Kar sağanağı";
    if (c >= 95) return "Fırtına"; return "Hava durumu";
}
let NEWS_ITEMS = [], newsIdx = 0;
function renderHome() {
    document.getElementById("content").innerHTML = `
    <div class="settings-container">
        <div style="display:flex;justify-content:space-between;align-items:center;flex-wrap:wrap">
            <div id="hClock" style="font-size:2.6em;color:#7fe0dc;font-weight:bold">--:--</div>
            <div id="hDate" style="text-align:right;color:#dadae4"></div>
        </div>
    </div>
    <div class="settings-container" style="padding:0;overflow:hidden;position:relative">
        <img id="hSky" src="/api/sky.bmp" style="width:100%;display:block;image-rendering:pixelated" alt="">
        <div id="hWx" style="position:absolute;left:8px;top:8px;background:rgba(0,0,0,.5);padding:8px 12px;border-radius:10px">Hava durumu yükleniyor…</div>
        <div id="hWx2" style="position:absolute;left:0;right:0;bottom:0;background:rgba(0,0,0,.6);padding:6px 12px;font-size:.9em"></div>
    </div>
    <div class="settings-container"><div style="color:#9a9aae;font-size:.85em">Dünya haberleri</div><div id="hNews" style="margin-top:6px">Haberler yükleniyor…</div></div>
    <div class="settings-container"><div style="color:#9a9aae;font-size:.85em;margin-bottom:6px">Lambalar</div><div id="hLamps"></div></div>`;
    refreshHome();
}
function refreshHome() {
    if (location.pathname !== "/") return;
    const g = ["Pazar","Pazartesi","Salı","Çarşamba","Perşembe","Cuma","Cumartesi"];
    const a = ["Ocak","Şubat","Mart","Nisan","Mayıs","Haziran","Temmuz","Ağustos","Eylül","Ekim","Kasım","Aralık"];
    const n = new Date(), p2 = x => String(x).padStart(2, "0");
    const set = (id, h) => { const e = document.getElementById(id); if (e) e.innerHTML = h; };
    set("hClock", p2(n.getHours()) + ":" + p2(n.getMinutes()));
    set("hDate", `${n.getDate()} ${a[n.getMonth()]} ${n.getFullYear()}<br>${g[n.getDay()]}`);
    fetchStatus().then(() => {
        const w = CURRENT_WEATHER || {};
        let h;
        let h2 = "";
        if (w.description === "OK") {
            h = `<div style="display:flex;align-items:center;gap:14px;flex-wrap:wrap">
                <div style="font-size:2.6em;color:#ffb27a;font-weight:bold">${Math.round(w.temp)}°</div>
                <div><div style="font-size:1.3em">${wmoText(w.weathercode)}</div><div style="color:#b8b8cc;font-size:.85em">${escHtml(w.city_name || "")}</div></div></div>`;
            h2 = `Rüzgâr ${Math.round(w.wind_speed)} km/s ${escHtml(w.wind_direction || "")} &nbsp; Nem %${w.humidity} &nbsp; Bulut %${w.clouds}<br><span style="color:#b8b8cc">Gün doğumu ${escHtml(w.sunrise_time || "")} &nbsp; Gün batımı ${escHtml(w.sunset_time || "")}</span>`;
        } else if (w.description === "NO_LOCATION") {
            h = `Konum ayarlanmamış. <a href="/weather" onclick="route('/weather');return false;" style="color:#7fe0dc">Oto-Hava</a> sayfasından şehir girin.`;
        } else h = "Hava verisi alınamadı";
        set("hWx", h); set("hWx2", h2);
        let l = "";
        DEVICES.forEach((d, i) => {
            const st = STATUS[d.name] || {};
            [1, 2, 3, 4].forEach(r => {
                const s = st["r" + r + "_state"];
                if (s === undefined) return;
                const on = s === "ON", off = s === "OFFLINE" || s === "ERR";
                l += `<button class="btn ${on ? 'on' : 'off'}" style="margin:3px" ${off ? 'disabled' : ''} onclick="toggleHome(${i},${r})">${escHtml(d.name)} R${r}: ${off ? 'çevrimdışı' : (on ? 'AÇIK' : 'kapalı')}</button>`;
            });
        });
        set("hLamps", l || "Henüz cihaz yok.");
    }).catch(() => {});
    fetch("/api/news").then(r => r.json()).then(j => { NEWS_ITEMS = j.items || []; showNews(); }).catch(() => {});
}
function skyLoop() {
    const im = document.getElementById("hSky");
    if (location.pathname !== "/" || !im) { setTimeout(skyLoop, 1000); return; }
    const next = () => setTimeout(skyLoop, 600);
    im.onload = next; im.onerror = () => setTimeout(skyLoop, 3000);
    im.src = "/api/sky.bmp?t=" + Date.now();
}
function showNews() {
    const e = document.getElementById("hNews");
    if (e && NEWS_ITEMS.length) e.innerHTML = NEWS_ITEMS.map(t => "• " + escHtml(t)).join("<br><br>");
}
function toggleHome(i, r) {
    const dev = DEVICES[i], st = STATUS[dev.name] || {};
    const action = st["r" + r + "_state"] === "ON" ? "off" : "on";
    sendControl(dev.ip, r, action).then(() => setTimeout(refreshHome, 600)).catch(() => toast("✗ " + dev.name + ": cihaz yanıt vermiyor", true));
}

// ===================== SİSTEM SAYFASI (ekrandaki Ayar gibi) =====================
function renderSistem() {
    document.getElementById("content").innerHTML = `
    <div class="settings-container"><h3>⚙️ Sistem</h3><div id="sysInfo">…</div></div>
    <div class="settings-container"><h3>Ekran</h3>
        <label>Parlaklık (arka ışık) <span id="blL"></span></label><input type="range" id="blS" min="10" max="100" style="width:100%" oninput="sysBl()">
        <label>Kontrast <span id="ctL"></span></label><input type="range" id="ctS" min="50" max="130" style="width:100%" oninput="sysCol()">
        <label>Renk parlaklığı <span id="dgL"></span></label><input type="range" id="dgS" min="40" max="100" style="width:100%" oninput="sysCol()">
        <label>Canlılık <span id="saL"></span></label><input type="range" id="saS" min="80" max="220" style="width:100%" oninput="sysCol()">
        <button class="btn" onclick="fetch('/api/sistem/rot')">Ekranı döndür</button>
    </div>
    <div class="settings-container"><h3>Yazılım güncelleme</h3>
        <div id="updInfo">…</div>
        <div style="display:flex;gap:6px;flex-wrap:wrap;margin:6px 0">
            <input id="updRepo" placeholder="GitHub: kullanıcı/depo" style="flex:1;min-width:160px">
            <button class="btn" onclick="updKaynak()">Kaydet</button>
        </div>
        <div style="display:flex;gap:6px;flex-wrap:wrap">
            <button class="btn" onclick="updDenetle()">Güncellemeyi denetle</button>
            <button class="btn" onclick="updGithub()">GitHub'dan güncelle</button>
        </div>
        <div style="margin-top:8px;display:flex;gap:6px;flex-wrap:wrap;align-items:center">
            <input type="file" id="updFile" accept=".bin">
            <button class="btn" onclick="updDosya()">Dosyadan güncelle</button>
        </div>
        <progress id="updPr" max="100" value="0" style="width:100%;display:none;margin-top:6px"></progress>
    </div>
    <div class="settings-container"><h3>Telegram</h3>
        <div id="tgState" style="margin-bottom:6px"></div>
        <div style="display:flex;gap:6px;flex-wrap:wrap">
            <input type="password" id="tgTok" class="compact-input" placeholder="Bot token">
            <input type="text" id="tgChat" class="compact-input" placeholder="Sohbet (chat) no">
            <button class="btn" onclick="sysTg()">Kaydet</button>
            <button class="btn off" onclick="handleTestTelegram()">Test mesajı</button>
        </div>
    </div>
    <div class="settings-container"><h3>Hızlı bağlantılar</h3>
        <button class="btn" onclick="route('/wifi')">WiFi ağları</button>
        <button class="btn" onclick="route('/weather')">Hava konumu</button>
        <button class="btn warning" onclick="restartHub()">Yeniden başlat</button>
    </div>`;
    fetch("/api/sistem").then(r => r.json()).then(j => {
        const up = j.up, s = document.getElementById("sysInfo");
        if (!s) return;
        s.innerHTML = `Ağ: ${escHtml(j.ssid)} &nbsp; Sinyal: ${j.rssi} dBm<br>IP: ${j.ip} &nbsp; Adres: crowhub.local<br>Sürüm: ${j.fw}<br>Çalışma: ${Math.floor(up / 3600)} sa ${Math.floor(up / 60) % 60} dk &nbsp; Bellek: ${Math.round(j.heap / 1024)} KB boş`;
        document.getElementById("blS").value = j.bl; document.getElementById("blL").textContent = "%" + j.bl;
        document.getElementById("ctS").value = j.ct; document.getElementById("ctL").textContent = "%" + j.ct;
        document.getElementById("dgS").value = j.dg; document.getElementById("dgL").textContent = "%" + j.dg;
        document.getElementById("saS").value = j.sa; document.getElementById("saL").textContent = "%" + j.sa;
        document.getElementById("tgState").innerHTML = j.tg ? `<span style="color:#8fe0a0">● Açık</span> (sohbet: ${escHtml(j.tg_chat)}). Değiştirmek için token'ı yeniden girin.` : `<span style="color:#ff9a9a">● Kapalı</span> — token ve sohbet no girin.`;
        document.getElementById("tgChat").value = j.tg_chat || "";
        updDurum();
    });
}
let blT = null;
function sysBl() {
    const v = document.getElementById("blS").value;
    document.getElementById("blL").textContent = "%" + v;
    clearTimeout(blT); blT = setTimeout(() => fetch("/api/sistem/set?bl=" + v), 300);
}
let updT = null;
function updDurum() {
    clearTimeout(updT);
    fetch("/api/guncelle").then(r => r.json()).then(j => {
        const i = document.getElementById("updInfo");
        if (!i) return;
        i.innerHTML = `Çalışan sürüm: <b>${escHtml(j.surum)}</b>${j.son ? " &nbsp; Son yayın: <b>" + escHtml(j.son) + "</b>" + (j.yeni ? " <span style='color:#2bff8a'>(yeni!)</span>" : "") : ""}<br>${escHtml(j.mesaj || "")}`;
        const rp = document.getElementById("updRepo");
        if (rp && !rp.value) rp.value = j.kaynak || "";
        const pr = document.getElementById("updPr");
        if (pr) { pr.style.display = j.durum == 2 ? "block" : "none"; pr.value = j.ilerleme; }
        if (j.durum == 1 || j.durum == 2 || j.durum == 3) updT = setTimeout(updDurum, 900);
    }).catch(() => { updT = setTimeout(updDurum, 2500); });
}
function updKaynak() {
    const v = document.getElementById("updRepo").value.trim();
    fetch("/api/guncelle/kaynak?repo=" + encodeURIComponent(v)).then(() => { toast("✓ Kaynak kaydedildi"); updDurum(); });
}
function updDenetle() { fetch("/api/guncelle/denetle").then(() => setTimeout(updDurum, 600)); }
function updGithub() {
    if (!confirm("Son yayın indirilip yüklensin mi? Cihaz yeniden başlar.")) return;
    fetch("/api/guncelle/github").then(() => setTimeout(updDurum, 600));
}
function updDosya() {
    const f = document.getElementById("updFile").files[0];
    if (!f) { toast("Önce .bin dosyası seçin", true); return; }
    if (!confirm(f.name + " yüklensin mi? Cihaz yeniden başlar.")) return;
    const x = new XMLHttpRequest(), fd = new FormData(), pr = document.getElementById("updPr");
    fd.append("firmware", f, f.name);
    pr.style.display = "block";
    x.upload.onprogress = e => { if (e.lengthComputable) pr.value = Math.round(e.loaded * 100 / e.total); };
    x.onload = () => { if (x.status == 200) toast("✓ Yüklendi, cihaz yeniden başlıyor…"); else toast(x.responseText || "Hata", true); };
    x.onerror = () => toast("Bağlantı kesildi (cihaz yeniden başlıyor olabilir)", true);
    x.open("POST", "/update");
    x.send(fd);
}
let colT = null;
function sysCol() {
    const c = document.getElementById("ctS").value, g = document.getElementById("dgS").value, a = document.getElementById("saS").value;
    document.getElementById("ctL").textContent = "%" + c; document.getElementById("dgL").textContent = "%" + g; document.getElementById("saL").textContent = "%" + a;
    clearTimeout(colT); colT = setTimeout(() => fetch("/api/sistem/set?ct=" + c + "&dg=" + g + "&sa=" + a), 300);
}
function sysTg() {
    const t = document.getElementById("tgTok").value.trim(), c = document.getElementById("tgChat").value.trim();
    if (!t) { toast("Bot token boş", true); return; }
    fetch("/api/telegram?token=" + encodeURIComponent(t) + "&chat=" + encodeURIComponent(c)).then(() => { toast("✓ Telegram kaydedildi"); document.getElementById("tgTok").value = ""; renderSistem(); });
}

// --- Donanim sayfasi icerigi: CrowPanel 7" HUB ---
const HW_FEATURES = [
    ["Dokunmatik ekran", "7\" 800×480 ekranda Ana (saat, hava, haber), Lambalar ve Ayar sayfaları; yatay / dikey kullanım."],
    ["WiFi kurulumu", "Bilgisayarsız: ekrandan ağ tara, seç, ekran klavyesiyle şifre gir. Bağlanamazsa kurulum ekranına döner; kayıtlı ağlar sırayla denenir."],
    ["Lamba / röle kontrolü", "ESP-01S röle düğümleri ve 4 kanallı LC röle modülü; ekrandan ve web'den aç/kapa, düğüm başına zamanlayıcı."],
    ["Otomasyon", "Gün doğumu / batımında, sabit saatte ya da hava kararınca lambaları açıp kapatır; rüzgar uyarısı."],
    ["Hava durumu", "Open-Meteo: sıcaklık, rüzgar, nem, 7 günlük tahmin; ekranda saate ve havaya göre canlı gökyüzü."],
    ["Uyarılar", "Hava (fırtına, kuvvetli rüzgar, yoğun yağış) ve deprem (AFAD) uyarı bandı."],
    ["Haberler", "Ekranın altında kayan haber şeridi."],
    ["Yazılım güncelleme", "Sistem sekmesinden GitHub'dan güncelleme; geliştirmede OTA."],
    ["Telegram", "Bot ve sohbet kimliği Ayar sayfasından girilir; açılış ve uyarı bildirimleri."],
    ["Diğer", "Notlar, olay kaydı, parlaklık / kontrast / canlılık ayarı, röle düğümleri için web'den firmware yükleyici, MQTT."],
];
const HW_USAGE = [
    "İlk açılış: ekranda WiFi ağını seç, ekran klavyesiyle şifreyi gir.",
    "Web arayüzü: aynı ağdan <b>http://crowhub.local</b> (ya da ekranda görünen IP).",
    "Konum ve Telegram: ekranda <b>Ayar</b> sayfasından ya da web'de <b>Oto-Hava</b> / <b>Sistem</b> sekmelerinden.",
    "Lambalar: <b>Cihazlar</b> sekmesinden röle düğümlerini ekle; ekranda <b>Lambalar</b> sayfasından kontrol et.",
    "Ekran ayarları ve güncelleme: web'de <b>Sistem</b> sekmesi.",
];
const HW_CHANGELOG = [
    ["7.4.0", "2026-10-20", ["Donanım sayfası: özellikler, kullanım, sürüm geçmişi, kart bilgisi, canlı durum"]],
    ["7.3.0", "2026-10-14", ["Yapay zeka efektleri kaldırıldı (flash %92)", "4 röleli düğüm desteği (2×2 lamba kartı, 4 röle zamanlayıcı)", "Cihazlar sayfasında Firmware Yükle bağlantısı"]],
    ["7.2.1", "2026-10-13", ["Röle düğümleri için 8 kart firmware'i ve hazır imajlar", "GitHub'dan güncelleme uçtan uca denendi", "Yayın imajlarından kişisel bilgiler temizlendi"]],
    ["Önceki", "2026-10-13", ["Canlı gökyüzü, hava kutuları, 7 günlük tahmin, kayan haber şeridi, nem", "Lambalar kompakt kartlar, kontrast / canlılık ayarı, uyarı bandı, kayıtlı WiFi ağları"]],
    ["7.0.0", "2026-10-12", ["ev-lamba Merkezi HUB arka ucu + LVGL dokunmatik arayüz (Ana / Lambalar / Ayar)", "Dokunmatik WiFi kurulumu; Telegram ve hava konumu ekrandan"]],
];
const HW_ROWS = [
    ["Kart", "Elecrow CrowPanel ESP32 HMI 7\" Basic, SKU DIS08070H, V3.0", "Ö"],
    ["Çip", "ESP32-S3 (QFN56) rev v0.2, 8 MB gömülü PSRAM", "Ö"],
    ["Flaş", "4 MB quad", "Ö"],
    ["Ekran", "800×480 RGB565 paralel, arka ışık IO2", "Ö"],
    ["Dokunmatik", "GT911 0x5D (I2C IO19/IO20), PCA9557 0x18 üzerinden RST/INT", "B"],
    ["Hoparlör", "I2S: SDIN IO17, LRCLK IO18, BCLK IO42 (kullanılmıyor)", "B"],
    ["Diğer", "TF kart yuvası (SPI IO10-13), USB-C (CH340, UART0)", "B"],
    ["Röle düğümleri", "ESP-01S / LC-ESP01-4R-12V — HTTP komut + UDP durum", "Ö"],
];
// --- DONANIM SAYFASI (ortak): ozellikler, kullanim, surum gecmisi, kart/modul tablosu, canli durum (/hw_info).
// Projeye ozel icerik: HW_FEATURES, HW_USAGE, HW_CHANGELOG, HW_ROWS (bu blogun ustunde).
// Ses karti yalniz /hw_info "ses_on" bildirirse, dokunmatik satiri yalniz "touch" bildirirse gorunur.
const RESET_TXT = {1:"güç verildi", 3:"yazılım", 4:"çökme (panic)", 5:"kesme bekçisi", 6:"görev bekçisi", 7:"bekçi", 8:"uyku", 9:"düşük gerilim"};
let infoTimer = null;

function renderInfo() {
    const feats = HW_FEATURES.map(f => `<li style="margin-bottom:6px"><b style="color:#03dac6">${f[0]}</b> — ${escHtml(f[1])}</li>`).join('');
    const usage = HW_USAGE.map(u => `<li style="margin-bottom:6px">${u}</li>`).join('');
    const changes = HW_CHANGELOG.map(([v, d, items]) => `<div style="margin-bottom:10px"><b style="color:#ff9800">${/^\d/.test(v) ? 'v' + v : v}</b>
        <small style="color:#888">${d}</small><ul style="padding-left:18px;margin:4px 0 0">${items.map(x => `<li>${escHtml(x)}</li>`).join('')}</ul></div>`).join('');
    const rows = HW_ROWS.map(r => `<tr><td style="color:#888;padding:4px 8px 4px 0;white-space:nowrap">${r[0]}</td>
        <td style="padding:4px 0">${escHtml(r[1])} <small style="color:${r[2]==='Ö'?'#03dac6':'#ff9800'}">[${r[2]}]</small></td></tr>`).join('');
    document.getElementById("content").innerHTML = `
    <div class="settings-container"><h3>✨ Özellikler</h3>
        <ul style="padding-left:18px;margin:0;font-size:14px;line-height:1.45">${feats}</ul></div>
    <div class="settings-container"><h3>📖 Kullanım</h3>
        <ol style="padding-left:18px;margin:0;font-size:14px;line-height:1.45">${usage}</ol></div>
    <div class="settings-container"><h3>📝 Sürüm geçmişi</h3>
        <div style="font-size:14px;line-height:1.45">${changes}</div></div>
    <div class="settings-container"><h3>📟 Kart ve modüller</h3>
        <table style="width:100%;border-collapse:collapse;font-size:14px">${rows}</table>
        <small style="color:#888">[Ö] kartta ölçüldü · [B] belgeden, kartta doğrulanmadı</small></div>
    <div class="settings-container"><h3>📊 Canlı durum</h3>
        <div id="hwLive" style="font-size:14px;line-height:1.7">Yükleniyor...</div></div>
    <div class="settings-container" id="sesCard" style="display:none"><h3>🔊 Ses</h3>
        <label><input type="checkbox" id="sesOn" onchange="sesSet('on', this.checked?1:0)"> Ses açık</label><br>
        <label><input type="checkbox" id="sesAcl" onchange="sesSet('acl', this.checked?1:0)"> Açılış melodisi</label><br>
        <label>Seviye <input type="range" id="sesLv" min="1" max="10" onchange="sesSet('lv', this.value)"> <span id="sesLvT"></span></label><br>
        <button class="btn" style="flex-grow:0;margin-top:8px" onclick="sesSet('test',1)">Deneme sesi</button></div>`;
    loadHw(true);
    if (infoTimer) clearInterval(infoTimer);
    infoTimer = setInterval(() => { if (location.pathname === "/info") loadHw(false); else { clearInterval(infoTimer); infoTimer = null; } }, 5000);
}

function loadHw(first) {
    fetch("/hw_info").then(r => r.json()).then(d => {
        const kb = v => (v / 1024).toFixed(0) + " KB", mb = v => (v / 1048576).toFixed(1) + " MB";
        const up = d.uptime, upTxt = `${Math.floor(up/86400)}g ${Math.floor(up%86400/3600)}s ${Math.floor(up%3600/60)}dk`;
        const dot = ok => `<span style="display:inline-block;width:10px;height:10px;border-radius:50%;margin-right:6px;background:${ok?'#4caf50':'#cf3030'};box-shadow:0 0 6px ${ok?'#4caf50':'#cf3030'}"></span>`;
        document.getElementById("hwLive").innerHTML = `
            Çip: ${escHtml(d.chip)} rev ${d.rev}, ${d.cores} çekirdek, ${d.mhz} MHz<br>
            Flaş: ${mb(d.flash)} · PSRAM: ${d.psram ? mb(d.psram) + (d.psram_free != null ? " (boş " + mb(d.psram_free) + ")" : "") : "yok"}<br>
            Uygulama: ${kb(d.sketch)} (boş yuva ${kb(d.sketch_free)})<br>
            Bellek: boş ${kb(d.heap)} · en büyük blok ${kb(d.heap_blk)} · en düşük ${kb(d.heap_min)}<br>
            Çalışma: ${upTxt} · son açılış: ${RESET_TXT[d.reset] || d.reset}<br>
            WiFi: ${escHtml(d.mac)} · ${d.rssi} dBm<br>
            ${d.touch !== undefined ? `Dokunmatik: ${dot(d.touch)}${d.touch ? "GT911 0x" + d.touch.toString(16).toUpperCase() : "bulunamadı"}<br>` : ''}
            Sürüm: ${escHtml(d.fw)} · SDK ${escHtml(d.sdk)}`;
        if (first && d.ses_on !== undefined) {
            document.getElementById("sesCard").style.display = "";
            document.getElementById("sesOn").checked = d.ses_on;
            document.getElementById("sesAcl").checked = d.ses_acl;
            document.getElementById("sesLv").value = d.ses_lv;
            document.getElementById("sesLvT").textContent = d.ses_lv;
        }
    }).catch(() => { document.getElementById("hwLive").textContent = "✗ Bilgi alınamadı"; });
}

function sesSet(k, v) {
    fetch(`/ses?${k}=${v}`).then(r => r.json()).then(j => {
        if (k === 'lv') document.getElementById("sesLvT").textContent = j.lv;
        if (k !== 'test') toast("✓ Ses ayarı kaydedildi");
    }).catch(() => toast("✗ Ses ayarı gönderilemedi", true));
}

// --- NAVİGASYON ---
function navigate() {
    const p = location.pathname;
    document.querySelectorAll('.nav a').forEach(a => a.className = (a.getAttribute('href') === p || (p === '/device' && a.getAttribute('href') === '/settings')) ? 'active' : '');
    if(p === "/info") {
        renderInfo();
    } else if(p === "/settings") {
        fetchDevices().then(renderSettings);
    } else if(p === "/log") {
        renderEvents();
    } else if(p === "/weather") {
        fetchDevices().then(renderWeather);
    } else if(p === "/notes") {
        renderNotes();
    } else if(p === "/wifi") {
        renderWifi();
    } else if(p === "/sistem") {
        renderSistem();
    } else if(p === "/device") {
        fetchDevices().then(renderDevice);
    } else if(p === "/lamps") {
        fetchDevices().then(() => {
            renderControl();
            update();
        });
    } else {
        fetchDevices().then(renderHome);
    }
}

// Sayfa Yüklendiğinde
window.onload = function() {
    window.onpopstate = navigate;
    navigate();
    connectWS();
    // Artık asıl güncellemeler WS push ile geliyor - bu sadece WS
    // koparsa (ev ağı kararsız olabilir) diye yedek/geç bir fallback.
    setInterval(update, 15000);
    setInterval(() => { if (location.pathname === '/log') renderEvents(); }, 10000);
    setInterval(refreshHome, 20000);
    skyLoop();
};
</script>
</body>
</html>
)rawliteral";