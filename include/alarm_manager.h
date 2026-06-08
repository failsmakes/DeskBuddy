#pragma once
#include <Arduino.h>
#include "config.h"
#include "storage.h"
#include "buzzer.h"

// ============================================================
//  alarm_manager.h  -  3x Alarm / Countdown / Stopwatch
// ============================================================

#define MAX_ALARMS       3
#define ALARM_AUTO_STOP_MS  180000UL   // 180 saniye sonra otomatik kapat

struct AlarmEntry {
    uint8_t  hour      = 7;
    uint8_t  minute    = 0;
    bool     enabled   = false;
    bool     firing    = false;
    bool     snoozed   = false;
    unsigned long snoozeUntilMs  = 0;
    unsigned long firingStartMs  = 0;  // alarm calınmaya başlama zamani
    uint8_t  lastMinuteChecked   = 255;
};

class AlarmManager {
public:
    AlarmEntry alarms[MAX_ALARMS];
    uint8_t  firingIdx    = 255;
    bool     alarmFiring  = false;

    // Countdown
    bool     countdownRunning  = false;
    bool     countdownFiring   = false;
    uint32_t countdownSetMs    = 0;
    uint32_t countdownLeftMs   = 0;
    unsigned long countdownStartedAt  = 0;
    unsigned long countdownFiringStartMs = 0;  // countdown alarm baslama zamani

    // Stopwatch
    bool     swRunning   = false;
    uint32_t swElapsedMs = 0;
    unsigned long swStartedAt = 0;

    // EEPROM
    void loadFromStorage() {
        for (uint8_t i = 0; i < MAX_ALARMS; i++) {
            alarms[i].hour    = storage.cfg.alarms[i].hour;
            alarms[i].minute  = storage.cfg.alarms[i].minute;
            alarms[i].enabled = storage.cfg.alarms[i].enabled;
            alarms[i].firing           = false;
            alarms[i].snoozed          = false;
            alarms[i].snoozeUntilMs    = 0;
            alarms[i].firingStartMs    = 0;
            alarms[i].lastMinuteChecked = 255;
        }
    }

    void saveToStorage() {
        for (uint8_t i = 0; i < MAX_ALARMS; i++) {
            storage.cfg.alarms[i].hour    = alarms[i].hour;
            storage.cfg.alarms[i].minute  = alarms[i].minute;
            storage.cfg.alarms[i].enabled = alarms[i].enabled;
        }
        storage.save();
    }

    void update(struct tm& localTime) {
        updateAlarms(localTime);
        updateCountdown();
    }

    // ---- Alarm kontrol ----
    void setAlarm(uint8_t idx, uint8_t h, uint8_t m) {
        if (idx >= MAX_ALARMS) return;
        alarms[idx].hour   = h;
        alarms[idx].minute = m;
    }
    void enableAlarm(uint8_t idx) {
        if (idx >= MAX_ALARMS) return;
        alarms[idx].enabled = true;
        alarms[idx].firing  = false;
        saveToStorage();
    }
    void disableAlarm(uint8_t idx) {
        if (idx >= MAX_ALARMS) return;
        alarms[idx].enabled = false;
        alarms[idx].firing  = false;
        _refreshFiringState();
        saveToStorage();
    }
    void snoozeAlarm() {
        if (firingIdx >= MAX_ALARMS) return;
        alarms[firingIdx].firing        = false;
        alarms[firingIdx].snoozed       = true;
        alarms[firingIdx].snoozeUntilMs = millis() + ALARM_SNOOZE_MS;
        buzzer.stop();
        _refreshFiringState();
    }
    void dismissAlarm() {
        if (firingIdx >= MAX_ALARMS) return;
        alarms[firingIdx].firing   = false;
        alarms[firingIdx].snoozed  = false;
        alarms[firingIdx].enabled  = false;
        alarms[firingIdx].firingStartMs = 0;
        buzzer.stop();
        _refreshFiringState();
    }

    uint8_t firingHour()   { return firingIdx < MAX_ALARMS ? alarms[firingIdx].hour   : 0; }
    uint8_t firingMinute() { return firingIdx < MAX_ALARMS ? alarms[firingIdx].minute : 0; }

