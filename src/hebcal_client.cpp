#include "hebcal_client.h"
#include "logger.h"
#include "time_sync.h"
#include <cmath>
#include <ctime>

String HebcalClient::cachedNextCandles;
String HebcalClient::cachedNextHavdalah;
bool HebcalClient::cacheValid = false;
bool HebcalClient::cachedShabbatNow = false;
unsigned long HebcalClient::lastShabbatNowUpdate = 0;
void (*HebcalClient::s_idleCallback)() = nullptr;

void HebcalClient::setIdleCallback(void (*callback)()) {
    s_idleCallback = callback;
}

void HebcalClient::setCachedNextTimes(const String& nextCandles, const String& nextHavdalah) {
    cachedNextCandles = nextCandles;
    cachedNextHavdalah = nextHavdalah;
    cacheValid = true;
}

bool HebcalClient::fetchShabbatTimes(time_t startDate, time_t endDate, ShabbatEvent* events, int maxEvents, int& eventCount) {
    #ifdef BOARD_ESP8266
    if (WiFi.status() != WL_CONNECTED) {
    #else
    if (!WiFi.isConnected()) {
    #endif
        LOG("Cannot fetch Hebcal: WiFi not connected");
        return false;
    }
    
    float lat = Storage::getLatitude();
    float lon = Storage::getLongitude();
    if (std::isnan(lat) || std::isnan(lon) || !std::isfinite(lat) || !std::isfinite(lon)) {
        LOG("Cannot fetch Hebcal: location not set (set city in Settings)");
        return false;
    }
    
    // Hebcal returns 404 for very old dates; use a sane range if time not set (e.g. 1970)
    const time_t minValidStart = 1577836800;  // 2020-01-01
    if (startDate < minValidStart) {
        startDate = minValidStart;
        endDate = startDate + (14 * 24 * 3600);
    }
    
    String url = buildHebcalURL(startDate, endDate);
    LOGF("Fetching Hebcal data: %s", url.c_str());
    
    int maxAttempts = Storage::getHebcalMaxAttempts();
    if (maxAttempts > 5) maxAttempts = 5;
    if (maxAttempts < 1) maxAttempts = 1;
    for (int attempt = 0; attempt < maxAttempts; attempt++) {
        if (attempt > 0) {
            for (int d = 0; d < 15; d++) { yield(); if (s_idleCallback) s_idleCallback(); delay(100); }
            LOG("Retrying Hebcal request...");
        }
        #ifdef BOARD_ESP8266
        LOGF("Hebcal dbg: freeHeap=%u", (unsigned)ESP.getFreeHeap());
        if (ESP.getFreeHeap() < 12000) {
            LOG("Hebcal dbg: low heap, waiting");
            for (int w = 0; w < 5; w++) { yield(); delay(200); }
            LOGF("Hebcal dbg: freeHeap after wait=%u", (unsigned)ESP.getFreeHeap());
        }
        String proxyUrl = Storage::getHebcalProxyURL();
        proxyUrl.trim();
        if (proxyUrl.length() > 0) {
            int pathStart = url.indexOf("/shabbat");
            String path = (pathStart >= 0) ? url.substring(pathStart) : "/shabbat?cfg=json";
            String proxyHost;
            int proxyPort = 80;
            if (proxyUrl.startsWith("http://")) proxyUrl = proxyUrl.substring(7);
            else if (proxyUrl.startsWith("https://")) { proxyUrl = proxyUrl.substring(8); proxyPort = 443; }
            int colon = proxyUrl.indexOf(':');
            int slash = proxyUrl.indexOf('/');
            if (colon > 0 && (slash < 0 || colon < slash)) {
                proxyHost = proxyUrl.substring(0, colon);
                proxyPort = proxyUrl.substring(colon + 1, slash > 0 ? slash : proxyUrl.length()).toInt();
                if (proxyPort <= 0) proxyPort = 80;
            } else {
                proxyHost = slash > 0 ? proxyUrl.substring(0, slash) : proxyUrl;
            }
            LOGF("Hebcal dbg: proxy %s:%d", proxyHost.c_str(), proxyPort);
            WiFiClient proxyClient;
            if (proxyPort == 443) {
                WiFiClientSecure proxyClientSecure;
                proxyClientSecure.setInsecure();
                proxyClientSecure.setBufferSizes(512, 512);
                if (!proxyClientSecure.connect(proxyHost.c_str(), 443)) {
                    LOG("Hebcal proxy: TLS connect failed");
                    if (attempt == maxAttempts - 1) return false;
                    continue;
                }
                proxyClientSecure.setTimeout(HEBCAL_TIMEOUT_MS / 1000);
                String req = "GET " + path + " HTTP/1.1\r\nHost: " + proxyHost + "\r\nConnection: close\r\n\r\n";
                proxyClientSecure.print(req);
                proxyClientSecure.flush();
                unsigned long dl = millis() + HEBCAL_TIMEOUT_MS;
                while (!proxyClientSecure.available() && millis() < dl) { yield(); if (s_idleCallback) s_idleCallback(); delay(5); }
                if (!proxyClientSecure.available()) {
                    LOG("Hebcal proxy: no response");
                    proxyClientSecure.stop();
                    if (attempt == maxAttempts - 1) return false;
                    continue;
                }
                int code = -1;
                int clen = -1;
                String ln;
                while (proxyClientSecure.connected() && millis() < dl) {
                    if (s_idleCallback) s_idleCallback();
                    ln = proxyClientSecure.readStringUntil('\n');
                    if (ln.length() == 0) break;
                    ln.trim();
                    if (ln.startsWith("HTTP/")) {
                        int cs = ln.indexOf(' ') + 1;
                        if (cs > 0) code = ln.substring(cs, cs + 3).toInt();
                    } else if (ln.startsWith("Content-Length:")) {
                        String c = ln.substring(15); c.trim(); clen = c.toInt();
                    }
                }
                if (code != 200) {
                    LOGF("Hebcal proxy error: %d", code);
                    proxyClientSecure.stop();
                    if (attempt == maxAttempts - 1) return false;
                    continue;
                }
                String payload;
                if (clen > 0 && clen < 32000) {
                    payload.reserve(clen);
                    while (payload.length() < (unsigned)clen && proxyClientSecure.connected() && millis() < dl) {
                        while (proxyClientSecure.available()) payload += (char)proxyClientSecure.read();
                        yield(); if (s_idleCallback) s_idleCallback(); delay(5);
                    }
                } else {
                    while ((proxyClientSecure.available() || proxyClientSecure.connected()) && millis() < dl) {
                        while (proxyClientSecure.available()) payload += (char)proxyClientSecure.read();
                        yield(); if (s_idleCallback) s_idleCallback(); delay(5);
                    }
                }
                proxyClientSecure.stop();
                if (payload.length() > 0 && parseHebcalResponse(payload, events, maxEvents, eventCount)) return true;
                if (attempt == maxAttempts - 1) return false;
                for (int d = 0; d < 5; d++) { yield(); if (s_idleCallback) s_idleCallback(); delay(100); }
                continue;
            }
            if (!proxyClient.connect(proxyHost.c_str(), proxyPort)) {
                LOG("Hebcal proxy: connect failed");
                if (attempt == maxAttempts - 1) return false;
                continue;
            }
            proxyClient.setTimeout(HEBCAL_TIMEOUT_MS / 1000);
            String req = "GET " + path + " HTTP/1.1\r\nHost: " + proxyHost + "\r\nConnection: close\r\n\r\n";
            proxyClient.print(req);
            proxyClient.flush();
            unsigned long dl = millis() + HEBCAL_TIMEOUT_MS;
            while (!proxyClient.available() && millis() < dl) { yield(); if (s_idleCallback) s_idleCallback(); delay(5); }
            if (!proxyClient.available()) {
                LOG("Hebcal proxy: no response");
                proxyClient.stop();
                if (attempt == maxAttempts - 1) return false;
                continue;
            }
            int code = -1;
            int clen = -1;
            String ln;
            while (proxyClient.connected() && millis() < dl) {
                if (s_idleCallback) s_idleCallback();
                ln = proxyClient.readStringUntil('\n');
                if (ln.length() == 0) break;
                ln.trim();
                if (ln.startsWith("HTTP/")) {
                    int cs = ln.indexOf(' ') + 1;
                    if (cs > 0) code = ln.substring(cs, cs + 3).toInt();
                } else if (ln.startsWith("Content-Length:")) {
                    String c = ln.substring(15); c.trim(); clen = c.toInt();
                }
            }
            if (code != 200) {
                LOGF("Hebcal proxy error: %d", code);
                proxyClient.stop();
                if (attempt == maxAttempts - 1) return false;
                continue;
            }
            String payload;
            if (clen > 0 && clen < 32000) {
                payload.reserve(clen);
                while (payload.length() < (unsigned)clen && proxyClient.connected() && millis() < dl) {
                    while (proxyClient.available()) payload += (char)proxyClient.read();
                    yield(); if (s_idleCallback) s_idleCallback(); delay(5);
                }
            } else {
                while ((proxyClient.available() || proxyClient.connected()) && millis() < dl) {
                    while (proxyClient.available()) payload += (char)proxyClient.read();
                    yield(); if (s_idleCallback) s_idleCallback(); delay(5);
                }
            }
            proxyClient.stop();
            if (payload.length() > 0 && parseHebcalResponse(payload, events, maxEvents, eventCount)) return true;
            if (attempt == maxAttempts - 1) return false;
            for (int d = 0; d < 5; d++) { yield(); if (s_idleCallback) s_idleCallback(); delay(100); }
            continue;
        }
        // ESP8266 direct: plain HTTP to hebcal.com (port 80). No TLS needed.
        // Use HTTPClient which handles chunked transfer-encoding automatically.
        {
            WiFiClient client;
            HTTPClient http;
            const char* host = "www.hebcal.com";
            int pathStart = url.indexOf("/shabbat");
            String path = (pathStart >= 0) ? url.substring(pathStart) : "/shabbat?cfg=json";
            LOGF("Hebcal: GET http://%s%s", host, path.c_str());
            if (!http.begin(client, host, 80, path, false)) {
                LOG("Hebcal: http.begin failed");
                http.end();
                if (attempt == maxAttempts - 1) return false;
                continue;
            }
            http.setTimeout(HEBCAL_TIMEOUT_MS);
            http.addHeader("User-Agent", "ShabbatAlert/1.0");
            http.addHeader("Accept", "application/json");
            int httpCode = http.GET();
            if (httpCode == HTTP_CODE_OK) {
                String payload = http.getString();
                http.end();
                LOGF("Hebcal: HTTP 200, %u bytes, heap=%u", (unsigned)payload.length(), (unsigned)ESP.getFreeHeap());
                if (payload.length() > 0 && parseHebcalResponse(payload, events, maxEvents, eventCount)) {
                    return true;
                }
                LOG("Hebcal JSON parse failed");
            } else {
                LOGF("Hebcal: HTTP %d", httpCode);
                http.end();
            }
            if (attempt == maxAttempts - 1) return false;
            delay(500);
        }
        #else
        WiFiClientSecure client;
        client.setInsecure();
        HTTPClient http;
        http.begin(client, url);
        #endif
        
        #ifndef BOARD_ESP8266
        http.setTimeout(HEBCAL_TIMEOUT_MS);
        http.addHeader("User-Agent", "ShabbatAlert/1.0 (ESP8266; https://github.com/shabbat-alert)");
        int httpCode = http.GET();
        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            http.end();
            return parseHebcalResponse(payload, events, maxEvents, eventCount);
        }
        const char* errMsg = (httpCode == -1) ? "Connection failed" : (httpCode == -4) ? "Not connected" : (httpCode == -5) ? "Connection lost" : "";
        if (httpCode < 0) LOGF("Hebcal API error: %d (%s)", httpCode, errMsg);
        else LOGF("Hebcal API error: %d", httpCode);
        http.end();
        if (httpCode >= 0 || attempt == maxAttempts - 1) return false;
        #endif
    }
    return false;
}

