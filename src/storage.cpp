#include "storage.h"

#ifdef BOARD_ESP32
Preferences Storage::preferences;

bool Storage::init() {
    return preferences.begin(STORAGE_NAMESPACE, false);
}

void Storage::clear() {
    preferences.clear();
    preferences.end();
    preferences.begin(STORAGE_NAMESPACE, false);
}

String Storage::getWiFiSSID() {
    if (!preferences.isKey(KEY_WIFI_SSID)) return "";
    return preferences.getString(KEY_WIFI_SSID, "");
}

void Storage::setWiFiSSID(const String& ssid) {
    preferences.putString(KEY_WIFI_SSID, ssid);
}

String Storage::getWiFiPassword() {
    if (!preferences.isKey(KEY_WIFI_PASS)) return "";
    return preferences.getString(KEY_WIFI_PASS, "");
}

void Storage::setWiFiPassword(const String& password) {
    preferences.putString(KEY_WIFI_PASS, password);
}

String Storage::getCityName() {
    if (!preferences.isKey(KEY_CITY_NAME)) return "";
    return preferences.getString(KEY_CITY_NAME, "");
}

void Storage::setCityName(const String& city) {
    preferences.putString(KEY_CITY_NAME, city);
}

float Storage::getLatitude() {
    return preferences.getFloat(KEY_LATITUDE, 0.0);
}

void Storage::setLatitude(float lat) {
    preferences.putFloat(KEY_LATITUDE, lat);
}

float Storage::getLongitude() {
    return preferences.getFloat(KEY_LONGITUDE, 0.0);
}

void Storage::setLongitude(float lon) {
    preferences.putFloat(KEY_LONGITUDE, lon);
}

String Storage::getTimezone() {
    return preferences.getString(KEY_TIMEZONE, DEFAULT_TIMEZONE);
}

void Storage::setTimezone(const String& tz) {
    preferences.putString(KEY_TIMEZONE, tz);
}

int Storage::getGeonameID() {
    return preferences.getInt(KEY_GEONAMEID, 0);
}

void Storage::setGeonameID(int id) {
    preferences.putInt(KEY_GEONAMEID, id);
}

int Storage::getCandleOffset() {
    return preferences.getInt(KEY_CANDLE_OFFSET, DEFAULT_CANDLE_OFFSET);
}

void Storage::setCandleOffset(int minutes) {
    preferences.putInt(KEY_CANDLE_OFFSET, minutes);
}

String Storage::getHavdalahMode() {
    return preferences.getString(KEY_HAVDALAH_MODE, DEFAULT_HAVDALAH_MODE);
}

void Storage::setHavdalahMode(const String& mode) {
    preferences.putString(KEY_HAVDALAH_MODE, mode);
}

int Storage::getHavdalahMinutes() {
    return preferences.getInt(KEY_HAVDALAH_MINUTES, DEFAULT_HAVDALAH_MINUTES);
}

void Storage::setHavdalahMinutes(int minutes) {
    preferences.putInt(KEY_HAVDALAH_MINUTES, minutes);
}

float Storage::getHavdalahDegrees() {
    return preferences.getFloat(KEY_HAVDALAH_DEGREES, DEFAULT_HAVDALAH_DEGREES);
}

void Storage::setHavdalahDegrees(float degrees) {
    preferences.putFloat(KEY_HAVDALAH_DEGREES, degrees);
}

bool Storage::getAlertEnabled() {
    return preferences.getBool(KEY_ALERT_ENABLED, true);
}

void Storage::setAlertEnabled(bool enabled) {
    preferences.putBool(KEY_ALERT_ENABLED, enabled);
}

int Storage::getBeepDurationMs() {
    return preferences.getInt(KEY_BEEP_DURATION_MS, DEFAULT_BEEP_DURATION_MS);
}

void Storage::setBeepDurationMs(int ms) {
    preferences.putInt(KEY_BEEP_DURATION_MS, ms);
}

int Storage::getBeepPauseMs() {
    return preferences.getInt(KEY_BEEP_PAUSE_MS, DEFAULT_BEEP_PAUSE_MS);
}

void Storage::setBeepPauseMs(int ms) {
    preferences.putInt(KEY_BEEP_PAUSE_MS, ms);
}

int Storage::getAlertBeepCount() {
    return preferences.getInt(KEY_ALERT_BEEP_COUNT, ALERT_BEEP_COUNT);
}

void Storage::setAlertBeepCount(int count) {
    preferences.putInt(KEY_ALERT_BEEP_COUNT, count);
}

