#include "ui_server.h"
#include "wifi_manager.h"
#include "diag.h"
#include <cmath>

extern void RGBSetShabbatPreview(bool enabled);
extern bool RGBGetShabbatPreview();

ESP8266WebServer UIServer::server(80);

// =============================================================================
// HTML pages stored in flash (PROGMEM) — eliminates per-request String of
// several KB and the resulting heap fragmentation that used to crash the
// device after ~24 hours of uptime.
// =============================================================================

static const char DASHBOARD_HTML[] PROGMEM =
    "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><title>Shabbat Alert</title><style>* { margin: 0; padding: 0; box-sizing: border-box; } body { font-family: Arial, sans-serif; background: #f5f5f5; padding: 20px; } .container { max-width: 600px; margin: 0 auto; background: white; border-radius: 10px; padding: 30px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); } h1 { color: #333; margin-bottom: 20px; text-align: center; } .status-card { background: #f8f9fa; border-radius: 8px; padding: 20px; margin: 15px 0; } .status-row { display: flex; justify-content: space-between; padding: 10px 0; border-bottom: 1px solid #e0e0e0; } .status-row:last-child { border-bottom: none; } .label { font-weight: 600; color: #666; } .value { color: #333; } .shabbat-on { color: #d32f2f; font-weight: bold; } .shabbat-off { color: #388e3c; font-weight: bold; } .countdown { font-size: 1.2em; color: #1976d2; } .nav { text-align: center; margin-top: 20px; } .nav a { display: inline-block; margin: 5px; padding: 10px 20px; background: #1976d2; color: white; text-decoration: none; border-radius: 5px; } .nav a:hover { background: #1565c0; } .refresh-btn { background: #4caf50; border: none; padding: 10px 20px; color: white; border-radius: 5px; cursor: pointer; } .refresh-btn:hover { background: #45a049; } .alarm-btn { background: #ff9800; border: none; padding: 10px 20px; color: white; border-radius: 5px; cursor: pointer; margin-left: 5px; } .alarm-btn:hover { background: #f57c00; } .bookmark { font-size: 0.9em; color: #666; margin-top: 10px; }</style></head><body><div class=\"container\"><h1>Shabbat Alert</h1><div class=\"bookmark\">You can open this page at <a href=\"http://" MDNS_HOSTNAME ".local\">http://" MDNS_HOSTNAME ".local</a></div><div id=\"status\"></div><div class=\"nav\"><a href=\"/settings\">Settings</a><a href=\"/diag\">Diag</a><button class=\"refresh-btn\" onclick=\"location.reload()\">Refresh</button><button class=\"alarm-btn\" onclick=\"testAlarm()\" id=\"alarmBtn\">Hear alarm</button><button class=\"alarm-btn\" onclick=\"togglePreview()\" id=\"previewBtn\">Preview Shabbat LEDs</button><span id=\"alarmFeedback\" style=\"margin-left:8px;\"></span></div></div><script>var currentRgbPreview=false;function testAlarm(){var btn=document.getElementById('alarmBtn');var fb=document.getElementById('alarmFeedback');btn.disabled=true;fb.textContent='Playing...';fetch('/api/test-alert',{method:'POST'}).then(function(){fb.textContent='Done';setTimeout(function(){fb.textContent='';btn.disabled=false;},1500);}).catch(function(e){fb.textContent='Error';btn.disabled=false;});}function setPreviewButtonState(){var btn=document.getElementById('previewBtn');if(!btn)return;btn.textContent=currentRgbPreview?'Stop Shabbat Preview':'Preview Shabbat LEDs';}function togglePreview(){var desired=!currentRgbPreview;var btn=document.getElementById('previewBtn');if(btn)btn.disabled=true;fetch('/api/rgb-preview',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({enabled:desired})}).then(function(r){return r.json();}).then(function(data){currentRgbPreview=!!data.preview;setPreviewButtonState();if(btn)btn.disabled=false;}).catch(function(e){if(btn)btn.disabled=false;console.error(e);});}function updateStatus(){fetch('/api/status').then(r=>r.json()).then(data=>{currentRgbPreview=!!data.rgb_preview;setPreviewButtonState();let html='<div class=\"status-card\">';if(data.next_event_title){var k=data.next_event_kind==='havdalah'?'havdalah':'candle lighting';var s=data.next_event_seconds_until||0;var d=Math.floor(s/86400),h=Math.floor((s%86400)/3600),m=Math.floor((s%3600)/60);var dur=(d>0?d+'d ':'')+h+'h '+m+'m';html+='<div class=\"status-row\" style=\"background:#fff8e1;padding:14px;margin:-20px -20px 10px -20px;border-radius:8px 8px 0 0;\"><span class=\"label\" style=\"font-size:1.05em;\">Next event:</span><span class=\"value\" style=\"font-weight:bold;\">'+data.next_event_title+' '+k+'<br><small style=\"color:#666;font-weight:normal;\">in '+dur+'</small></span></div>';}html+='<div class=\"status-row\"><span class=\"label\">Current Time'+(data.timezone?' ('+data.timezone+')':'')+':</span><span class=\"value\">'+data.datetime+(data.time_set?'':' (syncing...)')+'</span></div>';html+='<div class=\"status-row\"><span class=\"label\">Location:</span><span class=\"value\">'+data.city+'</span></div>';html+='<div class=\"status-row\"><span class=\"label\">Shabbat Status:</span><span class=\"value '+(data.shabbat_now?'shabbat-on':'shabbat-off')+'\">'+(data.shabbat_now?'ON':'OFF')+'</span></div>';html+='<div class=\"status-row\"><span class=\"label\">  Next Candle Lighting(sunset):</span><span class=\"value\">'+data.next_candles+'</span></div>';html+='<div class=\"status-row\"><span class=\"label\">Next Havdalah:</span><span class=\"value\">'+data.next_havdalah+'</span></div>';html+='</div>';fetch('/api/schedule').then(r=>r.json()).then(sched=>{if(sched.events&&sched.events.length>0){html+='<div class=\"status-card\"><h3>Upcoming Alerts</h3>';sched.events.slice(0,5).forEach(e=>{let minutes=Math.floor(e.seconds_until/60);let hours=Math.floor(minutes/60);let days=Math.floor(hours/24);let timeStr=days>0?days+'d ':'';timeStr+=(hours%24)+'h '+(minutes%60)+'m';html+='<div class=\"status-row\"><span class=\"label\">'+e.type+':</span><span class=\"countdown\">'+e.formatted+'<br><small style=\"color:#888\">in '+timeStr+'</small></span></div>';});html+='</div>';}document.getElementById('status').innerHTML=html;});}).catch(e=>console.error(e));}updateStatus();setInterval(updateStatus,30000);</script></body></html>";

