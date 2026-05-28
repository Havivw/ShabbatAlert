#include "rtttl_audio.h"
#include "config.h"
#include "logger.h"
#include <new>  // std::nothrow — required for non-throwing `new` on heap exhaustion

#include "AudioFileSourcePROGMEM.h"
#include "AudioGeneratorRTTTL.h"
#include "AudioOutputI2S.h"
#include "AudioOutputI2SNoDAC.h"

#define RTTTL_COUNT 8

// NOTE: original Pinky string used the non-standard "b.5" / "8b.5" / "c.7"
// pattern (dot BEFORE the octave digit).  ESP8266Audio's RTTTL parser
// expects the dot AFTER the octave (`b5.` / `8b5.` / `c7.`); anything else
// leaves the digit dangling, which made the next GetNextNote() see "5,p"
// and bail out — so the melody was cut after a single note.  Rewritten
// below in the canonical form.
static const char rtttl_pinky[] PROGMEM =
    "Pinky and the Brain:d=16,o=6,b=200:b5.,p,8e.,p,d#.,p,8e.,p,g.,p,4d#.,4p,b5.,p,8e.,p,d#.,p,8e.,p,g.,p,4d#.,4p,4e,8p,8e.,p,g.,p,8a#.,p,4a#,8p,a#.,p,8b.,p,a.,p,4g,8p,4f#,4p,e.,p,8a.,p,g#.,p,8a.,p,b.,p,4g#,4p,e.,p,8a.,p,g#.,p,8a.,p,b.,p,4g#,4p,c.,p,8b5.,p,8b5.,8p,b5.,p";

static const char rtttl_star_wars[] PROGMEM =
    "Star Wars:d=8,o=6,b=180:f5,f5,f5,2a#5.,2f.,d#,d,c,2a#.,4f.,d#,d,c,2a#.,4f.,d#,d,d#,2c,4p,f5,f5,f5,2a#5.,2f.,d#,d,c,2a#.,4f.,d#,d,c,2a#.,4f.,d#,d,d#,2c";

static const char rtttl_mozart[] PROGMEM =
    "Mozart:d=16,o=5,b=125:16d#,c#,c,c#,8e,8p,f#,e,d#,e,8g#,8p,a,g#,g,g#,d#6,c#6,c6,c#6,d#6,c#6,c6,c#6,4e6,8c#6,8e6,32b,32c#6,d#6,8c#6,8b,8c#6,32b,32c#6,d#6,8c#6,8b,8c#6,32b,32c#6,d#6,8c#6,8b,8a#,4g#,d#,32c#,c,c#,8e,8p,f#,e,d#,e,8g#,8p,a,g#,g,g#,d#6,c#6,c6,c#6,d#6,c#6,c6,c#6,4e6,8c#6,8e6,32b,32c#6,d#6,8c#6,8b,8c#6,32b,32c#6,d#6,8c#6,8b,8c#6,32b,32c#6,d#6,8c#6,8b,8a#,4g#";

static const char rtttl_under_the_sea[] PROGMEM =
    "Under the Sea:d=4,o=6,b=200:8d,8f,8a#,d7,d7,8a#,c7,d#7,d7,a#,8a#5,8d,8f,a#,a#,8c,a,c7,a#,p,8d,8f,8a#,d7,d7,8a#,c7,d#7,d7,a#,8a#5,8d,8f,a#,a#,8c,a,c7,16a#,16d,16a#,16d,16a#,16d,16a#";

// "c.7" rewritten as "c7." so the parser sees octave-then-dot (see Pinky note).
static const char rtttl_spiderman[] PROGMEM =
    "Spiderman:d=4,o=6,b=200:c,8d#,g.,p,f#,8d#,c.,p,c,8d#,g,8g#,g,f#,8d#,c.,p,f,8g#,c7.,p,a#,8g#,f.,p,c,8d#,g.,p,f#,8d#,c,p,8g#,2g,p,8f#,f#,8d#,f,8d#,2c";

static const char rtttl_mario[] PROGMEM =
    "mario:d=4,o=5,b=100:16e6,16e6,32p,8e6,16c6,8e6,8g6,8p,8g,8p,8c6,16p,8g,16p,8e,16p,8a,8b,16a#,8a,16g.,16e6,16g6,8a6,16f6,8g6,8e6,16c6,16d6,8b,16p,8c6,16p,8g,16p,8e,16p,8a,8b,16a#,8a,16g.,16e6,16g6,8a6,16f6,8g6,8e6,16c6,16d6,8b,8p,16g6,16f#6,16f6,16d#6,16p,16e6,16p,16g#,16a,16c6,16p,16a,16c6,16d6,8p,16g6,16f#6,16f6,16d#6,16p,16e6,16p,16c7,16p,16c7,16c7,p,16g6,16f#6,16f6,16d#6,16p,16e6,16p,16g#,16a,16c6,16p,16a,16c6,16d6,8p,16d#6,8p,16d6,8p,16c6";

