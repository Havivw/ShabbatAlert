#ifndef HEBCAL_CLIENT_H
#define HEBCAL_CLIENT_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ESP8266WiFi.h>
#include "config.h"
#include "event_kinds.h"
#include "storage.h"
#include "time_sync.h"

struct ShabbatEvent {
    time_t timestamp;
    ShabbatKind kind;
    char title[16];   // "Shabbat", "Shavuot", "Sukkot I", etc.  Empty = unknown.
};

class HebcalClient {
public:
    static bool fetchShabbatTimes(time_t startDate, time_t endDate, ShabbatEvent* events, int maxEvents, int& eventCount);
    static String getNextCandleLighting();
    static String getNextHavdalah();
    /** Cache-only: return next candle/havdalah without triggering HTTPS. Use in /api/status so settings page loads reliably. */
    static String getCachedNextCandleLighting();
    static String getCachedNextHavdalah();
    /** Null if unknown; else pointer to internal "YYYY-MM-DD HH:MM" (valid until next schedule refresh). */
    static const char* peekCachedNextCandlesCStr();
    /** Set cached next candle/havdalah strings so status/dashboard don't trigger HTTPS. */
    static void setCachedNextTimes(const String& nextCandles, const String& nextHavdalah);
    /** Cache-only: return last known "Shabbat now" (no HTTPS). Use in /api/status. */
    static bool getCachedShabbatNow();
    /** Set Shabbat-now cache from schedule (e.g. Scheduler::isNowInShabbatWindow()). */
    static void setCachedShabbatNow(bool value);
    /** Optional callback run during long waits (e.g. so web server can handle /api/status). Set from main. */
    static void setIdleCallback(void (*callback)());
    
private:
    /** Build Hebcal /hebcal query URL into out (NUL-terminated). Returns false if truncated. */
    static bool buildHebcalURL(time_t startDate, time_t endDate, char* out, size_t outCap);
    static String cachedNextCandles;
    static String cachedNextHavdalah;
    static bool cacheValid;
    static bool cachedShabbatNow;
    static bool parseHebcalResponse(const char* json, size_t jsonLen, ShabbatEvent* events, int maxEvents, int& eventCount);
    static bool parseHebcalFromDoc(JsonDocument& doc, ShabbatEvent* events, int maxEvents, int& eventCount);
    static time_t parseHebcalDate(const String& dateStr);
    static void (*s_idleCallback)();
};

#endif // HEBCAL_CLIENT_H

