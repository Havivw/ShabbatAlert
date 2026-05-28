#include "alerts.h"
#include "logger.h"
#include "storage.h"
#include "rtttl_audio.h"
#include "diag.h"

bool Alerts::isActive = false;
unsigned long Alerts::alertStartTime = 0;
unsigned long Alerts::activeDurationMs = ALERT_DURATION_MS;

void Alerts::init() {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    // Pin the buzzer line LOW immediately so it doesn't float during the
    // ~100ms between power-on and the I2S peripheral being configured for
    // the first playback.  Floating GPIO15 produced a brief boot "click"
    // through the buzzer that the user heard as "a sound when turn on".
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
    RtttlAudio::init();
    LOG("Alerts initialized");
}

void Alerts::update() {
    RtttlAudio::update();
    updateLED();
}

bool Alerts::trigger(AlertKind kind) {
    if (isActive) {
        // A previous alert is still playing; the caller should NOT mark this
        // event triggered — it will retry next loop iteration once the prior
        // alert finishes (within the scheduler's overdue grace window).
        return false;
    }
    isActive = true;
    alertStartTime = millis();
    activeDurationMs = Storage::getAlertDurationMs();
    if (activeDurationMs == 0) activeDurationMs = ALERT_DURATION_MS;
    LOGF("Alert triggered: %s", alertKindToString(kind));
    // Persist before kicking off audio so the EEPROM commit (~30ms) finishes
    // before the I2S DMA buffer starts being filled — no risk of mid-note glitch.
    Diag::log("alert %s", alertKindToString(kind));

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
    updateLED();
    return true;
}

void Alerts::test() {
    LOG("Testing alerts");
    // If audio is already playing (user double-clicked the button), tear it
    // down first so the new request can take over cleanly.  Calling
    // startPlayback while a previous instance was still alive would also
    // stop+restart, but it left the LED state stale.
    if (isActive) {
        RtttlAudio::stop();
    }
    isActive = true;
    alertStartTime = millis();
    activeDurationMs = Storage::getAlertDurationMs();
    if (activeDurationMs == 0) activeDurationMs = ALERT_DURATION_MS;

    String choice = Storage::getRingtone();
    if (choice == "random") {
        int n = RtttlAudio::getRingtoneCount();
        if (n > 0) RtttlAudio::startPlaybackByIndex(random(0, n));
        else      RtttlAudio::startPlaybackByIndex(0);
    } else if (choice.length() > 0) {
        // One call only — the previous "if (!isPlaying()) restart" cascade
        // was racy (rtttl->isRunning() can be momentarily false right after
        // begin() succeeds) and could thrash the audio output through
        // multiple stop/start cycles, leaving only the very first note
        // audible.  trigger() has always called startPlayback exactly once
        // and works fine; do the same here.
        RtttlAudio::startPlayback(choice);
    } else {
        RtttlAudio::startPlaybackByIndex(0);
    }
}

void Alerts::stop() {
    isActive = false;
    RtttlAudio::stop();
    digitalWrite(LED_PIN, LOW);
}

void Alerts::updateLED() {
    if (!isActive) return;

    unsigned long elapsed = millis() - alertStartTime;
    // alert_duration_ms is treated as a *floor* for the LED blink, but the
    // RTTTL melody is always allowed to play to its natural end so the user
    // hears the full tune (previously short defaults like 2s cut songs after
    // 1–2 notes — that's the "only one note" symptom we were chasing).
    bool melodyStillPlaying = RtttlAudio::isPlaying();
    if (elapsed < activeDurationMs || melodyStillPlaying) {
        bool state = (elapsed / 500) % 2;
        digitalWrite(LED_PIN, state ? HIGH : LOW);
    } else {
        LOGF("Alert auto-stopping after %lums (melody finished, duration met)", elapsed);
        stop();
    }
}

