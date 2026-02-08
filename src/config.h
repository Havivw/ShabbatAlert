#ifndef CONFIG_H
#define CONFIG_H

// Hardware Pin Definitions
#ifdef BOARD_ESP32
  #define BUZZER_PIN 25
  #define LED_PIN 26
  #define OLED_SDA 21
  #define OLED_SCL 22
  #define RESET_BUTTON_PIN 0   // GPIO0 (boot button) or use another free GPIO
  #define RESET_HOLD_MS     3000
#else // ESP8266
  // Speaker module (RX, D4, 5V, GND, D8): use D8 for signal (GPIO15). If signal is on D4 use 2; if on D5/buzzer use 14.
  #define BUZZER_PIN 15  // D8 (GPIO15) - speaker / NoDAC pin
  #define LED_PIN 12     // D6 on NodeMCU
  #define OLED_SDA 4     // D2 on NodeMCU
  #define OLED_SCL 5     // D1 on NodeMCU
  // Physical reset: hold button (D5–GND) to clear WiFi and restart into ShabbatAlert AP
  #define RESET_BUTTON_PIN 14  // D5 (GPIO14)
  #define RESET_HOLD_MS     3000
  // RTTTL audio: 1 = I2S DAC (BCK/WS/DATA), 0 = NoDAC on BUZZER_PIN
  #define AUDIO_OUTPUT_I2S 0
  #if AUDIO_OUTPUT_I2S
    #define AUDIO_I2S_BCK  14
    #define AUDIO_I2S_WS   12
    #define AUDIO_I2S_DATA 15
  #endif
#endif

// WiFi Configuration
#define AP_SSID_PREFIX "ShabbatAlert-"
#define AP_PASSWORD "shabbat123"
#define AP_CHANNEL 1
#define AP_MAX_CONNECTIONS 4
#define MDNS_HOSTNAME "shabbatalert"

// NTP Configuration
#define NTP_SERVER "pool.ntp.org"
#define NTP_OFFSET_SECONDS 0
#define NTP_UPDATE_INTERVAL_MS 3600000  // 1 hour

// Hebcal API Configuration
// ESP8266: use plain HTTP (hebcal.com serves JSON on port 80 without redirect)
// ESP32: use HTTPS (mbedTLS supports TLS 1.3)
#ifdef BOARD_ESP8266
#define HEBCAL_API_BASE "http://www.hebcal.com"
#else
#define HEBCAL_API_BASE "https://www.hebcal.com"
#endif
#define HEBCAL_TIMEOUT_MS 15000
#define HEBCAL_CACHE_DURATION_MS 86400000  // 24 hours
// Delay (ms) after WiFi connect before first Hebcal fetch
#define HEBCAL_DELAY_AFTER_WIFI_MS 10000

// Scheduling Configuration
#define SCHEDULE_REFRESH_INTERVAL_MS 86400000  // Daily
#define FRIDAY_REFRESH_HOUR 6  // Refresh on Friday at 6 AM
#define THURSDAY_REFRESH_HOUR 22  // Refresh on Thursday at 10 PM

// Alert Configuration
#define ALERT_DURATION_MS 2000
#define ALERT_BEEP_COUNT 3

// Storage Keys
#define STORAGE_NAMESPACE "shabbat"
#define KEY_WIFI_SSID "wifi_ssid"
#define KEY_WIFI_PASS "wifi_pass"
#define KEY_CITY_NAME "city_name"
#define KEY_LATITUDE "latitude"
#define KEY_LONGITUDE "longitude"
#define KEY_TIMEZONE "timezone"
#define KEY_GEONAMEID "geonameid"
#define KEY_CANDLE_OFFSET "candle_offset"
#define KEY_HAVDALAH_MODE "havdalah_mode"
#define KEY_HAVDALAH_MINUTES "havdalah_minutes"
#define KEY_HAVDALAH_DEGREES "havdalah_degrees"
#define KEY_ALERT_ENABLED "alert_enabled"
#define KEY_BEEP_DURATION_MS "beep_duration_ms"
#define KEY_BEEP_PAUSE_MS "beep_pause_ms"
#define KEY_ALERT_BEEP_COUNT "alert_beep_count"
#define KEY_ALERT_DURATION_MS "alert_duration_ms"
#define KEY_RINGTONE "ringtone"
#define KEY_SETTINGS_PASSWORD "settings_pass"
#define KEY_LAST_SCHEDULE "last_schedule"
#define KEY_LAST_SCHEDULE_TIME "last_schedule_time"
#define KEY_HEBCAL_MAX_ATTEMPTS "hebcal_max_attempts"
#define KEY_HEBCAL_PROXY_URL "hebcal_proxy_url"
#define KEY_CANDLE_ALERTS "candle_alerts"  // bitmask: 1=18min, 2=30min, 4=45min

// Default Values
#define DEFAULT_CANDLE_OFFSET 18
#define DEFAULT_HAVDALAH_MODE "M"
#define DEFAULT_HAVDALAH_MINUTES 50
#define DEFAULT_HAVDALAH_DEGREES 8.5f
#define DEFAULT_TIMEZONE "Asia/Jerusalem"
#define DEFAULT_BEEP_DURATION_MS 200
#define DEFAULT_BEEP_PAUSE_MS 200
#define DEFAULT_ALERT_DURATION_MS 2000
#define DEFAULT_RINGTONE "star_wars"
#define DEFAULT_HEBCAL_MAX_ATTEMPTS 2
#define DEFAULT_CANDLE_ALERTS 7  // all three: 18+30+45 (bitmask 1|2|4)

// Web Server Configuration
#define WEB_SERVER_PORT 80
#define MAX_LOG_ENTRIES 50
#define LOG_BUFFER_SIZE 200

#endif // CONFIG_H

