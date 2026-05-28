#include "wifi_manager.h"
#include "logger.h"
#include "geocoding.h"
#include <ArduinoJson.h>
#include <ESP8266mDNS.h>

bool WiFiManager::apMode = false;
bool WiFiManager::apRescueActive = false;
unsigned long WiFiManager::staDownSince = 0;
DNSServer WiFiManager::dnsServer;
ESP8266WebServer* WiFiManager::apServer = nullptr;

// How long STA must be down at runtime before we open a rescue AP (so the
// device is reachable from a phone even when the home WiFi can't be joined).
// The rescue AP coexists with STA via WIFI_AP_STA and is removed
// automatically once STA recovers.  At boot, if the very first connect
// attempt fails we open the rescue AP immediately (see connect()) — the
// runtime threshold below only applies to mid-session drops, where a brief
// router blip shouldn't open an AP.
#define AP_RESCUE_AFTER_MS 60000UL  // 60 s of continuous STA failure

void WiFiManager::init() {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    LOG("WiFi initialized (STA mode)");
}

bool WiFiManager::connect() {
    String ssid = Storage::getWiFiSSID();
    String password = Storage::getWiFiPassword();

    if (ssid.length() == 0) {
        LOG("No WiFi credentials stored, starting AP mode");
        startAP();
        return false;
    }

    LOGF("Connecting to WiFi: %s", ssid.c_str());
    WiFi.hostname(MDNS_HOSTNAME);
    WiFi.begin(ssid.c_str(), password.c_str());

    // Try for up to 30 seconds (slow APs / DHCP servers occasionally exceed
    // the previous 10-second window and would silently drop us to AP mode).
    const int maxAttempts = 60;  // 60 * 500ms = 30s
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        LOGF("WiFi connected! IP: %s", WiFi.localIP().toString().c_str());
        if (MDNS.begin(MDNS_HOSTNAME)) {
            LOG("mDNS started: " MDNS_HOSTNAME ".local");
            MDNS.addService("http", "tcp", 80);
        } else {
            LOG("mDNS begin failed");
        }
        return true;
    }

    // STA didn't come up in time.  Don't strand the device with no UI: open
    // the rescue AP right now so the user can immediately reach it from a
    // phone (red LEDs + no AP for 2 minutes was the previous symptom).
    // STA retries continue in parallel via WIFI_AP_STA + setAutoReconnect()
    // in init() + the periodic ensureConnected() call from loop().
    LOG("WiFi not yet connected on boot; opening rescue AP and retrying STA in background");
    staDownSince = millis();
    startAPRescue();
    return false;
}

void WiFiManager::startAPRescue() {
    if (apRescueActive || apMode) return;
    apRescueActive = true;
    uint32_t chipId = ESP.getChipId();
    String apSSID = String(AP_SSID_PREFIX) + String(chipId, HEX);
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(apSSID.c_str(), "", AP_CHANNEL, false, AP_MAX_CONNECTIONS);
    IPAddress apIP = WiFi.softAPIP();
    LOGF("AP rescue: STA down >2min, opened fallback %s (IP: %s) — main UI on port 80",
         apSSID.c_str(), apIP.toString().c_str());
    // Captive portal DNS so phones auto-redirect to http://<ap-ip>/
    dnsServer.start(53, "*", apIP);
}

void WiFiManager::stopAPRescue() {
    if (!apRescueActive) return;
    LOG("AP rescue: STA back online, tearing down fallback access point");
    apRescueActive = false;
    dnsServer.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
}

