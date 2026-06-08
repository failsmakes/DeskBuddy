#pragma once
#include <Arduino.h>
#include <time.h>
#include "config.h"
#include "storage.h"
#include "rtc_manager.h"

// ============================================================
//  time_manager.h  -  NTP + DS3231 RTC saat yonetimi
//
//  Oncelik sirasi:
//    1. NTP sync basarili -> sistem saatini ayarla + RTC'ye yaz
//    2. NTP sync basarisiz + RTC gecerli -> RTC'den oku, sistemi ayarla
//    3. Her ikisi de basarisiz -> saat bilinmiyor (valid=false)
//
//  Periyodik yenileme:
//    - Her saat NTP'den yenile, basarili olursa RTC'yi de guncelle
// ============================================================

class TimeManager {
public:
    bool synced     = false;   // NTP ile en az bir kez senkronize edildi mi?
    bool rtcUsed    = false;   // Saat RTC'den alindi mi?
    unsigned long lastSyncMs = 0;

    // Cagri sirasi: once rtcManager.begin(), sonra bu
    void begin() {
        configTime(storage.tzOffsetSeconds(), 0, NTP_SERVER1, NTP_SERVER2);
        bool ntpOK = waitForSync(5000);

        if (ntpOK) {
            synced  = true;
            rtcUsed = false;
            // NTP basarili: RTC'ye yaz
            if (rtcManager.available) {
                rtcManager.writeTime(time(nullptr));
            }
            DLOG("[Time] NTP sync OK");
        } else {
            DLOG("[Time] NTP failed, trying RTC...");
            // NTP basarisiz: RTC'den oku
            if (rtcManager.available) {
                time_t rtcTime = rtcManager.readTime();
                if (rtcTime > 0) {
                    // Sistem saatini RTC'den ayarla
                    // RTC UTC saklar, tz ofseti configTime ile uygulanir
                    struct timeval tv = { .tv_sec = rtcTime, .tv_usec = 0 };
                    settimeofday(&tv, nullptr);
                    synced  = true;
                    rtcUsed = true;
                    DLOGF("[Time] Time from RTC: %s", ctime(&rtcTime));
                } else {
                    DLOG("[Time] RTC read failed or invalid");
                }
            }
        }
    }

    // Periodik NTP yenileme; basarili olursa RTC'yi de guncelle
    void maintain() {
        if (millis() - lastSyncMs < NTP_SYNC_INTERVAL_MS) return;

        configTime(storage.tzOffsetSeconds(), 0, NTP_SERVER1, NTP_SERVER2);
        bool ok = waitForSync(3000);
        if (ok) {
            synced   = true;
            rtcUsed  = false;
            lastSyncMs = millis();
            if (rtcManager.available) {
                rtcManager.writeTime(time(nullptr));
                DLOG("[Time] NTP re-synced, RTC updated");
            }
        }
    }

    struct tm getLocalTime() {
        time_t now = time(nullptr);
        struct tm t;
        localtime_r(&now, &t);
        return t;
    }

    String timeString() {
        struct tm t = getLocalTime();
        char buf[9];
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
        return String(buf);
    }

    String dateString() {
        struct tm t = getLocalTime();
        char buf[11];
        snprintf(buf, sizeof(buf), "%02d.%02d.%04d", t.tm_mday, t.tm_mon + 1, t.tm_year + 1900);
        return String(buf);
    }

    time_t epoch() { return time(nullptr); }

private:
    bool waitForSync(uint32_t timeoutMs) {
        uint32_t t = millis();
        while (time(nullptr) < 100000 && millis() - t < timeoutMs) delay(200);
        bool ok = (time(nullptr) > 100000);
        if (ok) lastSyncMs = millis();
        return ok;
    }
};

extern TimeManager timeManager;