static const char SETUP_HTML[] PROGMEM =
    "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><title>Set city and times - Shabbat Alert</title><style>body{font-family:Arial,sans-serif;max-width:400px;margin:50px auto;padding:20px;}input,select{width:100%;padding:10px;margin:5px 0;box-sizing:border-box;}button{width:100%;padding:12px;background:#4CAF50;color:white;border:none;cursor:pointer;margin-top:10px;}button:hover{background:#45a049;}label{display:block;margin-top:10px;}.search-btn{width:auto;padding:5px 15px;margin-top:5px;}#city_result{margin-top:10px;padding:10px;background:#e8f5e9;border-radius:5px;display:none;}</style></head><body><h2>Set city and times</h2><p>Enter your location and candle/havdalah preferences.</p><form action=\"/setup\" method=\"POST\"><label>City Name:</label><input type=\"text\" name=\"city\" id=\"city_input\" placeholder=\"e.g., Jerusalem\" required><button type=\"button\" class=\"search-btn\" onclick=\"searchCity()\">Search City</button><div id=\"city_result\"></div><label>Latitude:</label><input type=\"number\" step=\"0.000001\" name=\"lat\" id=\"lat_input\" required><label>Longitude:</label><input type=\"number\" step=\"0.000001\" name=\"lon\" id=\"lon_input\" required><label>Timezone:</label><input type=\"text\" name=\"tz\" id=\"tz_input\" placeholder=\"Asia/Jerusalem\" value=\"Asia/Jerusalem\" required><label>Candle Lighting Offset (minutes):</label><input type=\"number\" name=\"candle_offset\" value=\"18\" min=\"0\" max=\"60\"><label>Havdalah:</label><select name=\"havdalah_mode\" id=\"havdalah_mode\"><option value=\"M\">Nightfall (8.5 deg below horizon)</option><option value=\"m50\">50 min after sunset</option><option value=\"m42\">42 min</option><option value=\"m72\">72 min</option><option value=\"degrees\">Custom degrees</option></select><label id=\"deg_label\" style=\"display:none;\">Havdalah degrees:</label><input type=\"number\" step=\"0.1\" name=\"havdalah_degrees\" id=\"havdalah_degrees\" placeholder=\"8.5\" min=\"4\" max=\"12\" style=\"display:none;\"><button type=\"submit\">Save</button></form><script>function searchCity(){var city=document.getElementById('city_input').value.trim();if(!city){alert('Enter a city name');return;}var r=document.getElementById('city_result');r.style.display='block';r.innerHTML='Searching...';fetch('/api/geocode',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({city:city})}).then(function(x){return x.text();}).then(function(t){try{var d=JSON.parse(t);if(d.error){r.innerHTML='<span style=\"color:red;\">'+d.error+'</span>';}else{document.getElementById('lat_input').value=d.latitude||'';document.getElementById('lon_input').value=d.longitude||'';if(d.timezone)document.getElementById('tz_input').value=d.timezone;r.innerHTML='<span style=\"color:green;\">Found: '+d.city+', '+d.country+'</span>';}}catch(e){r.innerHTML='Parse error';}}).catch(function(){r.innerHTML='Error';});}document.getElementById('havdalah_mode').onchange=function(){var v=document.getElementById('havdalah_mode').value;var show=v==='degrees';document.getElementById('deg_label').style.display=show?'block':'none';document.getElementById('havdalah_degrees').style.display=show?'block':'none';};</script></body></html>";

