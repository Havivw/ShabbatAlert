#ifndef EVENT_KINDS_H
#define EVENT_KINDS_H

#include <stdint.h>

enum class ShabbatKind : uint8_t { Candles, Havdalah };

enum class AlertKind : uint8_t { Candles18, Candles30, Candles45, Havdalah };

/** Stable JSON/log names (no heap). */
inline const char* alertKindToString(AlertKind k) {
    switch (k) {
        case AlertKind::Candles18: return "candles-18";
        case AlertKind::Candles30: return "candles-30";
        case AlertKind::Candles45: return "candles-45";
        case AlertKind::Havdalah: return "havdalah";
        default: return "";
    }
}

#endif
