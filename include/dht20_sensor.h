#pragma once
#include <Arduino.h>
#include <Wire.h>

// ============================================================
//  dht20_sensor.h  –  DHT20 (I2C) temperature & humidity
//  Uses AHTxx-style protocol (same as AHT20)
// ============================================================

#define DHT20_ADDR 0x38

class DHT20Sensor {
public:
    float temperature = 0.0f;
    float humidity    = 0.0f;
    bool  valid       = false;

    void begin() {
        Wire.begin();
        delay(100);
        // Initialise
        Wire.beginTransmission(DHT20_ADDR);
        Wire.write(0xBE);
        Wire.write(0x08);
        Wire.write(0x00);
        Wire.endTransmission();
        delay(10);
    }

    bool read() {
        // Trigger measurement
        Wire.beginTransmission(DHT20_ADDR);
        Wire.write(0xAC);
        Wire.write(0x33);
        Wire.write(0x00);
        Wire.endTransmission();
        delay(80);

        // Read 6 bytes
        Wire.requestFrom((uint8_t)DHT20_ADDR, (uint8_t)6);
        if (Wire.available() < 6) {
            valid = false;
            return false;
        }
        uint8_t data[6];
        for (int i = 0; i < 6; i++) data[i] = Wire.read();

        if (data[0] & 0x80) {
            valid = false;  // device busy
            return false;
        }

        uint32_t raw_hum = ((uint32_t)data[1] << 12) |
                           ((uint32_t)data[2] << 4)  |
                           (data[3] >> 4);
        uint32_t raw_tmp = (((uint32_t)data[3] & 0x0F) << 16) |
                           ((uint32_t)data[4] << 8)            |
                           data[5];

        humidity    = (float)raw_hum / 1048576.0f * 100.0f;
        temperature = (float)raw_tmp / 1048576.0f * 200.0f - 50.0f;
        valid = true;
        return true;
    }
};

extern DHT20Sensor dht20;
