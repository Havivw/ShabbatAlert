#include "logger.h"
#include <stdarg.h>

String Logger::logBuffer[MAX_LOG_ENTRIES];
int Logger::logIndex = 0;
int Logger::logCount = 0;

void Logger::init() {
    logIndex = 0;
    logCount = 0;
    log("Logger initialized");
}

void Logger::log(const String& message) {
    unsigned long now = millis();
    String timestamp = String(now / 1000) + "." + String(now % 1000);
    String logEntry = "[" + timestamp + "] " + message;
    
    Serial.println(logEntry);
    
    // Store in ring buffer
    logBuffer[logIndex] = logEntry;
    logIndex = (logIndex + 1) % MAX_LOG_ENTRIES;
    if (logCount < MAX_LOG_ENTRIES) {
        logCount++;
    }
}

void Logger::logf(const char* format, ...) {
    char buffer[LOG_BUFFER_SIZE];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, LOG_BUFFER_SIZE, format, args);
    va_end(args);
    log(String(buffer));
}

String Logger::getLogs() {
    String result = "";
    int start = (logCount < MAX_LOG_ENTRIES) ? 0 : logIndex;
    int count = logCount;
    
    for (int i = 0; i < count; i++) {
        int idx = (start + i) % MAX_LOG_ENTRIES;
        result += logBuffer[idx] + "\n";
    }
    
    return result;
}

void Logger::clear() {
    logIndex = 0;
    logCount = 0;
    for (int i = 0; i < MAX_LOG_ENTRIES; i++) {
        logBuffer[i] = "";
    }
    log("Logs cleared");
}