String HebcalClient::buildHebcalURL(time_t startDate, time_t endDate) {
    String base = String(HEBCAL_API_BASE) + "/shabbat?cfg=json";
    
    // Add location
    float lat = Storage::getLatitude();
    float lon = Storage::getLongitude();
    base += "&latitude=" + String(lat, 6);
    base += "&longitude=" + String(lon, 6);
    
    // Add dates
    struct tm* startTm = localtime(&startDate);
    char startStr[20];
    strftime(startStr, sizeof(startStr), "%Y-%m-%d", startTm);
    base += "&start=" + String(startStr);
    
    struct tm* endTm = localtime(&endDate);
    char endStr[20];
    strftime(endStr, sizeof(endStr), "%Y-%m-%d", endTm);
    base += "&end=" + String(endStr);
    
    // Add minhag settings
    int candleOffset = Storage::getCandleOffset();
    base += "&b=" + String(candleOffset);
    
    // Havdalah: M=on (8.5° below horizon) or m=minutes. Custom degrees map to M=on (API supports only 8.5°).
    String havdalahMode = Storage::getHavdalahMode();
    if (havdalahMode == "M" || havdalahMode == "degrees") {
        base += "&M=on";
    } else {
        int havdalahMinutes = Storage::getHavdalahMinutes();
        base += "&m=" + String(havdalahMinutes);
    }
    
    return base;
}