static const char SETTINGS_HTML[] PROGMEM =
    "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><title>Settings - Shabbat Alert</title><style>* { margin: 0; padding: 0; box-sizing: border-box; } body { font-family: Arial, sans-serif; background: #f5f5f5; padding: 20px; } .container { max-width: 600px; margin: 0 auto; background: white; border-radius: 10px; padding: 30px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); } h1 { color: #333; margin-bottom: 20px; } .form-group { margin: 15px 0; } label { display: block; margin-bottom: 5px; font-weight: 600; color: #666; } input, select { width: 100%; padding: 10px; border: 1px solid #ddd; border-radius: 5px; font-size: 14px; } button { padding: 12px; background: #1976d2; color: white; border: none; border-radius: 5px; cursor: pointer; font-size: 16px; margin-top: 10px; } button.search-btn { width: auto; padding: 5px 15px; background: #4caf50; margin-top: 5px; } button:hover { background: #1565c0; } button.search-btn:hover { background: #45a049; } .nav { text-align: center; margin-top: 20px; } .nav a { display: inline-block; margin: 5px; padding: 10px 20px; background: #666; color: white; text-decoration: none; border-radius: 5px; } #city_result { margin-top: 10px; padding: 10px; background: #e8f5e9; border-radius: 5px; display: none; } .alarm-btn { width: auto; padding: 8px 16px; background: #ff9800; margin-top: 5px; } .alarm-btn:hover { background: #f57c00; }</style></head><body><div class=\"container\"><h1>Settings</h1><p id=\"settingsLoadStatus\" style=\"color:#666;margin-bottom:10px;\"></p><form id=\"settingsForm\"><div class=\"form-group\"><label>WiFi SSID:</label><input type=\"text\" name=\"wifi_ssid\" id=\"wifi_ssid\"></div><div class=\"form-group\"><label>WiFi Password:</label><input type=\"password\" name=\"wifi_password\" id=\"wifi_password\"></div><div class=\"form-group\"><label>City Name:</label><input type=\"text\" name=\"city\" id=\"city\"><button type=\"button\" class=\"search-btn\" onclick=\"searchCity()\">Search City</button><div id=\"city_result\"></div></div><div class=\"form-group\"><label>Latitude:</label><input type=\"number\" step=\"0.000001\" name=\"latitude\" id=\"latitude\"></div><div class=\"form-group\"><label>Longitude:</label><input type=\"number\" step=\"0.000001\" name=\"longitude\" id=\"longitude\"></div><div class=\"form-group\"><label>Timezone:</label><input type=\"text\" name=\"timezone\" id=\"timezone\" placeholder=\"Asia/Jerusalem\"></div><div class=\"form-group\"><label>Alert before candle lighting:</label><div style=\"display:flex;gap:15px;margin-top:5px;\"><label style=\"font-weight:normal;display:flex;align-items:center;gap:5px;\"><input type=\"checkbox\" name=\"candle_18\" id=\"candle_18\" style=\"width:auto;\"> 18 min</label><label style=\"font-weight:normal;display:flex;align-items:center;gap:5px;\"><input type=\"checkbox\" name=\"candle_30\" id=\"candle_30\" style=\"width:auto;\"> 30 min</label><label style=\"font-weight:normal;display:flex;align-items:center;gap:5px;\"><input type=\"checkbox\" name=\"candle_45\" id=\"candle_45\" style=\"width:auto;\"> 45 min</label></div></div><div class=\"form-group\"><label>Havdalah:</label><select name=\"havdalah_mode\" id=\"havdalah_mode\"><option value=\"M\">Nightfall (8.5 deg below horizon)</option><option value=\"m50\">50 min after sunset</option><option value=\"m42\">42 min after sunset</option><option value=\"m72\">72 min after sunset</option><option value=\"degrees\">Custom degrees</option></select></div><div class=\"form-group\" id=\"havdalah_degrees_group\" style=\"display:none;\"><label>Havdalah degrees below horizon:</label><input type=\"number\" step=\"0.1\" name=\"havdalah_degrees\" id=\"havdalah_degrees\" min=\"4\" max=\"12\" placeholder=\"8.5\"></div><div class=\"form-group\"><label>Hebcal API max attempts (1-5):</label><input type=\"number\" name=\"hebcal_max_attempts\" id=\"hebcal_max_attempts\" min=\"1\" max=\"5\" placeholder=\"2\"></div><div class=\"form-group\"><h3>Alarm</h3><label style=\"font-weight:normal;display:flex;align-items:center;gap:8px;margin:8px 0;\"><input type=\"checkbox\" id=\"alert_enabled\" style=\"width:auto;\"> Alerts enabled (master switch — must be ON for any sound)</label><label>Alert sound:</label><select name=\"ringtone\" id=\"ringtone\"><option value=\"pinky\">Pinky and the Brain</option><option value=\"star_wars\">Star Wars</option><option value=\"mozart\">Mozart</option><option value=\"under_the_sea\">Under the Sea</option><option value=\"spiderman\">Spiderman</option><option value=\"mario\">Mario</option><option value=\"pink_panther\">Pink Panther</option><option value=\"hava_nagila\">Hava Nagila</option><option value=\"random\">Random</option></select><label>Alert duration (ms):</label><input type=\"number\" name=\"alert_duration_ms\" id=\"alert_duration_ms\" min=\"1000\" placeholder=\"2000\"></div><button type=\"submit\" style=\"width:100%;\">Save Settings</button></form><div class=\"form-group\"><label>Test alarm:</label><button type=\"button\" class=\"alarm-btn\" onclick=\"testAlarm()\" id=\"alarmBtn\">Hear alarm</button><span id=\"alarmFeedback\" style=\"margin-left:8px;\"></span></div><div class=\"nav\"><a href=\"/\">Dashboard</a><a href=\"/diag\">Diag</a></div></div><script>function testAlarm(){var btn=document.getElementById('alarmBtn');var fb=document.getElementById('alarmFeedback');if(btn){btn.disabled=true;}if(fb){fb.textContent='Playing...';}fetch('/api/test-alert',{method:'POST'}).then(function(){if(fb){fb.textContent='Done';}setTimeout(function(){if(fb){fb.textContent='';}if(btn){btn.disabled=false;}},1500);}).catch(function(e){if(fb){fb.textContent='Error';}if(btn){btn.disabled=false;}});}function searchCity(){var city=document.getElementById('city').value.trim();if(!city){alert('Please enter a city name');return;}var resultDiv=document.getElementById('city_result');resultDiv.style.display='block';resultDiv.innerHTML='Searching...';fetch('/api/geocode',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({city:city})}).then(function(r){if(!r.ok){throw new Error('HTTP error: '+r.status);}return r.text();}).then(function(text){try{var data=JSON.parse(text);if(data.error){resultDiv.innerHTML='<span style=\"color:red;\">Error: '+data.error+'</span>';}else{document.getElementById('latitude').value=data.latitude||'';document.getElementById('longitude').value=data.longitude||'';if(data.timezone){document.getElementById('timezone').value=data.timezone;}resultDiv.innerHTML='<span style=\"color:green;\">Found: '+data.city+', '+data.country+' ('+data.latitude+', '+data.longitude+')</span>';}}catch(e){resultDiv.innerHTML='<span style=\"color:red;\">Parse error: '+e.message+'</span>';}}).catch(function(e){resultDiv.innerHTML='<span style=\"color:red;\">Error: '+e.message+'</span>';});}fetch('/api/status').then(function(r){if(!r.ok)throw new Error(r.status);return r.text();}).then(function(text){try{var data=JSON.parse(text);document.getElementById('city').value=data.city||'';document.getElementById('latitude').value=(data.latitude!=null&&data.latitude!==''&&!isNaN(Number(data.latitude)))?String(data.latitude):'';document.getElementById('longitude').value=(data.longitude!=null&&data.longitude!==''&&!isNaN(Number(data.longitude)))?String(data.longitude):'';if(data.timezone){document.getElementById('timezone').value=data.timezone||'Asia/Jerusalem';}if(data.candle_alerts!=null){var ca=data.candle_alerts;document.getElementById('candle_18').checked=!!(ca&1);document.getElementById('candle_30').checked=!!(ca&2);document.getElementById('candle_45').checked=!!(ca&4);}document.getElementById('alert_enabled').checked=(data.alert_enabled!==false);if(data.hebcal_max_attempts!=null){document.getElementById('hebcal_max_attempts').value=data.hebcal_max_attempts;}if(data.wifi_ssid!=null){document.getElementById('wifi_ssid').value=data.wifi_ssid;}if(data.alert_duration_ms!=null){document.getElementById('alert_duration_ms').value=data.alert_duration_ms;}if(data.ringtone!=null){var rt=document.getElementById('ringtone');if(rt)rt.value=data.ringtone;}if(data.havdalah_mode!=null){var m=data.havdalah_mode;var min=data.havdalah_minutes;var sel=document.getElementById('havdalah_mode');if(m=='M')sel.value='M';else if(m=='m'&&min==50)sel.value='m50';else if(m=='m'&&min==42)sel.value='m42';else if(m=='m'&&min==72)sel.value='m72';else if(m=='degrees'){sel.value='degrees';document.getElementById('havdalah_degrees_group').style.display='block';if(data.havdalah_degrees!=null)document.getElementById('havdalah_degrees').value=data.havdalah_degrees;}else sel.value='M';}document.getElementById('havdalah_mode').onchange=function(){var v=document.getElementById('havdalah_mode').value;document.getElementById('havdalah_degrees_group').style.display=(v=='degrees')?'block':'none';};var statusEl=document.getElementById('settingsLoadStatus');if(statusEl)statusEl.textContent='Current settings loaded.';}catch(e){var statusEl=document.getElementById('settingsLoadStatus');if(statusEl)statusEl.textContent='Could not load current settings.';console.error('Status parse error:',e);}}).catch(function(e){var statusEl=document.getElementById('settingsLoadStatus');if(statusEl)statusEl.textContent='Could not load current settings.';console.error('Status fetch error:',e);});document.getElementById('settingsForm').addEventListener('submit',function(e){e.preventDefault();var formData=new FormData(e.target);var json={};formData.forEach(function(value,key){json[key]=value;});var ca=0;if(document.getElementById('candle_18').checked)ca|=1;if(document.getElementById('candle_30').checked)ca|=2;if(document.getElementById('candle_45').checked)ca|=4;json.candle_alerts=ca;delete json.candle_18;delete json.candle_30;delete json.candle_45;json.alert_enabled=document.getElementById('alert_enabled').checked;fetch('/api/settings',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(json)}).then(function(r){return r.text();}).then(function(text){try{var data=JSON.parse(text);alert('Settings saved!');}catch(e){alert('Error parsing response');}}).catch(function(e){alert('Error: '+e.message);});});</script></body></html>";

