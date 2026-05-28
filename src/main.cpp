#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include "config.h"
#include "logger.h"
#include "storage.h"
#include "wifi_manager.h"
#include "time_sync.h"
#include "scheduler.h"
#include "ui_server.h"
#include "alerts.h"
#include "hebcal_client.h"
#include "diag.h"
#if defined(USE_OLED) && USE_OLED == 1
#include <cstring>
#endif

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

#include <Adafruit_NeoPixel.h>
Adafruit_NeoPixel rgbStrip(RGB_STRIP_LEN, RGB_STRIP_PIN, NEO_GRB + NEO_KHZ800);
unsigned long lastCandleUpdate = 0;
static bool rgbShabbatPreview = false;
static unsigned long rgbPreviewUntil = 0;

void RGBSetShabbatPreview(bool enabled) {
    if (enabled) {
        rgbShabbatPreview = true;
        rgbPreviewUntil = millis() + 60000UL; // 60s preview
    } else {
        rgbShabbatPreview = false;
        rgbPreviewUntil = 0;
    }
}

bool RGBGetShabbatPreview() {
    return rgbShabbatPreview;
}

unsigned long lastScheduleCheck = 0;
unsigned long lastTimeSync = 0;
unsigned long lastShabbatNowCacheUpdate = 0;
#define SHABBAT_NOW_CACHE_INTERVAL_MS 90000
#if defined(DEBUG_HEAP) && DEBUG_HEAP
static unsigned long lastHeapDebugLog = 0;
#endif

// Scheduled reboot: release fragmented heap twice a day (00:00 and 12:00 local).
// -1 = no slot used yet this boot. After a reboot, this naturally resets to -1.
static int lastScheduledRebootHour = -1;

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

    // Persistent diagnostic ring buffer (must come after EEPROM is up).
    Diag::init();
    Diag::log("boot heap=%u", (unsigned)ESP.getFreeHeap());

    randomSeed(ESP.getCycleCount());
    Alerts::init();
    rgbStrip.begin();
    rgbStrip.setBrightness(RGB_BRIGHTNESS_INDICATOR);
    rgbStrip.setPixelColor(0, 255, 0, 0);
    rgbStrip.setPixelColor(1, 255, 0, 0);
    rgbStrip.show();
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
    
    // Try to connect to WiFi.  If credentials are stored but STA fails to
    // come up in 30s, connect() opens the rescue AP itself; if no creds are
    // stored, connect() opens the dedicated setup AP.  Either way the device
    // is reachable on its softAP IP from this point on.
    WiFiManager::connect();
    
    // Initialize time sync
    TimeSync::init();
    
    // Sync time if WiFi connected
    if (WiFiManager::isConnected()) {
        TimeSync::sync();
    }
    
    // Initialize scheduler (first Hebcal fetch runs from loop() after HEBCAL_DELAY_AFTER_WIFI_MS)
    Scheduler::init();
    Scheduler::restoreScheduleFromStorage();

    // Initialize web server
    UIServer::init();
    HebcalClient::setIdleCallback(UIServer::handle);

    LOG("=== Setup Complete ===");
    LOGF("IP Address: %s", WiFiManager::getIP().c_str());

    {
        int evCount = 0;
        (void)Scheduler::getEvents(evCount);
        Diag::log("cfg ae=%d ca=%d ev=%d",
                  Storage::getAlertEnabled() ? 1 : 0,
                  Storage::getCandleAlerts(),
                  evCount);
    }
}

