#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>

/** Serial-only logging (no RAM ring buffer — saves ~6KB on ESP8266). */
class Logger {
public:
    static void log(const char* message);
    static void log(const String& message) { log(message.c_str()); }
    static void logf(const char* format, ...);
};

#define LOG(msg) Logger::log(msg)
#define LOGF(fmt, ...) Logger::logf(fmt, __VA_ARGS__)

#endif // LOGGER_H
