#include "hebcal_client.h"
#include "logger.h"
#include "time_sync.h"
#include "storage.h"
#include "diag.h"
#include <cmath>
#include <ctime>
#include <cstdio>
#include <cstring>

#ifndef HEBCAL_URL_BUF
#define HEBCAL_URL_BUF 384
#endif
#ifndef HEBCAL_MAX_BODY
#define HEBCAL_MAX_BODY 32000
#endif

String HebcalClient::cachedNextCandles;
String HebcalClient::cachedNextHavdalah;
bool HebcalClient::cacheValid = false;
bool HebcalClient::cachedShabbatNow = false;
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
    if (WiFi.status() != WL_CONNECTED) {
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
    
    char urlBuf[HEBCAL_URL_BUF];
    if (!buildHebcalURL(startDate, endDate, urlBuf, sizeof(urlBuf))) {
        LOG("Hebcal: URL buffer overflow");
        return false;
    }
    LOGF("Fetching Hebcal data: %s", urlBuf);
    
    int maxAttempts = Storage::getHebcalMaxAttempts();
    if (maxAttempts > 5) maxAttempts = 5;
    if (maxAttempts < 1) maxAttempts = 1;
    for (int attempt = 0; attempt < maxAttempts; attempt++) {
        if (attempt > 0) {
            for (int d = 0; d < 15; d++) { yield(); if (s_idleCallback) s_idleCallback(); delay(100); }
            LOG("Retrying Hebcal request...");
        }
        LOGF("Hebcal dbg: freeHeap=%u", (unsigned)ESP.getFreeHeap());
        if (ESP.getFreeHeap() < 12000) {
            LOG("Hebcal dbg: low heap, waiting");
            for (int w = 0; w < 5; w++) { yield(); delay(200); }
            LOGF("Hebcal dbg: freeHeap after wait=%u", (unsigned)ESP.getFreeHeap());
        }
        String proxyUrl = Storage::getHebcalProxyURL();
        proxyUrl.trim();
        if (proxyUrl.length() > 0) {
            const char* hebcalPath = "/hebcal?v=1&cfg=json";
            const char* hostSlash = strstr(urlBuf, "//");
            if (hostSlash) {
                hostSlash = strchr(hostSlash + 2, '/');
                if (hostSlash) hebcalPath = hostSlash;
            }
            String proxyHost;
            int proxyPort = 80;
            if (proxyUrl.startsWith("http://")) proxyUrl = proxyUrl.substring(7);
            else if (proxyUrl.startsWith("https://")) { proxyUrl = proxyUrl.substring(8); proxyPort = 443; }
            int colon = proxyUrl.indexOf(':');
            int slashPos = proxyUrl.indexOf('/');
            if (colon > 0 && (slashPos < 0 || colon < slashPos)) {
                proxyHost = proxyUrl.substring(0, colon);
                proxyPort = proxyUrl.substring(colon + 1, slashPos > 0 ? slashPos : proxyUrl.length()).toInt();
                if (proxyPort <= 0) proxyPort = 80;
            } else {
                proxyHost = slashPos > 0 ? proxyUrl.substring(0, slashPos) : proxyUrl;
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
                String req = String("GET ") + hebcalPath + " HTTP/1.1\r\nHost: " + proxyHost + "\r\nConnection: close\r\n\r\n";
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
                if (clen > HEBCAL_MAX_BODY) clen = HEBCAL_MAX_BODY;
                String payload;
                char tmpBuf[128];
                if (clen > 0 && clen <= HEBCAL_MAX_BODY) {
                    payload.reserve(clen);
                    while (payload.length() < (unsigned)clen && proxyClientSecure.connected() && millis() < dl) {
                        int avail = proxyClientSecure.available();
                        if (avail > 0) {
                            int toRead = avail > (int)sizeof(tmpBuf) ? (int)sizeof(tmpBuf) : avail;
                            int got = proxyClientSecure.readBytes(tmpBuf, toRead);
                            if (got > 0) payload.concat(tmpBuf, (unsigned)got);
                        }
                        yield(); if (s_idleCallback) s_idleCallback(); delay(5);
                    }
                } else {
                    payload.reserve(2048);
                    while ((proxyClientSecure.available() || proxyClientSecure.connected()) && millis() < dl) {
                        int avail = proxyClientSecure.available();
                        if (avail > 0) {
                            int toRead = avail > (int)sizeof(tmpBuf) ? (int)sizeof(tmpBuf) : avail;
                            int got = proxyClientSecure.readBytes(tmpBuf, toRead);
                            if (got > 0) payload.concat(tmpBuf, (unsigned)got);
                            if (payload.length() >= HEBCAL_MAX_BODY) break;
                        }
                        yield(); if (s_idleCallback) s_idleCallback(); delay(5);
                    }
                }
                proxyClientSecure.stop();
                if (payload.length() > 0 && parseHebcalResponse(payload.c_str(), payload.length(), events, maxEvents, eventCount)) {
                    payload.reserve(0);
                    Diag::log("hbcl ok n=%d (pxs)", eventCount);
                    return true;
                }
                payload.reserve(0);
                if (attempt == maxAttempts - 1) { Diag::log("hbcl FAIL (pxs)"); return false; }
                for (int d = 0; d < 5; d++) { yield(); if (s_idleCallback) s_idleCallback(); delay(100); }
                continue;
            }
            if (!proxyClient.connect(proxyHost.c_str(), proxyPort)) {
                LOG("Hebcal proxy: connect failed");
                if (attempt == maxAttempts - 1) return false;
                continue;
            }
            proxyClient.setTimeout(HEBCAL_TIMEOUT_MS / 1000);
            String req = String("GET ") + hebcalPath + " HTTP/1.1\r\nHost: " + proxyHost + "\r\nConnection: close\r\n\r\n";
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
            if (clen > HEBCAL_MAX_BODY) clen = HEBCAL_MAX_BODY;
            String payload;
            char tmpBuf[128];
            if (clen > 0 && clen <= HEBCAL_MAX_BODY) {
                payload.reserve(clen);
                while (payload.length() < (unsigned)clen && proxyClient.connected() && millis() < dl) {
                    int avail = proxyClient.available();
                    if (avail > 0) {
                        int toRead = avail > (int)sizeof(tmpBuf) ? (int)sizeof(tmpBuf) : avail;
                        int got = proxyClient.readBytes(tmpBuf, toRead);
                        if (got > 0) payload.concat(tmpBuf, (unsigned)got);
                    }
                    yield(); if (s_idleCallback) s_idleCallback(); delay(5);
                }
            } else {
                payload.reserve(2048);
                while ((proxyClient.available() || proxyClient.connected()) && millis() < dl) {
                    int avail = proxyClient.available();
                    if (avail > 0) {
                        int toRead = avail > (int)sizeof(tmpBuf) ? (int)sizeof(tmpBuf) : avail;
                        int got = proxyClient.readBytes(tmpBuf, toRead);
                        if (got > 0) payload.concat(tmpBuf, (unsigned)got);
                        if (payload.length() >= HEBCAL_MAX_BODY) break;
                    }
                    yield(); if (s_idleCallback) s_idleCallback(); delay(5);
                }
            }
            proxyClient.stop();
            if (payload.length() > 0 && parseHebcalResponse(payload.c_str(), payload.length(), events, maxEvents, eventCount)) {
                payload.reserve(0);
                Diag::log("hbcl ok n=%d (px)", eventCount);
                return true;
            }
            payload.reserve(0);
            if (attempt == maxAttempts - 1) { Diag::log("hbcl FAIL (px)"); return false; }
            for (int d = 0; d < 5; d++) { yield(); if (s_idleCallback) s_idleCallback(); delay(100); }
            continue;
        }
        // ESP8266 direct: plain HTTP to hebcal.com (port 80). No TLS needed.
        // Use HTTPClient which handles chunked transfer-encoding automatically.
        {
            WiFiClient client;
            HTTPClient http;
            const char* host = "www.hebcal.com";
            const char* path = "/hebcal?v=1&cfg=json";
            const char* slash = strstr(urlBuf, "//");
            if (slash) {
                slash = strchr(slash + 2, '/');
                if (slash) path = slash;
            }
            LOGF("Hebcal: GET http://%s%s", host, path);
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
                DynamicJsonDocument doc(4096);
                DeserializationError err = deserializeJson(doc, http.getStream());
                http.end();
                LOGF("Hebcal: HTTP 200, heap=%u", (unsigned)ESP.getFreeHeap());
                if (err == DeserializationError::Ok && parseHebcalFromDoc(doc, events, maxEvents, eventCount)) {
                    Diag::log("hbcl ok n=%d", eventCount);
                    return true;
                }
                LOG("Hebcal JSON parse failed");
            } else {
                LOGF("Hebcal: HTTP %d", httpCode);
                http.end();
            }
            if (attempt == maxAttempts - 1) { Diag::log("hbcl FAIL c=%d", httpCode); return false; }
            delay(500);
        }
    }
    return false;
}