unsigned long Storage::getAlertDurationMs() {
    return preferences.getULong64(KEY_ALERT_DURATION_MS, ALERT_DURATION_MS);
}

void Storage::setAlertDurationMs(unsigned long ms) {
    preferences.putULong64(KEY_ALERT_DURATION_MS, ms);
}

String Storage::getRingtone() {
    return preferences.getString(KEY_RINGTONE, DEFAULT_RINGTONE);
}

void Storage::setRingtone(const String& value) {
    preferences.putString(KEY_RINGTONE, value);
}

String Storage::getSettingsPassword() {
    return preferences.getString(KEY_SETTINGS_PASSWORD, "");
}

void Storage::setSettingsPassword(const String& password) {
    preferences.putString(KEY_SETTINGS_PASSWORD, password);
}

String Storage::getLastSchedule() {
    return preferences.getString(KEY_LAST_SCHEDULE, "");
}

void Storage::setLastSchedule(const String& schedule) {
    preferences.putString(KEY_LAST_SCHEDULE, schedule);
}

unsigned long Storage::getLastScheduleTime() {
    return preferences.getULong64(KEY_LAST_SCHEDULE_TIME, 0);
}

void Storage::setLastScheduleTime(unsigned long time) {
    preferences.putULong64(KEY_LAST_SCHEDULE_TIME, time);
}

int Storage::getHebcalMaxAttempts() {
    int v = preferences.getInt(KEY_HEBCAL_MAX_ATTEMPTS, DEFAULT_HEBCAL_MAX_ATTEMPTS);
    if (v < 1) return 1;
    if (v > 5) return 5;
    return v;
}

void Storage::setHebcalMaxAttempts(int attempts) {
    if (attempts < 1) attempts = 1;
    if (attempts > 5) attempts = 5;
    preferences.putInt(KEY_HEBCAL_MAX_ATTEMPTS, attempts);
}

String Storage::getHebcalProxyURL() {
    return preferences.getString(KEY_HEBCAL_PROXY_URL, "");
}

void Storage::setHebcalProxyURL(const String& url) {
    preferences.putString(KEY_HEBCAL_PROXY_URL, url);
}

int Storage::getCandleAlerts() {
    return preferences.getInt(KEY_CANDLE_ALERTS, DEFAULT_CANDLE_ALERTS);
}

void Storage::setCandleAlerts(int bitmask) {
    preferences.putInt(KEY_CANDLE_ALERTS, bitmask);
}

bool Storage::isConfigured() {
    return getWiFiSSID().length() > 0 && getLatitude() != 0.0 && getLongitude() != 0.0;
}

#else // ESP8266 - Use EEPROM

bool Storage::eepromInitialized = false;

// EEPROM address offsets (each string max 64 bytes, total ~512 bytes used)
#define EEPROM_SIZE 640
#define ADDR_WIFI_SSID 0
#define ADDR_WIFI_PASS 64
#define ADDR_CITY_NAME 128
#define ADDR_TIMEZONE 192
#define ADDR_SETTINGS_PASS 256
#define ADDR_LAST_SCHEDULE 320
#define ADDR_LATITUDE 384
#define ADDR_LONGITUDE 388
#define ADDR_GEONAMEID 392
#define ADDR_CANDLE_OFFSET 396
#define ADDR_HAVDALAH_MODE 400
#define ADDR_HAVDALAH_MINUTES 404
#define ADDR_ALERT_ENABLED 408
#define ADDR_LAST_SCHEDULE_TIME 412
#define ADDR_BEEP_DURATION_MS 416
#define ADDR_BEEP_PAUSE_MS 420
#define ADDR_ALERT_BEEP_COUNT 424
#define ADDR_ALERT_DURATION_MS 428
#define ADDR_HAVDALAH_DEGREES 432
#define ADDR_RINGTONE 436
#define RINGTONE_MAX_LEN 24
#define ADDR_HEBCAL_MAX_ATTEMPTS 500
#define ADDR_HEBCAL_PROXY_URL 504
#define HEBCAL_PROXY_URL_MAX_LEN 96
#define ADDR_CANDLE_ALERTS 600

bool Storage::init() {
    if (!eepromInitialized) {
        EEPROM.begin(EEPROM_SIZE);
        eepromInitialized = true;
    }
    return true;
}

void Storage::clear() {
    for (int i = 0; i < EEPROM_SIZE; i++) {
        EEPROM.write(i, 0);
    }
    EEPROM.commit();
}

