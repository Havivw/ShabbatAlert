#ifndef HEBCAL_CLIENT_H
#define HEBCAL_CLIENT_H

#include <Arduino.h>
#include <ArduinoJson.h>
#ifdef BOARD_ESP8266
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ESP8266WiFi.h>
#else
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WiFi.h>
#endif
#include "config.h"
#include "storage.h"
#include "time_sync.h"

struct ShabbatEvent {
    time_t timestamp;
    String type;  // "candles" or "havdalah"
    String description;
};

class HebcalClient {
public:
    static bool fetchShabbatTimes(time_t startDate, time_t endDate, ShabbatEvent* events, int maxEvents, int& eventCount);
    static bool isShabbatNow();
    static String getNextCandleLighting();
    static String getNextHavdalah();
    /** Cache-only: return next candle/havdalah without triggering HTTPS. Use in /api/status so settings page loads reliably. */
    static String getCachedNextCandleLighting();
    static String getCachedNextHavdalah();
    /** Set cached next candle/havdalah strings so status/dashboard don't trigger HTTPS. */
    static void setCachedNextTimes(const String& nextCandles, const String& nextHavdalah);
    /** Cache-only: return last known "Shabbat now" (no HTTPS). Use in /api/status. */
    static bool getCachedShabbatNow();
    /** Update Shabbat-now cache from loop (calls isShabbatNow when WiFi + location valid). */
    static void refreshShabbatNowCache();
    /** Optional callback run during long waits (e.g. so web server can handle /api/status). Set from main. */
    static void setIdleCallback(void (*callback)());
    
private:
    static String buildHebcalURL(time_t startDate, time_t endDate);
    static String cachedNextCandles;
    static String cachedNextHavdalah;
    static bool cacheValid;
    static bool cachedShabbatNow;
    static unsigned long lastShabbatNowUpdate;
    static bool parseHebcalResponse(const String& json, ShabbatEvent* events, int maxEvents, int& eventCount);
    static bool parseHebcalFromDoc(JsonDocument& doc, ShabbatEvent* events, int maxEvents, int& eventCount);
    static time_t parseHebcalDate(const String& dateStr);
    static void (*s_idleCallback)();
};

#endif // HEBCAL_CLIENT_H