static const char LOW_MEM_DASHBOARD[] PROGMEM =
    "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"></head><body><p>Dashboard temporarily unavailable (low memory).</p><p><a href=\"/\">Try again</a></p></body></html>";

static const char LOW_MEM_SETTINGS[] PROGMEM =
    "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"></head><body><p>Settings temporarily unavailable (low memory).</p><p><a href=\"/settings\">Try again</a></p></body></html>";

static const char SAVED_REDIRECT_HTML[] PROGMEM =
    "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><meta http-equiv=\"refresh\" content=\"2;url=/\"><title>Saved</title><style>body{font-family:Arial,sans-serif;max-width:400px;margin:50px auto;padding:20px;text-align:center;}</style></head><body><h2>Settings saved!</h2><p>Redirecting to dashboard...</p><p><a href=\"/\">Click here</a> if not redirected.</p></body></html>";

// Shown when a phone connects to the rescue/setup AP — the device has no
// useful dashboard data without WiFi, so we send the credentials form
// straight away instead of the dashboard.
static const char WIFI_RESCUE_HTML[] PROGMEM =
    "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><title>Shabbat Alert - WiFi setup</title>"
    "<style>body{font-family:Arial,sans-serif;max-width:420px;margin:40px auto;padding:20px;background:#f5f5f5;}"
    ".card{background:#fff;border-radius:10px;padding:25px;box-shadow:0 2px 10px rgba(0,0,0,.08);}"
    "h2{margin:0 0 6px;color:#333;}p.lead{color:#666;margin:0 0 18px;font-size:.95em;}"
    "label{display:block;margin-top:14px;font-weight:600;color:#555;font-size:.9em;}"
    "input{width:100%;padding:10px;margin-top:5px;border:1px solid #ddd;border-radius:5px;box-sizing:border-box;font-size:14px;}"
    "button{width:100%;padding:12px;background:#4CAF50;color:#fff;border:none;border-radius:5px;cursor:pointer;margin-top:18px;font-size:15px;}"
    "button:hover{background:#45a049;}small{color:#888;display:block;margin-top:14px;font-size:.8em;text-align:center;}"
    "</style></head><body><div class=\"card\">"
    "<h2>WiFi setup</h2>"
    "<p class=\"lead\">The device couldn't reach your home WiFi. Enter your network name and password to reconnect.</p>"
    "<form action=\"/wifi\" method=\"POST\">"
    "<label>WiFi network name (SSID)</label><input type=\"text\" name=\"ssid\" required placeholder=\"e.g. MyHomeWiFi\" autocomplete=\"off\">"
    "<label>WiFi password</label><input type=\"password\" name=\"password\" placeholder=\"Leave blank for open network\" autocomplete=\"off\">"
    "<button type=\"submit\">Save and reconnect</button>"
    "</form>"
    "<small>The device will restart and try to join the new network.</small>"
    "</div></body></html>";

static const char WIFI_SAVED_HTML[] PROGMEM =
    "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><title>WiFi saved</title>"
    "<style>body{font-family:Arial,sans-serif;max-width:400px;margin:60px auto;padding:20px;text-align:center;background:#f5f5f5;}"
    ".card{background:#fff;border-radius:10px;padding:30px;box-shadow:0 2px 10px rgba(0,0,0,.08);}</style></head>"
    "<body><div class=\"card\"><h2>WiFi saved</h2><p>The device is restarting and will connect to your network.</p>"
    "<p style=\"color:#666;font-size:.9em;\">You can close this page.</p></div></body></html>";

