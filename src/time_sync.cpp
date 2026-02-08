#include "time_sync.h"
#include "logger.h"

bool TimeSync::timeSet = false;
unsigned long TimeSync::millisAtSync = 0;
time_t TimeSync::timeAtSync = 0;

void TimeSync::init() {
    setTimezone(Storage::getTimezone());
}

bool TimeSync::sync() {
    #ifdef BOARD_ESP8266
    if (WiFi.status() != WL_CONNECTED) {
    #else
    if (!WiFi.isConnected()) {
    #endif
        LOG("Cannot sync time: WiFi not connected");
        return false;
    }
    
    configTime(0, 0, NTP_SERVER);
    
    LOG("Waiting for NTP time sync...");
    int attempts = 0;
    while (!timeSet && attempts < 20) {
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            timeSet = true;
            millisAtSync = millis();
            timeAtSync = mktime(&timeinfo);
            LOGF("Time synced: %s", getFormattedDateTime().c_str());
            return true;
        }
        delay(500);
        attempts++;
    }
    
    LOG("NTP sync failed");
    return false;
}

time_t TimeSync::getNow() {
    if (!timeSet) {
        return 0;
    }
    
    unsigned long elapsed = millis() - millisAtSync;
    return timeAtSync + (elapsed / 1000);
}

String TimeSync::getFormattedTime() {
    time_t now = getNow();
    if (now == 0) {
        return "Not set";
    }
    
    struct tm* timeinfo = localtime(&now);
    char buffer[20];
    strftime(buffer, sizeof(buffer), "%H:%M:%S", timeinfo);
    return String(buffer);
}

String TimeSync::getFormattedDateTime() {
    time_t now = getNow();
    if (now == 0) {
        return "Not set";
    }
    
    struct tm* timeinfo = localtime(&now);
    char buffer[50];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
    return String(buffer);
}

bool TimeSync::isTimeSet() {
    return timeSet;
}

unsigned long TimeSync::getMillisOffset() {
    return millisAtSync;
}

void TimeSync::applyTimezone(const String& tz) {
    setTimezone(tz);
}

void TimeSync::setTimezone(const String& tz) {
    // Set timezone offset based on common timezones
    // This is a simplified approach - for production, use proper timezone library
    if (tz.indexOf("Jerusalem") >= 0) {
        setenv("TZ", "IST-2IDT,M3.4.0/26,M10.5.0", 1);
    } else if (tz.indexOf("New_York") >= 0) {
        setenv("TZ", "EST5EDT,M3.2.0,M11.1.0", 1);
    } else if (tz.indexOf("Los_Angeles") >= 0) {
        setenv("TZ", "PST8PDT,M3.2.0,M11.1.0", 1);
    } else if (tz.indexOf("London") >= 0) {
        setenv("TZ", "GMT0BST,M3.5.0,M10.5.0", 1);
    } else {
        // Default to UTC
        setenv("TZ", "UTC0", 1);
    }
    tzset();
}