bool HebcalClient::buildHebcalURL(time_t startDate, time_t endDate, char* out, size_t outCap) {
    const time_t minValidStart = 1577836800;  // 2020-01-01
    if (startDate < minValidStart) {
        startDate = minValidStart;
        endDate = startDate + (14 * 24 * 3600);
    }
    float lat = Storage::getLatitude();
    float lon = Storage::getLongitude();
    struct tm* startTm = gmtime(&startDate);
    if (!startTm) startTm = localtime(&startDate);
    char startStr[20];
    if (startTm) strftime(startStr, sizeof(startStr), "%Y-%m-%d", startTm);
    else snprintf(startStr, sizeof(startStr), "2020-01-01");
    struct tm* endTm = gmtime(&endDate);
    if (!endTm) endTm = localtime(&endDate);
    char endStr[20];
    if (endTm) strftime(endStr, sizeof(endStr), "%Y-%m-%d", endTm);
    else snprintf(endStr, sizeof(endStr), "2020-01-15");
    int candleOffset = Storage::getCandleOffset();
    // maj=on is CRITICAL: without it, Hebcal returns candle/havdalah entries
    // ONLY for Friday/Saturday — every yom tov candle lighting (Erev Shavuot,
    // Erev Pesach, Rosh Hashana eve, etc.) is silently missing.  That defeats
    // the whole multi-event refactor for any holiday not falling on Friday.
    //
    // i=on (Israel mode) is detected from EITHER the configured timezone OR
    // the physical lat/lon being inside Israel's borders.  The dual check is
    // important: if a user manually edits their timezone or geocoding fails
    // to return "Israel" as country (e.g. Nominatim returns Hebrew/Arabic
    // names), the lat/lon fallback still gets it right.  Without i=on,
    // Hebcal returns diaspora data for Israeli locations: extra "Day 2" yom
    // tov candle lighting entries that don't exist in Israel.  When yom tov
    // falls adjacent to Shabbat (e.g. Shavuot 2026), that extra candle
    // produced an asymmetric candle/havdalah count that left the device's
    // "Shabbat now" indicator stuck for the entire holiday-into-Shabbat run.
    String tz = Storage::getTimezone();
    bool israelTz  = (tz.indexOf("Jerusalem") >= 0);
    bool israelGeo = (lat >= 29.4f && lat <= 33.4f && lon >= 34.2f && lon <= 35.9f);
    bool israelMode = israelTz || israelGeo;
    int n = snprintf(out, outCap,
                     "%s/hebcal?v=1&cfg=json&c=on&maj=on&leyning=off&latitude=%.6f&longitude=%.6f&start=%s&end=%s&b=%d%s",
                     HEBCAL_API_BASE, lat, lon, startStr, endStr, candleOffset,
                     israelMode ? "&i=on" : "");
    if (n < 0 || (size_t)n >= outCap) return false;
    String havdalahMode = Storage::getHavdalahMode();
    size_t L = strlen(out);
    if (havdalahMode == "M" || havdalahMode == "degrees") {
        // Hebcal v1 only accepts &M=on (their default 8.5° nightfall) — there
        // is no parameter for custom degrees in this API.  If the user picked
        // a non-default degrees value, surface that we can't honor it so they
        // don't keep wondering why havdalah doesn't match their setting.
        if (havdalahMode == "degrees") {
            float d = Storage::getHavdalahDegrees();
            if (d > 0.0f && (d < 8.4f || d > 8.6f)) {
                static bool warnedOnce = false;
                if (!warnedOnce) {
                    warnedOnce = true;
                    Diag::log("hbcl warn deg=%d.%d ignored", (int)d, ((int)(d*10))%10);
                }
            }
        }
        n = snprintf(out + L, outCap - L, "&M=on");
    } else {
        n = snprintf(out + L, outCap - L, "&m=%d", Storage::getHavdalahMinutes());
    }
    if (n < 0 || L + (size_t)n >= outCap) return false;
    return strlen(out) < outCap;
}

