#ifndef TIME_SYNC_H
#define TIME_SYNC_H

#include <Arduino.h>
#include <time.h>
#include <ESP8266WiFi.h>
#include <sys/time.h>
#include "config.h"
#include "storage.h"

class TimeSync {
public:
    static void init();
    static bool sync();
    static time_t getNow();
    static String getFormattedTime();
    static String getFormattedDateTime();
    static bool isTimeSet();
    static unsigned long getMillisOffset();
    /** Seconds to add to UTC to get local time (for formatting event timestamps). */
    static long getTimezoneOffsetSeconds();
    /** Apply timezone from storage (call after changing timezone in settings). */
    static void applyTimezone(const String& tz);
    
private:
    static bool timeSet;
    static unsigned long millisAtSync;
    static time_t timeAtSync;
    static long timezoneOffsetSeconds;
    static void setTimezone(const String& tz);
};

#endif // TIME_SYNC_H

