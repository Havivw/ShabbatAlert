#include "logger.h"
#include <stdarg.h>

/** Stack buffer for formatted lines; keep modest to limit peak stack use. */
#define LOG_FMT_MAX 192

void Logger::log(const char* message) {
    if (!message) {
        message = "";
    }
    unsigned long now = millis();
    Serial.printf("[%lu.%03lu] %s\n", now / 1000UL, now % 1000UL, message);
}

void Logger::logf(const char* format, ...) {
    char buffer[LOG_FMT_MAX];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    log(buffer);
}
