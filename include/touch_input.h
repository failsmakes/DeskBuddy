#pragma once
#include <Arduino.h>
#include "config.h"

// ============================================================
//  touch_input.h  -  Capacitive touch pad handling
//  Detects: SHORT_TAP, LONG_PRESS for each pad
// ============================================================

enum TouchEvent { TOUCH_NONE, TOUCH_SHORT, TOUCH_LONG };

class TouchPad {
public:
    uint8_t       pin;
    const char*   name;        // debug icin pad adi
    bool          _wasDown     = false;
    unsigned long _downTime    = 0;
    bool          _longFired   = false;
    uint32_t      _lastRawLog  = 0;  // son raw deger log zamani

    TouchPad(uint8_t p, const char* n = "?") : pin(p), name(n) {}

    // Ham touch degerini doner (debug icin)
    uint16_t rawValue() {
        return touchRead(pin);
    }

    // Her loop iterasyonunda cagir. Olay olursa doner.
    TouchEvent update() {
        uint16_t raw = touchRead(pin);
        bool     down = (raw < TOUCH_THRESHOLD);

        // DEBUG: Ham degeri periyodik logla (500ms'de bir)
        #if DEBUG_SERIAL
        uint32_t now = millis();
        if (now - _lastRawLog >= 500) {
            _lastRawLog = now;
            Serial.printf("[Touch] %s pin=T%d raw=%-4d thresh=%d  %s\n",
                name, pin, raw, TOUCH_THRESHOLD,
                down ? "<<DOWN>>" : "");
        }
        #endif

        if (down && !_wasDown) {
            _wasDown   = true;
            _downTime  = millis();
            _longFired = false;
            DLOGF("[Touch] %s PRESS START  raw=%d", name, raw);
        }
        else if (down && _wasDown && !_longFired) {
            if (millis() - _downTime >= TOUCH_LONG_PRESS_MS) {
                _longFired = true;
                DLOGF("[Touch] %s LONG_PRESS  held=%lums",
                      name, (unsigned long)(millis() - _downTime));
                return TOUCH_LONG;
            }
        }
        else if (!down && _wasDown) {
            _wasDown = false;
            if (!_longFired) {
                unsigned long held = millis() - _downTime;
                if (held >= TOUCH_DEBOUNCE_MS) {
                    DLOGF("[Touch] %s SHORT_TAP   held=%lums", name, held);
                    return TOUCH_SHORT;
                } else {
                    DLOGF("[Touch] %s BOUNCE IGNORED  held=%lums  (min=%d)",
                          name, held, TOUCH_DEBOUNCE_MS);
                }
            }
        }
        return TOUCH_NONE;
    }

    bool isHeld() { return _wasDown; }
};

// ============================================================
//  TouchInput - 4 pad birlesik yonetici
// ============================================================
class TouchInput {
public:
    TouchPad interact { PIN_TOUCH_INTERACT, "INTERACT(T0)" };
    TouchPad plus     { PIN_TOUCH_PLUS,     "PLUS(T3)"     };
    TouchPad minus_   { PIN_TOUCH_MINUS,    "MINUS(T6)"    };
    TouchPad ok       { PIN_TOUCH_OK,       "OK(T7)"       };

    struct Events {
        TouchEvent interact;
        TouchEvent plus;
        TouchEvent minus_;
        TouchEvent ok;
    };

    // Raw degerleri tek satirda logla (2 saniyede bir)
    void logRawValues() {
        #if DEBUG_SERIAL
        static uint32_t _last = 0;
        if (millis() - _last < 2000) return;
        _last = millis();
        Serial.printf("[Touch RAW] INTERACT=%3d  PLUS=%3d  MINUS=%3d  OK=%3d  (thresh=%d)\n",
            touchRead(PIN_TOUCH_INTERACT),
            touchRead(PIN_TOUCH_PLUS),
            touchRead(PIN_TOUCH_MINUS),
            touchRead(PIN_TOUCH_OK),
            TOUCH_THRESHOLD);
        #endif
    }

    Events poll() {
        logRawValues();
        return {
            interact.update(),
            plus.update(),
            minus_.update(),
            ok.update()
        };
    }
};

extern TouchInput touch;
