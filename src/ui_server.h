#ifndef UI_SERVER_H
#define UI_SERVER_H

#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include "config.h"
#include "storage.h"
#include "time_sync.h"
#include "scheduler.h"
#include "hebcal_client.h"
#include "alerts.h"
#include "logger.h"
#include "geocoding.h"
#include "wifi_manager.h"

class UIServer {
public:
    static void init();
    static void handle();
    
private:
    static ESP8266WebServer server;

    // Web page handlers
    static void handleRoot();
    static void handleSetupPage();
    static void handleSetupSubmit();
    static void handleSettings();
    static void handleDiag();
    /** POST /wifi — save SSID/password and reboot.  Reachable from the
     *  rescue / setup AP credentials form. */
    static void handleWiFiSubmit();
    
    // API handlers
    static void handleAPIStatus();
    static void handleAPISchedule();
    static void handleAPISettings();
    static void handleAPIGeocode();
    static void handleAPITestAlert();
    static void handleAPIRgbPreview();
    static void handleAPINotFound();
    
    // Helper functions
    static bool checkAuth();
};

#endif // UI_SERVER_H