bool HebcalClient::parseHebcalResponse(const char* json, size_t jsonLen, ShabbatEvent* events, int maxEvents, int& eventCount) {
    eventCount = 0;
    if (!json || jsonLen == 0) return false;

    DynamicJsonDocument doc(4096);
    (void)jsonLen;  // NUL-terminated buffer
    DeserializationError error = deserializeJson(doc, json);
    if (error) {
        LOGF("JSON parse error: %s", error.c_str());
        return false;
    }
    // Delegate to the shared parser so proxy and direct paths produce identical
    // results — including the holiday-title labelling.
    return parseHebcalFromDoc(doc, events, maxEvents, eventCount);
}

// Day-of-year derived from broken-down time (no timezone-dependent helpers).
// Sufficient for "is this event on the same date as that one" within a 14-day
// window — wraparound at year boundary handled by also considering ±1.
static int dayKeyFromUtc(time_t utc, long tzOffsetSec) {
    time_t local = utc + tzOffsetSec;
    struct tm lt;
    gmtime_r(&local, &lt);
    return lt.tm_year * 400 + lt.tm_yday;
}

// Extract a comparable day key from a Hebcal date string of either form:
//   "2026-05-21"              (date-only — used for holiday entries)
//   "2026-05-21T19:17:00+03:00"  (full timestamp — used for candle/havdalah)
// Returns 0 on parse failure.  Using a year×400+day-of-year style key keeps
// it comparable with dayKeyFromUtc() above.
static int dayKeyFromHebcalString(const String& dateStr) {
    int year, month, day;
    if (sscanf(dateStr.c_str(), "%d-%d-%d", &year, &month, &day) != 3) return 0;
    // Convert to days-since-epoch then back to (year, yday) the cheap way:
    // build a struct tm and let mktime_r-style fields work for us.  Actually,
    // a far simpler key: year*10000 + month*100 + day.  Unique per local date
    // and trivially comparable with ±1 (with month-rollover handled by also
    // accepting adjacent month-end values).
    return year * 10000 + month * 100 + day;
}