void WiFiManager::ensureConnected() {
    if (apMode) return;  // user explicitly in AP mode (no creds), don't override

    if (WiFi.status() == WL_CONNECTED) {
        if (apRescueActive) stopAPRescue();
        staDownSince = 0;
        return;
    }

    if (Storage::getWiFiSSID().length() == 0) return;

    // Track the moment STA went down (or stayed down from boot).
    if (staDownSince == 0) staDownSince = millis();

    // After AP_RESCUE_AFTER_MS of failed STA, raise the rescue AP so the
    // device is reachable for diagnostics / re-entering credentials.
    if (!apRescueActive && (millis() - staDownSince) > AP_RESCUE_AFTER_MS) {
        startAPRescue();
    }

    static unsigned long lastAttempt = 0;
    static bool lastResultLogged = false;
    unsigned long now = millis();
    if (lastAttempt != 0 && (now - lastAttempt) < 60000UL) return;
    lastAttempt = now;

    LOG("WiFi background reconnect attempt");
    WiFi.reconnect();
    // Give the radio a brief opportunity to come up so the next loop iteration
    // can already see WL_CONNECTED if the AP responds quickly.
    for (int i = 0; i < 10; i++) {
        if (WiFi.status() == WL_CONNECTED) break;
        delay(100);
    }
    if (WiFi.status() == WL_CONNECTED) {
        LOGF("WiFi reconnected! IP: %s", WiFi.localIP().toString().c_str());
        // Re-advertise mDNS in case it dropped while disconnected.
        MDNS.begin(MDNS_HOSTNAME);
        MDNS.addService("http", "tcp", 80);
        if (apRescueActive) stopAPRescue();
        staDownSince = 0;
        lastResultLogged = false;
    } else if (!lastResultLogged) {
        LOG("WiFi still disconnected; will keep retrying every 60s");
        lastResultLogged = true;
    }
}

void WiFiManager::startAP() {
    apMode = true;
    uint32_t chipId = ESP.getChipId();
    String apSSID = String(AP_SSID_PREFIX) + String(chipId, HEX);
    
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apSSID.c_str(), "", AP_CHANNEL, false, AP_MAX_CONNECTIONS);
    
    IPAddress apIP = WiFi.softAPIP();
    LOGF("AP started: %s (IP: %s)", apSSID.c_str(), apIP.toString().c_str());
    
    // Setup captive portal
    setupCaptivePortal();
}

void WiFiManager::setupCaptivePortal() {
    apServer = new ESP8266WebServer(80);

    // Register routes - order matters, more specific routes first
    apServer->on("/api/geocode", HTTP_ANY, handleAPGeocode);
    apServer->on("/setup", HTTP_POST, handleAPSetup);
    apServer->on("/", handleAPRoot);
    apServer->onNotFound(handleAPNotFound);
    
    // DNS server for captive portal
    dnsServer.start(53, "*", WiFi.softAPIP());
    
    apServer->begin();
    LOG("Captive portal started");
    LOG("Registered /api/geocode endpoint");
}

void WiFiManager::handleAPRoot() {
    apServer->sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    apServer->sendHeader("Pragma", "no-cache");
    apServer->sendHeader("Expires", "0");
    String html = "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><meta http-equiv=\"Cache-Control\" content=\"no-cache, no-store, must-revalidate\"><title>Shabbat Alert - Step 1</title><style>body { font-family: Arial, sans-serif; max-width: 400px; margin: 50px auto; padding: 20px; } input { width: 100%; padding: 10px; margin: 5px 0; box-sizing: border-box; } button { width: 100%; padding: 12px; background: #4CAF50; color: white; border: none; cursor: pointer; margin-top: 10px; } button:hover { background: #45a049; } label { display: block; margin-top: 10px; } .step { color: #666; font-size: 0.9em; margin-bottom: 15px; }</style></head><body><h2>Shabbat Alert</h2><p class=\"step\">Step 1 of 2: Connect to your home WiFi</p><p>Enter your WiFi name and password. City and times are set in step 2 after the device connects.</p><form action=\"/setup\" method=\"POST\"><label>WiFi network name (SSID):</label><input type=\"text\" name=\"ssid\" required placeholder=\"Your network name\"><label>WiFi password (leave blank if open network):</label><input type=\"password\" name=\"password\" placeholder=\"Optional\"><button type=\"submit\">Save and connect</button></form></body></html>";
    apServer->send(200, "text/html", html);
}

