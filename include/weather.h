#pragma once
#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "storage.h"
#include "config.h"

// ============================================================
//  weather.h  –  OpenWeatherMap current + 5-day forecast
// ============================================================

struct WeatherDay {
    char   desc[24];     // e.g. "clear sky"
    char   icon[4];      // OWM icon code: "01d"
    float  tempMin;
    float  tempMax;
    float  tempCurrent;
    int    humidity;
    uint8_t conditionId; // OWM condition group (2xx=storm,5xx=rain,800=clear…)
};

class Weather {
public:
    WeatherDay current;
    WeatherDay forecast[5];
    bool       valid     = false;
    unsigned long lastFetchMs = 0;

    // Fetch current weather + 5-day for given city string ("Istanbul,TR")
    bool fetch(const char* city, bool isPrimary = true) {
        if (!storage.cfg.owmApiKey[0]) return false;

        HTTPClient http;
        char url[256];

        // Current weather
        snprintf(url, sizeof(url),
            "%s/data/2.5/weather?q=%s&appid=%s&units=metric&lang=en",
            OWM_BASE_URL, urlEncode(city).c_str(), storage.cfg.owmApiKey);

        http.begin(url);
        int code = http.GET();
        if (code == 200) {
            String body = http.getString();
            parseCurrent(body, current);
        }
        http.end();

        // 5-day / 3-hour forecast
        snprintf(url, sizeof(url),
            "%s/data/2.5/forecast?q=%s&appid=%s&units=metric&cnt=40",
            OWM_BASE_URL, urlEncode(city).c_str(), storage.cfg.owmApiKey);

        http.begin(url);
        code = http.GET();
        if (code == 200) {
            String body = http.getString();
            parseForecast(body);
        }
        http.end();

        valid = true;
        lastFetchMs = millis();
        return true;
    }

    bool needsUpdate() {
        return !valid || (millis() - lastFetchMs > WEATHER_UPDATE_MS);
    }

private:
    void parseCurrent(const String& json, WeatherDay& w) {
        JsonDocument doc;
        if (deserializeJson(doc, json)) return;
        w.tempCurrent = doc["main"]["temp"] | 0.0f;
        w.tempMin     = doc["main"]["temp_min"] | 0.0f;
        w.tempMax     = doc["main"]["temp_max"] | 0.0f;
        w.humidity    = doc["main"]["humidity"] | 0;
        w.conditionId = (uint8_t)((int)(doc["weather"][0]["id"] | 800) / 100);
        const char* d = doc["weather"][0]["description"] | "N/A";
        strncpy(w.desc, d, sizeof(w.desc) - 1);
        const char* ic = doc["weather"][0]["icon"] | "01d";
        strncpy(w.icon, ic, sizeof(w.icon) - 1);
    }

    void parseForecast(const String& json) {
        JsonDocument doc;
        if (deserializeJson(doc, json)) return;
        JsonArray list = doc["list"].as<JsonArray>();

        // Pick one entry per day (noon-ish, every 8 slots = 24h)
        int day = 0;
        int slot = 0;
        for (JsonObject item : list) {
            if (day >= 5) break;
            if (slot % 8 == 4) {  // ~noon slot
                forecast[day].tempMin     = item["main"]["temp_min"] | 0.0f;
                forecast[day].tempMax     = item["main"]["temp_max"] | 0.0f;
                forecast[day].tempCurrent = item["main"]["temp"]     | 0.0f;
                forecast[day].humidity    = item["main"]["humidity"] | 0;
                forecast[day].conditionId = (uint8_t)((int)(item["weather"][0]["id"] | 800) / 100);
                const char* d  = item["weather"][0]["description"] | "N/A";
                const char* ic = item["weather"][0]["icon"] | "01d";
                strncpy(forecast[day].desc, d,  sizeof(forecast[day].desc) - 1);
                strncpy(forecast[day].icon, ic, sizeof(forecast[day].icon) - 1);
                day++;
            }
            slot++;
        }
    }

    String urlEncode(const char* src) {
        String out;
        while (*src) {
            if (isAlphaNumeric(*src) || *src == '-' || *src == '_' || *src == '.' || *src == ',') {
                out += *src;
            } else {
                char buf[4];
                snprintf(buf, sizeof(buf), "%%%02X", (uint8_t)*src);
                out += buf;
            }
            src++;
        }
        return out;
    }
};

extern Weather weatherPrimary;
extern Weather weatherSecondary;