    // ---- Countdown ----
    void countdownSet(uint32_t ms) {
        countdownSetMs  = ms;
        countdownLeftMs = ms;
        countdownFiring = false;
    }
    void countdownStart() {
        if (!countdownRunning && countdownLeftMs > 0) {
            countdownRunning    = true;
            countdownStartedAt  = millis();
        }
    }
    void countdownStop() {
        if (countdownRunning) {
            countdownLeftMs -= (millis() - countdownStartedAt);
            countdownRunning = false;
        }
    }
    void countdownReset() {
        countdownRunning = false;
        countdownFiring  = false;
        countdownFiringStartMs = 0;
        countdownLeftMs  = countdownSetMs;
        buzzer.stop();
    }
    void countdownDismiss() {
        countdownFiring = false;
        countdownFiringStartMs = 0;
        buzzer.stop();
        countdownReset();
    }
    uint32_t countdownCurrent() {
        if (!countdownRunning) return countdownLeftMs;
        uint32_t elapsed = millis() - countdownStartedAt;
        return (elapsed >= countdownLeftMs) ? 0 : countdownLeftMs - elapsed;
    }

    // ---- Stopwatch ----
    void swStart() {
        if (!swRunning) { swRunning = true; swStartedAt = millis(); }
    }
    void swStop() {
        if (swRunning) { swElapsedMs += millis() - swStartedAt; swRunning = false; }
    }
    void swReset() { swRunning = false; swElapsedMs = 0; }
    uint32_t swCurrent() {
        return swRunning ? swElapsedMs + (millis() - swStartedAt) : swElapsedMs;
    }

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
    void updateAlarms(struct tm& t) {
        for (uint8_t i = 0; i < MAX_ALARMS; i++) {
            AlarmEntry& a = alarms[i];
            if (!a.enabled) continue;

            // Snooze suresi doldu mu?
            if (a.snoozed && millis() >= a.snoozeUntilMs) {
                a.snoozed       = false;
                a.firing        = true;
                a.firingStartMs = millis();
            }

            // Saat eslesme
            if (!a.firing && !a.snoozed) {
                if (t.tm_hour == a.hour && t.tm_min == a.minute) {
                    if (a.lastMinuteChecked != (uint8_t)t.tm_min) {
                        a.lastMinuteChecked = (uint8_t)t.tm_min;
                        a.firing        = true;
                        a.firingStartMs = millis();
                    }
                } else {
                    a.lastMinuteChecked = 255;
                }
            }

            // 180 saniye auto-stop
            if (a.firing && a.firingStartMs > 0) {
                if (millis() - a.firingStartMs >= ALARM_AUTO_STOP_MS) {
                    DLOGF("[Alarm] Auto-stop alarm %d after %lus", i,
                          (unsigned long)(ALARM_AUTO_STOP_MS / 1000));
                    a.firing   = false;
                    a.enabled  = false;   // otomatik kapaninca alarmi da kapat
                    a.firingStartMs = 0;
                    saveToStorage();
                }
            }
        }
        _refreshFiringState();
        if (alarmFiring) buzzer.startAlert();
    }

    void updateCountdown() {
        if (!countdownRunning) return;
        if (countdownCurrent() == 0) {
            countdownRunning       = false;
            countdownFiring        = true;
            countdownFiringStartMs = millis();
            buzzer.startAlert();
        }
        // 180 saniye auto-stop
        if (countdownFiring && countdownFiringStartMs > 0) {
            if (millis() - countdownFiringStartMs >= ALARM_AUTO_STOP_MS) {
                DLOGF("[Alarm] Auto-stop countdown after %lus",
                      (unsigned long)(ALARM_AUTO_STOP_MS / 1000));
                countdownDismiss();
            }
        }
    }

    void _refreshFiringState() {
        firingIdx   = 255;
        alarmFiring = false;
        for (uint8_t i = 0; i < MAX_ALARMS; i++) {
            if (alarms[i].firing) {
                firingIdx   = i;
                alarmFiring = true;
                return;
            }
        }
    }
};

extern AlarmManager alarmMgr;
