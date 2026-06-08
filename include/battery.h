#pragma once
#include <Arduino.h>
#include <driver/adc.h>          // ESP32 IDF ADC driver - en güvenilir yöntem
#include <esp_adc_cal.h>         // ADC kalibrasyon

// ============================================================
//  battery.h  -  Lolin D32 batarya voltaj okuyucu
//
//  Lolin D32: GPIO35 = ADC1_CHANNEL_7
//  Dahili 100k+100k voltaj bölücüsü: Vbat/2 -> GPIO35
//
//  ESP32 Arduino'da analogRead() güvenilmez olabiliyor.
//  ESP-IDF ADC driver (adc1_get_raw) + esp_adc_cal ile
//  kalibre edilmiş okuma kullanıyoruz.
//
//  NOT: analogSetAttenuation() TÜM ADC kanallarını etkiler,
//  touch okumalarını bozar. Bu yüzden kullanmıyoruz.
// ============================================================

#define BAT_ADC_CHANNEL   ADC1_CHANNEL_7   // GPIO35
#define BAT_ADC_ATTEN     ADC_ATTEN_DB_11  // 0-3.9V
#define BAT_ADC_WIDTH     ADC_WIDTH_BIT_12 // 12-bit
#define BAT_SAMPLES       16
#define BAT_DIVIDER       2.0f
#define BAT_FULL_V        4.20f
#define BAT_EMPTY_V       3.00f
#define BAT_CRITICAL_V    3.30f
#define BAT_UPDATE_MS     30000UL
// Lolin D32 şarj edilirken USB'den ölçülen tipik değer (~4.2V bölücü çıkışı)
#define BAT_USB_MIN_V     4.10f    // bu değerin üstü = USB şarjda

class Battery {
public:
    float   voltage    = 0.0f;
    uint8_t percent    = 0;
    bool    critical   = false;
    bool    charging   = false;   // USB'den şarj oluyor
    bool    valid      = false;
    bool    hasBattery = false;   // pil takılı mı
    unsigned long lastReadMs = 0;

    void begin() {
        // IDF seviyesinde ADC1 yapılandır - sadece bu kanalı etkiler
        adc1_config_width(BAT_ADC_WIDTH);
        adc1_config_channel_atten(BAT_ADC_CHANNEL, BAT_ADC_ATTEN);

        // Kalibrasyon karakterizasyonu
        esp_adc_cal_characterize(ADC_UNIT_1, BAT_ADC_ATTEN,
                                  BAT_ADC_WIDTH, 1100, &_chars);
        delay(20);
        read();
    }

    bool needsUpdate() {
        return !valid || (millis() - lastReadMs > BAT_UPDATE_MS);
    }

    void update() {
        if (needsUpdate()) read();
    }

    void read() {
        // Warm-up: ilk 3 okumayı at
        for (int i = 0; i < 3; i++) adc1_get_raw(BAT_ADC_CHANNEL);

        // BAT_SAMPLES adet okuyup ortalama al
        uint32_t sum = 0;
        for (int i = 0; i < BAT_SAMPLES; i++) {
            sum += adc1_get_raw(BAT_ADC_CHANNEL);
            delay(2);
        }
        uint32_t raw = sum / BAT_SAMPLES;

        // IDF kalibrasyon ile mV'ye çevir
        uint32_t mv = esp_adc_cal_raw_to_voltage(raw, &_chars);

        // Bölücü: GPIO35'te Vbat/2 var, gerçek voltajı hesapla
        voltage = (mv / 1000.0f) * BAT_DIVIDER;

        DLOGF("[Battery] raw=%lu  mv=%lu  voltage=%.3fV", raw, mv, voltage);

        // Pil takılı mı? (0.5V altı = pin float = pil yok)
        if (voltage < 0.5f) {
            hasBattery = false;
            charging   = false;
            critical   = false;
            percent    = 0;
            valid      = true;
            lastReadMs = millis();
            DLOG("[Battery] No battery detected (USB only)");
            return;
        }

        hasBattery = true;

        // USB şarjda mı? (voltaj BAT_FULL_V'ye yakın veya üstünde)
        charging = (voltage >= BAT_USB_MIN_V);

        if (voltage >= BAT_FULL_V) {
            percent = 100;
        } else if (voltage <= BAT_EMPTY_V) {
            percent = 0;
        } else {
            percent = (uint8_t)(((voltage - BAT_EMPTY_V) /
                                  (BAT_FULL_V - BAT_EMPTY_V)) * 100.0f);
        }

        critical   = (!charging && voltage < BAT_CRITICAL_V);
        valid      = true;
        lastReadMs = millis();

        DLOGF("[Battery] voltage=%.3fV  percent=%d%%  charging=%d  critical=%d",
              voltage, percent, charging, critical);
    }

    uint8_t iconLevel() const {
        if (percent >= 88) return 4;
        if (percent >= 63) return 3;
        if (percent >= 38) return 2;
        if (percent >= 13) return 1;
        return 0;
    }

private:
    esp_adc_cal_characteristics_t _chars;
};

extern Battery battery;