// Same key shape, but from a UTC timestamp + local offset (for matching against
// the holiday strings above).
static int dayKeyFromUtcLocalDate(time_t utc, long tzOffsetSec) {
    time_t local = utc + tzOffsetSec;
    struct tm lt;
    gmtime_r(&local, &lt);
    return (lt.tm_year + 1900) * 10000 + (lt.tm_mon + 1) * 100 + lt.tm_mday;
}

bool HebcalClient::parseHebcalFromDoc(JsonDocument& doc, ShabbatEvent* events, int maxEvents, int& eventCount) {
    eventCount = 0;
    if (!doc.containsKey("items")) return false;
    JsonArray items = doc["items"];

    // First pass: collect candle/havdalah events.
    for (JsonObject item : items) {
        if (eventCount >= maxEvents) break;
        String category = item["category"] | "";
        String title = item["title"] | "";
        String dateStr = item["date"] | "";
        bool isCandle = (category == "candles" || title.indexOf("Candle lighting") >= 0);
        bool isHavd   = (category == "havdalah" || title.indexOf("Havdalah") >= 0);
        if (!isCandle && !isHavd) continue;
        time_t timestamp = parseHebcalDate(dateStr);
        if (timestamp <= 0) continue;
        events[eventCount].timestamp = timestamp;
        events[eventCount].kind = isCandle ? ShabbatKind::Candles : ShabbatKind::Havdalah;
        events[eventCount].title[0] = 0;
        eventCount++;
    }

    // Second pass: associate holiday names with candle/havdalah events using
    // semantically-correct rules (the previous ±1-day window was too loose —
    // it labelled e.g. Saturday-night havdalah as "Shavuot" because that
    // havdalah is within ±1 day of Shavuot, even though in Israel Shavuot is
    // only 1 day and ended Friday at sunset).
    //
    // Rules:
    //   CANDLE  → labelled with holiday name only if it BEGINS the holiday:
    //             same date as an "Erev XXX" entry, OR the day BEFORE a
    //             holiday-day entry (when Hebcal omits the Erev for that holiday).
    //   HAVDALAH → labelled with holiday name only if it ENDS the holiday on
    //             that same date.
    //   Everything else falls through to the default "Shabbat" label below.
    long tzOff = TimeSync::getTimezoneOffsetSeconds();

    // Collect holiday entries into a small local table.  Cap at 12 to cover
    // worst-case Sukkot (Erev + 7 days + Shmini Atzeret + Simchat Torah).
    struct HInfo { int date; char title[16]; bool isErev; };
    HInfo holidays[12];
    int holidayCount = 0;
    for (JsonObject item : items) {
        if (holidayCount >= 12) break;
        String category = item["category"] | "";
        if (category != "holiday") continue;
        String title = item["title"] | "";
        String dateStr = item["date"] | "";
        if (title.length() == 0) continue;
        int hkey = dayKeyFromHebcalString(dateStr);
        if (hkey == 0) continue;
        bool isErev = title.startsWith("Erev ");
        int start = isErev ? 5 : 0;
        size_t copyLen = title.length() - start;
        if (copyLen >= sizeof(holidays[0].title)) copyLen = sizeof(holidays[0].title) - 1;
        holidays[holidayCount].date = hkey;
        holidays[holidayCount].isErev = isErev;
        memcpy(holidays[holidayCount].title, title.c_str() + start, copyLen);
        holidays[holidayCount].title[copyLen] = 0;
        holidayCount++;
    }

    // dayKey + 1 with naive YYYYMMDD math — close enough inside Hebcal's
    // 2-week window (month rollover hits 1 candle per ~30 days and is harmless
    // since we also accept the same-date Erev match).
    auto dateLabel = [&](int ekey, ShabbatKind kind, char* out, size_t outCap) {
        if (kind == ShabbatKind::Candles) {
            // 1) Erev XXX entry on the same date → this candle begins the holiday.
            for (int h = 0; h < holidayCount; h++) {
                if (holidays[h].isErev && holidays[h].date == ekey) {
                    strncpy(out, holidays[h].title, outCap - 1);
                    out[outCap - 1] = 0;
                    return true;
                }
            }
            // 2) No Erev entry — but a holiday-day entry exists tomorrow.
            for (int h = 0; h < holidayCount; h++) {
                if (!holidays[h].isErev && holidays[h].date == ekey + 1) {
                    strncpy(out, holidays[h].title, outCap - 1);
                    out[outCap - 1] = 0;
                    return true;
                }
            }
        } else {  // Havdalah
            // Holiday-day entry on the same date → this havdalah ends that
            // holiday.  Otherwise this is a plain Shabbat havdalah.
            for (int h = 0; h < holidayCount; h++) {
                if (!holidays[h].isErev && holidays[h].date == ekey) {
                    strncpy(out, holidays[h].title, outCap - 1);
                    out[outCap - 1] = 0;
                    return true;
                }
            }
        }
        return false;
    };

    for (int i = 0; i < eventCount; i++) {
        if (events[i].title[0] != 0) continue;
        int ekey = dayKeyFromUtcLocalDate(events[i].timestamp, tzOff);
        dateLabel(ekey, events[i].kind, events[i].title, sizeof(events[i].title));
    }

    // Fill remaining unlabelled events with "Shabbat" (the common case).
    for (int i = 0; i < eventCount; i++) {
        if (events[i].title[0] == 0) {
            strcpy(events[i].title, "Shabbat");
        }
    }

    LOGF("Parsed %d Shabbat/holiday events", eventCount);
    return eventCount > 0;
}

