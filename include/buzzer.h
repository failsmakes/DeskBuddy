#pragma once
#include <Arduino.h>
#include "config.h"

// ============================================================
//  buzzer.h  –  Non-blocking buzzer driver
// ============================================================

class Buzzer {
public:
    bool    active     = false;
    bool    _beeping   = false;
    unsigned long _last = 0;

    void begin() {
        pinMode(PIN_BUZZER, OUTPUT);
        digitalWrite(PIN_BUZZER, LOW);
    }

    // Start repeating alert pattern
    void startAlert() { active = true; }

    // Stop all sound
    void stop() {
        active   = false;
        _beeping = false;
        noTone(PIN_BUZZER);
        digitalWrite(PIN_BUZZER, LOW);
    }

    // Single short beep (blocking, for UI feedback)
    void beep(uint16_t ms = 80) {
        tone(PIN_BUZZER, BUZZER_FREQ_HZ, ms);
    }

    // Call every loop() iteration
    void update() {
        if (!active) return;
        unsigned long now = millis();
        if (_beeping) {
            if (now - _last >= BUZZER_ON_MS) {
                noTone(PIN_BUZZER);
                digitalWrite(PIN_BUZZER, LOW);
                _beeping = false;
                _last    = now;
            }
        } else {
            if (now - _last >= BUZZER_OFF_MS) {
                tone(PIN_BUZZER, BUZZER_FREQ_HZ);
                _beeping = true;
                _last    = now;
            }
        }
    }
};

extern Buzzer buzzer;
