#pragma once
#include <Arduino.h>
#include "config.h"
#include "storage.h"
#include "buzzer.h"

// ============================================================
//  alarm_manager.h  -  3x Alarm / Countdown / Stopwatch
// ============================================================

#define MAX_ALARMS 3

struct AlarmEntry {
    uint8_t  hour      = 7;
    uint8_t  minute    = 0;
    bool     enabled   = false;
    bool     firing    = false;
    bool     snoozed   = false;
    unsigned long snoozeUntilMs = 0;
    uint8_t  lastMinuteChecked  = 255;
};

class AlarmManager {
public:
    // ---- 3 Alarm yuvasi ----
    AlarmEntry alarms[MAX_ALARMS];

    // Hangi alarm caliyor (255 = yok)
    uint8_t  firingIdx    = 255;

    // Kisa erisim yardimcilari (ekran ve pad kodu icin)
    bool alarmFiring  = false;   // herhangi biri caliyor mu?

    // ---- Countdown ----
    bool     countdownRunning  = false;
    bool     countdownFiring   = false;
    uint32_t countdownSetMs    = 0;
    uint32_t countdownLeftMs   = 0;
    unsigned long countdownStartedAt = 0;

    // ---- Stopwatch ----
    bool     swRunning   = false;
    uint32_t swElapsedMs = 0;
    unsigned long swStartedAt = 0;

    // ---- EEPROM entegrasyonu ----

    // Boot'ta storage'dan alarm verilerini yukle
    void loadFromStorage() {
        for (uint8_t i = 0; i < MAX_ALARMS; i++) {
            alarms[i].hour    = storage.cfg.alarms[i].hour;
            alarms[i].minute  = storage.cfg.alarms[i].minute;
            alarms[i].enabled = storage.cfg.alarms[i].enabled;
            // Runtime state'leri sifirla
            alarms[i].firing              = false;
            alarms[i].snoozed             = false;
            alarms[i].snoozeUntilMs       = 0;
            alarms[i].lastMinuteChecked   = 255;
        }
    }

    // Alarm degisikliklerini EEPROM'a kaydet
    void saveToStorage() {
        for (uint8_t i = 0; i < MAX_ALARMS; i++) {
            storage.cfg.alarms[i].hour    = alarms[i].hour;
            storage.cfg.alarms[i].minute  = alarms[i].minute;
            storage.cfg.alarms[i].enabled = alarms[i].enabled;
        }
        storage.save();
    }

    // ---- loop() icinde her iterasyonda cagir ----
    void update(struct tm& localTime) {
        updateAlarms(localTime);
        updateCountdown();
    }

    // ---- Alarm kontrol ----
    void setAlarm(uint8_t idx, uint8_t h, uint8_t m) {
        if (idx >= MAX_ALARMS) return;
        alarms[idx].hour   = h;
        alarms[idx].minute = m;
        // Not: save() enableAlarm/disableAlarm'dan cagrilir
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
        alarms[firingIdx].firing       = false;
        alarms[firingIdx].snoozed      = true;
        alarms[firingIdx].snoozeUntilMs = millis() + ALARM_SNOOZE_MS;
        buzzer.stop();
        _refreshFiringState();
    }
    void dismissAlarm() {
        if (firingIdx >= MAX_ALARMS) return;
        alarms[firingIdx].firing   = false;
        alarms[firingIdx].snoozed  = false;
        alarms[firingIdx].enabled  = false;
        buzzer.stop();
        _refreshFiringState();
    }

    // Calan alarmin saat/dakika bilgisi
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
        countdownLeftMs  = countdownSetMs;
        buzzer.stop();
    }
    void countdownDismiss() {
        countdownFiring = false;
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

    // Format ms -> "HH:MM:SS"
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
                a.snoozed = false;
                a.firing  = true;
            }

            // Saat eslesme kontrolu
            if (!a.firing && !a.snoozed) {
                if (t.tm_hour == a.hour && t.tm_min == a.minute) {
                    if (a.lastMinuteChecked != (uint8_t)t.tm_min) {
                        a.lastMinuteChecked = (uint8_t)t.tm_min;
                        a.firing = true;
                    }
                } else {
                    a.lastMinuteChecked = 255;
                }
            }
        }
        _refreshFiringState();
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

    // alarmFiring ve firingIdx'i guncelle
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
