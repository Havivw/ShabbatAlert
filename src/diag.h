#ifndef DIAG_H
#define DIAG_H

#include <Arduino.h>

// Persistent ring-buffer of diagnostic events stored in EEPROM.
// Survives reboot so the user can read Sunday what happened Friday/Saturday,
// when they can't observe the device live (Shabbat).
//
// EEPROM layout (within the existing 1024-byte allocation; addr 720 onward
// was previously unused — see storage.cpp for the full map):
//
//   720      magic byte (0xC3)
//   721      writeIndex (0..SLOT_COUNT-1)
//   722-723  reserved
//   724..    10 slots × 28 bytes  =  280 bytes
//
// Each slot: 4 bytes unix-ts (little-endian) + 24 bytes NUL-padded text.
class Diag {
public:
    static void init();
    static void log(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
    /** Append all entries (oldest first) to `out` as "[yyyy-mm-dd HH:MM:SS] text\n". */
    static void getEntriesText(String& out);
    static void clear();

private:
    static uint8_t writeIndex;
    static bool initialized;
};

#endif // DIAG_H
