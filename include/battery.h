#pragma once
#include <Arduino.h>

// ============================================================
//  battery.h  -  Lolin D32 batarya voltaj okuyucu
//
//  Lolin D32 kartinda dahili voltaj bolucusu (100k+100k) vardir.
//  Batarya + ucu --> 100k --> GPIO35 --> 100k --> GND
//  Yani ADC okunan deger gercek voltajin yarisina esittir.
//
//  Li-Ion pillar icin:
//    4.20V = %100 (tam dolu)
//    4.00V = %75
//    3.80V = %50
//    3.60V = %25
//    3.30V = %5   (kritik)
//    3.00V = %0   (bos)
//
//  ADC karakterizasyonu: ESP32 ADC lineer degil, bu yuzden
//  coklu olcum ortalamayla yumusatiyoruz.
// ============================================================

#define BAT_PIN          35
#define BAT_SAMPLES      16       // ortalama icin ornek sayisi
#define BAT_DIVIDER      2.0f     // voltaj bolucusu orani
#define BAT_VREF         3.3f     // ESP32 ADC referans voltaji
#define BAT_ADC_MAX      4095.0f  // 12-bit ADC
#define BAT_FULL_V       4.20f    // %100 voltaj
#define BAT_EMPTY_V      3.00f    // %0 voltaj
#define BAT_CRITICAL_V   3.30f    // kritik uyari esigi
#define BAT_UPDATE_MS    30000UL  // 30 saniyede bir guncelle

// ESP32 ADC dogrusal olmadigi icin lookup table ile duzeltiyoruz
// (ADC ham degeri -> gercek voltaj carpani, ampirik degerler)
static const float BAT_ADC_CORRECTION = 1.045f; // tipik ESP32 ADC hatasi telafisi

class Battery {
public:
    float   voltage    = 0.0f;   // olculen voltaj (V)
    uint8_t percent    = 0;      // doluluk yuzdesi (0-100)
    bool    charging   = false;  // sarj durumu (GPIO ile takip edilmiyorsa false)
    bool    critical   = false;  // kritik seviye
    bool    valid      = false;

    unsigned long lastReadMs = 0;

    void begin() {
        analogReadResolution(12);
        analogSetAttenuation(ADC_11db); // 0-3.9V arasi olcum icin
        read(); // ilk okuma
    }

    // Guncelleme gerekiyor mu?
    bool needsUpdate() {
        return !valid || (millis() - lastReadMs > BAT_UPDATE_MS);
    }

    void update() {
        if (needsUpdate()) read();
    }

    void read() {
        // Coklu ornek al, ortalamasini hesapla
        uint32_t sum = 0;
        for (int i = 0; i < BAT_SAMPLES; i++) {
            sum += analogRead(BAT_PIN);
            delay(2);
        }
        float raw = (float)(sum / BAT_SAMPLES);

        // ADC -> voltaj donusumu
        // GPIO35 bolucuden gelen: Vbat/2
        // ADC: raw / 4095 * Vref
        // Gercek voltaj: (raw / 4095 * Vref) * 2 * duzeltme_katsayisi
        voltage = (raw / BAT_ADC_MAX) * BAT_VREF * BAT_DIVIDER * BAT_ADC_CORRECTION;

        // Voltaji yuzdege donustur (lineer interpolasyon)
        if (voltage >= BAT_FULL_V) {
            percent = 100;
        } else if (voltage <= BAT_EMPTY_V) {
            percent = 0;
        } else {
            percent = (uint8_t)(((voltage - BAT_EMPTY_V) / (BAT_FULL_V - BAT_EMPTY_V)) * 100.0f);
        }

        critical = (voltage < BAT_CRITICAL_V);
        valid    = true;
        lastReadMs = millis();
    }

    // Sarj durumu ikonu icin seviye kodu doner: 0-4
    // 0=bos, 1=ceyrek, 2=yari, 3=uc ceyrek, 4=dolu
    uint8_t iconLevel() const {
        if (percent >= 88) return 4;
        if (percent >= 63) return 3;
        if (percent >= 38) return 2;
        if (percent >= 13) return 1;
        return 0;
    }
};

extern Battery battery;
