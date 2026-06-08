#pragma once
#include <Arduino.h>
#include <esp_sleep.h>
#include "config.h"

// ============================================================
//  sleep_manager.h  -  ESP32 derin uyku yonetimi
//
//  Calisma mantiği:
//    - Son pad dokunuşundan SLEEP_TIMEOUT_MS sonra uyku moduna girer
//    - Uyku modunda:
//        * TFT arka ışığı kapatılır (PIN_TFT_BL LOW)
//        * esp_deep_sleep_start() cagrilir
//    - Uyanma kaynagi: GPIO4 (PIN_TOUCH_INTERACT_GPIO) -> ext0 wakeup
//        * Kapasitif touch pini GPIO4 = T0
//        * Touch interrupt yerine basit GPIO pull-down + conductive pad
//          kullaniliyorsa EXT0 uyanma calışır
//        * Not: ESP32 deep sleep'te kapasitif touch wakeup (touch_pad_wakeup)
//          desteklenir; bunu da alternatif olarak etkinleştiriyoruz
//    - Uyanmada: ESP32 yeniden baslatilir, setup() cagirilir
//
//  Sleep timeout web arayuzu uzerinden ayarlanir ve EEPROM'da saklanir.
//  Varsayilan: 5 dakika (300 saniye)
// ============================================================

// SLEEP_TIMEOUT_DEFAULT_S config.h'de tanimli (300 saniye = 5 dakika)

class SleepManager {
public:
    uint32_t timeoutMs      = SLEEP_TIMEOUT_DEFAULT_S * 1000UL;
    unsigned long lastActivityMs = 0;
    bool     enabled        = true;

    void begin() {
        lastActivityMs = millis();
        // Backlight pinini cikis olarak ayarla
        pinMode(PIN_TFT_BL, OUTPUT);
        digitalWrite(PIN_TFT_BL, HIGH);  // Ekrani ac
    }

    // Her pad dokunuşunda cagir - zamanlayicıyı sifirla
    void resetTimer() {
        lastActivityMs = millis();
        wakeDisplay();
    }

    // loop() icinde her iterasyonda cagir
    void update() {
        if (!enabled || timeoutMs == 0) return;
        if (millis() - lastActivityMs >= timeoutMs) {
            goToSleep();
        }
    }

    // Ekrani ac (uyku sonrasi veya herhangi bir etkinlikte)
    void wakeDisplay() {
        digitalWrite(PIN_TFT_BL, HIGH);
    }

    // Ekrani kapat ve derin uykuya gec
    void goToSleep() {
        DLOG("[Sleep] Entering deep sleep...");
        Serial.flush();

        // Ekrani kapat
        digitalWrite(PIN_TFT_BL, LOW);
        delay(50);

        // Uyanma kaynagi: Touch pad T0 (GPIO4)
        // ESP32 touch wakeup: dokunuş değeri eşiğin ALTINA düşünce uyanır
        esp_sleep_enable_touchpad_wakeup();
        touchSleepWakeUpEnable(PIN_TOUCH_INTERACT_GPIO, TOUCH_SLEEP_THRESHOLD);

        // Yedek olarak EXT0 da etkinleştir (pad direkt GPIO'ya bağlıysa)
        // esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_TOUCH_INTERACT_GPIO, 0);

        esp_deep_sleep_start();
        // Buraya hicbir zaman ulasilmaz; ESP32 reset olur
    }

    // Kalan sure (ms) - 0 donerse zaman asimi
    uint32_t remainingMs() {
        if (!enabled || timeoutMs == 0) return UINT32_MAX;
        unsigned long elapsed = millis() - lastActivityMs;
        if (elapsed >= timeoutMs) return 0;
        return timeoutMs - elapsed;
    }

    // Web arayuzunden timeout ayarla (saniye cinsinden)
    void setTimeoutSeconds(uint32_t seconds) {
        timeoutMs = seconds * 1000UL;
        lastActivityMs = millis();
    }
};

extern SleepManager sleepMgr;