// Helper that sets standard no-cache headers and streams a PROGMEM page.
static void sendNoCachePage_P(ESP8266WebServer& srv, const char* progmemBody) {
    srv.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    srv.sendHeader("Pragma", "no-cache");
    srv.sendHeader("Expires", "0");
    srv.send_P(200, PSTR("text/html"), progmemBody);
}

// True when the HTTP request arrived via the soft-AP interface (i.e. phone
// connected to the rescue/setup AP) rather than via the home network.
// soft-AP DHCP hands out 192.168.4.0/24 by default on the ESP8266.
static bool requestFromAPInterface(ESP8266WebServer& srv) {
    IPAddress ip = srv.client().remoteIP();
    return ip[0] == 192 && ip[1] == 168 && ip[2] == 4;
}

void UIServer::init() {
    // Web pages
    server.on("/", handleRoot);
    server.on("/setup", HTTP_GET, handleSetupPage);
    server.on("/setup", HTTP_POST, handleSetupSubmit);
    server.on("/wifi", HTTP_POST, handleWiFiSubmit);
    server.on("/settings", handleSettings);
    server.on("/diag", handleDiag);
    
    // API endpoints
    server.on("/api/status", HTTP_GET, handleAPIStatus);
    server.on("/api/schedule", HTTP_GET, handleAPISchedule);
    server.on("/api/settings", HTTP_POST, handleAPISettings);
    server.on("/api/geocode", HTTP_ANY, handleAPIGeocode);
    server.on("/api/test-alert", HTTP_POST, handleAPITestAlert);
    server.on("/api/rgb-preview", HTTP_POST, handleAPIRgbPreview);
    server.onNotFound(handleAPINotFound);
    
    server.begin();
    LOG("Web server started");
}

void UIServer::handle() {
    server.handleClient();
}

void UIServer::handleRoot() {
    // If a phone connected to the rescue/setup AP, the home WiFi is unreachable
    // — show the WiFi credentials form straight away instead of a dashboard
    // that would just display "syncing..." with no real data.
    bool apActive = WiFiManager::isSetupAPMode() || WiFiManager::isAPRescueActive();
    if (apActive && requestFromAPInterface(server)) {
        sendNoCachePage_P(server, WIFI_RESCUE_HTML);
        return;
    }
    if (WiFiManager::isConnected() && !Storage::isConfigured()) {
        server.sendHeader("Location", "/setup", true);
        server.send(302, "text/plain", "");
        return;
    }
    if (ESP.getFreeHeap() < 8000) {
        server.sendHeader("Cache-Control", "no-cache");
        server.send_P(200, PSTR("text/html"), LOW_MEM_DASHBOARD);
        return;
    }
    sendNoCachePage_P(server, DASHBOARD_HTML);
}

void UIServer::handleWiFiSubmit() {
    if (!server.hasArg("ssid") || server.arg("ssid").length() == 0) {
        server.send(400, "text/plain", "Missing WiFi SSID");
        return;
    }
    Storage::setWiFiSSID(server.arg("ssid"));
    Storage::setWiFiPassword(server.arg("password"));
    LOGF("WiFi credentials updated via AP form: %s", server.arg("ssid").c_str());
    server.send_P(200, PSTR("text/html"), WIFI_SAVED_HTML);
    delay(1000);   // give the browser time to receive the response
    ESP.restart();
}

void UIServer::handleSettings() {
    if (!checkAuth()) {
        server.send(401, "text/plain", "Unauthorized");
        return;
    }
    if (ESP.getFreeHeap() < 8000) {
        server.sendHeader("Cache-Control", "no-cache");
        server.send_P(200, PSTR("text/html"), LOW_MEM_SETTINGS);
        return;
    }
    sendNoCachePage_P(server, SETTINGS_HTML);
}