String Storage::readString(int address, int maxLen) {
    String result = "";
    for (int i = 0; i < maxLen; i++) {
        char c = EEPROM.read(address + i);
        if (c == 0 || c == 255) break;
        result += c;
    }
    return result;
}

void Storage::writeString(int address, const String& value) {
    int len = value.length();
    if (len > 63) len = 63; // Max 63 chars + null terminator
    for (int i = 0; i < len; i++) {
        EEPROM.write(address + i, value[i]);
    }
    EEPROM.write(address + len, 0); // Null terminator
    EEPROM.commit();
}

float Storage::readFloat(int address) {
    union {
        float f;
        byte b[4];
    } data;
    for (int i = 0; i < 4; i++) {
        data.b[i] = EEPROM.read(address + i);
    }
    return data.f;
}

void Storage::writeFloat(int address, float value) {
    union {
        float f;
        byte b[4];
    } data;
    data.f = value;
    for (int i = 0; i < 4; i++) {
        EEPROM.write(address + i, data.b[i]);
    }
    EEPROM.commit();
}

int Storage::readInt(int address) {
    return (int)EEPROM.read(address) | ((int)EEPROM.read(address + 1) << 8) |
           ((int)EEPROM.read(address + 2) << 16) | ((int)EEPROM.read(address + 3) << 24);
}

void Storage::writeInt(int address, int value) {
    EEPROM.write(address, value & 0xFF);
    EEPROM.write(address + 1, (value >> 8) & 0xFF);
    EEPROM.write(address + 2, (value >> 16) & 0xFF);
    EEPROM.write(address + 3, (value >> 24) & 0xFF);
    EEPROM.commit();
}

unsigned long Storage::readULong(int address) {
    unsigned long result = 0;
    for (int i = 0; i < 4; i++) {
        result |= ((unsigned long)EEPROM.read(address + i)) << (i * 8);
    }
    return result;
}

void Storage::writeULong(int address, unsigned long value) {
    for (int i = 0; i < 4; i++) {
        EEPROM.write(address + i, (value >> (i * 8)) & 0xFF);
    }
    EEPROM.commit();
}

bool Storage::readBool(int address) {
    return EEPROM.read(address) != 0;
}

void Storage::writeBool(int address, bool value) {
    EEPROM.write(address, value ? 1 : 0);
    EEPROM.commit();
}

// Implementation for ESP8266 (init and clear are already defined above)

String Storage::getWiFiSSID() {
    return readString(ADDR_WIFI_SSID, 64);
}

void Storage::setWiFiSSID(const String& ssid) {
    writeString(ADDR_WIFI_SSID, ssid);
}

String Storage::getWiFiPassword() {
    return readString(ADDR_WIFI_PASS, 64);
}

void Storage::setWiFiPassword(const String& password) {
    writeString(ADDR_WIFI_PASS, password);
}

String Storage::getCityName() {
    return readString(ADDR_CITY_NAME, 64);
}

void Storage::setCityName(const String& city) {
    writeString(ADDR_CITY_NAME, city);
}

float Storage::getLatitude() {
    return readFloat(ADDR_LATITUDE);
}

void Storage::setLatitude(float lat) {
    writeFloat(ADDR_LATITUDE, lat);
}

float Storage::getLongitude() {
    return readFloat(ADDR_LONGITUDE);
}

void Storage::setLongitude(float lon) {
    writeFloat(ADDR_LONGITUDE, lon);
}

String Storage::getTimezone() {
    String tz = readString(ADDR_TIMEZONE, 64);
    return tz.length() > 0 ? tz : String(DEFAULT_TIMEZONE);
}

void Storage::setTimezone(const String& tz) {
    writeString(ADDR_TIMEZONE, tz);
}

int Storage::getGeonameID() {
    return readInt(ADDR_GEONAMEID);
}

void Storage::setGeonameID(int id) {
    writeInt(ADDR_GEONAMEID, id);
}

int Storage::getCandleOffset() {
    int offset = readInt(ADDR_CANDLE_OFFSET);
    return offset != 0 ? offset : DEFAULT_CANDLE_OFFSET;
}

void Storage::setCandleOffset(int minutes) {
    writeInt(ADDR_CANDLE_OFFSET, minutes);
}

String Storage::getHavdalahMode() {
    String mode = readString(ADDR_HAVDALAH_MODE, 2);
    return mode.length() > 0 ? mode : String(DEFAULT_HAVDALAH_MODE);
}

void Storage::setHavdalahMode(const String& mode) {
    writeString(ADDR_HAVDALAH_MODE, mode);
}

