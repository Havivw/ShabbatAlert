#include "ui_server.h"
#include "wifi_manager.h"
#include <cmath>

#ifdef BOARD_ESP8266
ESP8266WebServer UIServer::server(80);
#else
WebServer UIServer::server(80);
#endif

void UIServer::init() {
    // Web pages
    server.on("/", handleRoot);
    server.on("/setup", HTTP_GET, handleSetupPage);
    server.on("/setup", HTTP_POST, handleSetupSubmit);
    server.on("/settings", handleSettings);
    server.on("/logs", handleLogs);
    
    // API endpoints
    server.on("/api/status", HTTP_GET, handleAPIStatus);
    server.on("/api/schedule", HTTP_GET, handleAPISchedule);
    server.on("/api/settings", HTTP_POST, handleAPISettings);
    server.on("/api/geocode", HTTP_ANY, handleAPIGeocode);
    server.on("/api/test-alert", HTTP_POST, handleAPITestAlert);
    server.onNotFound(handleAPINotFound);
    
    server.begin();
    LOG("Web server started");
}

void UIServer::handle() {
    server.handleClient();
}

void UIServer::handleRoot() {
    if (WiFiManager::isConnected() && !Storage::isConfigured()) {
        server.sendHeader("Location", "/setup", true);
        server.send(302, "text/plain", "");
        return;
    }
    String html = getDashboardHTML();
#ifdef BOARD_ESP8266
    const unsigned int len = html.length();
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "0");
    server.setContentLength(len);
    server.send(200, "text/html", "");
    const unsigned int CHUNK = 384;
    for (unsigned int i = 0; i < len; i += CHUNK) {
        unsigned int n = (i + CHUNK <= len) ? CHUNK : (len - i);
        server.sendContent(html.substring(i, i + n));
        yield();
    }
#else
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "0");
    server.send(200, "text/html", html);
#endif
}

void UIServer::handleSettings() {
    if (!checkAuth()) {
        server.send(401, "text/plain", "Unauthorized");
        return;
    }
    String html = getSettingsHTML();
#ifdef BOARD_ESP8266
    // ESP8266: send in chunks to avoid ERR_CONTENT_LENGTH_MISMATCH for large responses
    const unsigned int len = html.length();
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "0");
    server.setContentLength(len);
    server.send(200, "text/html", "");
    const unsigned int CHUNK = 384;
    for (unsigned int i = 0; i < len; i += CHUNK) {
        unsigned int n = (i + CHUNK <= len) ? CHUNK : (len - i);
        server.sendContent(html.substring(i, i + n));
        yield();
    }
#else
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "0");
    server.send(200, "text/html", html);
#endif
}

void UIServer::handleLogs() {
    if (!checkAuth()) {
        server.send(401, "text/plain", "Unauthorized");
        return;
    }
    String html = getLogsHTML();
#ifdef BOARD_ESP8266
    const unsigned int len = html.length();
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "0");
    server.setContentLength(len);
    server.send(200, "text/html", "");
    const unsigned int CHUNK = 384;
    for (unsigned int i = 0; i < len; i += CHUNK) {
        unsigned int n = (i + CHUNK <= len) ? CHUNK : (len - i);
        server.sendContent(html.substring(i, i + n));
        yield();
    }
#else
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "0");
    server.send(200, "text/html", html);
#endif
}

void UIServer::handleSetupPage() {
    server.send(200, "text/html", getSetupHTML());
}

void UIServer::handleSetupSubmit() {
    if (!server.hasArg("city") || !server.hasArg("lat") || !server.hasArg("lon")) {
        server.send(400, "text/plain", "Missing required fields (city, lat, lon)");
        return;
    }
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
    Scheduler::requestRefresh();
    String html = "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><meta http-equiv=\"refresh\" content=\"2;url=/\"><title>Saved</title><style>body{font-family:Arial,sans-serif;max-width:400px;margin:50px auto;padding:20px;text-align:center;}</style></head><body><h2>Settings saved!</h2><p>Redirecting to dashboard...</p><p><a href=\"/\">Click here</a> if not redirected.</p></body></html>";
    server.send(200, "text/html", html);
}