void loop() {
    // Physical reset: two-phase hold to clear WiFi credentials.
    //   0–3s   : ignored (debounce against accidental brushes)
    //   3–10s  : LED blinks rapidly as a "you're about to wipe WiFi" warning;
    //            releasing the button here CANCELS the wipe
    //   >=10s  : wipe and reboot
    // Previously a single 3-second hold was enough — easy to trigger by accident
    // if the device is on a shelf and something rests against the button.
    static const unsigned long RESET_WARN_MS  = 3000;
    static const unsigned long RESET_WIPE_MS  = 10000;
    if (RESET_BUTTON_PIN >= 0) {
        if (digitalRead(RESET_BUTTON_PIN) == LOW) {
            if (resetButtonPressedAt == 0) {
                resetButtonPressedAt = millis();
            }
            unsigned long held = millis() - resetButtonPressedAt;
            if (held >= RESET_WIPE_MS) {
                LOG("Reset button: 10s confirmed — clearing WiFi");
                Diag::log("wifi WIPED via reset btn");
                Storage::setWiFiSSID("");
                Storage::setWiFiPassword("");
                ESP.restart();
            } else if (held >= RESET_WARN_MS) {
                // Fast LED blink (125ms cycle) so the user can SEE the warning
                // window and release before the wipe fires.
                bool on = ((millis() / 125) & 1);
                digitalWrite(LED_PIN, on ? HIGH : LOW);
            }
        } else {
            if (resetButtonPressedAt != 0 &&
                (millis() - resetButtonPressedAt) >= RESET_WARN_MS) {
                LOG("Reset button: released during warning — wipe cancelled");
                Diag::log("wifi wipe cancelled");
                digitalWrite(LED_PIN, LOW);
            }
            resetButtonPressedAt = 0;
        }
    }
    
    // Handle WiFi AP mode if active
    WiFiManager::handleAP();
    // If STA is configured but currently down, retry every 60s in the
    // background instead of stranding the device in AP mode.
    WiFiManager::ensureConnected();

    if (WiFiManager::isConnected()) {
        MDNS.update();
    }

    // Handle web server
    UIServer::handle();

    // Sync time periodically.  Also fire an immediate sync the first time we
    // see WiFi come up after boot (so an unsynced device that took >30s to
    // join the AP doesn't have to wait NTP_UPDATE_INTERVAL_MS to get a clock
    // — without that, alert triggering stays disabled, which is exactly the
    // "no sound" failure mode we used to hit after a flaky reconnect).
    if (WiFiManager::isConnected()) {
        bool needFirstSync = !TimeSync::isTimeSet();
        bool periodicDue = (millis() - lastTimeSync > NTP_UPDATE_INTERVAL_MS);
        if (needFirstSync || periodicDue) {
            TimeSync::sync();
            lastTimeSync = millis();
        }
    }
    
    // Update scheduler
    Scheduler::update();

    // Conditional twice-daily reboot — only fires when heap fragmentation has
    // actually degraded.  A healthy device at 00:00 / 12:00 just logs its
    // heap stats and stays up; a fragmented one reboots to release memory.
    // This trades the previous "always reboot" certainty for less interruption
    // on devices that don't need it.  The /diag log captures both branches so
    // you can see the trajectory: if `rbt skip` keeps appearing for weeks the
    // reboot can be removed entirely; if `rbt go` shows up, it's earning its
    // keep.  Threshold = max contiguous block, which is what allocation
    // failures actually depend on (free heap alone misses fragmentation).
    static const unsigned int REBOOT_MAX_BLOCK_THRESHOLD = 8000;  // bytes
    if (TimeSync::isTimeSet() && WiFiManager::isConnected()) {
        time_t nowUtc = TimeSync::getNow();
        time_t local = nowUtc + TimeSync::getTimezoneOffsetSeconds();
        struct tm* tmL = gmtime(&local);
        if (tmL) {
            int hour = tmL->tm_hour;
            int minute = tmL->tm_min;
            bool atRebootSlot = (hour == 0 || hour == 12) && minute == 0;
            bool busy = Alerts::isPlaying();
            if (atRebootSlot && !busy && lastScheduledRebootHour != hour) {
                lastScheduledRebootHour = hour;
                unsigned int freeH = (unsigned int)ESP.getFreeHeap();
                unsigned int maxB  = (unsigned int)ESP.getMaxFreeBlockSize();
                if (maxB < REBOOT_MAX_BLOCK_THRESHOLD) {
                    LOG("Scheduled reboot: releasing memory (max block low)");
                    Diag::log("rbt go h=%d f=%u m=%u", hour, freeH, maxB);
                    delay(200);
                    ESP.restart();
                } else {
                    LOGF("Reboot slot h=%d skipped, heap healthy (max=%u)", hour, maxB);
                    Diag::log("rbt skip h=%d m=%u", hour, maxB);
                }
            }
            // Reset the latch when we leave the reboot minute, so the next slot can fire.
            if (!atRebootSlot && lastScheduledRebootHour != -1) {
                lastScheduledRebootHour = -1;
            }
        }
    }
    
    // Update "Shabbat now" from schedule (candle to havdalah).  Purely local
    // computation — does NOT require WiFi.  The previous WiFi gate could
    // leave the RGB strip stuck in Shabbat-flicker mode for the entire
    // duration of a router/WiFi outage spanning havdalah.
    if (Storage::isConfigured() &&
        (millis() - lastShabbatNowCacheUpdate) >= (unsigned long)SHABBAT_NOW_CACHE_INTERVAL_MS) {
        HebcalClient::setCachedShabbatNow(Scheduler::isNowInShabbatWindow());
        lastShabbatNowCacheUpdate = millis();
    }
    
    // Update alerts (RTTTL playback loop, LED blink)
    Alerts::update();
    // RGB strip: red = boot, green = connected, candle flicker = Shabbat/preview (each LED different)
    // Auto-timeout preview after configured duration
    if (rgbShabbatPreview && rgbPreviewUntil != 0 && (long)(millis() - rgbPreviewUntil) >= 0) {
        rgbShabbatPreview = false;
        rgbPreviewUntil = 0;
    }
    bool shabbatNow = HebcalClient::getCachedShabbatNow();
    bool effectiveShabbat = shabbatNow || rgbShabbatPreview;
    if (effectiveShabbat) {
        rgbStrip.setBrightness(RGB_BRIGHTNESS_SHABBAT);
        unsigned long t = millis();
        if (t - lastCandleUpdate >= 45) {
            lastCandleUpdate = t;
            for (int i = 0; i < RGB_STRIP_LEN; i++) {
                int r = 255;
                int g = 60 + random(0, 80) + (i * 20);
                if (g > 255) g = 255;
                int b = random(0, 25);
                int dim = 120 + random(0, 130);
                r = (r * dim) / 255;
                g = (g * dim) / 255;
                b = (b * dim) / 255;
                rgbStrip.setPixelColor(i, (uint8_t)r, (uint8_t)g, (uint8_t)b);
            }
            rgbStrip.show();
        }
    } else if (WiFiManager::isConnected()) {
        rgbStrip.setBrightness(RGB_BRIGHTNESS_INDICATOR);
        rgbStrip.setPixelColor(0, 0, 255, 0);
        rgbStrip.setPixelColor(1, 0, 255, 0);
        rgbStrip.show();
    } else {
        rgbStrip.setBrightness(RGB_BRIGHTNESS_INDICATOR);
        rgbStrip.setPixelColor(0, 255, 0, 0);
        rgbStrip.setPixelColor(1, 255, 0, 0);
        rgbStrip.show();
    }
    // Check for due alerts (candle time, havdalah).  Grace window: an event
    // fires if its scheduled time is in the recent past (last 10 minutes).
    // We do NOT want very-old events firing after a long downtime (havdalah
    // from yesterday triggering today), but 5 min was too tight: the
    // conditional 00:00/12:00 reboot + slow WiFi reconnect + NTP sync can
    // exceed 5 min on a flaky router, silently swallowing any alert whose
    // time falls in that window (e.g. havdalah at 20:25 after a 20:18 reboot).
    static const time_t ALERT_OVERDUE_GRACE_SEC = 600;
    if (TimeSync::isTimeSet() && Storage::getAlertEnabled()) {
        time_t now = TimeSync::getNow();
        int count = 0;
        AlertEvent* events = Scheduler::getEvents(count);
        for (int i = 0; i < count; i++) {
            if (events[i].triggered) continue;
            if (events[i].timestamp > now) continue;
            time_t overdue = now - events[i].timestamp;
            if (overdue <= ALERT_OVERDUE_GRACE_SEC) {
                // Only mark triggered if the alert actually fired.  If a
                // previous alert is still playing, trigger() declines and we
                // leave the event un-triggered so it retries next loop — the
                // old behavior marked it triggered anyway, silently dropping
                // alerts whose times happened to overlap.
                if (Alerts::trigger(events[i].kind)) {
                    Scheduler::markEventTriggered(i);
                }
            } else {
                // Grace window blown (device was offline / NTP just synced):
                // mark it triggered so we don't keep evaluating a stale event
                // every iteration.
                Scheduler::markEventTriggered(i);
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
        
        bool shabbatNow = HebcalClient::getCachedShabbatNow();
        display.println(shabbatNow ? "Shabbat: ON" : "Shabbat: OFF");
        
        const char* nextCandles = HebcalClient::peekCachedNextCandlesCStr();
        if (nextCandles && strlen(nextCandles) > 11) {
            display.print("Candles: ");
            display.println(nextCandles + 11);
        }
        
        display.display();
        lastOLEDUpdate = millis();
    }
    #endif
    
#if defined(DEBUG_HEAP) && DEBUG_HEAP
    if (millis() - lastHeapDebugLog >= 300000UL) {
        lastHeapDebugLog = millis();
        LOGF("heap free=%u maxBlock=%u", (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxFreeBlockSize());
    }
#endif

    // Small delay to prevent watchdog issues
    delay(10);
}

