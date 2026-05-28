#ifndef ALERTS_H
#define ALERTS_H

#include <Arduino.h>
#include "config.h"
#include "event_kinds.h"

class Alerts {
public:
    static void init();
    static void update();
    /** Returns true if the alert was actually started; false if declined
     *  because a previous alert is still active (caller should NOT mark the
     *  scheduler event as triggered in that case — let it retry next loop). */
    static bool trigger(AlertKind kind);
    static void test();
    static void stop();
    /** True while LED blink / RTTTL playback is in progress. */
    static bool isPlaying() { return isActive; }
    
private:
    static bool isActive;
    static unsigned long alertStartTime;
    static unsigned long activeDurationMs;
    static void updateLED();
};

#endif // ALERTS_H

