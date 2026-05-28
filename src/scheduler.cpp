#include "scheduler.h"
#include "logger.h"
#include "config.h"
#include "storage.h"
#include "wifi_manager.h"
#include <ctime>

AlertEvent Scheduler::events[Scheduler::MAX_EVENTS];
int Scheduler::eventCount = 0;
ShabbatEvent Scheduler::scheduled[Scheduler::MAX_SCHEDULED];
int Scheduler::scheduledCount = 0;
unsigned long Scheduler::lastRefresh = 0;
bool Scheduler::pendingRefresh = false;
unsigned long Scheduler::refreshNotBefore = 0;
unsigned long Scheduler::firstWifiConnectedAt = 0;
int Scheduler::consecutiveFailCount = 0;
unsigned long Scheduler::lastFailAt = 0;

#define HEBCAL_FAIL_BACKOFF_MS 300000   // 5 min after 5 consecutive failures before retrying
#define HEBCAL_RETRY_COOLDOWN_MS 120000 // 2 min after any failure before retrying (avoid hammering)

void Scheduler::init() {
    eventCount = 0;
    scheduledCount = 0;
    lastRefresh = 0;
    pendingRefresh = false;
    refreshNotBefore = 0;
    firstWifiConnectedAt = 0;
    consecutiveFailCount = 0;
    lastFailAt = 0;
    // First Hebcal fetch runs from update() only after WiFi has been connected for HEBCAL_DELAY_AFTER_WIFI_MS
}

void Scheduler::requestRefresh() {
    pendingRefresh = true;
    refreshNotBefore = 0;
}

void Scheduler::requestRefreshDelayed() {
    pendingRefresh = true;
    refreshNotBefore = millis() + 5000;
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
        if (refreshNotBefore != 0 && (long)(millis() - refreshNotBefore) < 0)
            return;
        refreshNotBefore = 0;
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

    // Don't fetch a fresh schedule while we're currently inside a Shabbat /
    // holiday window — the cached events are still valid until the next
    // havdalah, and a mid-Shabbat refresh used to flip the indicator off.
    if (isNowInShabbatWindow()) {
        LOG("Skipping schedule refresh during active Shabbat/holiday window");
        // Only nudge lastRefresh forward by ~1 hour so we retry promptly once
        // the window genuinely ends.  Pushing it a full day used to
        // perpetuate a bug where isNowInShabbatWindow returned stuck-true
        // forever and the schedule was never refreshed again.
        unsigned long oneHourAgo = (millis() > SCHEDULE_REFRESH_INTERVAL_MS)
            ? millis() - SCHEDULE_REFRESH_INTERVAL_MS + 3600000UL
            : millis();
        lastRefresh = oneHourAgo;
        return;
    }

    time_t now = TimeSync::getNow();
    // Look back 1 day so a refresh right after candle lighting still includes
    // the active event (rare with the Shabbat-window guard above, but cheap).
    time_t startDate = now - (1 * 24 * 3600);
    time_t endDate = now + (14 * 24 * 3600);

    ShabbatEvent fetched[12];
    int fetchedCount = 0;

    if (!HebcalClient::fetchShabbatTimes(startDate, endDate, fetched, 12, fetchedCount)) {
        LOG("Failed to fetch Shabbat times, using cache if available");
        consecutiveFailCount++;
        lastFailAt = millis();
        return;
    }
    consecutiveFailCount = 0;

    // Take every upcoming candle/havdalah, up to MAX_SCHEDULED, in chronological
    // order.  This is the multi-event replacement for the old "single pair"
    // pick, so a back-to-back yom tov + Shabbat (e.g. Shavuot adjacent to
    // Shabbat) produces alerts at BOTH candle lightings, not just one.
    scheduledCount = 0;
    for (int i = 0; i < fetchedCount && scheduledCount < MAX_SCHEDULED; i++) {
        // Keep events that are in the future or just-recently-past (within 2h
        // — covers a refresh that lands seconds after candle lighting).
        if (fetched[i].timestamp + 7200 < now) continue;
        scheduled[scheduledCount++] = fetched[i];
    }
    sortScheduled();

    if (scheduledCount == 0) {
        LOG("No upcoming candle/havdalah in Hebcal response");
        return;
    }

    // Build the "next candles" / "next havdalah" display strings from the
    // first upcoming event of each kind.  gmtime_r(t + offset) for the same
    // TZ-quirk reason as time_sync.cpp.
    String nextCandlesStr = "Unknown";
    String nextHavdalahStr = "Unknown";
    {
        long offset = TimeSync::getTimezoneOffsetSeconds();
        struct tm lt;
        char buf[30];
        bool gotC = false, gotH = false;
        for (int i = 0; i < scheduledCount && (!gotC || !gotH); i++) {
            if (scheduled[i].timestamp <= now) continue;
            time_t local = scheduled[i].timestamp + offset;
            gmtime_r(&local, &lt);
            strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &lt);
            if (scheduled[i].kind == ShabbatKind::Candles && !gotC) {
                nextCandlesStr = String(buf);
                gotC = true;
            } else if (scheduled[i].kind == ShabbatKind::Havdalah && !gotH) {
                nextHavdalahStr = String(buf);
                gotH = true;
            }
        }
    }
    HebcalClient::setCachedNextTimes(nextCandlesStr, nextHavdalahStr);

    rebuildAlertEvents();
    HebcalClient::setCachedShabbatNow(isNowInShabbatWindow());

    Storage::persistScheduleRuntimeCache(scheduled, scheduledCount, nextCandlesStr, nextHavdalahStr);

    lastRefresh = millis();
    LOGF("Schedule refreshed: %d scheduled, %d alerts", scheduledCount, eventCount);
}