// Plain-text diagnostic dump — designed to be readable on a phone after Shabbat
// without JavaScript or styling.  Top section is live state, bottom is the
// persistent ring buffer (boots/NTP/Hebcal/alerts).
void UIServer::handleDiag() {
    String body;
    body.reserve(2048);

    body += "=== Shabbat Alert Diagnostics ===\n";

    // Live state
    body += "time: ";
    body += TimeSync::getFormattedDateTime();
    body += " (set=";
    body += TimeSync::isTimeSet() ? "1" : "0";
    body += ")\n";

    {
        char line[80];
        snprintf(line, sizeof(line), "heap: free=%u maxBlock=%u\n",
                 (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxFreeBlockSize());
        body += line;
    }

    body += "wifi: ";
    body += WiFiManager::isConnected() ? "connected" : "DISCONNECTED";
    body += " ip=";
    body += WiFiManager::getIP();
    body += "\n";

    body += "alert_enabled: ";
    body += Storage::getAlertEnabled() ? "yes" : "NO (alerts will not fire!)";
    body += "\n";

    {
        char line[64];
        int ca = Storage::getCandleAlerts();
        snprintf(line, sizeof(line), "candle_alerts: 0x%x  (18min=%d 30min=%d 45min=%d)\n",
                 ca, (ca & 1) ? 1 : 0, (ca & 2) ? 1 : 0, (ca & 4) ? 1 : 0);
        body += line;
    }

    {
        int evCount = 0;
        AlertEvent* ev = Scheduler::getEvents(evCount);
        char line[80];
        snprintf(line, sizeof(line), "alerts: %d  fails=%d  lastRefresh=%lums ago\n",
                 evCount,
                 Scheduler::getConsecutiveFailCount(),
                 Scheduler::getLastRefreshMillisAgo());
        body += line;

        long tzOff = TimeSync::getTimezoneOffsetSeconds();
        for (int i = 0; i < evCount; i++) {
            time_t local = ev[i].timestamp + tzOff;
            struct tm lt;
            gmtime_r(&local, &lt);
            char tbuf[20];
            strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", &lt);
            char el[120];
            snprintf(el, sizeof(el), "  alert[%d] %-11s @ %s  [%s]%s\n",
                     i, alertKindToString(ev[i].kind), tbuf,
                     ev[i].title[0] ? ev[i].title : "?",
                     ev[i].triggered ? " (fired)" : "");
            body += el;
        }

        // Also dump the underlying scheduled candle/havdalah list — useful for
        // confirming back-to-back yom-tov + Shabbat was picked up correctly.
        body += "scheduled events:\n";
        const ShabbatEvent* nx = Scheduler::getNextScheduledEvent();
        if (nx) {
            char nbuf[120];
            time_t now = TimeSync::getNow();
            long secs = (long)(nx->timestamp - now);
            long hours = secs / 3600;
            long mins  = (secs % 3600) / 60;
            snprintf(nbuf, sizeof(nbuf), "  next: %s %s in %ldh %ldm\n",
                     nx->title[0] ? nx->title : "?",
                     (nx->kind == ShabbatKind::Candles) ? "candle" : "havdalah",
                     hours, mins);
            body += nbuf;
        }
    }

    body += "\n=== Event log (oldest first) ===\n";
    String log;
    Diag::getEntriesText(log);
    body += log;

    server.sendHeader("Cache-Control", "no-cache");
    server.send(200, "text/plain", body);
}

void UIServer::handleSetupPage() {
    server.send_P(200, PSTR("text/html"), SETUP_HTML);
}

void UIServer::handleSetupSubmit() {
    if (!server.hasArg("city") || !server.hasArg("lat") || !server.hasArg("lon")) {
        server.send(400, "text/plain", "Missing required fields (city, lat, lon)");
        return;
    }
    Storage::beginBatch();
    Storage::setCityName(server.arg("city"));
    Storage::setLatitude(server.arg("lat").toFloat());
    Storage::setLongitude(server.arg("lon").toFloat());
    Storage::setTimezone(server.arg("tz").length() > 0 ? server.arg("tz") : DEFAULT_TIMEZONE);
    int candleOffset = server.arg("candle_offset").toInt();
    Storage::setCandleOffset((candleOffset >= 0 && candleOffset <= 60) ? candleOffset : DEFAULT_CANDLE_OFFSET);
    String havdalahMode = server.arg("havdalah_mode");
    if (havdalahMode == "m50") {
        Storage::setHavdalahMode("m");
        Storage::setHavdalahMinutes(50);
    } else if (havdalahMode == "m42") {
        Storage::setHavdalahMode("m");
        Storage::setHavdalahMinutes(42);
    } else if (havdalahMode == "m72") {
        Storage::setHavdalahMode("m");
        Storage::setHavdalahMinutes(72);
    } else if (havdalahMode == "degrees") {
        Storage::setHavdalahMode("degrees");
        float deg = server.arg("havdalah_degrees").toFloat();
        Storage::setHavdalahDegrees(deg > 0.0f ? deg : DEFAULT_HAVDALAH_DEGREES);
    } else {
        Storage::setHavdalahMode(havdalahMode.length() > 0 ? havdalahMode : "M");
    }
    Storage::endBatch();
    Scheduler::requestRefresh();
    server.send_P(200, PSTR("text/html"), SAVED_REDIRECT_HTML);
}

void UIServer::handleAPIStatus() {
    JsonDocument doc;

    doc["time"] = TimeSync::getFormattedTime();
    doc["datetime"] = TimeSync::getFormattedDateTime();
    doc["time_set"] = TimeSync::isTimeSet();
    String tz = Storage::getTimezone();
    if (tz.length() == 0 || tz == "UTC") tz = DEFAULT_TIMEZONE;
    doc["timezone"] = tz;
    doc["city"] = Storage::getCityName();
    float lat = Storage::getLatitude();
    float lon = Storage::getLongitude();
    if (std::isfinite(lat)) doc["latitude"] = lat;
    if (std::isfinite(lon)) doc["longitude"] = lon;
    doc["shabbat_now"] = HebcalClient::getCachedShabbatNow();
    if (std::isnan(lat) || std::isnan(lon) || !std::isfinite(lat) || !std::isfinite(lon)) {
        doc["next_candles"] = "Set location in Settings";
        doc["next_havdalah"] = "Set location in Settings";
    } else {
        doc["next_candles"] = HebcalClient::getCachedNextCandleLighting();
        doc["next_havdalah"] = HebcalClient::getCachedNextHavdalah();
    }
    doc["havdalah_mode"] = Storage::getHavdalahMode();
    doc["havdalah_minutes"] = Storage::getHavdalahMinutes();
    doc["havdalah_degrees"] = Storage::getHavdalahDegrees();
    doc["candle_offset"] = Storage::getCandleOffset();
    doc["candle_alerts"] = Storage::getCandleAlerts();
    doc["alert_enabled"] = Storage::getAlertEnabled();
    doc["alert_duration_ms"] = Storage::getAlertDurationMs();
    doc["ringtone"] = Storage::getRingtone();
    doc["wifi_ssid"] = Storage::getWiFiSSID();
    doc["hebcal_max_attempts"] = Storage::getHebcalMaxAttempts();
    doc["rgb_preview"] = RGBGetShabbatPreview();
    // Diagnostic fields (used by /diag and the dashboard health badge)
    doc["free_heap"] = (unsigned)ESP.getFreeHeap();
    doc["max_block"] = (unsigned)ESP.getMaxFreeBlockSize();
    {
        int evCount = 0;
        (void)Scheduler::getEvents(evCount);
        doc["events_count"] = evCount;
    }
    doc["hebcal_fail_count"] = Scheduler::getConsecutiveFailCount();
    doc["hebcal_last_refresh_ms_ago"] = Scheduler::getLastRefreshMillisAgo();

    // Next scheduled event (any kind) with its holiday title.  Used by the
    // dashboard "Next event" badge so the user sees "Shavuot" vs "Shabbat"
    // at a glance.
    const ShabbatEvent* nx = Scheduler::getNextScheduledEvent();
    if (nx) {
        doc["next_event_title"] = nx->title;
        doc["next_event_kind"]  = (nx->kind == ShabbatKind::Candles) ? "candles" : "havdalah";
        doc["next_event_timestamp"] = (unsigned long)nx->timestamp;
        time_t now = TimeSync::getNow();
        doc["next_event_seconds_until"] = (long)(nx->timestamp - now);
    }

    if (doc.overflowed()) {
        server.send_P(200, PSTR("application/json"),
            PSTR("{\"error\":\"overflow\",\"time\":\"\",\"datetime\":\"\",\"city\":\"\",\"timezone\":\"Asia/Jerusalem\",\"candle_offset\":18,\"hebcal_max_attempts\":2,\"next_candles\":\"Set location in Settings\",\"next_havdalah\":\"Set location in Settings\"}"));
        return;
    }
    char buf[1280];
    size_t n = serializeJson(doc, buf, sizeof(buf));
    if (n == 0 || n >= sizeof(buf)) {
        server.send(500, "application/json", "{\"error\":\"serialize\"}");
        return;
    }
    server.setContentLength(n);
    server.send(200, "application/json", "");
    server.sendContent(buf, n);
}

void UIServer::handleAPISchedule() {
    int count = 0;
    AlertEvent* events = Scheduler::getUpcomingEvents(count);

    JsonDocument doc;
    JsonArray eventsArray = doc["events"].to<JsonArray>();
    time_t now = TimeSync::getNow();
    long tzOff = TimeSync::getTimezoneOffsetSeconds();
    // Return all upcoming alerts (capped at 10 by getUpcomingEvents).  The old
    // version filtered to one candle + one havdalah, which hid the 30-min and
    // 18-min reminders from the dashboard's "Upcoming Alerts" list.
    for (int i = 0; i < count; i++) {
        JsonObject event = eventsArray.add<JsonObject>();
        event["timestamp"] = events[i].timestamp;
        event["type"] = alertKindToString(events[i].kind);
        event["seconds_until"] = (unsigned long)(events[i].timestamp - now);
        time_t local = events[i].timestamp + tzOff;
        struct tm* tm = gmtime(&local);
        char buffer[30];
        if (tm) strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", tm);
        else buffer[0] = '\0';
        event["formatted"] = buffer;
    }
    char out[512];
    size_t n = serializeJson(doc, out, sizeof(out));
    if (n == 0 || n >= sizeof(out)) {
        server.send(500, "application/json", "{\"error\":\"serialize\"}");
        return;
    }
    server.setContentLength(n);
    server.send(200, "application/json", "");
    server.sendContent(out, n);
}

void UIServer::handleAPISettings() {
    if (!checkAuth()) {
        server.send(401, "application/json", "{\"error\":\"Unauthorized\"}");
        return;
    }
    
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"No data\"}");
        return;
    }
    
    String body = server.arg("plain");
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);

    if (error) {
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }

    // Bundle every setter below into one flash erase instead of ~15.
    Storage::beginBatch();

    if (doc["wifi_ssid"].is<String>()) {
        String newSsid = doc["wifi_ssid"].as<String>();
        // Refuse to overwrite a stored SSID with an empty one — that's almost
        // always an accidental form-field clear, not an intentional wipe.  Use
        // the physical reset button (10s hold) to clear credentials on purpose.
        if (newSsid.length() > 0) {
            String oldSsid = Storage::getWiFiSSID();
            if (oldSsid != newSsid) {
                Diag::log("wifi ssid changed");
            }
            Storage::setWiFiSSID(newSsid);
        } else {
            LOG("Settings POST: ignoring empty wifi_ssid (use reset button to clear)");
        }
    }
    if (doc["wifi_password"].is<String>()) {
        // Only update password when SSID is being set (or already non-empty).
        // Don't allow clearing the password alone.
        String pw = doc["wifi_password"].as<String>();
        if (pw.length() > 0 || Storage::getWiFiSSID().length() > 0) {
            Storage::setWiFiPassword(pw);
        }
    }
    if (doc["city"].is<String>()) {
        Storage::setCityName(doc["city"].as<String>());
    }
    if (doc["latitude"].is<float>()) {
        Storage::setLatitude(doc["latitude"].as<float>());
    }
    if (doc["longitude"].is<float>()) {
        Storage::setLongitude(doc["longitude"].as<float>());
    }
    if (doc["timezone"].is<String>()) {
        String tz = doc["timezone"].as<String>();
        Storage::setTimezone(tz);
        TimeSync::applyTimezone(tz);
    }
    if (doc["candle_offset"].is<int>()) {
        Storage::setCandleOffset(doc["candle_offset"].as<int>());
    }
    if (doc["candle_alerts"].is<int>()) {
        int ca = doc["candle_alerts"].as<int>();
        if (ca >= 0 && ca <= 7) {
            Storage::setCandleAlerts(ca);
            Scheduler::rebuildAlertEvents();
        }
    }
    if (doc["havdalah_mode"].is<String>()) {
        String mode = doc["havdalah_mode"].as<String>();
        if (mode == "m50") {
            Storage::setHavdalahMode("m");
            Storage::setHavdalahMinutes(50);
        } else if (mode == "m42") {
            Storage::setHavdalahMode("m");
            Storage::setHavdalahMinutes(42);
        } else if (mode == "m72") {
            Storage::setHavdalahMode("m");
            Storage::setHavdalahMinutes(72);
        } else if (mode == "degrees") {
            Storage::setHavdalahMode("degrees");
            if (doc["havdalah_degrees"].is<float>()) {
                Storage::setHavdalahDegrees(doc["havdalah_degrees"].as<float>());
            } else if (doc["havdalah_degrees"].is<int>()) {
                Storage::setHavdalahDegrees((float)doc["havdalah_degrees"].as<int>());
            }
        } else {
            Storage::setHavdalahMode(mode);
        }
    }
    if (doc["havdalah_minutes"].is<int>()) {
        Storage::setHavdalahMinutes(doc["havdalah_minutes"].as<int>());
    }
    if (doc["havdalah_degrees"].is<float>()) {
        Storage::setHavdalahDegrees(doc["havdalah_degrees"].as<float>());
    } else if (doc["havdalah_degrees"].is<int>()) {
        Storage::setHavdalahDegrees((float)doc["havdalah_degrees"].as<int>());
    }
    if (doc["alert_enabled"].is<bool>()) {
        Storage::setAlertEnabled(doc["alert_enabled"].as<bool>());
    }
    if (doc["alert_duration_ms"].is<int>()) {
        Storage::setAlertDurationMs((unsigned long)doc["alert_duration_ms"].as<int>());
    }
    if (doc["ringtone"].is<String>()) {
        String v = doc["ringtone"].as<String>();
        if (v == "random" || v == "pinky" || v == "star_wars" || v == "mozart" ||
            v == "under_the_sea" || v == "spiderman" || v == "mario" || v == "pink_panther" ||
            v == "hava_nagila") {
            Storage::setRingtone(v);
        }
    }
    if (doc["hebcal_max_attempts"].is<int>()) {
        int v = doc["hebcal_max_attempts"].as<int>();
        if (v >= 1 && v <= 5) Storage::setHebcalMaxAttempts(v);
    }
    if (doc["hebcal_proxy_url"].is<String>()) {
        Storage::setHebcalProxyURL(doc["hebcal_proxy_url"].as<String>());
    }
    
    // Single flash erase for the whole bundle.
    Storage::endBatch();

    // Refresh schedule if anything that affects the Hebcal URL changed
    // (delayed 5s so /settings can reopen without low memory).  Timezone is
    // included because Israel-mode (`&i=on`) is keyed off it — without this,
    // changing the timezone would silently keep using the previous calendar
    // mode until the next daily refresh.
    if (doc["latitude"].is<float>() || doc["longitude"].is<float>() ||
        doc["timezone"].is<String>() || doc["candle_offset"].is<int>() ||
        doc["havdalah_mode"].is<String>() || doc["havdalah_minutes"].is<int>()) {
        Scheduler::requestRefreshDelayed();
    }

    server.send(200, "application/json", "{\"status\":\"ok\"}");
}