// Pure-arithmetic civil-date → days-since-Unix-epoch (Howard Hinnant).
// Touches no global state (no setenv/tzset/mktime) so it can't disturb the
// installed TZ rule or the running SNTP service.
static long daysFromCivil(int y, int m, int d) {
    if (m <= 2) y -= 1;
    int era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return (long)era * 146097L + (long)doe - 719468L;
}

time_t HebcalClient::parseHebcalDate(const String& dateStr) {
    // Hebcal date format: "2024-01-12T16:30:00+02:00" (local-for-the-location
    // wall time + UTC offset).  We need a UTC epoch so getNow() comparisons
    // work regardless of the device's display TZ.
    int year, month, day, hour, minute, second;
    int tzOffsetH = 0, tzOffsetM = 0;
    char tzSign = '+';

    int parsed = sscanf(dateStr.c_str(), "%d-%d-%dT%d:%d:%d%c%d:%d",
                        &year, &month, &day, &hour, &minute, &second,
                        &tzSign, &tzOffsetH, &tzOffsetM);
    if (parsed < 6) return 0;
    if (parsed < 7) {  // no offset present → treat as UTC
        tzSign = '+';
        tzOffsetH = 0;
        tzOffsetM = 0;
    }

    long offsetSec = (tzOffsetH * 3600L) + (tzOffsetM * 60L);
    if (tzSign == '-') offsetSec = -offsetSec;

    long days = daysFromCivil(year, month, day);
    long long epochLocal = (long long)days * 86400LL + hour * 3600L + minute * 60L + second;
    return (time_t)(epochLocal - offsetSec);
}

