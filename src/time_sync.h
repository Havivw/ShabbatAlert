#ifndef TIME_SYNC_H
#define TIME_SYNC_H

#include <Arduino.h>
#include <time.h>
#ifdef BOARD_ESP8266
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif
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
    /** Apply timezone from storage (call after changing timezone in settings). */
    static void applyTimezone(const String& tz);
    
private:
    static bool timeSet;
    static unsigned long millisAtSync;
    static time_t timeAtSync;
    static void setTimezone(const String& tz);
};

#endif // TIME_SYNC_H

