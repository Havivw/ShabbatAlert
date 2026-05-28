#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <Arduino.h>
#include "event_kinds.h"
#include "hebcal_client.h"
#include "time_sync.h"
#include "storage.h"

struct AlertEvent {
    time_t timestamp;
    AlertKind kind;
    bool triggered;
    char title[16];   // copied from the originating ShabbatEvent
};

class Scheduler {
public:
    static void init();
    static void update();
    static void refreshSchedule();
    /** Request a schedule refresh on next update() (from loop); avoids calling refreshSchedule from HTTP handlers. */
    static void requestRefresh();
    /** Like requestRefresh() but runs after 5s so /settings can be reopened without low memory. */
    static void requestRefreshDelayed();
    /** Rebuild alert events from cached shabbat data (no network call). Call after changing candle_alerts bitmask. */
    static void rebuildAlertEvents();
    static AlertEvent* getEvents(int& count);
    static void markEventTriggered(int index);
    static AlertEvent* getUpcomingEvents(int& count);
    static bool hasUpcomingAlert(unsigned long& timeUntil);
    static String getScheduleJSON();
    /** True if we're currently between a candle lighting and the following
     *  havdalah (Shabbat or holiday active).  Count-based so a yom-tov ⇨
     *  Shabbat back-to-back stays "on" through the whole 2- or 3-day stretch. */
    static bool isNowInShabbatWindow();
    /** Restore scheduled events from flash after reboot (call after init when time may be set). */
    static void restoreScheduleFromStorage();
    /** Returns the next upcoming scheduled candle/havdalah event (any kind),
     *  or nullptr if none.  Used by /api/status to show "next event" with its
     *  title (e.g. "Shavuot" vs "Shabbat") on the dashboard. */
    static const ShabbatEvent* getNextScheduledEvent();

    // Diagnostic getters (read-only)
    static int getConsecutiveFailCount() { return consecutiveFailCount; }
    /** Millis since the last successful Hebcal refresh, or 0 if never refreshed since boot. */
    static unsigned long getLastRefreshMillisAgo() {
        if (lastRefresh == 0) return 0;
        return millis() - lastRefresh;
    }

    // Up to 6 candle/havdalah events held simultaneously.  Covers a 2-week
    // window including back-to-back yom tov + Shabbat (e.g. Shavuot adjacent
    // to Shabbat = 2 candles + 1 havdalah, or in diaspora 3 candles + 2 havdalahs).
    static constexpr int MAX_SCHEDULED = 6;

private:
    // Worst-case alert count: MAX_SCHEDULED candles × 3 alerts each (45/30/18 min)
    // + same number of havdalahs × 1 alert = 4 × MAX_SCHEDULED.  Cap at 18 to
    // keep RAM bounded; realistic max is ~12.
    static constexpr int MAX_EVENTS = 18;
    static AlertEvent events[MAX_EVENTS];
    static int eventCount;
    /** Sorted by timestamp ascending; all events are >= "now − 1 day" so we
     *  can detect "currently inside Shabbat" right at the start of one. */
    static ShabbatEvent scheduled[MAX_SCHEDULED];
    static int scheduledCount;
    static unsigned long lastRefresh;
    static bool pendingRefresh;
    static unsigned long refreshNotBefore;
    static unsigned long firstWifiConnectedAt;
    static int consecutiveFailCount;
    static unsigned long lastFailAt;
    static void addAlertEvents(const ShabbatEvent& shabbatEvent);
    static void sortEvents();
    static void sortScheduled();
    static bool shouldRefresh();
};

#endif // SCHEDULER_H