static const char rtttl_pink_panther[] PROGMEM =
    "PinkPanther:d=4, o=5, b=160:8d#, 8e, 2p, 8f#, 8g, 2p, 8d#, 8e, 16p, 8f#, 8g, 16p, 8c6, 8b, 16p, 8d#, 8e, 16p, 8b, 2a#, 2p, 16a, 16g, 16e, 16d, 2e";

// ESP8266Audio's RTTTL header parser is strict: it requires the params in
// d,o,b order.  The original "b=120,o=5,d=8" failed begin() instantly.
static const char rtttl_hava_nagila[] PROGMEM =
    "hava_nagila:d=8,o=5,b=120:8e,8p,4e,8p,8g#,8f,8e,8g#,8p,4g#,8p,8b,8a,8g#,8a,8p,4a,8p,8c6,8b,8a,4g#,8f,8e,4g#";

static const char* const rtttl_ptrs[RTTTL_COUNT] PROGMEM = {
    rtttl_pinky,
    rtttl_star_wars,
    rtttl_mozart,
    rtttl_under_the_sea,
    rtttl_spiderman,
    rtttl_mario,
    rtttl_pink_panther,
    rtttl_hava_nagila,
};

static const char* const rtttl_ids[RTTTL_COUNT] = {
    "pinky", "star_wars", "mozart", "under_the_sea", "spiderman", "mario", "pink_panther", "hava_nagila",
};

static AudioGeneratorRTTTL* rtttl = nullptr;
static AudioFileSourcePROGMEM* file = nullptr;
static AudioOutput* out = nullptr;
static bool playing = false;

void RtttlAudio::init() {
    LOG("RtttlAudio init");
}

void RtttlAudio::stop() {
    if (rtttl) {
        rtttl->stop();
        delete rtttl;
        rtttl = nullptr;
    }
    if (file) {
        delete file;
        file = nullptr;
    }
    if (out) {
        delete out;
        out = nullptr;
    }
    playing = false;
}

void RtttlAudio::startPlaybackByIndex(int index) {
    if (index < 0 || index >= RTTTL_COUNT) return;
    stop();
    const char* ptr = (const char*)pgm_read_ptr(&rtttl_ptrs[index]);
    size_t len = strlen_P(ptr);
    // ESP8266 `new` returns nullptr on heap exhaustion (no exceptions).  After
    // days of uptime with WiFi/HTTP/JSON churn, the heap can be fragmented
    // enough that one of these allocations fails — the old code then called
    // rtttl->begin(file, out) blindly, crashing on the nullptr.  A boot loop
    // in the middle of Shabbat is exactly the "alert silently never fired"
    // failure mode the user reported.
    file = new (std::nothrow) AudioFileSourcePROGMEM(ptr, len);
#if AUDIO_OUTPUT_I2S
    out = new (std::nothrow) AudioOutputI2S();
    if (out) ((AudioOutputI2S*)out)->SetPinout(AUDIO_I2S_BCK, AUDIO_I2S_WS, AUDIO_I2S_DATA);
#else
    out = new (std::nothrow) AudioOutputI2SNoDAC(BUZZER_PIN);
#endif
    rtttl = new (std::nothrow) AudioGeneratorRTTTL();
    if (!file || !out || !rtttl) {
        LOGF("RTTTL alloc FAILED index=%d len=%u heap=%u",
             index, (unsigned)len, (unsigned)ESP.getFreeHeap());
        stop();
        return;
    }
    if (rtttl->begin(file, out)) {
        playing = true;
        LOGF("RTTTL begin OK index=%d len=%u running=%d", index, (unsigned)len, rtttl->isRunning() ? 1 : 0);
    } else {
        LOGF("RTTTL begin FAILED index=%d len=%u", index, (unsigned)len);
        stop();
    }
}

void RtttlAudio::startPlayback(const String& id) {
    for (int i = 0; i < RTTTL_COUNT; i++) {
        if (id == rtttl_ids[i]) {
            startPlaybackByIndex(i);
            return;
        }
    }
    LOGF("RTTTL unknown id: %s", id.c_str());
}

void RtttlAudio::update() {
    if (!playing || !rtttl) return;
    // Only feed the generator while it considers itself running.  If a
    // melody parser ever bails (running=false), let alerts.cpp's
    // duration timer eventually call stop() — that gives the I2S DMA
    // buffer time to drain whatever's already queued instead of being
    // torn down mid-note (which is what made truncated melodies sound
    // like a single click).
    if (!rtttl->isRunning()) return;
    if (!rtttl->loop()) {
        LOGF("RTTTL loop ended naturally (running=%d)", rtttl->isRunning() ? 1 : 0);
        // Don't delete here either — let the duration floor in alerts.cpp
        // hold the alert active long enough for the buffer to drain, then
        // Alerts::stop() → RtttlAudio::stop() will tear things down.
        playing = false;
    }
}

bool RtttlAudio::isPlaying() {
    return playing && rtttl && rtttl->isRunning();
}

int RtttlAudio::getRingtoneCount() {
    return RTTTL_COUNT;
}

String RtttlAudio::getRingtoneIdAt(int index) {
    if (index < 0 || index >= RTTTL_COUNT) return "";
    return String(rtttl_ids[index]);
}