void UIServer::handleAPIGeocode() {
    if (server.method() == HTTP_OPTIONS) {
        server.send(200);
        return;
    }
    if (server.method() != HTTP_POST) {
        server.send(405, "application/json", "{\"error\":\"Method Not Allowed\"}");
        return;
    }
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"No data\"}");
        return;
    }
    
    String body = server.arg("plain");
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);
    
    if (error) {
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }
    
    if (!doc["city"].is<String>()) {
        server.send(400, "application/json", "{\"error\":\"Missing city parameter\"}");
        return;
    }
    
    String cityName = doc["city"].as<String>();
    String countryCode = doc["country"] | "";
    
    Location location = countryCode.length() > 0 
        ? Geocoding::searchCityWithCountry(cityName, countryCode)
        : Geocoding::searchCity(cityName);
    
    if (location.valid) {
        Storage::beginBatch();
        Storage::setLatitude(location.latitude);
        Storage::setLongitude(location.longitude);
        Storage::setCityName(location.city);
        if (location.timezone.length() > 0) {
            Storage::setTimezone(location.timezone);
            TimeSync::applyTimezone(location.timezone);
        }
        Storage::endBatch();
        Scheduler::requestRefresh();
        JsonDocument response;
        response["latitude"] = location.latitude;
        response["longitude"] = location.longitude;
        response["city"] = location.city;
        response["country"] = location.country;
        response["timezone"] = location.timezone;
        response["status"] = "ok";
        char out[384];
        size_t n = serializeJson(response, out, sizeof(out));
        if (n == 0 || n >= sizeof(out)) {
            server.send(500, "application/json", "{\"error\":\"serialize\"}");
            return;
        }
        server.setContentLength(n);
        server.send(200, "application/json", "");
        server.sendContent(out, n);
    } else {
        server.send(404, "application/json", "{\"error\":\"City not found\"}");
    }
}

