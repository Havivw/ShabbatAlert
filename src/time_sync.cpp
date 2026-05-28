#include "time_sync.h"
#include "logger.h"
#include "config.h"
#include "diag.h"
#include <coredecls.h>  // settimeofday_cb()

bool TimeSync::timeSet = false;
unsigned long TimeSync::millisAtSync = 0;
time_t TimeSync::timeAtSync = 0;
long TimeSync::timezoneOffsetSeconds = 0;

// Set by the ESP8266 core whenever SNTP delivers a fresh timestamp.  We use
// this to detect "stale" hours — periods where configTime() re-armed SNTP
// but no response ever came back (DNS blocked, UDP/123 dropped, etc.).
// Volatile because the callback runs in a different context than sync().
static volatile bool sntpRefreshedSinceLastCheck = false;
// Edge-trigger flag so we only emit "ntp stale" on the transition into
// staleness, not every hour while NTP stays down (which would re-create the
// exact spammy-FAIL pattern the earlier fix eliminated).
static bool staleAlreadyLogged = false;

static void onSystemTimeSet(bool fromSntp) {
    if (fromSntp) sntpRefreshedSinceLastCheck = true;
}

// Map a stored zone name (Olson-style) to a POSIX TZ string with the right
// DST rules.  Only the zones we actually expose in the UI are listed; anything
// else falls back to UTC so the display is at least consistent.
static const char* posixTzFor(const String& tz) {
    if (tz.indexOf("Jerusalem")   >= 0) return "IST-2IDT,M3.4.4/26,M10.5.0";
    if (tz.indexOf("New_York")    >= 0) return "EST5EDT,M3.2.0,M11.1.0";
    if (tz.indexOf("Los_Angeles") >= 0) return "PST8PDT,M3.2.0,M11.1.0";
    if (tz.indexOf("London")      >= 0) return "GMT0BST,M3.5.0/1,M10.5.0";
    return "UTC0";
}

static String resolveStoredTz() {
    String tz = Storage::getTimezone();
    if (tz.length() == 0 || tz == "UTC") tz = DEFAULT_TIMEZONE;
    return tz;
}

// Compute UTC offset (seconds) for a given epoch using the currently-installed
// TZ rules.  mktime() interprets the supplied broken-down time as LOCAL, so
// feeding it the gmtime() decomposition yields (utc - offset); subtracting
// from utc therefore gives offset.
static long computeUtcOffsetForEpoch(time_t utc) {
    if (utc == 0) return 0;
    struct tm gt;
    gmtime_r(&utc, &gt);
    gt.tm_isdst = -1;  // let mktime apply current DST rules
    time_t asLocalEpoch = mktime(&gt);
    if (asLocalEpoch == (time_t)-1) return 0;
    return (long)(utc - asLocalEpoch);
}

void TimeSync::init() {
    settimeofday_cb(onSystemTimeSet);
    setTimezone(resolveStoredTz());
}

bool TimeSync::sync() {
    if (WiFi.status() != WL_CONNECTED) {
        LOG("Cannot sync time: WiFi not connected");
        return false;
    }

    // Use the TZ-aware configTime() overload — this is the only path on the
    // ESP8266 Arduino core that reliably installs a POSIX TZ rule alongside
    // SNTP.  Plain setenv("TZ",…) + tzset() does NOT take effect once SNTP is
    // running, which silently leaves localtime_r() returning UTC.
    //
    // Passing three independent servers gives SNTP a fallback pool when one
    // becomes unreachable — pool.ntp.org wedged for 6 consecutive hourly
    // retries in production before a power cycle.
    String tz = resolveStoredTz();
    configTime(posixTzFor(tz), NTP_SERVER, NTP_SERVER_2, NTP_SERVER_3);

    // Periodic resync path: configTime() above re-armed SNTP, which will
    // refresh the system clock in the background.  We already have a valid
    // clock, so don't busy-wait (and don't log a phantom failure when the
    // loop below is gated on `!timeSet`).  Just refresh our cached snapshot.
    if (timeSet) {
        // SNTP delivery is asynchronous, so instead of waiting for *this*
        // resync's response we evaluate whether the PREVIOUS interval's
        // re-arm actually produced one.  Edge-triggered: a sustained outage
        // logs `ntp stale` exactly once, not hourly, so this diagnostic
        // doesn't recreate the spammy-FAIL pattern the earlier fix removed.
        if (sntpRefreshedSinceLastCheck) {
            staleAlreadyLogged = false;  // recovered (or first periodic check)
        } else if (!staleAlreadyLogged) {
            Diag::log("ntp stale");
            staleAlreadyLogged = true;
        }
        sntpRefreshedSinceLastCheck = false;

        time_t now = time(nullptr);
        if (now > 1577836800) {
            millisAtSync = millis();
            timeAtSync = now;
            timezoneOffsetSeconds = computeUtcOffsetForEpoch(now);
            return true;
        }
        // System clock regressed below the sanity floor — treat as cold sync.
        timeSet = false;
    }

    LOG("Waiting for NTP time sync...");
    int attempts = 0;
    while (!timeSet && attempts < 20) {
        time_t now = time(nullptr);
        // SNTP starts the clock at 1970; once we cross 2016 we know it's real.
        if (now > 1577836800 /* 2020-01-01 */) {
            timeSet = true;
            millisAtSync = millis();
            timeAtSync = now;  // already UTC epoch from SNTP
            timezoneOffsetSeconds = computeUtcOffsetForEpoch(now);
            LOGF("Time synced: %s (offset %lds)",
                 getFormattedDateTime().c_str(),
                 timezoneOffsetSeconds);
            // Only log first sync after boot — periodic hourly resyncs would
            // otherwise burn ~24 EEPROM erase cycles/day for no diagnostic value.
            static bool loggedFirst = false;
            if (!loggedFirst) {
                loggedFirst = true;
                Diag::log("ntp ok off=%lds", timezoneOffsetSeconds);
            }
            return true;
        }
        delay(500);
        attempts++;
    }

    LOG("NTP sync failed");
    Diag::log("ntp FAIL");
    return false;
}

