#include "diag.h"
#include <EEPROM.h>
#include <stdarg.h>
#include <string.h>
#include "time_sync.h"
#include "logger.h"

static const uint8_t DIAG_MAGIC = 0xC3;
static const int DIAG_BASE      = 720;
static const int DIAG_DATA_BASE = 724;
static const int DIAG_SLOTS     = 10;
static const int DIAG_SLOT_SZ   = 28;   // 4 bytes ts + 24 bytes text
static const int DIAG_TEXT_LEN  = 24;

uint8_t Diag::writeIndex = 0;
bool Diag::initialized = false;

// Read a little-endian uint32 from EEPROM.
static uint32_t readU32(int addr) {
    uint32_t v = 0;
    v |= ((uint32_t)EEPROM.read(addr + 0));
    v |= ((uint32_t)EEPROM.read(addr + 1)) << 8;
    v |= ((uint32_t)EEPROM.read(addr + 2)) << 16;
    v |= ((uint32_t)EEPROM.read(addr + 3)) << 24;
    return v;
}

// Write a little-endian uint32 to EEPROM (caller must commit).
static void writeU32(int addr, uint32_t v) {
    EEPROM.write(addr + 0, v & 0xFF);
    EEPROM.write(addr + 1, (v >> 8) & 0xFF);
    EEPROM.write(addr + 2, (v >> 16) & 0xFF);
    EEPROM.write(addr + 3, (v >> 24) & 0xFF);
}

void Diag::init() {
    // EEPROM.begin() must have been called already (by Storage::init).
    uint8_t m = EEPROM.read(DIAG_BASE);
    if (m != DIAG_MAGIC) {
        // First boot with this build, or EEPROM cleared.  Reset the ring.
        EEPROM.write(DIAG_BASE, DIAG_MAGIC);
        EEPROM.write(DIAG_BASE + 1, 0);
        EEPROM.write(DIAG_BASE + 2, 0);
        EEPROM.write(DIAG_BASE + 3, 0);
        for (int i = 0; i < DIAG_SLOTS * DIAG_SLOT_SZ; i++) {
            EEPROM.write(DIAG_DATA_BASE + i, 0);
        }
        EEPROM.commit();
        writeIndex = 0;
    } else {
        writeIndex = EEPROM.read(DIAG_BASE + 1);
        if (writeIndex >= DIAG_SLOTS) writeIndex = 0;
    }
    initialized = true;
}

void Diag::log(const char* fmt, ...) {
    if (!initialized) return;

    char buf[DIAG_TEXT_LEN + 1];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    buf[DIAG_TEXT_LEN] = 0;

    // Mirror to serial — useful when a developer IS watching live.
    LOGF("diag: %s", buf);

    uint32_t ts = (uint32_t)TimeSync::getNow();  // 0 if NTP hasn't synced yet
    int slotAddr = DIAG_DATA_BASE + writeIndex * DIAG_SLOT_SZ;

    writeU32(slotAddr, ts);
    size_t len = strlen(buf);
    if (len > DIAG_TEXT_LEN) len = DIAG_TEXT_LEN;
    for (int i = 0; i < DIAG_TEXT_LEN; i++) {
        EEPROM.write(slotAddr + 4 + i, (i < (int)len) ? (uint8_t)buf[i] : (uint8_t)0);
    }

    writeIndex = (writeIndex + 1) % DIAG_SLOTS;
    EEPROM.write(DIAG_BASE + 1, writeIndex);
    EEPROM.commit();  // ~30ms; never called during audio playback (see callers).
}

void Diag::getEntriesText(String& out) {
    out = "";
    if (!initialized) {
        out = "diag not initialized\n";
        return;
    }

    long tzOffset = TimeSync::getTimezoneOffsetSeconds();

    // Walk oldest → newest: the slot we'd write next is the oldest currently
    // populated (or empty if the buffer hasn't wrapped yet).
    for (int i = 0; i < DIAG_SLOTS; i++) {
        int idx = (writeIndex + i) % DIAG_SLOTS;
        int slotAddr = DIAG_DATA_BASE + idx * DIAG_SLOT_SZ;
        uint32_t ts = readU32(slotAddr);
        char text[DIAG_TEXT_LEN + 1];
        for (int j = 0; j < DIAG_TEXT_LEN; j++) {
            text[j] = (char)EEPROM.read(slotAddr + 4 + j);
        }
        text[DIAG_TEXT_LEN] = 0;
        if (text[0] == 0) continue;  // empty slot

        char line[80];
        if (ts > 1577836800UL) {  // > 2020-01-01 → real time
            time_t local = (time_t)ts + tzOffset;
            struct tm lt;
            gmtime_r(&local, &lt);
            char tbuf[20];
            strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", &lt);
            int n = snprintf(line, sizeof(line), "[%s] %s\n", tbuf, text);
            if (n > 0 && n < (int)sizeof(line)) out += line;
        } else {
            int n = snprintf(line, sizeof(line), "[pre-ntp] %s\n", text);
            if (n > 0 && n < (int)sizeof(line)) out += line;
        }
    }
    if (out.length() == 0) out = "(no diagnostic events recorded yet)\n";
}

void Diag::clear() {
    EEPROM.write(DIAG_BASE, 0);
    EEPROM.commit();
    initialized = false;
    init();
}
