#ifndef UI_SERVER_H
#define UI_SERVER_H

#include <Arduino.h>
#ifdef BOARD_ESP8266
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#else
#include <WebServer.h>
#include <ESPmDNS.h>
#endif
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
    #ifdef BOARD_ESP8266
    static ESP8266WebServer server;
    #else
    static WebServer server;
    #endif
    
    // Web page handlers
    static void handleRoot();
    static void handleSetupPage();
    static void handleSetupSubmit();
    static void handleSettings();
    static void handleLogs();
    
    // API handlers
    static void handleAPIStatus();
    static void handleAPISchedule();
    static void handleAPISettings();
    static void handleAPIGeocode();
    static void handleAPITestAlert();
    static void handleAPINotFound();
    
    // Helper functions
    static bool checkAuth();
    static String getDashboardHTML();
    static String getSetupHTML();
    static String getSettingsHTML();
    static String getLogsHTML();
};

#endif // UI_SERVER_H

