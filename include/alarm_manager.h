#pragma once
#include <Arduino.h>
#include "config.h"
#include "buzzer.h"

// ============================================================
//  alarm_manager.h  –  Alarm / Countdown / Stopwatch
// ============================================================

class AlarmManager {
public:
    // ---- Alarm ----
    bool     alarmEnabled  = false;
    uint8_t  alarmHour     = 7;
    uint8_t  alarmMinute   = 0;
    bool     alarmFiring   = false;
    bool     alarmSnoozed  = false;
    unsigned long snoozeUntilMs = 0;

    // ---- Countdown ----
    bool     countdownRunning = false;
    bool     countdownFiring  = false;
    uint32_t countdownSetMs   = 0;   // set duration in ms
    uint32_t countdownLeftMs  = 0;   // remaining
    unsigned long countdownStartedAt = 0;

    // ---- Stopwatch ----
    bool     swRunning  = false;
    uint32_t swElapsedMs = 0;
    unsigned long swStartedAt = 0;

    // ---- Called every loop ----
    void update(struct tm& localTime) {
        updateAlarm(localTime);
        updateCountdown();
        updateStopwatch();
    }

    // --- Alarm control ---
    void setAlarm(uint8_t h, uint8_t m) { alarmHour = h; alarmMinute = m; }
    void enableAlarm()  { alarmEnabled = true;  alarmFiring = false; }
    void disableAlarm() { alarmEnabled = false; alarmFiring = false; buzzer.stop(); }
    void snoozeAlarm()  {
        alarmFiring = false;
        alarmSnoozed = true;
        snoozeUntilMs = millis() + ALARM_SNOOZE_MS;
        buzzer.stop();
    }
    void dismissAlarm() {
        alarmFiring  = false;
        alarmSnoozed = false;
        alarmEnabled = false;
        buzzer.stop();
    }

    // --- Countdown control ---
    void countdownSet(uint32_t ms) { countdownSetMs = ms; countdownLeftMs = ms; countdownFiring = false; }
    void countdownStart() {
        if (!countdownRunning && countdownLeftMs > 0) {
            countdownRunning = true;
            countdownStartedAt = millis();
        }
    }
    void countdownStop()  {
        if (countdownRunning) {
            countdownLeftMs -= (millis() - countdownStartedAt);
            countdownRunning = false;
        }
    }
    void countdownReset() {
        countdownRunning = false;
        countdownFiring  = false;
        countdownLeftMs  = countdownSetMs;
        buzzer.stop();
    }
    void countdownDismiss() {
        countdownFiring = false;
        buzzer.stop();
        countdownReset();
    }

    // --- Stopwatch control ---
    void swStart() {
        if (!swRunning) {
            swRunning    = true;
            swStartedAt  = millis();
        }
    }
    void swStop() {
        if (swRunning) {
            swElapsedMs += millis() - swStartedAt;
            swRunning = false;
        }
    }
    void swReset() {
        swRunning    = false;
        swElapsedMs  = 0;
    }

    uint32_t swCurrent() {
        return swRunning ? swElapsedMs + (millis() - swStartedAt) : swElapsedMs;
    }

    uint32_t countdownCurrent() {
        if (!countdownRunning) return countdownLeftMs;
        uint32_t elapsed = millis() - countdownStartedAt;
        return (elapsed >= countdownLeftMs) ? 0 : countdownLeftMs - elapsed;
    }

    // Format ms → "HH:MM:SS"
    String formatMs(uint32_t ms) {
        uint32_t s = ms / 1000;
        char buf[9];
        snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu",
            (unsigned long)(s / 3600),
            (unsigned long)((s % 3600) / 60),
            (unsigned long)(s % 60));
        return String(buf);
    }

private:
    bool _alarmFiredThisMinute = false;
    uint8_t _lastMinuteChecked = 255;

    void updateAlarm(struct tm& t) {
        if (!alarmEnabled) return;

        // Snooze check
        if (alarmSnoozed && millis() >= snoozeUntilMs) {
            alarmSnoozed = false;
            alarmFiring  = true;
        }

        // Time match
        if (!alarmFiring && !alarmSnoozed) {
            if (t.tm_hour == alarmHour && t.tm_min == alarmMinute) {
                if (_lastMinuteChecked != (uint8_t)t.tm_min) {
                    _lastMinuteChecked = (uint8_t)t.tm_min;
                    alarmFiring = true;
                }
            } else {
                _lastMinuteChecked = 255;
            }
        }

        if (alarmFiring) buzzer.startAlert();
    }

    void updateCountdown() {
        if (!countdownRunning) return;
        if (countdownCurrent() == 0) {
            countdownRunning = false;
            countdownFiring  = true;
            buzzer.startAlert();
        }
    }

    void updateStopwatch() {
        // Nothing extra needed; swCurrent() is computed on demand
    }
};

extern AlarmManager alarmMgr;
