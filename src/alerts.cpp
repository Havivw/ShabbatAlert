#include "alerts.h"
#include "logger.h"
#include "storage.h"
#include "rtttl_audio.h"

bool Alerts::isActive = false;
unsigned long Alerts::alertStartTime = 0;

void Alerts::init() {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
#ifdef BOARD_ESP8266
    RtttlAudio::init();
#else
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
#endif
    LOG("Alerts initialized");
}

void Alerts::update() {
#ifdef BOARD_ESP8266
    RtttlAudio::update();
#endif
    updateLED();
}

void Alerts::trigger(const String& alertType) {
    if (isActive) {
        return;
    }
    isActive = true;
    alertStartTime = millis();
    LOGF("Alert triggered: %s", alertType.c_str());

#ifdef BOARD_ESP8266
    String choice = Storage::getRingtone();
    if (choice == "random") {
        int n = RtttlAudio::getRingtoneCount();
        if (n > 0) {
            int idx = random(0, n);
            RtttlAudio::startPlaybackByIndex(idx);
        }
    } else {
        RtttlAudio::startPlayback(choice);
    }
#else
    int count = Storage::getAlertBeepCount();
    if (alertType.indexOf("candles") >= 0) {
        beep(count);
    } else if (alertType.indexOf("havdalah") >= 0) {
        beep(count * 2);
    } else {
        beep(1);
    }
#endif
    updateLED();
}

void Alerts::test() {
    LOG("Testing alerts");
#ifdef BOARD_ESP8266
    isActive = true;
    alertStartTime = millis();
    String choice = Storage::getRingtone();
    if (choice == "random") {
        int n = RtttlAudio::getRingtoneCount();
        if (n > 0) {
            RtttlAudio::startPlaybackByIndex(random(0, n));
        } else {
            RtttlAudio::startPlaybackByIndex(0);
        }
    } else if (choice.length() > 0) {
        RtttlAudio::startPlayback(choice);
        if (!RtttlAudio::isPlaying()) {
            RtttlAudio::startPlayback("pinky");
            if (!RtttlAudio::isPlaying()) {
                RtttlAudio::startPlaybackByIndex(0);
            }
        }
    } else {
        RtttlAudio::startPlayback("pinky");
        if (!RtttlAudio::isPlaying()) {
            RtttlAudio::startPlaybackByIndex(0);
        }
    }
#else
    int duration = Storage::getBeepDurationMs();
    if (duration <= 0) duration = DEFAULT_BEEP_DURATION_MS;
    beep(3);
    for (int i = 0; i < 5; i++) {
        digitalWrite(LED_PIN, HIGH);
        delay(duration);
        digitalWrite(LED_PIN, LOW);
        delay(duration);
    }
#endif
}

void Alerts::stop() {
    isActive = false;
#ifdef BOARD_ESP8266
    RtttlAudio::stop();
#else
    digitalWrite(BUZZER_PIN, LOW);
#endif
    digitalWrite(LED_PIN, LOW);
}

void Alerts::beep(int count) {
    int duration = Storage::getBeepDurationMs();
    int pause = Storage::getBeepPauseMs();
    if (duration <= 0) duration = DEFAULT_BEEP_DURATION_MS;
    if (pause <= 0) pause = DEFAULT_BEEP_PAUSE_MS;
    for (int i = 0; i < count; i++) {
        digitalWrite(BUZZER_PIN, HIGH);
        delay(duration);
        digitalWrite(BUZZER_PIN, LOW);
        if (i < count - 1) {
            delay(pause);
        }
    }
}

void Alerts::updateLED() {
    if (isActive) {
        unsigned long elapsed = millis() - alertStartTime;
        unsigned long duration = Storage::getAlertDurationMs();
        if (duration == 0) duration = ALERT_DURATION_MS;
        if (elapsed < duration) {
            // Blink LED during alert
            bool state = (elapsed / 500) % 2;
            digitalWrite(LED_PIN, state ? HIGH : LOW);
        } else {
            stop();
        }
    }
}