void Scheduler::restoreScheduleFromStorage() {
    String nc, nh;
    int restored = Storage::loadScheduleRuntimeCache(scheduled, MAX_SCHEDULED, nc, nh);
    if (restored <= 0) {
        scheduledCount = 0;
        return;
    }
    scheduledCount = restored;
    sortScheduled();

    // Display strings aren't persisted — regenerate them from the first
    // upcoming candle / havdalah in the restored events so the dashboard
    // doesn't say "Unknown" during the gap between boot and first refresh.
    String nextCandlesStr = "Unknown";
    String nextHavdalahStr = "Unknown";
    if (TimeSync::isTimeSet()) {
        long offset = TimeSync::getTimezoneOffsetSeconds();
        time_t now = TimeSync::getNow();
        struct tm lt;
        char buf[30];
        bool gotC = false, gotH = false;
        for (int i = 0; i < scheduledCount && (!gotC || !gotH); i++) {
            if (scheduled[i].timestamp <= now) continue;
            time_t local = scheduled[i].timestamp + offset;
            gmtime_r(&local, &lt);
            strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &lt);
            if (scheduled[i].kind == ShabbatKind::Candles && !gotC) {
                nextCandlesStr = String(buf); gotC = true;
            } else if (scheduled[i].kind == ShabbatKind::Havdalah && !gotH) {
                nextHavdalahStr = String(buf); gotH = true;
            }
        }
    }
    HebcalClient::setCachedNextTimes(nextCandlesStr, nextHavdalahStr);

    rebuildAlertEvents();
    if (TimeSync::isTimeSet()) {
        HebcalClient::setCachedShabbatNow(isNowInShabbatWindow());
    }
    // Do NOT set lastRefresh — the persisted cache could be stale (last
    // refresh was on a previous boot) and we want shouldRefresh() to fire a
    // fresh fetch promptly after WiFi/NTP come up.
    LOGF("Restored %d scheduled events from flash", scheduledCount);
}

void Scheduler::rebuildAlertEvents() {
    eventCount = 0;
    for (int i = 0; i < scheduledCount; i++) {
        addAlertEvents(scheduled[i]);
    }
    sortEvents();
}

void Scheduler::addAlertEvents(const ShabbatEvent& sev) {
    if (sev.kind == ShabbatKind::Candles) {
        int alerts = Storage::getCandleAlerts();
        // bit 2 = 45 min, bit 1 = 30 min, bit 0 = 18 min
        struct { int bit; int offsetMin; AlertKind kind; } variants[] = {
            { 4, 45, AlertKind::Candles45 },
            { 2, 30, AlertKind::Candles30 },
            { 1, 18, AlertKind::Candles18 },
        };
        for (auto& v : variants) {
            if ((alerts & v.bit) && eventCount < MAX_EVENTS) {
                events[eventCount].timestamp = sev.timestamp - (v.offsetMin * 60);
                events[eventCount].kind = v.kind;
                events[eventCount].triggered = false;
                memcpy(events[eventCount].title, sev.title, sizeof(events[eventCount].title));
                events[eventCount].title[sizeof(events[eventCount].title) - 1] = 0;
                eventCount++;
            }
        }
    } else if (sev.kind == ShabbatKind::Havdalah) {
        if (eventCount < MAX_EVENTS) {
            events[eventCount].timestamp = sev.timestamp;
            events[eventCount].kind = AlertKind::Havdalah;
            events[eventCount].triggered = false;
            memcpy(events[eventCount].title, sev.title, sizeof(events[eventCount].title));
            events[eventCount].title[sizeof(events[eventCount].title) - 1] = 0;
            eventCount++;
        }
    }
}

