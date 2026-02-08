#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#ifdef BOARD_ESP8266
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#else
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#endif
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
    
private:
    static bool apMode;
    static DNSServer dnsServer;
    #ifdef BOARD_ESP8266
    static ESP8266WebServer* apServer;
    #else
    static WebServer* apServer;
    #endif
    static void handleAPRoot();
    static void handleAPSetup();
    static void handleAPGeocode();
    static void handleAPNotFound();
    static void setupCaptivePortal();
};

#endif // WIFI_MANAGER_H

