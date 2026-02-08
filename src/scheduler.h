#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <Arduino.h>
#include "hebcal_client.h"
#include "time_sync.h"
#include "storage.h"

struct AlertEvent {
    time_t timestamp;
    String type;  // "candles-18", "candles-30", "candles-45", or "havdalah"
    bool triggered;
};

class Scheduler {
public:
    static void init();
    static void update();
    static void refreshSchedule();
    /** Request a schedule refresh on next update() (from loop); avoids calling refreshSchedule from HTTP handlers. */
    static void requestRefresh();
    /** Rebuild alert events from cached shabbat data (no network call). Call after changing candle_alerts bitmask. */
    static void rebuildAlertEvents();
    static AlertEvent* getUpcomingEvents(int& count);
    static bool hasUpcomingAlert(unsigned long& timeUntil);
    static String getScheduleJSON();
    
private:
    static AlertEvent events[20];
    static int eventCount;
    static ShabbatEvent shabbatCache[10];
    static int shabbatCacheCount;
    static unsigned long lastRefresh;
    static bool pendingRefresh;
    static unsigned long firstWifiConnectedAt;
    static int consecutiveFailCount;
    static unsigned long lastFailAt;
    static void addAlertEvents(const ShabbatEvent& shabbatEvent, int index);
    static void sortEvents();
    static bool shouldRefresh();
};

#endif // SCHEDULER_H

