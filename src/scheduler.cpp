#include "scheduler.h"
#include "logger.h"
#include "config.h"
#include "storage.h"
#include "wifi_manager.h"
#include <ctime>

AlertEvent Scheduler::events[20];
int Scheduler::eventCount = 0;
ShabbatEvent Scheduler::shabbatCache[10];
int Scheduler::shabbatCacheCount = 0;
unsigned long Scheduler::lastRefresh = 0;
bool Scheduler::pendingRefresh = false;
unsigned long Scheduler::firstWifiConnectedAt = 0;
int Scheduler::consecutiveFailCount = 0;
unsigned long Scheduler::lastFailAt = 0;

#define HEBCAL_FAIL_BACKOFF_MS 300000   // 5 min after 5 consecutive failures before retrying
#define HEBCAL_RETRY_COOLDOWN_MS 120000 // 2 min after any failure before retrying (avoid hammering)

void Scheduler::init() {
    eventCount = 0;
    lastRefresh = 0;
    pendingRefresh = false;
    firstWifiConnectedAt = 0;
    consecutiveFailCount = 0;
    lastFailAt = 0;
    // First Hebcal fetch runs from update() only after WiFi has been connected for HEBCAL_DELAY_AFTER_WIFI_MS
}

void Scheduler::requestRefresh() {
    pendingRefresh = true;
}

void Scheduler::update() {
    // Track when WiFi first connected (delay countdown starts from this, not boot)
    if (WiFiManager::isConnected()) {
        if (firstWifiConnectedAt == 0) firstWifiConnectedAt = millis();
    } else {
        firstWifiConnectedAt = 0;
    }

    if (shouldRefresh()) {
        refreshSchedule();
    }
    if (pendingRefresh && WiFiManager::isConnected()) {
        pendingRefresh = false;
        refreshSchedule();
    }
}

void Scheduler::refreshSchedule() {
    if (!TimeSync::isTimeSet()) {
        LOG("Cannot refresh schedule: time not set");
        return;
    }
    
    if (!Storage::isConfigured()) {
        LOG("Cannot refresh schedule: not configured");
        return;
    }
    
    time_t now = TimeSync::getNow();
    time_t endDate = now + (14 * 24 * 3600); // 2 weeks ahead
    
    ShabbatEvent shabbatEvents[10];
    int shabbatCount = 0;
    
    if (!HebcalClient::fetchShabbatTimes(now, endDate, shabbatEvents, 10, shabbatCount)) {
        LOG("Failed to fetch Shabbat times, using cache if available");
        consecutiveFailCount++;
        lastFailAt = millis();
        return;
    }
    consecutiveFailCount = 0;
    
    // Cache next candle/havdalah strings so /api/status doesn't trigger HTTPS (fixes settings load)
    String nextCandles = "Unknown";
    String nextHavdalah = "Unknown";
    for (int i = 0; i < shabbatCount; i++) {
        if (shabbatEvents[i].type == "candles" && shabbatEvents[i].timestamp > now) {
            struct tm* tm = localtime(&shabbatEvents[i].timestamp);
            char buf[30];
            strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", tm);
            nextCandles = String(buf);
            break;
        }
    }
    for (int i = 0; i < shabbatCount; i++) {
        if (shabbatEvents[i].type == "havdalah" && shabbatEvents[i].timestamp > now) {
            struct tm* tm = localtime(&shabbatEvents[i].timestamp);
            char buf[30];
            strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", tm);
            nextHavdalah = String(buf);
            break;
        }
    }
    HebcalClient::setCachedNextTimes(nextCandles, nextHavdalah);
    
    // Cache raw shabbat events for later rebuild (e.g. when user changes alert checkboxes)
    shabbatCacheCount = shabbatCount;
    for (int i = 0; i < shabbatCount && i < 10; i++) {
        shabbatCache[i] = shabbatEvents[i];
    }
    
    // Build alert events from shabbat data
    rebuildAlertEvents();
    
    lastRefresh = millis();
    LOGF("Schedule refreshed: %d events", eventCount);
}

void Scheduler::rebuildAlertEvents() {
    eventCount = 0;
    for (int i = 0; i < shabbatCacheCount; i++) {
        addAlertEvents(shabbatCache[i], i);
    }
    sortEvents();
    
    // Update cache
    String scheduleJSON = getScheduleJSON();
    Storage::setLastSchedule(scheduleJSON);
    Storage::setLastScheduleTime(millis());
    
}