void WiFiManager::handleAPGeocode() {
    LOG("=== handleAPGeocode called ===");
    LOGF("Method: %s", apServer->method() == HTTP_POST ? "POST" : "OTHER");
    LOGF("URI: %s", apServer->uri().c_str());
    
    String body = "";
    String cityName = "";
    String countryCode = "";
    
    // Check if we have JSON body (POST with Content-Type: application/json)
    if (apServer->hasArg("plain")) {
        body = apServer->arg("plain");
        DynamicJsonDocument doc(512);
        DeserializationError error = deserializeJson(doc, body);
        
        if (error) {
            apServer->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
            return;
        }
        
        if (doc["city"].is<String>()) {
            cityName = doc["city"].as<String>();
            countryCode = doc["country"] | "";
        } else {
            apServer->send(400, "application/json", "{\"error\":\"Missing city parameter\"}");
            return;
        }
    } else if (apServer->hasArg("city")) {
        // Handle as form data
        cityName = apServer->arg("city");
        countryCode = apServer->arg("country");
    } else {
        apServer->send(400, "application/json", "{\"error\":\"No data\"}");
        return;
    }
    
    Location location = countryCode.length() > 0 
        ? Geocoding::searchCityWithCountry(cityName, countryCode)
        : Geocoding::searchCity(cityName);
    
    if (location.valid) {
        DynamicJsonDocument response(512);
        response["latitude"] = location.latitude;
        response["longitude"] = location.longitude;
        response["city"] = location.city;
        response["country"] = location.country;
        response["timezone"] = location.timezone;
        response["status"] = "ok";
        
        String jsonResponse;
        serializeJson(response, jsonResponse);
        apServer->send(200, "application/json", jsonResponse);
    } else {
        apServer->send(404, "application/json", "{\"error\":\"City not found\"}");
    }
}

void WiFiManager::handleAPSetup() {
    if (!apServer->hasArg("ssid")) {
        apServer->send(400, "text/plain", "Missing WiFi SSID");
        return;
    }
    
    Storage::setWiFiSSID(apServer->arg("ssid"));
    Storage::setWiFiPassword(apServer->arg("password"));
    
    String response = R"(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>Settings Saved</title>
    <style>
        body { font-family: Arial, sans-serif; max-width: 400px; margin: 50px auto; padding: 20px; text-align: center; }
    </style>
</head>
<body>
    <h2>Settings Saved!</h2>
    <p>Connecting to WiFi...</p>
    <p>The device will restart in a few seconds.</p>
    <script>
        setTimeout(function() { window.location.href = '/'; }, 3000);
    </script>
</body>
</html>
)";
    
    apServer->send(200, "text/html", response);
    delay(1000);
    
    // Restart to connect to WiFi
    ESP.restart();
}

void WiFiManager::handleAPNotFound() {
    String uri = apServer->uri();
    LOGF("AP 404: %s", uri.c_str());
    // Don't redirect API calls - return proper 404 JSON for them
    if (uri.startsWith("/api/")) {
        apServer->send(404, "application/json", "{\"error\":\"Not found\"}");
        return;
    }
    // Redirect to root for captive portal (non-API requests)
    handleAPRoot();
}

void WiFiManager::handleAP() {
    if (apMode && apServer) {
        dnsServer.processNextRequest();
        apServer->handleClient();
    } else if (apRescueActive) {
        // Rescue AP shares the main UIServer on port 80; we just need to
        // service the captive-portal DNS.
        dnsServer.processNextRequest();
    }
}

bool WiFiManager::isConnected() {
    // True only when STA is up.  apRescueActive doesn't count as "internet
    // connected" since it's a local-only rescue network.
    return WiFi.status() == WL_CONNECTED && !apMode;
}

String WiFiManager::getIP() {
    if (apMode) {
        return WiFi.softAPIP().toString();
    }
    if (WiFi.status() == WL_CONNECTED) {
        return WiFi.localIP().toString();
    }
    if (apRescueActive) {
        return WiFi.softAPIP().toString();
    }
    return WiFi.localIP().toString();
}

void WiFiManager::stopAP() {
    if (apMode) {
        apMode = false;
        dnsServer.stop();
        if (apServer) {
            apServer->stop();
            delete apServer;
            apServer = nullptr;
        }
        WiFi.mode(WIFI_STA);
    }
}

