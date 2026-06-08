#pragma once
#include <Arduino.h>
#include "config.h"

enum TouchEvent { TOUCH_NONE, TOUCH_SHORT, TOUCH_LONG };

class TouchPad {
public:
    uint8_t       pin;
    const char*   name;
    bool          _wasDown   = false;
    unsigned long _downTime  = 0;
    bool          _longFired = false;

    TouchPad(uint8_t p, const char* n = "?") : pin(p), name(n) {}

    uint16_t rawValue() { return touchRead(pin); }

    TouchEvent update() {
        uint16_t raw = touchRead(pin);
        bool     down = (raw < TOUCH_THRESHOLD);

        if (down && !_wasDown) {
            _wasDown   = true;
            _downTime  = millis();
            _longFired = false;
            DLOGF("[Touch] %s DOWN  raw=%d", name, raw);
        }
        else if (down && _wasDown && !_longFired) {
            if (millis() - _downTime >= TOUCH_LONG_PRESS_MS) {
                _longFired = true;
                DLOGF("[Touch] %s LONG  held=%lums", name,
                      (unsigned long)(millis() - _downTime));
                return TOUCH_LONG;
            }
        }
        else if (!down && _wasDown) {
            _wasDown = false;
            if (!_longFired) {
                unsigned long held = millis() - _downTime;
                if (held >= TOUCH_DEBOUNCE_MS) {
                    DLOGF("[Touch] %s SHORT  held=%lums", name, held);
                    return TOUCH_SHORT;
                }
            }
        }
        return TOUCH_NONE;
    }

    bool isHeld() { return _wasDown; }
};

class TouchInput {
public:
    TouchPad interact { PIN_TOUCH_INTERACT, "INTERACT" };
    TouchPad plus     { PIN_TOUCH_PLUS,     "PLUS"     };
    TouchPad minus_   { PIN_TOUCH_MINUS,    "MINUS"    };
    TouchPad ok       { PIN_TOUCH_OK,       "OK"       };

    struct Events {
        TouchEvent interact;
        TouchEvent plus;
        TouchEvent minus_;
        TouchEvent ok;
    };

    // Raw değerleri 10 saniyede bir logla (spam değil)
    void logRawValues() {
        #if DEBUG_SERIAL
        static uint32_t _last = 0;
        if (millis() - _last < 10000) return;
        _last = millis();
        Serial.printf("[Touch RAW] I=%3d P=%3d M=%3d OK=%3d (thresh=%d)\n",
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
