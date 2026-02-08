#include <Arduino.h>
#ifdef BOARD_ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#else
#include <WiFi.h>
#include <ESPmDNS.h>
#endif
#include "config.h"
#include "logger.h"
#include "storage.h"
#include "wifi_manager.h"
#include "time_sync.h"
#include "scheduler.h"
#include "ui_server.h"
#include "alerts.h"
#include "hebcal_client.h"

#if defined(USE_OLED) && USE_OLED == 1
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
unsigned long lastOLEDUpdate = 0;
#endif

unsigned long lastScheduleCheck = 0;
unsigned long lastTimeSync = 0;
unsigned long lastShabbatNowCacheUpdate = 0;
#define SHABBAT_NOW_CACHE_INTERVAL_MS 90000

#ifndef RESET_BUTTON_PIN
#define RESET_BUTTON_PIN -1
#define RESET_HOLD_MS 3000
#endif
static unsigned long resetButtonPressedAt = 0;  // 0 = not pressed

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    LOG("=== Shabbat Alert Starting ===");
    
    // Initialize storage
    if (!Storage::init()) {
        LOG("Storage initialization failed!");
        return;
    }
    
    // Initialize hardware
#ifdef BOARD_ESP8266
    randomSeed(ESP.getCycleCount());
#endif
    Alerts::init();
    
    // Physical reset button: hold to clear WiFi and restart into ShabbatAlert AP
    if (RESET_BUTTON_PIN >= 0) {
        pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);
    }
    
    #if defined(USE_OLED) && USE_OLED == 1
    Wire.begin(OLED_SDA, OLED_SCL);
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        LOG("OLED initialization failed");
    } else {
        display.clearDisplay();
        display.setTextColor(SSD1306_WHITE);
        display.setTextSize(1);
        display.setCursor(0, 0);
        display.println("Shabbat Alert");
        display.println("Initializing...");
        display.display();
    }
    #endif
    
    // Initialize WiFi
    WiFiManager::init();
    
    // Try to connect to WiFi
    if (!WiFiManager::connect()) {
        LOG("Starting in AP mode for setup");
    }
    
    // Initialize time sync
    TimeSync::init();
    
    // Sync time if WiFi connected
    if (WiFiManager::isConnected()) {
        TimeSync::sync();
    }
    
    // Initialize scheduler (first Hebcal fetch runs from loop() after HEBCAL_DELAY_AFTER_WIFI_MS)
    Scheduler::init();
    
    // Initialize web server
    UIServer::init();
    HebcalClient::setIdleCallback(UIServer::handle);

    LOG("=== Setup Complete ===");
    LOGF("IP Address: %s", WiFiManager::getIP().c_str());
}

void loop() {
    // Physical reset: hold button for RESET_HOLD_MS to clear WiFi and restart
    if (RESET_BUTTON_PIN >= 0) {
        if (digitalRead(RESET_BUTTON_PIN) == LOW) {
            if (resetButtonPressedAt == 0) {
                resetButtonPressedAt = millis();
            } else if ((millis() - resetButtonPressedAt) >= RESET_HOLD_MS) {
                LOG("Reset button: clearing WiFi, restarting into AP mode");
                Storage::setWiFiSSID("");
                Storage::setWiFiPassword("");
                ESP.restart();
            }
        } else {
            resetButtonPressedAt = 0;
        }
    }
    
    // Handle WiFi AP mode if active
    WiFiManager::handleAP();
    
#ifdef BOARD_ESP8266
    if (WiFiManager::isConnected()) {
        MDNS.update();
    }
#endif
    
    // Handle web server
    UIServer::handle();
    
    // Sync time periodically
    if (WiFiManager::isConnected() && (millis() - lastTimeSync > NTP_UPDATE_INTERVAL_MS)) {
        TimeSync::sync();
        lastTimeSync = millis();
    }
    
    // Update scheduler
    Scheduler::update();
    
    // Update "Shabbat now" cache periodically (so /api/status never does HTTPS for it)
    if (WiFiManager::isConnected() && Storage::isConfigured() &&
        (millis() - lastShabbatNowCacheUpdate) >= (unsigned long)SHABBAT_NOW_CACHE_INTERVAL_MS) {
        HebcalClient::refreshShabbatNowCache();
        lastShabbatNowCacheUpdate = millis();
    }
    
    // Update alerts (RTTTL playback loop, LED blink)
    Alerts::update();
    
    // Check for upcoming alerts
    if (TimeSync::isTimeSet() && Storage::getAlertEnabled()) {
        unsigned long timeUntil;
        if (Scheduler::hasUpcomingAlert(timeUntil)) {
            time_t now = TimeSync::getNow();
            
            // Check if any alert should trigger
            int count = 0;
            AlertEvent* events = Scheduler::getUpcomingEvents(count);
            
            for (int i = 0; i < count; i++) {
                if (!events[i].triggered && events[i].timestamp <= now) {
                    Alerts::trigger(events[i].type);
                    events[i].triggered = true;
                }
            }
        }
    }
    
    // Update OLED display
    #if defined(USE_OLED) && USE_OLED == 1
    if (millis() - lastOLEDUpdate > 1000) {
        display.clearDisplay();
        display.setCursor(0, 0);
        
        if (TimeSync::isTimeSet()) {
            display.println(TimeSync::getFormattedTime());
        } else {
            display.println("Time not set");
        }
        
        display.println(Storage::getCityName());
        
        bool shabbatNow = HebcalClient::isShabbatNow();
        display.println(shabbatNow ? "Shabbat: ON" : "Shabbat: OFF");
        
        String nextCandles = HebcalClient::getNextCandleLighting();
        if (nextCandles != "Unknown") {
            display.print("Candles: ");
            display.println(nextCandles.substring(11)); // Just time part
        }
        
        display.display();
        lastOLEDUpdate = millis();
    }
    #endif
    
    // Small delay to prevent watchdog issues
    delay(10);
}