bool HebcalClient::parseHebcalResponse(const String& json, ShabbatEvent* events, int maxEvents, int& eventCount) {
    eventCount = 0;
    
    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, json);
    
    if (error) {
        LOGF("JSON parse error: %s", error.c_str());
        return false;
    }
    
    if (!doc.containsKey("items")) {
        LOG("No 'items' key in Hebcal response");
        return false;
    }
    
    JsonArray items = doc["items"];
    for (JsonObject item : items) {
        if (eventCount >= maxEvents) break;
        
        String category = item["category"] | "";
        String title = item["title"] | "";
        String dateStr = item["date"] | "";
        
        if (category == "candles" || title.indexOf("Candle lighting") >= 0) {
            time_t timestamp = parseHebcalDate(dateStr);
            if (timestamp > 0) {
                events[eventCount].timestamp = timestamp;
                events[eventCount].type = "candles";
                events[eventCount].description = title;
                eventCount++;
            }
        } else if (category == "havdalah" || title.indexOf("Havdalah") >= 0) {
            time_t timestamp = parseHebcalDate(dateStr);
            if (timestamp > 0) {
                events[eventCount].timestamp = timestamp;
                events[eventCount].type = "havdalah";
                events[eventCount].description = title;
                eventCount++;
            }
        }
    }
    
    LOGF("Parsed %d Shabbat events", eventCount);
    return eventCount > 0;
}