bool HebcalClient::getCachedShabbatNow() {
    return cachedShabbatNow;
}

void HebcalClient::setCachedShabbatNow(bool value) {
    cachedShabbatNow = value;
}

String HebcalClient::getCachedNextCandleLighting() {
    if (cacheValid && cachedNextCandles.length() > 0) return cachedNextCandles;
    return "Unknown";
}

String HebcalClient::getCachedNextHavdalah() {
    if (cacheValid && cachedNextHavdalah.length() > 0) return cachedNextHavdalah;
    return "Unknown";
}

const char* HebcalClient::peekCachedNextCandlesCStr() {
    if (!cacheValid || cachedNextCandles.length() == 0) return nullptr;
    return cachedNextCandles.c_str();
}

String HebcalClient::getNextCandleLighting() {
    if (cacheValid && cachedNextCandles.length() > 0) return cachedNextCandles;
    time_t now = TimeSync::getNow();
    if (now == 0) return "Unknown";
    
    time_t endDate = now + (14 * 24 * 3600); // 2 weeks ahead
    ShabbatEvent events[10];
    int count = 0;
    
    if (fetchShabbatTimes(now, endDate, events, 10, count)) {
        long tzOffset = TimeSync::getTimezoneOffsetSeconds();
        for (int i = 0; i < count; i++) {
            if (events[i].kind == ShabbatKind::Candles && events[i].timestamp > now) {
                time_t local = events[i].timestamp + tzOffset;
                struct tm* tm = gmtime(&local);
                if (tm) {
                    char buffer[30];
                    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", tm);
                    cachedNextCandles = String(buffer);
                    cacheValid = true;
                    return cachedNextCandles;
                }
                break;
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
        long tzOffset = TimeSync::getTimezoneOffsetSeconds();
        for (int i = 0; i < count; i++) {
            if (events[i].kind == ShabbatKind::Havdalah && events[i].timestamp > now) {
                time_t local = events[i].timestamp + tzOffset;
                struct tm* tm = gmtime(&local);
                if (tm) {
                    char buffer[30];
                    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", tm);
                    cachedNextHavdalah = String(buffer);
                    cacheValid = true;
                    return cachedNextHavdalah;
                }
                break;
            }
        }
    }
    
    return "Unknown";
}