void Scheduler::addAlertEvents(const ShabbatEvent& shabbatEvent, int index) {
    if (shabbatEvent.type == "candles") {
        int alerts = Storage::getCandleAlerts();
        // bit 2 = 45 min, bit 1 = 30 min, bit 0 = 18 min
        if ((alerts & 4) && eventCount < 20) {
            events[eventCount].timestamp = shabbatEvent.timestamp - (45 * 60);
            events[eventCount].type = "candles-45";
            events[eventCount].triggered = false;
            eventCount++;
        }
        if ((alerts & 2) && eventCount < 20) {
            events[eventCount].timestamp = shabbatEvent.timestamp - (30 * 60);
            events[eventCount].type = "candles-30";
            events[eventCount].triggered = false;
            eventCount++;
        }
        if ((alerts & 1) && eventCount < 20) {
            events[eventCount].timestamp = shabbatEvent.timestamp - (18 * 60);
            events[eventCount].type = "candles-18";
            events[eventCount].triggered = false;
            eventCount++;
        }
    } else if (shabbatEvent.type == "havdalah") {
        // Alert at havdalah time
        if (eventCount < 20) {
            events[eventCount].timestamp = shabbatEvent.timestamp;
            events[eventCount].type = "havdalah";
            events[eventCount].triggered = false;
            eventCount++;
        }
    }
}

void Scheduler::sortEvents() {
    // Simple bubble sort by timestamp
    for (int i = 0; i < eventCount - 1; i++) {
        for (int j = 0; j < eventCount - i - 1; j++) {
            if (events[j].timestamp > events[j + 1].timestamp) {
                AlertEvent temp = events[j];
                events[j] = events[j + 1];
                events[j + 1] = temp;
            }
        }
    }
}

bool Scheduler::shouldRefresh() {
    unsigned long now = millis();
    
    // First refresh: only after WiFi has been connected for HEBCAL_DELAY_AFTER_WIFI_MS (not from boot)
    if (lastRefresh == 0 && TimeSync::isTimeSet() && Storage::isConfigured() &&
        WiFiManager::isConnected() && firstWifiConnectedAt != 0 &&
        now >= firstWifiConnectedAt + (unsigned long)HEBCAL_DELAY_AFTER_WIFI_MS) {
        // After any failure, wait HEBCAL_RETRY_COOLDOWN_MS (2 min) before retrying to avoid hammering
        if (consecutiveFailCount > 0 && (now - lastFailAt) < (unsigned long)HEBCAL_RETRY_COOLDOWN_MS) {
            return false;
        }
        // After 5 consecutive failures, back off for 5 min before retrying
        if (consecutiveFailCount >= 5 && (now - lastFailAt) < (unsigned long)HEBCAL_FAIL_BACKOFF_MS) {
            return false;
        }
        return true;
    }
    // Refresh daily (only when WiFi connected)
    if (lastRefresh != 0 && WiFiManager::isConnected() &&
        now - lastRefresh > SCHEDULE_REFRESH_INTERVAL_MS) {
        return true;
    }
    
    // Refresh on Thursday night / Friday morning (only when WiFi connected)
    if (TimeSync::isTimeSet() && WiFiManager::isConnected()) {
        time_t t = TimeSync::getNow();
        struct tm* timeinfo = localtime(&t);
        int hour = timeinfo->tm_hour;
        int wday = timeinfo->tm_wday; // 0 = Sunday, 5 = Friday
        
        if ((wday == 4 && hour >= THURSDAY_REFRESH_HOUR) || (wday == 5 && hour < FRIDAY_REFRESH_HOUR)) {
            if (now - lastRefresh > 3600000) { // At least 1 hour since last refresh
                return true;
            }
        }
    }
    
    return false;
}

AlertEvent* Scheduler::getUpcomingEvents(int& count) {
    time_t now = TimeSync::getNow();
    count = 0;
    static AlertEvent upcoming[10];
    
    for (int i = 0; i < eventCount && count < 10; i++) {
        if (events[i].timestamp > now && !events[i].triggered) {
            upcoming[count++] = events[i];
        }
    }
    
    return upcoming;
}

bool Scheduler::hasUpcomingAlert(unsigned long& timeUntil) {
    time_t now = TimeSync::getNow();
    
    for (int i = 0; i < eventCount; i++) {
        if (events[i].timestamp > now && !events[i].triggered) {
            timeUntil = events[i].timestamp - now;
            return true;
        }
    }
    
    return false;
}

String Scheduler::getScheduleJSON() {
    String json = "[";
    for (int i = 0; i < eventCount; i++) {
        if (i > 0) json += ",";
        json += "{";
        json += "\"timestamp\":" + String(events[i].timestamp) + ",";
        json += "\"type\":\"" + events[i].type + "\",";
        json += "\"triggered\":" + String(events[i].triggered ? "true" : "false");
        json += "}";
    }
    json += "]";
    return json;
}