bool HebcalClient::parseHebcalFromDoc(JsonDocument& doc, ShabbatEvent* events, int maxEvents, int& eventCount) {
    eventCount = 0;
    if (!doc.containsKey("items")) return false;
    JsonArray items = doc["items"];
    for (JsonObject item : items) {
        if (eventCount >= maxEvents) break;
        String category = item["category"] | "";
        String title = item["title"] | "";
        String dateStr = item["date"] | "";
        if (category == "candles" || title.indexOf("Candle lighting") >= 0) {
            time_t timestamp = parseHebcalDate(dateStr);
            if (timestamp > 0) {
                events[eventCount].timestamp = timestamp;
                events[eventCount].type = "candles";
                events[eventCount].description = title;
                eventCount++;
            }
        } else if (category == "havdalah" || title.indexOf("Havdalah") >= 0) {
            time_t timestamp = parseHebcalDate(dateStr);
            if (timestamp > 0) {
                events[eventCount].timestamp = timestamp;
                events[eventCount].type = "havdalah";
                events[eventCount].description = title;
                eventCount++;
            }
        }
    }
    LOGF("Parsed %d Shabbat events", eventCount);
    return eventCount > 0;
}

time_t HebcalClient::parseHebcalDate(const String& dateStr) {
    // Hebcal date format: "2024-01-12T16:30:00+02:00"
    struct tm timeinfo = {0};
    
    int year, month, day, hour, minute, second, tzOffset = 0;
    char tzSign = '+';
    
    if (sscanf(dateStr.c_str(), "%d-%d-%dT%d:%d:%d%c%d:%d", 
               &year, &month, &day, &hour, &minute, &second, &tzSign, &tzOffset, &tzOffset) >= 6) {
        timeinfo.tm_year = year - 1900;
        timeinfo.tm_mon = month - 1;
        timeinfo.tm_mday = day;
        timeinfo.tm_hour = hour;
        timeinfo.tm_min = minute;
        timeinfo.tm_sec = second;
        
        return mktime(&timeinfo);
    }
    
    return 0;
}

