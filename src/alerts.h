#ifndef ALERTS_H
#define ALERTS_H

#include <Arduino.h>
#include "config.h"

class Alerts {
public:
    static void init();
    static void update();
    static void trigger(const String& alertType);
    static void test();
    static void stop();
    
private:
    static bool isActive;
    static unsigned long alertStartTime;
    static void beep(int count = 1);
    static void updateLED();
};

#endif // ALERTS_H

