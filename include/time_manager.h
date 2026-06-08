#pragma once
#include <Arduino.h>
#include <time.h>
#include <esp_sntp.h>
#include "config.h"
#include "storage.h"
#include "rtc_manager.h"

// ============================================================
//  time_manager.h  -  NTP + DS3231 RTC saat yonetimi
//
//  KOK SORUN VE COZUM:
//  -------------------
//  configTime(gmtOffset, dst, server) -> icinde setenv("TZ","UTC0") var,
//  sonradan yapilan setenv("TZ","UTC-3") eziliyor.
//
//  configTzTime(tzString, server1, server2) kullan:
//  - Hem TZ'yi dogru ayarlar hem NTP'yi baslatir
//  - Tek cagri, cakisma yok
//
//  POSIX TZ string kurali (TERS isaret!):
//    UTC+3  (Turkiye) -> "UTC-3"
//    UTC-5  (EST)     -> "UTC+5"
//    UTC+5:30 (IST)   -> "UTC-5:30"
//
//  localtime_r() bu TZ'yi otomatik uygular.
// ============================================================

#define MIN_VALID_EPOCH  1704067200UL   // 1 Ocak 2024 UTC

class TimeManager {
public:
    bool synced         = false;
    bool rtcUsed        = false;
    unsigned long lastSyncMs  = 0;
    bool _firstSyncDone = false;

    void begin() {
        // TZ string olustur ve configTzTime ile hem TZ hem NTP baslat
        char tzStr[16];
        _buildTzString(tzStr, sizeof(tzStr));

        DLOGF("[Time] configTzTime(\"%s\", ...) offset=%+.2fh",
              tzStr, storage.tzOffsetSeconds() / 3600.0f);

        // configTzTime: tek cagri, hem TZ ayarlar hem SNTP baslatir
        configTzTime(tzStr, NTP_SERVER1, NTP_SERVER2);

        bool ntpOK = _waitForSync(8000);

        if (ntpOK) {
            synced         = true;
            rtcUsed        = false;
            _firstSyncDone = true;
            lastSyncMs     = millis();

            time_t utcNow = time(nullptr);
            struct tm lt = getLocalTime();
            DLOGF("[Time] NTP OK  UTC=%lu  Local=%02d:%02d:%02d %02d.%02d.%04d",
                  (unsigned long)utcNow,
                  lt.tm_hour, lt.tm_min, lt.tm_sec,
                  lt.tm_mday, lt.tm_mon+1, lt.tm_year+1900);

            if (rtcManager.available)
                rtcManager.writeTime(utcNow);
        } else {
            DLOG("[Time] NTP failed, trying RTC...");
            if (rtcManager.available) {
                time_t rtcUtc = rtcManager.readTime();
                if (rtcUtc >= MIN_VALID_EPOCH) {
                    struct timeval tv = { rtcUtc, 0 };
                    settimeofday(&tv, nullptr);
                    synced         = true;
                    rtcUsed        = true;
                    _firstSyncDone = true;
                    lastSyncMs     = millis();
                    struct tm lt = getLocalTime();
                    DLOGF("[Time] RTC OK  UTC=%lu  Local=%02d:%02d:%02d",
                          (unsigned long)rtcUtc,
                          lt.tm_hour, lt.tm_min, lt.tm_sec);
                } else {
                    DLOGF("[Time] RTC invalid  epoch=%lu", (unsigned long)rtcUtc);
                }
            } else {
                DLOG("[Time] No RTC");
            }
        }
        if (!synced) DLOG("[Time] WARNING: No valid time source!");
    }

    void maintain() {
        if (!_firstSyncDone) { begin(); return; }
        if (millis() - lastSyncMs < NTP_SYNC_INTERVAL_MS) return;

        DLOG("[Time] Periodic NTP re-sync...");
        char tzStr[16];
        _buildTzString(tzStr, sizeof(tzStr));
        configTzTime(tzStr, NTP_SERVER1, NTP_SERVER2);

        bool ok = _waitForSync(5000);
        lastSyncMs = millis();
        if (ok) {
            synced  = true;
            rtcUsed = false;
            time_t utcNow = time(nullptr);
            if (rtcManager.available) rtcManager.writeTime(utcNow);
            DLOGF("[Time] Re-sync OK  UTC=%lu", (unsigned long)utcNow);
        } else {
            DLOG("[Time] Re-sync failed, keeping current time");
        }
    }

    // Web'den timezone degisince cagrilir
    void applyTimezone() {
        char tzStr[16];
        _buildTzString(tzStr, sizeof(tzStr));
        // Sadece TZ uygula, NTP'yi yeniden baslatma
        // (mevcut epoch dogru, sadece localtime donusumu degisecek)
        setenv("TZ", tzStr, 1);
        tzset();
        DLOGF("[Time] TZ updated: \"%s\" (UTC%+.2f)",
              tzStr, storage.tzOffsetSeconds() / 3600.0f);

        // Log: simdi saat ne gosteriyor
        struct tm lt = getLocalTime();
        DLOGF("[Time] After TZ update: %02d:%02d:%02d",
              lt.tm_hour, lt.tm_min, lt.tm_sec);
    }

    struct tm getLocalTime() {
        time_t now = time(nullptr);
        time_t localEpoch = now + storage.tzOffsetSeconds();
        struct tm t;
        memset(&t, 0, sizeof(t));
        gmtime_r(&localEpoch, &t);
        return t;
    }

    String timeString() {
        struct tm t = getLocalTime();
        char buf[9];
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
                 t.tm_hour, t.tm_min, t.tm_sec);
        return String(buf);
    }

    String dateString() {
        struct tm t = getLocalTime();
        char buf[11];
        snprintf(buf, sizeof(buf), "%02d.%02d.%04d",
                 t.tm_mday, t.tm_mon+1, t.tm_year+1900);
        return String(buf);
    }

    time_t epoch() { return time(nullptr); }

private:
    // POSIX TZ string olustur
    // POSIX kurali: UTC+3 -> "UTC-3" (isaret TERS)
    // Dakika ofseti: UTC+5:30 -> "UTC-5:30"
    void _buildTzString(char* buf, size_t sz) {
        long tzSec = storage.tzOffsetSeconds();
        int  h = (int)(tzSec / 3600L);
        int  m = (int)(abs(tzSec % 3600L) / 60);

        // POSIX isareti ters: UTC+3 -> "UTC-3"
        int posixH = -h;

        if (m == 0) {
            snprintf(buf, sz, "UTC%+d", posixH);
        } else {
            // Dakika her zaman pozitif, isareti saat verir
            snprintf(buf, sz, "UTC%+d:%02d", posixH, m);
        }
    }

    bool _waitForSync(uint32_t timeoutMs) {
        uint32_t start = millis();
        while (millis() - start < timeoutMs) {
            if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
                time_t now = time(nullptr);
                if (now >= MIN_VALID_EPOCH) {
                    DLOGF("[Time] SNTP sync in %lums", (unsigned long)(millis()-start));
                    return true;
                }
            }
            delay(200);
        }
        time_t now = time(nullptr);
        DLOGF("[Time] SNTP timeout  status=%d  epoch=%lu",
              (int)sntp_get_sync_status(), (unsigned long)now);
        return (now >= MIN_VALID_EPOCH);
    }
};

extern TimeManager timeManager;
