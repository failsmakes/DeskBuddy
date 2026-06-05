#pragma once
#include <Arduino.h>
#include <EEPROM.h>
#include "config.h"

// ============================================================
//  storage.h  –  EEPROM layout & helpers
// ============================================================
//
//  Layout (total ≤ 512 bytes)
//  ───────────────────────────────────────────────────────────
//  Addr   Size  Field
//     0      1  magic byte (0xDB)
//     1      1  wifi_count  (0-5)
//     2    480  wifi records: 5 × (32 SSID + 64 PASS) = 480
//   482     33  city_primary   (null-terminated, max 32 chars)
//   515     33  city_secondary (null-terminated, max 32 chars)
//   548     33  owm_api_key    (null-terminated, max 32 chars)
//   581      1  timezone_offset_sign  (0=+, 1=-)
//   582      1  timezone_hours
//   583      1  timezone_minutes
//  ───────────────────────────────────────────────────────────
//  Total: 584  (<= 512? No — EEPROM_SIZE may need bumping)
//  → set EEPROM_SIZE 1024 in config.h if compile fails
// ============================================================

struct WiFiCredential {
    char ssid[WIFI_SSID_LEN];
    char pass[WIFI_PASS_LEN];
};

struct AppConfig {
    uint8_t          magic;
    uint8_t          wifiCount;
    WiFiCredential   wifiList[MAX_WIFI_NETWORKS];
    char             cityPrimary[33];
    char             citySecondary[33];
    char             owmApiKey[33];
    int8_t           tzOffsetHours;    // signed: -12 .. +14
    uint8_t          tzOffsetMinutes;  // 0 or 30 or 45
    uint32_t         sleepTimeoutS;    // uyku zaman asimi (saniye), 0=devre disi
};

class Storage {
public:
    AppConfig cfg;

    void begin() {
        EEPROM.begin(sizeof(AppConfig) + 16);
        load();
    }

    void load() {
        EEPROM.get(0, cfg);
        if (cfg.magic != EEPROM_MAGIC) {
            // First boot — write defaults
            memset(&cfg, 0, sizeof(cfg));
            cfg.magic = EEPROM_MAGIC;
            cfg.wifiCount = 0;
            strncpy(cfg.cityPrimary,   "Istanbul,TR", 32);
            strncpy(cfg.citySecondary, "London,GB",   32);
            strncpy(cfg.owmApiKey,     OWM_API_KEY,   32);
            cfg.tzOffsetHours   = 3;
            cfg.tzOffsetMinutes = 0;
            cfg.sleepTimeoutS   = SLEEP_TIMEOUT_DEFAULT_S;
            save();
        }
    }

    void save() {
        cfg.magic = EEPROM_MAGIC;
        EEPROM.put(0, cfg);
        EEPROM.commit();
    }

    // --- WiFi helpers ---
    bool addWifi(const char* ssid, const char* pass) {
        if (cfg.wifiCount >= MAX_WIFI_NETWORKS) return false;
        strncpy(cfg.wifiList[cfg.wifiCount].ssid, ssid, WIFI_SSID_LEN - 1);
        strncpy(cfg.wifiList[cfg.wifiCount].pass, pass, WIFI_PASS_LEN - 1);
        cfg.wifiCount++;
        save();
        return true;
    }

    bool updateWifi(uint8_t idx, const char* ssid, const char* pass) {
        if (idx >= cfg.wifiCount) return false;
        strncpy(cfg.wifiList[idx].ssid, ssid, WIFI_SSID_LEN - 1);
        strncpy(cfg.wifiList[idx].pass, pass, WIFI_PASS_LEN - 1);
        save();
        return true;
    }

    bool deleteWifi(uint8_t idx) {
        if (idx >= cfg.wifiCount) return false;
        for (uint8_t i = idx; i < cfg.wifiCount - 1; i++) {
            cfg.wifiList[i] = cfg.wifiList[i + 1];
        }
        memset(&cfg.wifiList[cfg.wifiCount - 1], 0, sizeof(WiFiCredential));
        cfg.wifiCount--;
        save();
        return true;
    }

    // Ikincil konum tanimli mi?
    bool hasSecondaryCity() const {
        return cfg.citySecondary[0] != '\0';
    }

    long tzOffsetSeconds() const {
        return (long)cfg.tzOffsetHours * 3600L
             + (cfg.tzOffsetHours >= 0 ? 1 : -1) * (long)cfg.tzOffsetMinutes * 60L;
    }
};

extern Storage storage;
