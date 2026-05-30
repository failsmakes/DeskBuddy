#pragma once
#include <Arduino.h>
#include <time.h>
#include "config.h"
#include "storage.h"

// ============================================================
//  time_manager.h  –  NTP sync & time helpers
// ============================================================

class TimeManager {
public:
    bool synced = false;
    unsigned long lastSyncMs = 0;

    void begin() {
        configTime(storage.tzOffsetSeconds(), 0, NTP_SERVER1, NTP_SERVER2);
        waitForSync(5000);
    }

    bool waitForSync(uint32_t timeoutMs) {
        uint32_t t = millis();
        while (!time(nullptr) && millis() - t < timeoutMs) delay(200);
        synced = (time(nullptr) > 100000);
        if (synced) lastSyncMs = millis();
        return synced;
    }

    void maintain() {
        if (!synced || millis() - lastSyncMs > NTP_SYNC_INTERVAL_MS) {
            configTime(storage.tzOffsetSeconds(), 0, NTP_SERVER1, NTP_SERVER2);
            waitForSync(3000);
        }
    }

    // Returns local time struct
    struct tm getLocalTime() {
        time_t now = time(nullptr);
        struct tm t;
        localtime_r(&now, &t);
        return t;
    }

    // HH:MM:SS string
    String timeString() {
        struct tm t = getLocalTime();
        char buf[9];
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
        return String(buf);
    }

    // DD.MM.YYYY string
    String dateString() {
        struct tm t = getLocalTime();
        char buf[11];
        snprintf(buf, sizeof(buf), "%02d.%02d.%04d", t.tm_mday, t.tm_mon + 1, t.tm_year + 1900);
        return String(buf);
    }

    // Epoch (UTC)
    time_t epoch() { return time(nullptr); }
};

extern TimeManager timeManager;
