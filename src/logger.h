#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include "config.h"

class Logger {
public:
    static void init();
    static void log(const String& message);
    static void logf(const char* format, ...);
    static String getLogs();
    static void clear();

private:
    static String logBuffer[MAX_LOG_ENTRIES];
    static int logIndex;
    static int logCount;
};

#define LOG(msg) Logger::log(msg)
#define LOGF(fmt, ...) Logger::logf(fmt, __VA_ARGS__)

#endif // LOGGER_H