void UIServer::handleAPIStatus() {
    DynamicJsonDocument doc(4096);
    
    doc["time"] = TimeSync::getFormattedTime();
    doc["datetime"] = TimeSync::getFormattedDateTime();
    doc["time_set"] = TimeSync::isTimeSet();
    doc["timezone"] = Storage::getTimezone();
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
    doc["alert_duration_ms"] = Storage::getAlertDurationMs();
    doc["ringtone"] = Storage::getRingtone();
    doc["wifi_ssid"] = Storage::getWiFiSSID();
    doc["hebcal_max_attempts"] = Storage::getHebcalMaxAttempts();
    // hebcal_proxy_url removed from UI (plain HTTP used now)
    
    String response;
    serializeJson(doc, response);
    if (doc.overflowed()) {
        response = "{\"error\":\"overflow\",\"time\":\"\",\"datetime\":\"\",\"city\":\"\",\"timezone\":\"Asia/Jerusalem\",\"candle_offset\":18,\"hebcal_max_attempts\":2,\"next_candles\":\"Set location in Settings\",\"next_havdalah\":\"Set location in Settings\"}";
    }
    server.send(200, "application/json", response);
}

void UIServer::handleAPISchedule() {
    int count = 0;
    AlertEvent* events = Scheduler::getUpcomingEvents(count);
    
    DynamicJsonDocument doc(2048);
    JsonArray eventsArray = doc.createNestedArray("events");
    
    for (int i = 0; i < count; i++) {
        JsonObject event = eventsArray.createNestedObject();
        event["timestamp"] = events[i].timestamp;
        event["type"] = events[i].type;
        
        time_t now = TimeSync::getNow();
        unsigned long secondsUntil = events[i].timestamp - now;
        event["seconds_until"] = secondsUntil;
        
        struct tm* tm = localtime(&events[i].timestamp);
        char buffer[30];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", tm);
        event["formatted"] = String(buffer);
    }
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
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
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, body);
    
    if (error) {
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }
    
    if (doc["wifi_ssid"].is<String>()) {
        Storage::setWiFiSSID(doc["wifi_ssid"].as<String>());
    }
    if (doc["wifi_password"].is<String>()) {
        Storage::setWiFiPassword(doc["wifi_password"].as<String>());
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
            v == "under_the_sea" || v == "spiderman" || v == "mario" || v == "pink_panther") {
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
    
    // Refresh schedule if location changed (deferred to loop via requestRefresh)
    if (doc["latitude"].is<float>() || doc["longitude"].is<float>()) {
        Scheduler::requestRefresh();
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
    DynamicJsonDocument doc(512);
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
        Storage::setLatitude(location.latitude);
        Storage::setLongitude(location.longitude);
        Storage::setCityName(location.city);
        if (location.timezone.length() > 0) {
            Storage::setTimezone(location.timezone);
            TimeSync::applyTimezone(location.timezone);
        }
        Scheduler::requestRefresh();
        DynamicJsonDocument response(512);
        response["latitude"] = location.latitude;
        response["longitude"] = location.longitude;
        response["city"] = location.city;
        response["country"] = location.country;
        response["timezone"] = location.timezone;
        response["status"] = "ok";
        String jsonResponse;
        serializeJson(response, jsonResponse);
        server.send(200, "application/json", jsonResponse);
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
    if (server.uri() == "/settings" || server.uri() == "/settings/") {
        handleSettings();
        return;
    }
    if (server.uri() == "/logs" || server.uri() == "/logs/") {
        handleLogs();
        return;
    }
    server.send(404, "text/plain", "Not Found");
}

bool UIServer::checkAuth() {
    // Password protection disabled - settings and logs are open on local network
    return true;
}

String UIServer::getDashboardHTML() {
    String html = "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><title>Shabbat Alert</title><style>* { margin: 0; padding: 0; box-sizing: border-box; } body { font-family: Arial, sans-serif; background: #f5f5f5; padding: 20px; } .container { max-width: 600px; margin: 0 auto; background: white; border-radius: 10px; padding: 30px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); } h1 { color: #333; margin-bottom: 20px; text-align: center; } .status-card { background: #f8f9fa; border-radius: 8px; padding: 20px; margin: 15px 0; } .status-row { display: flex; justify-content: space-between; padding: 10px 0; border-bottom: 1px solid #e0e0e0; } .status-row:last-child { border-bottom: none; } .label { font-weight: 600; color: #666; } .value { color: #333; } .shabbat-on { color: #d32f2f; font-weight: bold; } .shabbat-off { color: #388e3c; font-weight: bold; } .countdown { font-size: 1.2em; color: #1976d2; } .nav { text-align: center; margin-top: 20px; } .nav a { display: inline-block; margin: 5px; padding: 10px 20px; background: #1976d2; color: white; text-decoration: none; border-radius: 5px; } .nav a:hover { background: #1565c0; } .refresh-btn { background: #4caf50; border: none; padding: 10px 20px; color: white; border-radius: 5px; cursor: pointer; } .refresh-btn:hover { background: #45a049; } .alarm-btn { background: #ff9800; border: none; padding: 10px 20px; color: white; border-radius: 5px; cursor: pointer; margin-left: 5px; } .alarm-btn:hover { background: #f57c00; } .bookmark { font-size: 0.9em; color: #666; margin-top: 10px; }</style></head><body><div class=\"container\"><h1>Shabbat Alert</h1><div class=\"bookmark\">You can open this page at <a href=\"http://" MDNS_HOSTNAME ".local\">http://" MDNS_HOSTNAME ".local</a></div><div id=\"status\"></div><div class=\"nav\"><a href=\"/settings\">Settings</a><a href=\"/logs\">Logs</a><button class=\"refresh-btn\" onclick=\"location.reload()\">Refresh</button><button class=\"alarm-btn\" onclick=\"testAlarm()\" id=\"alarmBtn\">Hear alarm</button><span id=\"alarmFeedback\" style=\"margin-left:8px;\"></span></div></div><script>function testAlarm(){var btn=document.getElementById('alarmBtn');var fb=document.getElementById('alarmFeedback');btn.disabled=true;fb.textContent='Playing...';fetch('/api/test-alert',{method:'POST'}).then(function(){fb.textContent='Done';setTimeout(function(){fb.textContent='';btn.disabled=false;},1500);}).catch(function(e){fb.textContent='Error';btn.disabled=false;});}function updateStatus(){fetch('/api/status').then(r=>r.json()).then(data=>{let html='<div class=\"status-card\">';html+='<div class=\"status-row\"><span class=\"label\">Current Time'+(data.timezone?' ('+data.timezone+')':'')+':</span><span class=\"value\">'+data.datetime+(data.time_set?'':' (syncing...)')+'</span></div>';html+='<div class=\"status-row\"><span class=\"label\">Location:</span><span class=\"value\">'+data.city+'</span></div>';html+='<div class=\"status-row\"><span class=\"label\">Shabbat Status:</span><span class=\"value '+(data.shabbat_now?'shabbat-on':'shabbat-off')+'\">'+(data.shabbat_now?'ON':'OFF')+'</span></div>';html+='<div class=\"status-row\"><span class=\"label\">Next Candle Lighting:</span><span class=\"value\">'+data.next_candles+'</span></div>';html+='<div class=\"status-row\"><span class=\"label\">Next Havdalah:</span><span class=\"value\">'+data.next_havdalah+'</span></div>';html+='</div>';fetch('/api/schedule').then(r=>r.json()).then(sched=>{if(sched.events&&sched.events.length>0){html+='<div class=\"status-card\"><h3>Upcoming Alerts</h3>';sched.events.slice(0,5).forEach(e=>{let minutes=Math.floor(e.seconds_until/60);let hours=Math.floor(minutes/60);let days=Math.floor(hours/24);let timeStr=days>0?days+'d ':'';timeStr+=(hours%24)+'h '+(minutes%60)+'m';html+='<div class=\"status-row\"><span class=\"label\">'+e.type+':</span><span class=\"countdown\">'+e.formatted+'<br><small style=\"color:#888\">in '+timeStr+'</small></span></div>';});html+='</div>';}document.getElementById('status').innerHTML=html;});}).catch(e=>console.error(e));}updateStatus();setInterval(updateStatus,30000);</script></body></html>";
    return html;
}

String UIServer::getSetupHTML() {
    String html = "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><title>Set city and times - Shabbat Alert</title><style>body{font-family:Arial,sans-serif;max-width:400px;margin:50px auto;padding:20px;}input,select{width:100%;padding:10px;margin:5px 0;box-sizing:border-box;}button{width:100%;padding:12px;background:#4CAF50;color:white;border:none;cursor:pointer;margin-top:10px;}button:hover{background:#45a049;}label{display:block;margin-top:10px;}.search-btn{width:auto;padding:5px 15px;margin-top:5px;}#city_result{margin-top:10px;padding:10px;background:#e8f5e9;border-radius:5px;display:none;}</style></head><body><h2>Set city and times</h2><p>Enter your location and candle/havdalah preferences.</p><form action=\"/setup\" method=\"POST\"><label>City Name:</label><input type=\"text\" name=\"city\" id=\"city_input\" placeholder=\"e.g., Jerusalem\" required><button type=\"button\" class=\"search-btn\" onclick=\"searchCity()\">Search City</button><div id=\"city_result\"></div><label>Latitude:</label><input type=\"number\" step=\"0.000001\" name=\"lat\" id=\"lat_input\" required><label>Longitude:</label><input type=\"number\" step=\"0.000001\" name=\"lon\" id=\"lon_input\" required><label>Timezone:</label><input type=\"text\" name=\"tz\" id=\"tz_input\" placeholder=\"Asia/Jerusalem\" value=\"Asia/Jerusalem\" required><label>Candle Lighting Offset (minutes):</label><input type=\"number\" name=\"candle_offset\" value=\"18\" min=\"0\" max=\"60\"><label>Havdalah:</label><select name=\"havdalah_mode\" id=\"havdalah_mode\"><option value=\"M\">Nightfall (8.5 deg below horizon)</option><option value=\"m50\">50 min after sunset</option><option value=\"m42\">42 min</option><option value=\"m72\">72 min</option><option value=\"degrees\">Custom degrees</option></select><label id=\"deg_label\" style=\"display:none;\">Havdalah degrees:</label><input type=\"number\" step=\"0.1\" name=\"havdalah_degrees\" id=\"havdalah_degrees\" placeholder=\"8.5\" min=\"4\" max=\"12\" style=\"display:none;\"><button type=\"submit\">Save</button></form><script>function searchCity(){var city=document.getElementById('city_input').value.trim();if(!city){alert('Enter a city name');return;}var r=document.getElementById('city_result');r.style.display='block';r.innerHTML='Searching...';fetch('/api/geocode',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({city:city})}).then(function(x){return x.text();}).then(function(t){try{var d=JSON.parse(t);if(d.error){r.innerHTML='<span style=\"color:red;\">'+d.error+'</span>';}else{document.getElementById('lat_input').value=d.latitude||'';document.getElementById('lon_input').value=d.longitude||'';if(d.timezone)document.getElementById('tz_input').value=d.timezone;r.innerHTML='<span style=\"color:green;\">Found: '+d.city+', '+d.country+'</span>';}}catch(e){r.innerHTML='Parse error';}}).catch(function(){r.innerHTML='Error';});}document.getElementById('havdalah_mode').onchange=function(){var v=document.getElementById('havdalah_mode').value;var show=v==='degrees';document.getElementById('deg_label').style.display=show?'block':'none';document.getElementById('havdalah_degrees').style.display=show?'block':'none';};</script></body></html>";
    return html;
}

String UIServer::getSettingsHTML() {
    String html = "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><title>Settings - Shabbat Alert</title><style>* { margin: 0; padding: 0; box-sizing: border-box; } body { font-family: Arial, sans-serif; background: #f5f5f5; padding: 20px; } .container { max-width: 600px; margin: 0 auto; background: white; border-radius: 10px; padding: 30px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); } h1 { color: #333; margin-bottom: 20px; } .form-group { margin: 15px 0; } label { display: block; margin-bottom: 5px; font-weight: 600; color: #666; } input, select { width: 100%; padding: 10px; border: 1px solid #ddd; border-radius: 5px; font-size: 14px; } button { padding: 12px; background: #1976d2; color: white; border: none; border-radius: 5px; cursor: pointer; font-size: 16px; margin-top: 10px; } button.search-btn { width: auto; padding: 5px 15px; background: #4caf50; margin-top: 5px; } button:hover { background: #1565c0; } button.search-btn:hover { background: #45a049; } .nav { text-align: center; margin-top: 20px; } .nav a { display: inline-block; margin: 5px; padding: 10px 20px; background: #666; color: white; text-decoration: none; border-radius: 5px; } #city_result { margin-top: 10px; padding: 10px; background: #e8f5e9; border-radius: 5px; display: none; } .alarm-btn { width: auto; padding: 8px 16px; background: #ff9800; margin-top: 5px; } .alarm-btn:hover { background: #f57c00; }</style></head><body><div class=\"container\"><h1>Settings</h1><p id=\"settingsLoadStatus\" style=\"color:#666;margin-bottom:10px;\"></p><form id=\"settingsForm\"><div class=\"form-group\"><label>WiFi SSID:</label><input type=\"text\" name=\"wifi_ssid\" id=\"wifi_ssid\"></div><div class=\"form-group\"><label>WiFi Password:</label><input type=\"password\" name=\"wifi_password\" id=\"wifi_password\"></div><div class=\"form-group\"><label>City Name:</label><input type=\"text\" name=\"city\" id=\"city\"><button type=\"button\" class=\"search-btn\" onclick=\"searchCity()\">Search City</button><div id=\"city_result\"></div></div><div class=\"form-group\"><label>Latitude:</label><input type=\"number\" step=\"0.000001\" name=\"latitude\" id=\"latitude\"></div><div class=\"form-group\"><label>Longitude:</label><input type=\"number\" step=\"0.000001\" name=\"longitude\" id=\"longitude\"></div><div class=\"form-group\"><label>Timezone:</label><input type=\"text\" name=\"timezone\" id=\"timezone\" placeholder=\"Asia/Jerusalem\"></div><div class=\"form-group\"><label>Alert before candle lighting:</label><div style=\"display:flex;gap:15px;margin-top:5px;\"><label style=\"font-weight:normal;display:flex;align-items:center;gap:5px;\"><input type=\"checkbox\" name=\"candle_18\" id=\"candle_18\" style=\"width:auto;\"> 18 min</label><label style=\"font-weight:normal;display:flex;align-items:center;gap:5px;\"><input type=\"checkbox\" name=\"candle_30\" id=\"candle_30\" style=\"width:auto;\"> 30 min</label><label style=\"font-weight:normal;display:flex;align-items:center;gap:5px;\"><input type=\"checkbox\" name=\"candle_45\" id=\"candle_45\" style=\"width:auto;\"> 45 min</label></div></div><div class=\"form-group\"><label>Havdalah:</label><select name=\"havdalah_mode\" id=\"havdalah_mode\"><option value=\"M\">Nightfall (8.5 deg below horizon)</option><option value=\"m50\">50 min after sunset</option><option value=\"m42\">42 min after sunset</option><option value=\"m72\">72 min after sunset</option><option value=\"degrees\">Custom degrees</option></select></div><div class=\"form-group\" id=\"havdalah_degrees_group\" style=\"display:none;\"><label>Havdalah degrees below horizon:</label><input type=\"number\" step=\"0.1\" name=\"havdalah_degrees\" id=\"havdalah_degrees\" min=\"4\" max=\"12\" placeholder=\"8.5\"></div><div class=\"form-group\"><label>Hebcal API max attempts (1-5):</label><input type=\"number\" name=\"hebcal_max_attempts\" id=\"hebcal_max_attempts\" min=\"1\" max=\"5\" placeholder=\"2\"></div><div class=\"form-group\"><h3>Alarm</h3><label>Alert sound:</label><select name=\"ringtone\" id=\"ringtone\"><option value=\"pinky\">Pinky and the Brain</option><option value=\"star_wars\">Star Wars</option><option value=\"mozart\">Mozart</option><option value=\"under_the_sea\">Under the Sea</option><option value=\"spiderman\">Spiderman</option><option value=\"mario\">Mario</option><option value=\"pink_panther\">Pink Panther</option><option value=\"random\">Random</option></select><label>Alert duration (ms):</label><input type=\"number\" name=\"alert_duration_ms\" id=\"alert_duration_ms\" min=\"1000\" placeholder=\"2000\"></div><button type=\"submit\" style=\"width:100%;\">Save Settings</button></form><div class=\"form-group\"><label>Test alarm:</label><button type=\"button\" class=\"alarm-btn\" onclick=\"testAlarm()\" id=\"alarmBtn\">Hear alarm</button><span id=\"alarmFeedback\" style=\"margin-left:8px;\"></span></div><div class=\"nav\"><a href=\"/\">Dashboard</a><a href=\"/logs\">Logs</a></div></div><script>function testAlarm(){var btn=document.getElementById('alarmBtn');var fb=document.getElementById('alarmFeedback');if(btn){btn.disabled=true;}if(fb){fb.textContent='Playing...';}fetch('/api/test-alert',{method:'POST'}).then(function(){if(fb){fb.textContent='Done';}setTimeout(function(){if(fb){fb.textContent='';}if(btn){btn.disabled=false;}},1500);}).catch(function(e){if(fb){fb.textContent='Error';}if(btn){btn.disabled=false;}});}function searchCity(){var city=document.getElementById('city').value.trim();if(!city){alert('Please enter a city name');return;}var resultDiv=document.getElementById('city_result');resultDiv.style.display='block';resultDiv.innerHTML='Searching...';fetch('/api/geocode',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({city:city})}).then(function(r){if(!r.ok){throw new Error('HTTP error: '+r.status);}return r.text();}).then(function(text){try{var data=JSON.parse(text);if(data.error){resultDiv.innerHTML='<span style=\"color:red;\">Error: '+data.error+'</span>';}else{document.getElementById('latitude').value=data.latitude||'';document.getElementById('longitude').value=data.longitude||'';if(data.timezone){document.getElementById('timezone').value=data.timezone;}resultDiv.innerHTML='<span style=\"color:green;\">Found: '+data.city+', '+data.country+' ('+data.latitude+', '+data.longitude+')</span>';}}catch(e){resultDiv.innerHTML='<span style=\"color:red;\">Parse error: '+e.message+'</span>';}}).catch(function(e){resultDiv.innerHTML='<span style=\"color:red;\">Error: '+e.message+'</span>';});}fetch('/api/status').then(function(r){if(!r.ok)throw new Error(r.status);return r.text();}).then(function(text){try{var data=JSON.parse(text);document.getElementById('city').value=data.city||'';document.getElementById('latitude').value=(data.latitude!=null&&data.latitude!==''&&!isNaN(Number(data.latitude)))?String(data.latitude):'';document.getElementById('longitude').value=(data.longitude!=null&&data.longitude!==''&&!isNaN(Number(data.longitude)))?String(data.longitude):'';if(data.timezone){document.getElementById('timezone').value=data.timezone||'Asia/Jerusalem';}if(data.candle_alerts!=null){var ca=data.candle_alerts;document.getElementById('candle_18').checked=!!(ca&1);document.getElementById('candle_30').checked=!!(ca&2);document.getElementById('candle_45').checked=!!(ca&4);}if(data.hebcal_max_attempts!=null){document.getElementById('hebcal_max_attempts').value=data.hebcal_max_attempts;}if(data.wifi_ssid!=null){document.getElementById('wifi_ssid').value=data.wifi_ssid;}if(data.alert_duration_ms!=null){document.getElementById('alert_duration_ms').value=data.alert_duration_ms;}if(data.ringtone!=null){var rt=document.getElementById('ringtone');if(rt)rt.value=data.ringtone;}if(data.havdalah_mode!=null){var m=data.havdalah_mode;var min=data.havdalah_minutes;var sel=document.getElementById('havdalah_mode');if(m=='M')sel.value='M';else if(m=='m'&&min==50)sel.value='m50';else if(m=='m'&&min==42)sel.value='m42';else if(m=='m'&&min==72)sel.value='m72';else if(m=='degrees'){sel.value='degrees';document.getElementById('havdalah_degrees_group').style.display='block';if(data.havdalah_degrees!=null)document.getElementById('havdalah_degrees').value=data.havdalah_degrees;}else sel.value='M';}document.getElementById('havdalah_mode').onchange=function(){var v=document.getElementById('havdalah_mode').value;document.getElementById('havdalah_degrees_group').style.display=(v=='degrees')?'block':'none';};var statusEl=document.getElementById('settingsLoadStatus');if(statusEl)statusEl.textContent='Current settings loaded.';}catch(e){var statusEl=document.getElementById('settingsLoadStatus');if(statusEl)statusEl.textContent='Could not load current settings.';console.error('Status parse error:',e);}}).catch(function(e){var statusEl=document.getElementById('settingsLoadStatus');if(statusEl)statusEl.textContent='Could not load current settings.';console.error('Status fetch error:',e);});document.getElementById('settingsForm').addEventListener('submit',function(e){e.preventDefault();var formData=new FormData(e.target);var json={};formData.forEach(function(value,key){json[key]=value;});var ca=0;if(document.getElementById('candle_18').checked)ca|=1;if(document.getElementById('candle_30').checked)ca|=2;if(document.getElementById('candle_45').checked)ca|=4;json.candle_alerts=ca;delete json.candle_18;delete json.candle_30;delete json.candle_45;fetch('/api/settings',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(json)}).then(function(r){return r.text();}).then(function(text){try{var data=JSON.parse(text);alert('Settings saved!');}catch(e){alert('Error parsing response');}}).catch(function(e){alert('Error: '+e.message);});});</script></body></html>";
    return html;
}

String UIServer::getLogsHTML() {
    String logs = Logger::getLogs();
    String html = "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><title>Logs - Shabbat Alert</title><style>* { margin: 0; padding: 0; box-sizing: border-box; } body { font-family: monospace; background: #1e1e1e; color: #d4d4d4; padding: 20px; } .container { max-width: 800px; margin: 0 auto; } h1 { margin-bottom: 20px; } pre { background: #252526; padding: 15px; border-radius: 5px; overflow-x: auto; } .nav { text-align: center; margin-top: 20px; } .nav a { display: inline-block; margin: 5px; padding: 10px 20px; background: #007acc; color: white; text-decoration: none; border-radius: 5px; }</style></head><body><div class=\"container\"><h1>System Logs</h1><pre>";
    html += logs;
    html += "</pre><div class=\"nav\"><a href=\"/\">Dashboard</a><a href=\"/settings\">Settings</a></div></div></body></html>";
    return html;
}

