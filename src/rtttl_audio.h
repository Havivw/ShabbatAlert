#ifndef RTTTL_AUDIO_H
#define RTTTL_AUDIO_H

#include <Arduino.h>

// RTTTL playback (ESP8266 only with ESP8266Audio). No-op on ESP32.
class RtttlAudio {
public:
    static void init();
    static void startPlayback(const String& id);
    static void startPlaybackByIndex(int index);
    static void update();
    static void stop();
    static bool isPlaying();
    static int getRingtoneCount();
    static String getRingtoneIdAt(int index);
};

#endif // RTTTL_AUDIO_H
