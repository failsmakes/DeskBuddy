#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <stdlib.h>
#include <time.h>
#include "config.h"

// ============================================================
//  rtc_manager.h  -  DS3231 RTC (ZS-042 modulu) surucusu
//
//  ZS-042 modulu uzerinde DS3231 RTC + AT24C32 EEPROM chipi bulunur.
//  I2C baglantisi:
//    SDA -> GPIO21  (EEPROM icin de ayni hat)
//    SCL -> GPIO22
//    VCC -> 3.3V
//    GND -> GND
//
//  DS3231 I2C adresi: 0x68
//  AT24C32 I2C adresi: 0x57 (A0-A2 = GND durumunda)
//
//  Calisma mantiği:
//    - Boot'ta NTP sync basarili olursa RTC guncellenir
//    - NTP sync basarisiz olursa (wifi yok) RTC'den saat okunur
//    - Her NTP senkronizasyonunda RTC de guncellenir
// ============================================================

#define DS3231_ADDR     0x68
#define DS3231_REG_TIME 0x00
#define DS3231_REG_TEMP 0x11

// BCD <-> decimal donusum yardimcilari
static inline uint8_t bcd2dec(uint8_t b) { return (b >> 4) * 10 + (b & 0x0F); }
static inline uint8_t dec2bcd(uint8_t d) { return ((d / 10) << 4) | (d % 10); }

static time_t utc_mktime(struct tm* t) {
    char tzBuf[64] = {0};
    char* oldTz = getenv("TZ");
    if (oldTz) strncpy(tzBuf, oldTz, sizeof(tzBuf) - 1);
    setenv("TZ", "UTC0", 1);
    tzset();
    time_t epoch = mktime(t);
    if (oldTz) setenv("TZ", tzBuf, 1);
    else unsetenv("TZ");
    tzset();
    return epoch;
}

class RTCManager {
public:
    bool available = false;   // DS3231 bulundu mu?
    bool valid     = false;   // gecerli saat verisi var mi?
    float tempC    = 0.0f;    // DS3231 dahili sicaklik sensoru

    void begin() {
        Wire.begin(PIN_RTC_SDA, PIN_RTC_SCL);
        // DS3231 varligi kontrol et
        Wire.beginTransmission(DS3231_ADDR);
        available = (Wire.endTransmission() == 0);
        if (available) {
            DLOG("[RTC] DS3231 found on I2C bus");
            // Oscillator stop flag temizle (ilk acilista gerekebilir)
            clearOscillatorStopFlag();
        } else {
            DLOG("[RTC] DS3231 not found - RTC backup disabled");
        }
    }

    // RTC'den time_t yap. RTC'ye UTC olarak yaziyoruz, bu nedenle okurken de UTC cinsinden epoch hesaplamaliyiz.
    time_t readTime() {
        if (!available) return 0;

        Wire.beginTransmission(DS3231_ADDR);
        Wire.write(DS3231_REG_TIME);
        if (Wire.endTransmission() != 0) return 0;

        Wire.requestFrom((uint8_t)DS3231_ADDR, (uint8_t)7);
        if (Wire.available() < 7) return 0;

        uint8_t sec  = bcd2dec(Wire.read() & 0x7F);
        uint8_t min_ = bcd2dec(Wire.read() & 0x7F);
        uint8_t hour = bcd2dec(Wire.read() & 0x3F);
        Wire.read();  // day-of-week, kullanmiyoruz
        uint8_t day  = bcd2dec(Wire.read() & 0x3F);
        uint8_t mon  = bcd2dec(Wire.read() & 0x1F);
        uint8_t yr   = bcd2dec(Wire.read());

        struct tm t;
        memset(&t, 0, sizeof(t));
        t.tm_sec   = sec;
        t.tm_min   = min_;
        t.tm_hour  = hour;
        t.tm_mday  = day;
        t.tm_mon   = mon - 1;         // tm_mon: 0-11
        t.tm_year  = yr + 100;        // 2000+yr - 1900
        t.tm_isdst = 0;

        // DS3231 time is stored as UTC when we write it, so convert with UTC semantics.
        time_t epoch = utc_mktime(&t);
        valid = (epoch > 0);
        return epoch;
    }

    // Sistem saatini (Unix epoch, UTC) RTC'ye yaz
    bool writeTime(time_t utcEpoch) {
        if (!available) return false;

        struct tm t;
        gmtime_r(&utcEpoch, &t);   // UTC olarak yaz; okurken tz ofseti uygulanir

        Wire.beginTransmission(DS3231_ADDR);
        Wire.write(DS3231_REG_TIME);
        Wire.write(dec2bcd(t.tm_sec));
        Wire.write(dec2bcd(t.tm_min));
        Wire.write(dec2bcd(t.tm_hour));
        Wire.write(dec2bcd(t.tm_wday + 1));   // 1-7
        Wire.write(dec2bcd(t.tm_mday));
        Wire.write(dec2bcd(t.tm_mon + 1));
        Wire.write(dec2bcd(t.tm_year - 100)); // 0-99
        bool ok = (Wire.endTransmission() == 0);
        if (ok) {
            valid = true;
            DLOG("[RTC] Time written to DS3231");
        }
        return ok;
    }

    // DS3231 dahili sicaklik okuma (yaklasik +/-3 derece hassasiyet)
    float readTemperature() {
        if (!available) return 0.0f;
        Wire.beginTransmission(DS3231_ADDR);
        Wire.write(DS3231_REG_TEMP);
        if (Wire.endTransmission() != 0) return 0.0f;
        Wire.requestFrom((uint8_t)DS3231_ADDR, (uint8_t)2);
        if (Wire.available() < 2) return 0.0f;
        int8_t  msb = (int8_t)Wire.read();
        uint8_t lsb = Wire.read();
        tempC = msb + ((lsb >> 6) * 0.25f);
        return tempC;
    }

private:
    void clearOscillatorStopFlag() {
        // Status register (0x0F), bit7 = OSF
        Wire.beginTransmission(DS3231_ADDR);
        Wire.write(0x0F);
        Wire.endTransmission();
        Wire.requestFrom((uint8_t)DS3231_ADDR, (uint8_t)1);
        if (!Wire.available()) return;
        uint8_t status = Wire.read();
        if (status & 0x80) {
            // OSF set - saat kaybolmus, ama yine de temizleyelim
            Wire.beginTransmission(DS3231_ADDR);
            Wire.write(0x0F);
            Wire.write(status & ~0x80);
            Wire.endTransmission();
            DLOG("[RTC] Warning: oscillator stop flag was set (power loss?)");
        }
    }
};

extern RTCManager rtcManager;