time_t TimeSync::getNow() {
    if (!timeSet) {
        return 0;
    }
    
    unsigned long elapsed = millis() - millisAtSync;
    return timeAtSync + (elapsed / 1000);
}

// Format helper that does NOT rely on localtime_r() honoring the installed TZ
// — some ESP8266 newlib builds ignore setenv/tzset for TZ rules, so we apply
// the cached offset manually and use gmtime_r() on the shifted epoch.
static void formatLocal(time_t utc, const char* fmt, char* out, size_t outSz) {
    long offset = TimeSync::getTimezoneOffsetSeconds();
    time_t local = utc + offset;
    struct tm lt;
    gmtime_r(&local, &lt);
    strftime(out, outSz, fmt, &lt);
}

String TimeSync::getFormattedTime() {
    time_t now = getNow();
    if (now == 0) return "Not set";
    char buffer[20];
    formatLocal(now, "%H:%M:%S", buffer, sizeof(buffer));
    return String(buffer);
}

String TimeSync::getFormattedDateTime() {
    time_t now = getNow();
    if (now == 0) return "Not set";
    char buffer[50];
    formatLocal(now, "%Y-%m-%d %H:%M:%S", buffer, sizeof(buffer));
    return String(buffer);
}

bool TimeSync::isTimeSet() {
    return timeSet;
}

unsigned long TimeSync::getMillisOffset() {
    return millisAtSync;
}

long TimeSync::getTimezoneOffsetSeconds() {
    time_t now = getNow();
    if (now == 0) {
        return timezoneOffsetSeconds;  // last cached snapshot
    }
    long offset = computeUtcOffsetForEpoch(now);
    if (offset != 0) {
        timezoneOffsetSeconds = offset;  // refresh cache
        return offset;
    }
    // mktime() returned 0 → likely the TZ rule didn't install (older newlib).
    // Fall back to the cached value from the last successful sync.
    return timezoneOffsetSeconds;
}

void TimeSync::applyTimezone(const String& tz) {
    setTimezone(tz);
}

void TimeSync::setTimezone(const String& tz) {
    String useTz = tz;
    if (useTz.length() == 0 || useTz == "UTC") {
        useTz = DEFAULT_TIMEZONE;
    }
    const char* posix = posixTzFor(useTz);

    // configTime() is the documented way to (re)install a TZ on the ESP8266
    // Arduino core; it writes TZ, calls tzset(), and (re)starts SNTP in the
    // right order so localtime_r() / mktime() actually honor the rule.
    configTime(posix, NTP_SERVER, NTP_SERVER_2, NTP_SERVER_3);

    // Refresh cached offset so callers asking before the next NTP tick get
    // a sane value (e.g. right after the user changes timezone in the UI).
    time_t now = getNow();
    if (now != 0) {
        long offset = computeUtcOffsetForEpoch(now);
        if (offset != 0) timezoneOffsetSeconds = offset;
    }

    // Hard-coded fallback so the display is always within 1h of wall-clock
    // even if mktime() refuses to honor the new rule (older newlib builds).
    if (timezoneOffsetSeconds == 0) {
        if (useTz.indexOf("Jerusalem")   >= 0) timezoneOffsetSeconds = 3 * 3600;  // IDT
        else if (useTz.indexOf("New_York")    >= 0) timezoneOffsetSeconds = -4 * 3600;
        else if (useTz.indexOf("Los_Angeles") >= 0) timezoneOffsetSeconds = -7 * 3600;
        else if (useTz.indexOf("London")      >= 0) timezoneOffsetSeconds =  1 * 3600;
    }
}