int Storage::getHavdalahMinutes() {
    int minutes = readInt(ADDR_HAVDALAH_MINUTES);
    return minutes != 0 ? minutes : DEFAULT_HAVDALAH_MINUTES;
}

void Storage::setHavdalahMinutes(int minutes) {
    writeInt(ADDR_HAVDALAH_MINUTES, minutes);
}

float Storage::getHavdalahDegrees() {
    float v = readFloat(ADDR_HAVDALAH_DEGREES);
    return (v > 0.0f) ? v : DEFAULT_HAVDALAH_DEGREES;
}

void Storage::setHavdalahDegrees(float degrees) {
    writeFloat(ADDR_HAVDALAH_DEGREES, degrees);
}

bool Storage::getAlertEnabled() {
    return readBool(ADDR_ALERT_ENABLED);
}

void Storage::setAlertEnabled(bool enabled) {
    writeBool(ADDR_ALERT_ENABLED, enabled);
}

int Storage::getBeepDurationMs() {
    int v = readInt(ADDR_BEEP_DURATION_MS);
    return (v > 0) ? v : DEFAULT_BEEP_DURATION_MS;
}

void Storage::setBeepDurationMs(int ms) {
    writeInt(ADDR_BEEP_DURATION_MS, ms);
}

int Storage::getBeepPauseMs() {
    int v = readInt(ADDR_BEEP_PAUSE_MS);
    return (v > 0) ? v : DEFAULT_BEEP_PAUSE_MS;
}

void Storage::setBeepPauseMs(int ms) {
    writeInt(ADDR_BEEP_PAUSE_MS, ms);
}

int Storage::getAlertBeepCount() {
    int v = readInt(ADDR_ALERT_BEEP_COUNT);
    return (v > 0) ? v : ALERT_BEEP_COUNT;
}

void Storage::setAlertBeepCount(int count) {
    writeInt(ADDR_ALERT_BEEP_COUNT, count);
}

unsigned long Storage::getAlertDurationMs() {
    unsigned long v = readULong(ADDR_ALERT_DURATION_MS);
    return (v > 0) ? v : ALERT_DURATION_MS;
}

void Storage::setAlertDurationMs(unsigned long ms) {
    writeULong(ADDR_ALERT_DURATION_MS, ms);
}

String Storage::getRingtone() {
    String v = readString(ADDR_RINGTONE, RINGTONE_MAX_LEN);
    return v.length() > 0 ? v : String(DEFAULT_RINGTONE);
}

void Storage::setRingtone(const String& value) {
    writeString(ADDR_RINGTONE, value);
}

String Storage::getSettingsPassword() {
    return readString(ADDR_SETTINGS_PASS, 64);
}

void Storage::setSettingsPassword(const String& password) {
    writeString(ADDR_SETTINGS_PASS, password);
}

String Storage::getLastSchedule() {
    return readString(ADDR_LAST_SCHEDULE, 64);
}

void Storage::setLastSchedule(const String& schedule) {
    writeString(ADDR_LAST_SCHEDULE, schedule);
}

unsigned long Storage::getLastScheduleTime() {
    return readULong(ADDR_LAST_SCHEDULE_TIME);
}

void Storage::setLastScheduleTime(unsigned long time) {
    writeULong(ADDR_LAST_SCHEDULE_TIME, time);
}

int Storage::getHebcalMaxAttempts() {
    int v = readInt(ADDR_HEBCAL_MAX_ATTEMPTS);
    if (v < 1 || v > 5) return DEFAULT_HEBCAL_MAX_ATTEMPTS;
    return v;
}

void Storage::setHebcalMaxAttempts(int attempts) {
    if (attempts < 1) attempts = 1;
    if (attempts > 5) attempts = 5;
    writeInt(ADDR_HEBCAL_MAX_ATTEMPTS, attempts);
}

String Storage::getHebcalProxyURL() {
    return readString(ADDR_HEBCAL_PROXY_URL, HEBCAL_PROXY_URL_MAX_LEN);
}

void Storage::setHebcalProxyURL(const String& url) {
    writeString(ADDR_HEBCAL_PROXY_URL, url);
}

int Storage::getCandleAlerts() {
    int v = readInt(ADDR_CANDLE_ALERTS);
    return (v > 0 && v <= 7) ? v : DEFAULT_CANDLE_ALERTS;
}

void Storage::setCandleAlerts(int bitmask) {
    writeInt(ADDR_CANDLE_ALERTS, bitmask);
}

bool Storage::isConfigured() {
    return getWiFiSSID().length() > 0 && getLatitude() != 0.0 && getLongitude() != 0.0;
}

#endif // BOARD_ESP32
