#ifndef STORAGE_H
#define STORAGE_H

#include <Arduino.h>
#ifdef BOARD_ESP32
#include <Preferences.h>
#else
#include <EEPROM.h>
#endif
#include "config.h"

class Storage {
public:
    static bool init();
    static void clear();
    
    // WiFi settings
    static String getWiFiSSID();
    static void setWiFiSSID(const String& ssid);
    static String getWiFiPassword();
    static void setWiFiPassword(const String& password);
    
    // Location settings
    static String getCityName();
    static void setCityName(const String& city);
    static float getLatitude();
    static void setLatitude(float lat);
    static float getLongitude();
    static void setLongitude(float lon);
    static String getTimezone();
    static void setTimezone(const String& tz);
    static int getGeonameID();
    static void setGeonameID(int id);
    
    // Minhag settings
    static int getCandleOffset();
    static void setCandleOffset(int minutes);
    static String getHavdalahMode();
    static void setHavdalahMode(const String& mode);
    static int getHavdalahMinutes();
    static void setHavdalahMinutes(int minutes);
    static float getHavdalahDegrees();
    static void setHavdalahDegrees(float degrees);
    
    // Alert settings
    static bool getAlertEnabled();
    static void setAlertEnabled(bool enabled);
    static int getBeepDurationMs();
    static void setBeepDurationMs(int ms);
    static int getBeepPauseMs();
    static void setBeepPauseMs(int ms);
    static int getAlertBeepCount();
    static void setAlertBeepCount(int count);
    static unsigned long getAlertDurationMs();
    static void setAlertDurationMs(unsigned long ms);
    static String getRingtone();
    static void setRingtone(const String& value);
    
    // Security
    static String getSettingsPassword();
    static void setSettingsPassword(const String& password);
    
    // Schedule cache
    static String getLastSchedule();
    static void setLastSchedule(const String& schedule);
    static unsigned long getLastScheduleTime();
    static void setLastScheduleTime(unsigned long time);
    
    // Hebcal
    static int getHebcalMaxAttempts();
    static void setHebcalMaxAttempts(int attempts);
    static String getHebcalProxyURL();
    static void setHebcalProxyURL(const String& url);
    
    // Candle alert offsets bitmask (1=18min, 2=30min, 4=45min)
    static int getCandleAlerts();
    static void setCandleAlerts(int bitmask);

    // Check if configured
    static bool isConfigured();

private:
    #ifdef BOARD_ESP32
    static Preferences preferences;
    #else
    static bool eepromInitialized;
    static String readString(int address, int maxLen);
    static void writeString(int address, const String& value);
    static float readFloat(int address);
    static void writeFloat(int address, float value);
    static int readInt(int address);
    static void writeInt(int address, int value);
    static unsigned long readULong(int address);
    static void writeULong(int address, unsigned long value);
    static bool readBool(int address);
    static void writeBool(int address, bool value);
    #endif
};

#endif // STORAGE_H

