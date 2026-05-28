#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include "config.h"
#include "storage.h"
#include "geocoding.h"

class WiFiManager {
public:
    static void init();
    static bool connect();
    static void startAP();
    static void handleAP();
    static bool isConnected();
    static String getIP();
    static void stopAP();
    /** Background reconnect: if STA is configured but disconnected, retry
     *  every 60s without dropping into AP mode.  Call from loop(). */
    static void ensureConnected();
    /** True when the dedicated setup AP is up (no credentials stored). */
    static bool isSetupAPMode() { return apMode; }
    /** True when the rescue AP is up alongside STA (STA failed). */
    static bool isAPRescueActive() { return apRescueActive; }
    
private:
    static bool apMode;
    /** True when the rescue AP is up alongside STA (because STA failed for
     *  too long). The main UI on port 80 stays reachable from both
     *  interfaces, and the AP is torn down automatically once STA recovers. */
    static bool apRescueActive;
    /** millis() snapshot of when STA last went down. 0 = currently connected. */
    static unsigned long staDownSince;
    static DNSServer dnsServer;
    static ESP8266WebServer* apServer;
    static void handleAPRoot();
    static void handleAPSetup();
    static void handleAPGeocode();
    static void handleAPNotFound();
    static void setupCaptivePortal();
    static void startAPRescue();
    static void stopAPRescue();
};

#endif // WIFI_MANAGER_H