void UIServer::handleAPITestAlert() {
    if (!checkAuth()) {
        server.send(401, "application/json", "{\"error\":\"Unauthorized\"}");
        return;
    }
    
    Alerts::test();
    server.send(200, "application/json", "{\"status\":\"ok\"}");
}

void UIServer::handleAPIRgbPreview() {
    if (!checkAuth()) {
        server.send(401, "application/json", "{\"error\":\"Unauthorized\"}");
        return;
    }
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"No data\"}");
        return;
    }
    String body = server.arg("plain");
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);
    if (error) {
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }
    bool enabled = false;
    if (doc["enabled"].is<bool>()) {
        enabled = doc["enabled"].as<bool>();
    } else if (doc["enabled"].is<int>()) {
        enabled = doc["enabled"].as<int>() != 0;
    }
    RGBSetShabbatPreview(enabled);
    bool current = RGBGetShabbatPreview();
    char out[64];
    int n = snprintf(out, sizeof(out), "{\"status\":\"ok\",\"preview\":%s}", current ? "true" : "false");
    if (n <= 0 || n >= (int)sizeof(out)) n = snprintf(out, sizeof(out), "{\"status\":\"ok\"}");
    server.send(200, "application/json", out);
}

void UIServer::handleAPINotFound() {
    if (server.uri() == "/favicon.ico") {
        server.send(204);
        return;
    }
    if ((server.uri() == "/api/status" || server.uri() == "/api/status/") && server.method() == HTTP_GET) {
        handleAPIStatus();
        return;
    }
    if (server.uri() == "/api/geocode" || server.uri() == "/api/geocode/") {
        handleAPIGeocode();
        return;
    }
    if ((server.uri() == "/api/test-alert" || server.uri() == "/api/test-alert/") && server.method() == HTTP_POST) {
        handleAPITestAlert();
        return;
    }
    if ((server.uri() == "/api/rgb-preview" || server.uri() == "/api/rgb-preview/") && server.method() == HTTP_POST) {
        handleAPIRgbPreview();
        return;
    }
    if (server.uri() == "/settings" || server.uri() == "/settings/") {
        handleSettings();
        return;
    }
    server.send(404, "text/plain", "Not Found");
}

bool UIServer::checkAuth() {
    // Password protection disabled - settings and logs are open on local network
    return true;
}