void Scheduler::sortEvents() {
    // Bubble sort by timestamp (eventCount is small, < 18).
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

void Scheduler::sortScheduled() {
    for (int i = 0; i < scheduledCount - 1; i++) {
        for (int j = 0; j < scheduledCount - i - 1; j++) {
            if (scheduled[j].timestamp > scheduled[j + 1].timestamp) {
                ShabbatEvent temp = scheduled[j];
                scheduled[j] = scheduled[j + 1];
                scheduled[j + 1] = temp;
            }
        }
    }
}

bool Scheduler::isNowInShabbatWindow() {
    if (scheduledCount == 0) return false;
    time_t now = TimeSync::getNow();
    if (now == 0) return false;
    // Use the kind of the MOST RECENT past event (Candles → in window,
    // Havdalah → out).  The earlier count-based approach got stuck whenever
    // Hebcal returned asymmetric candle/havdalah counts for a multi-day
    // yom-tov-into-Shabbat sequence (e.g. diaspora Shavuot 2 candles + 1
    // havdalah → count never returned to 0 → "Shabbat" stayed on indefinitely).
    ShabbatKind last = ShabbatKind::Havdalah;
    bool any = false;
    for (int i = 0; i < scheduledCount; i++) {
        if (scheduled[i].timestamp > now) break;
        last = scheduled[i].kind;
        any = true;
    }
    return any && last == ShabbatKind::Candles;
}

const ShabbatEvent* Scheduler::getNextScheduledEvent() {
    time_t now = TimeSync::getNow();
    if (now == 0) {
        // Without a clock we can't tell what's "upcoming"; return first if any.
        return scheduledCount > 0 ? &scheduled[0] : nullptr;
    }
    for (int i = 0; i < scheduledCount; i++) {
        if (scheduled[i].timestamp > now) return &scheduled[i];
    }
    return nullptr;
}

bool Scheduler::shouldRefresh() {
    unsigned long now = millis();

    // Single fail-cooldown gate applied to *every* refresh path below.  The
    // previous version checked these only on the first-refresh branch, so once
    // we had a successful refresh the daily/Thursday branches would re-fire on
    // every loop iteration when subsequent fetches failed — hammering Hebcal
    // for the full HTTP timeout each time.
    if (consecutiveFailCount > 0 && (now - lastFailAt) < (unsigned long)HEBCAL_RETRY_COOLDOWN_MS) {
        return false;
    }
    if (consecutiveFailCount >= 5 && (now - lastFailAt) < (unsigned long)HEBCAL_FAIL_BACKOFF_MS) {
        return false;
    }

    // First refresh: only after WiFi has been connected for HEBCAL_DELAY_AFTER_WIFI_MS (not from boot).
    // Use subtraction (wrap-safe) instead of `now >= firstWifiConnectedAt + DELAY`
    // — the latter can fire immediately if firstWifiConnectedAt was set near
    // millis()'s 49.7-day wraparound, defeating the post-connect delay.
    if (lastRefresh == 0 && TimeSync::isTimeSet() && Storage::isConfigured() &&
        WiFiManager::isConnected() && firstWifiConnectedAt != 0 &&
        (now - firstWifiConnectedAt) >= (unsigned long)HEBCAL_DELAY_AFTER_WIFI_MS) {
        return true;
    }
    // Refresh daily (only when WiFi connected)
    if (lastRefresh != 0 && WiFiManager::isConnected() &&
        now - lastRefresh > SCHEDULE_REFRESH_INTERVAL_MS) {
        return true;
    }

    // Refresh on Thursday night / Friday morning (only when WiFi connected).
    // gmtime(t + offset) is used instead of localtime() for the same TZ-quirk
    // reason as elsewhere — otherwise the Thursday-night refresh would fire
    // at Thursday 22:00 *UTC* (i.e. Friday 01:00 IDT) and miss its purpose.
    if (TimeSync::isTimeSet() && WiFiManager::isConnected()) {
        time_t t = TimeSync::getNow();
        time_t local = t + TimeSync::getTimezoneOffsetSeconds();
        struct tm* timeinfo = gmtime(&local);
        int hour = timeinfo ? timeinfo->tm_hour : 0;
        int wday = timeinfo ? timeinfo->tm_wday : 0;  // 0 = Sunday, 5 = Friday

        if ((wday == 4 && hour >= THURSDAY_REFRESH_HOUR) || (wday == 5 && hour < FRIDAY_REFRESH_HOUR)) {
            if (now - lastRefresh > 3600000) { // At least 1 hour since last refresh
                return true;
            }
        }
    }

    return false;
}

AlertEvent* Scheduler::getEvents(int& count) {
    count = eventCount;
    return events;
}

void Scheduler::markEventTriggered(int index) {
    if (index >= 0 && index < eventCount) {
        events[index].triggered = true;
    }
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
        json += "\"type\":\"" + String(alertKindToString(events[i].kind)) + "\",";
        json += "\"triggered\":" + String(events[i].triggered ? "true" : "false");
        json += "}";
    }
    json += "]";
    return json;
}

