#pragma once
#include <Arduino.h>
#include "config.h"

// ============================================================
//  dht_sensor.h  -  DHT11 / DHT22 sicaklik & nem sensoru
//
//  One-wire protokol: tek bir veri pini (DATA) uzerinden iletisim.
//  I2C KULLANILMAZ.
//
//  Baglantilar:
//    DHT VCC  -> 3.3V (ya da 5V, her iki tip de calısır)
//    DHT GND  -> GND
//    DHT DATA -> GPIO26 (PIN_DHT_DATA, config.h'den degistirilebilir)
//    DATA ile VCC arasına 10kΩ pull-up direnç ekleyin
//
//  DHT11 vs DHT22 farki:
//    - DHT11: daha ucuz, daha az hassas, daha yavas (1 sn aralik)
//    - DHT22: daha hassas (0.1 derece), 2 sn aralik
//    - Her ikisi de ayni protokolü kullanir, sadece veri cozumlemesi
//      ve okuma araligi farklidir
//    - Sensor tipini config.h'de DHT_TYPE ile tanimlayin
//
//  Protokol ozeti:
//    1. Host start signal: DATA LOW > 18ms (DHT11) / 1ms (DHT22)
//    2. Host release: DATA HIGH 20-40us
//    3. Sensor response: LOW 80us -> HIGH 80us
//    4. 40 bit veri: 8b nem_int + 8b nem_dec + 8b temp_int + 8b temp_dec + 8b checksum
//    5. Bit 0: LOW 50us + HIGH 26-28us
//       Bit 1: LOW 50us + HIGH 70us
// ============================================================

#define DHT_TYPE_DHT11  11
#define DHT_TYPE_DHT22  22

class DHTSensor {
public:
    float   temperature = 0.0f;
    float   humidity    = 0.0f;
    bool    valid       = false;
    uint8_t type;          // DHT11 veya DHT22

    DHTSensor(uint8_t dhtType = DHT_TYPE_DHT22) : type(dhtType) {}

    void begin() {
        pinMode(PIN_DHT_DATA, INPUT_PULLUP);
        // Ilk okuma icin sensor'un stabilize olmasi lazim
        delay(type == DHT_TYPE_DHT11 ? 1000 : 2000);
    }

    bool read() {
        uint8_t data[5] = {0};

        // ── 1. Start signal: DATA pini LOW ────────────────────
        pinMode(PIN_DHT_DATA, OUTPUT);
        digitalWrite(PIN_DHT_DATA, LOW);
        delay(type == DHT_TYPE_DHT11 ? 20 : 2);  // DHT11: >=18ms, DHT22: >=1ms
        digitalWrite(PIN_DHT_DATA, HIGH);
        delayMicroseconds(30);  // 20-40us bekleme
        pinMode(PIN_DHT_DATA, INPUT_PULLUP);

        // ── 2. Sensor response bekleme ────────────────────────
        // LOW 80us bekliyoruz
        if (!waitFor(LOW, 100)) { valid = false; return false; }
        // HIGH 80us bekliyoruz
        if (!waitFor(HIGH, 100)) { valid = false; return false; }
        // LOW ile veri baslar
        if (!waitFor(LOW, 100)) { valid = false; return false; }

        // ── 3. 40 bit veri oku ───────────────────────────────
        for (int i = 0; i < 40; i++) {
            // Her bit: once 50us LOW
            if (!waitFor(HIGH, 80)) { valid = false; return false; }
            // HIGH suresi: 26-28us = bit 0, ~70us = bit 1
            delayMicroseconds(35);
            if (digitalRead(PIN_DHT_DATA) == HIGH) {
                data[i / 8] |= (1 << (7 - (i % 8)));
                if (!waitFor(LOW, 100)) { valid = false; return false; }
            }
        }

        // ── 4. Checksum kontrol ──────────────────────────────
        uint8_t chk = data[0] + data[1] + data[2] + data[3];
        if (chk != data[4]) {
            Serial.printf("[DHT] Checksum error: calc=%02X got=%02X\n", chk, data[4]);
            valid = false;
            return false;
        }

        // ── 5. Veriyi coz ─────────────────────────────────────
        if (type == DHT_TYPE_DHT11) {
            humidity    = data[0];
            temperature = data[2];
            // DHT11 negatif sicaklik: data[3] bit7 set ise negatif
            if (data[3] & 0x80) temperature = -temperature;
        } else {
            // DHT22: 16-bit signed, 0.1 derece cozunurluk
            humidity    = ((data[0] << 8) | data[1]) * 0.1f;
            int16_t rawTemp = ((data[2] & 0x7F) << 8) | data[3];
            temperature = rawTemp * 0.1f;
            if (data[2] & 0x80) temperature = -temperature;
        }

        valid = true;
        return true;
    }

private:
    // Beklenen seviyeye gelinceye kadar bekle, timeoutUs mikrosan
    bool waitFor(uint8_t level, uint32_t timeoutUs) {
        uint32_t t = micros();
        while (digitalRead(PIN_DHT_DATA) != level) {
            if (micros() - t > timeoutUs) return false;
        }
        return true;
    }
};

// Global singleton - tip config.h'deki DHT_TYPE ile belirlenir
extern DHTSensor dhtSensor;
