#pragma once
#include <Arduino.h>
#include "config.h"

// ============================================================
//  touch_input.h  –  Capacitive touch pad handling
//  Detects: SHORT_TAP, LONG_PRESS for each pad
// ============================================================

enum TouchEvent { TOUCH_NONE, TOUCH_SHORT, TOUCH_LONG };

class TouchPad {
public:
    uint8_t     pin;
    bool        _wasDown     = false;
    unsigned long _downTime  = 0;
    bool        _longFired   = false;

    TouchPad(uint8_t p) : pin(p) {}

    // Call every loop iteration. Returns event if one occurred.
    TouchEvent update() {
        bool down = (touchRead(pin) < TOUCH_THRESHOLD);

        if (down && !_wasDown) {
            // Finger just landed
            _wasDown   = true;
            _downTime  = millis();
            _longFired = false;
        }
        else if (down && _wasDown && !_longFired) {
            if (millis() - _downTime >= TOUCH_LONG_PRESS_MS) {
                _longFired = true;
                return TOUCH_LONG;
            }
        }
        else if (!down && _wasDown) {
            _wasDown = false;
            if (!_longFired) {
                unsigned long held = millis() - _downTime;
                if (held >= TOUCH_DEBOUNCE_MS) {
                    return TOUCH_SHORT;
                }
            }
        }
        return TOUCH_NONE;
    }

    bool isHeld() {
        return _wasDown;
    }
};

// ============================================================
//  TouchInput — aggregates all 4 pads
// ============================================================
class TouchInput {
public:
    TouchPad interact { PIN_TOUCH_INTERACT };
    TouchPad plus     { PIN_TOUCH_PLUS     };
    TouchPad minus_   { PIN_TOUCH_MINUS    };
    TouchPad ok       { PIN_TOUCH_OK       };

    struct Events {
        TouchEvent interact;
        TouchEvent plus;
        TouchEvent minus_;
        TouchEvent ok;
    };

    Events poll() {
        return {
            interact.update(),
            plus.update(),
            minus_.update(),
            ok.update()
        };
    }
};

extern TouchInput touch;
