#ifndef RTTTL_AUDIO_H
#define RTTTL_AUDIO_H

#include <Arduino.h>

// RTTTL playback via ESP8266Audio (NoDAC by default).
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