bool HebcalClient::isShabbatNow() {
    // Use Assur Melacha endpoint
    #ifdef BOARD_ESP8266
    if (WiFi.status() != WL_CONNECTED) {
    #else
    if (!WiFi.isConnected()) {
    #endif
        return false;
    }
    
    float lat = Storage::getLatitude();
    float lon = Storage::getLongitude();
    if (std::isnan(lat) || std::isnan(lon) || !std::isfinite(lat) || !std::isfinite(lon)) {
        return false;
    }
    String url = String(HEBCAL_API_BASE) + "/assur?cfg=json&latitude=" + String(lat, 6) + "&longitude=" + String(lon, 6);
    
    #ifdef BOARD_ESP8266
    WiFiClient client;
    HTTPClient http;
    const char* host = "www.hebcal.com";
    int pathStart = url.indexOf("/assur");
    String path = (pathStart >= 0) ? url.substring(pathStart) : "/assur?cfg=json";
    if (!http.begin(client, host, 80, path, false)) {
        http.end();
        return false;
    }
    #else
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.begin(client, url);
    #endif

    http.setTimeout(5000);
    http.addHeader("User-Agent", "ShabbatAlert/1.0 (ESP8266; https://github.com/shabbat-alert)");
    int httpCode = http.GET();
    
    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        DynamicJsonDocument doc(512);
        if (deserializeJson(doc, payload) == DeserializationError::Ok) {
            bool assur = doc["assur"] | false;
            http.end();
            return assur;
        }
    }
    
    http.end();
    return false;
}

bool HebcalClient::getCachedShabbatNow() {
    return cachedShabbatNow;
}

void HebcalClient::refreshShabbatNowCache() {
#ifdef BOARD_ESP8266
    if (WiFi.status() != WL_CONNECTED) return;
#else
    if (!WiFi.isConnected()) return;
#endif
    float lat = Storage::getLatitude();
    float lon = Storage::getLongitude();
    if (std::isnan(lat) || std::isnan(lon) || !std::isfinite(lat) || !std::isfinite(lon)) return;
    cachedShabbatNow = isShabbatNow();
    lastShabbatNowUpdate = millis();
}

String HebcalClient::getCachedNextCandleLighting() {
    if (cacheValid && cachedNextCandles.length() > 0) return cachedNextCandles;
    return "Unknown";
}

String HebcalClient::getCachedNextHavdalah() {
    if (cacheValid && cachedNextHavdalah.length() > 0) return cachedNextHavdalah;
    return "Unknown";
}

String HebcalClient::getNextCandleLighting() {
    if (cacheValid && cachedNextCandles.length() > 0) return cachedNextCandles;
    time_t now = TimeSync::getNow();
    if (now == 0) return "Unknown";
    
    time_t endDate = now + (14 * 24 * 3600); // 2 weeks ahead
    ShabbatEvent events[10];
    int count = 0;
    
    if (fetchShabbatTimes(now, endDate, events, 10, count)) {
        for (int i = 0; i < count; i++) {
            if (events[i].type == "candles" && events[i].timestamp > now) {
                struct tm* tm = localtime(&events[i].timestamp);
                char buffer[30];
                strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", tm);
                String s(buffer);
                cachedNextCandles = s;
                cacheValid = true;
                return s;
            }
        }
    }
    
    return "Unknown";
}

String HebcalClient::getNextHavdalah() {
    if (cacheValid && cachedNextHavdalah.length() > 0) return cachedNextHavdalah;
    time_t now = TimeSync::getNow();
    if (now == 0) return "Unknown";
    
    time_t endDate = now + (14 * 24 * 3600);
    ShabbatEvent events[10];
    int count = 0;
    
    if (fetchShabbatTimes(now, endDate, events, 10, count)) {
        for (int i = 0; i < count; i++) {
            if (events[i].type == "havdalah" && events[i].timestamp > now) {
                struct tm* tm = localtime(&events[i].timestamp);
                char buffer[30];
                strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", tm);
                String s(buffer);
                cachedNextHavdalah = s;
                cacheValid = true;
                return s;
            }
        }
    }
    
    return "Unknown";
}

