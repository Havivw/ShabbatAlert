#include "rtttl_audio.h"
#include "config.h"
#include "logger.h"

#ifdef BOARD_ESP8266
#include "AudioFileSourcePROGMEM.h"
#include "AudioGeneratorRTTTL.h"
#include "AudioOutputI2S.h"
#include "AudioOutputI2SNoDAC.h"
#endif

#ifdef BOARD_ESP8266

#define RTTTL_COUNT 7

static const char rtttl_pinky[] PROGMEM =
    "Pinky and the Brain:d=16,o=6,b=200:b.5,p,8e.,p,d#.,p,8e.,p,g.,p,4d#.,4p,b.5,p,8e.,p,d#.,p,8e.,p,g.,p,4d#.,4p,4e,8p,8e.,p,g.,p,8a#.,p,4a#,8p,a#.,p,8b.,p,a.,p,4g,8p,4f#,4p,e.,p,8a.,p,g#.,p,8a.,p,b.,p,4g#,4p,e.,p,8a.,p,g#.,p,8a.,p,b.,p,4g#,4p,c.,p,8b.5,p,8b.5,8p,b.5,p";

static const char rtttl_star_wars[] PROGMEM =
    "Star Wars:d=8,o=6,b=180:f5,f5,f5,2a#5.,2f.,d#,d,c,2a#.,4f.,d#,d,c,2a#.,4f.,d#,d,d#,2c,4p,f5,f5,f5,2a#5.,2f.,d#,d,c,2a#.,4f.,d#,d,c,2a#.,4f.,d#,d,d#,2c";

static const char rtttl_mozart[] PROGMEM =
    "Mozart:d=16,o=5,b=125:16d#,c#,c,c#,8e,8p,f#,e,d#,e,8g#,8p,a,g#,g,g#,d#6,c#6,c6,c#6,d#6,c#6,c6,c#6,4e6,8c#6,8e6,32b,32c#6,d#6,8c#6,8b,8c#6,32b,32c#6,d#6,8c#6,8b,8c#6,32b,32c#6,d#6,8c#6,8b,8a#,4g#,d#,32c#,c,c#,8e,8p,f#,e,d#,e,8g#,8p,a,g#,g,g#,d#6,c#6,c6,c#6,d#6,c#6,c6,c#6,4e6,8c#6,8e6,32b,32c#6,d#6,8c#6,8b,8c#6,32b,32c#6,d#6,8c#6,8b,8c#6,32b,32c#6,d#6,8c#6,8b,8a#,4g#";

static const char rtttl_under_the_sea[] PROGMEM =
    "Under the Sea:d=4,o=6,b=200:8d,8f,8a#,d7,d7,8a#,c7,d#7,d7,a#,8a#5,8d,8f,a#,a#,8c,a,c7,a#,p,8d,8f,8a#,d7,d7,8a#,c7,d#7,d7,a#,8a#5,8d,8f,a#,a#,8c,a,c7,16a#,16d,16a#,16d,16a#,16d,16a#";

static const char rtttl_spiderman[] PROGMEM =
    "Spiderman:d=4,o=6,b=200:c,8d#,g.,p,f#,8d#,c.,p,c,8d#,g,8g#,g,f#,8d#,c.,p,f,8g#,c.7,p,a#,8g#,f.,p,c,8d#,g.,p,f#,8d#,c,p,8g#,2g,p,8f#,f#,8d#,f,8d#,2c";

static const char rtttl_mario[] PROGMEM =
    "mario:d=4,o=5,b=100:16e6,16e6,32p,8e6,16c6,8e6,8g6,8p,8g,8p,8c6,16p,8g,16p,8e,16p,8a,8b,16a#,8a,16g.,16e6,16g6,8a6,16f6,8g6,8e6,16c6,16d6,8b,16p,8c6,16p,8g,16p,8e,16p,8a,8b,16a#,8a,16g.,16e6,16g6,8a6,16f6,8g6,8e6,16c6,16d6,8b,8p,16g6,16f#6,16f6,16d#6,16p,16e6,16p,16g#,16a,16c6,16p,16a,16c6,16d6,8p,16g6,16f#6,16f6,16d#6,16p,16e6,16p,16c7,16p,16c7,16c7,p,16g6,16f#6,16f6,16d#6,16p,16e6,16p,16g#,16a,16c6,16p,16a,16c6,16d6,8p,16d#6,8p,16d6,8p,16c6";

static const char rtttl_pink_panther[] PROGMEM =
    "PinkPanther:d=4, o=5, b=160:8d#, 8e, 2p, 8f#, 8g, 2p, 8d#, 8e, 16p, 8f#, 8g, 16p, 8c6, 8b, 16p, 8d#, 8e, 16p, 8b, 2a#, 2p, 16a, 16g, 16e, 16d, 2e";

static const char* const rtttl_ptrs[RTTTL_COUNT] PROGMEM = {
    rtttl_pinky,
    rtttl_star_wars,
    rtttl_mozart,
    rtttl_under_the_sea,
    rtttl_spiderman,
    rtttl_mario,
    rtttl_pink_panther,
};

static const char* const rtttl_ids[RTTTL_COUNT] = {
    "pinky", "star_wars", "mozart", "under_the_sea", "spiderman", "mario", "pink_panther",
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
    file = new AudioFileSourcePROGMEM(ptr, len);
#if AUDIO_OUTPUT_I2S
    out = new AudioOutputI2S();
    ((AudioOutputI2S*)out)->SetPinout(AUDIO_I2S_BCK, AUDIO_I2S_WS, AUDIO_I2S_DATA);
#else
    out = new AudioOutputI2SNoDAC(BUZZER_PIN);
#endif
    rtttl = new AudioGeneratorRTTTL();
    if (rtttl->begin(file, out)) {
        playing = true;
        LOGF("RTTTL playing index %d", index);
    } else {
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
    if (rtttl->isRunning()) {
        if (!rtttl->loop()) {
            rtttl->stop();
            playing = false;
            delete rtttl;
            delete file;
            delete out;
            rtttl = nullptr;
            file = nullptr;
            out = nullptr;
        }
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

#else // !BOARD_ESP8266 (e.g. ESP32)

void RtttlAudio::init() {}
void RtttlAudio::startPlayback(const String& id) { (void)id; }
void RtttlAudio::startPlaybackByIndex(int index) { (void)index; }
void RtttlAudio::update() {}
void RtttlAudio::stop() {}
bool RtttlAudio::isPlaying() { return false; }
int RtttlAudio::getRingtoneCount() { return 0; }
String RtttlAudio::getRingtoneIdAt(int index) { (void)index; return ""; }

#endif
