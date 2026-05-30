// ============================================================
//  DeskBuddy  –  main.cpp
//
//  Hardware: Wemos Lolin D32 + ILI9341 TFT + DHT20 + Touch pads
//  Libraries needed (install via Arduino Library Manager):
//    • TFT_eSPI          (Bodmer)
//    • RoboEyesTFT       (yousseftechdev) — install manually from GitHub
//    • ArduinoJson       (Benoit Blanchon)
//    • WebServer         (built-in ESP32)
//    • EEPROM            (built-in ESP32)
//    • Wire              (built-in)
//
//  TFT_eSPI User_Setup.h must be configured for ILI9341:
//    #define ILI9341_DRIVER
//    #define TFT_MOSI 23
//    #define TFT_SCLK 18
//    #define TFT_CS    5
//    #define TFT_DC   16
//    #define TFT_RST   17
//    #define TFT_BL    32   (optional backlight pin)
// ============================================================

#include <Arduino.h>
#include <Wire.h>

#include "config.h"
#include "storage.h"
#include "wifi_manager.h"
#include "time_manager.h"
#include "dht20_sensor.h"
#include "touch_input.h"
#include "buzzer.h"
#include "weather.h"
#include "alarm_manager.h"
#include "app_state.h"
#include "display_renderer.h"
#include "web_server.h"
#include "battery.h"

// ─── RoboEyesTFT ─────────────────────────────────────────────
// https://github.com/yousseftechdev/RoboEyesTFT
// Arduino.h defines DEFAULT=1; undef before including to avoid conflict.
#ifdef DEFAULT
  #undef DEFAULT
#endif
#include "RoboEyesTFT_eSPI.h"

// ─── Global singletons ───────────────────────────────────────
Storage          storage;
WiFiManager      wifiManager;
TimeManager      timeManager;
DHT20Sensor      dht20;
TouchInput       touch;
Buzzer           buzzer;
Weather          weatherPrimary;
Weather          weatherSecondary;
AlarmManager     alarmMgr;
AppState         appState;
DisplayRenderer  display;
ConfigWebServer  webServer;
Battery          battery;

RoboEyesTFT*     eyes = nullptr;

// ─── Timing helpers ──────────────────────────────────────────
unsigned long lastDHTread     = 0;
unsigned long lastWeatherFetch= 0;
unsigned long lastWifiMaintain= 0;
const unsigned long DHT_INTERVAL     = 10000;   // 10 s
const unsigned long WIFI_CHECK_MS    = 30000;   // 30 s
const unsigned long REDRAW_INTERVAL  = 1000;    // 1 s for non-buddy screens
unsigned long lastRedraw = 0;

// ─── Eye colour for current mood ─────────────────────────────
uint16_t moodColour() {
    switch (appState.buddyMood) {
        case MOOD_HAPPY:   return EYE_COLOR_HAPPY;
        case MOOD_TIRED:   return EYE_COLOR_TIRED;
        case MOOD_ANGRY:   return EYE_COLOR_ANGRY;
        default:           return EYE_COLOR_DEFAULT;
    }
}

// ─── Apply mood to RoboEyes ───────────────────────────────────
void applyMood(BuddyMood mood) {
    if (!eyes) return;
    eyes->setColors(moodColour(), TFT_BLACK);
    switch (mood) {
        case MOOD_DEFAULT:
            eyes->setMood(DEFAULT);
            break;
        case MOOD_HAPPY:
            eyes->setMood(HAPPY);
            break;
        case MOOD_TIRED:
            eyes->setMood(TIRED);
            break;
        case MOOD_ANGRY:
            eyes->setMood(ANGRY);
            break;
    }
}

// ─── Advance buddy mood one step ─────────────────────────────
void advanceMood() {
    appState.moodSeqIdx = (appState.moodSeqIdx + 1) % MOOD_SEQUENCE_LEN;
    appState.buddyMood  = MOOD_SEQUENCE[appState.moodSeqIdx];
    applyMood(appState.buddyMood);
}

// ─── Mode transitions ─────────────────────────────────────────
void nextMode() {
    int next = ((int)appState.mode + 1) % MODE_COUNT;
    appState.mode = (AppMode)next;
    appState.showSecondary = false;
    if (appState.mode == MODE_BUDDY) {
        // Re-init eyes on screen
        display.tft.fillScreen(TFT_BLACK);
        applyMood(appState.buddyMood);
    } else {
        // Eyes not needed; clear screen
        display.tft.fillScreen(TFT_BLACK);
    }
}

// ─── Handle touch events per mode ────────────────────────────
void handleTouch(TouchInput::Events& ev) {
    // ── Short tap = mode cycle (always) ──────────────────────
    if (ev.interact == TOUCH_SHORT) {
        if (appState.mode == MODE_ALARM) {
            // In alarm sub-screen: short tap snoozes / advances
            if (alarmMgr.alarmFiring) { alarmMgr.snoozeAlarm(); return; }
        }
        nextMode();
        buzzer.beep(50);
        return;
    }

    // ── Long press = mode-specific action ────────────────────
    if (ev.interact == TOUCH_LONG) {
        switch (appState.mode) {
            case MODE_BUDDY:
                advanceMood();
                buzzer.beep(80);
                break;

            case MODE_DATETIME:
                appState.showSecondary = !appState.showSecondary;
                break;

            case MODE_WEATHER_P:
            case MODE_WEATHER_S:
                appState.showSecondary = !appState.showSecondary;
                break;

            case MODE_ALARM:
                if (alarmMgr.alarmFiring)       { alarmMgr.dismissAlarm(); return; }
                if (alarmMgr.countdownFiring)    { alarmMgr.countdownDismiss(); return; }
                // Cycle alarm sub-screens
                appState.alarmSub = (AlarmSubScreen)
                    (((int)appState.alarmSub + 1) % (int)ALARM_SUB_COUNT);
                break;

            default: break;
        }
        return;
    }

    // ── +, -, OK pads (used in alarm mode) ───────────────────
    if (appState.mode != MODE_ALARM) return;

    switch (appState.alarmSub) {
        case ALARM_SUB_ALARM:
            if (ev.plus  == TOUCH_SHORT) { alarmMgr.alarmMinute = (alarmMgr.alarmMinute + 1) % 60; }
            if (ev.minus_ == TOUCH_SHORT) { alarmMgr.alarmMinute = (alarmMgr.alarmMinute + 59) % 60; }
            if (ev.plus  == TOUCH_LONG)  { alarmMgr.alarmHour = (alarmMgr.alarmHour + 1) % 24; }
            if (ev.minus_ == TOUCH_LONG) { alarmMgr.alarmHour = (alarmMgr.alarmHour + 23) % 24; }
            if (ev.ok    == TOUCH_SHORT) {
                if (alarmMgr.alarmEnabled) alarmMgr.disableAlarm();
                else                       alarmMgr.enableAlarm();
            }
            break;

        case ALARM_SUB_COUNTDOWN: {
            uint32_t step = 60000; // 1 min step
            if (ev.plus   == TOUCH_SHORT) alarmMgr.countdownSet(alarmMgr.countdownSetMs + step);
            if (ev.minus_  == TOUCH_SHORT && alarmMgr.countdownSetMs >= step)
                alarmMgr.countdownSet(alarmMgr.countdownSetMs - step);
            if (ev.ok     == TOUCH_SHORT) {
                if (alarmMgr.countdownRunning) alarmMgr.countdownStop();
                else                           alarmMgr.countdownStart();
            }
            if (ev.ok     == TOUCH_LONG) alarmMgr.countdownReset();
            break;
        }

        case ALARM_SUB_STOPWATCH:
            if (ev.ok    == TOUCH_SHORT) {
                if (alarmMgr.swRunning) alarmMgr.swStop();
                else                    alarmMgr.swStart();
            }
            if (ev.ok    == TOUCH_LONG) alarmMgr.swReset();
            break;

        default: break;
    }
}

// ─── Draw current screen ─────────────────────────────────────
void drawCurrentScreen() {
    struct tm t = timeManager.getLocalTime();

    switch (appState.mode) {
        case MODE_BUDDY:
            // RoboEyesTFT handles its own draw loop
            if (eyes) eyes->update();
            break;

        case MODE_DATETIME:
            if (!appState.showSecondary) {
                display.drawDateTime(t, false);
            } else {
                // Secondary city time — apply tz offset difference roughly
                // For simplicity show same NTP time + city name (full tz offset support needs separate NTP)
                display.drawDateTime(t, true,
                    storage.cfg.citySecondary,
                    weatherSecondary.valid ? &weatherSecondary.current : nullptr);
            }
            break;

        case MODE_WEATHER_P:
            if (!appState.showSecondary) {
                display.drawWeatherCurrent(weatherPrimary.current, storage.cfg.cityPrimary);
            } else {
                display.drawForecast(weatherPrimary.forecast);
            }
            break;

        case MODE_WEATHER_S:
            if (!appState.showSecondary) {
                display.drawWeatherCurrent(weatherSecondary.current, storage.cfg.citySecondary);
            } else {
                display.drawForecast(weatherSecondary.forecast);
            }
            break;

        case MODE_ALARM:
            switch (appState.alarmSub) {
                case ALARM_SUB_ALARM:
                    display.drawAlarmScreen(alarmMgr.alarmHour, alarmMgr.alarmMinute, alarmMgr.alarmEnabled);
                    break;
                case ALARM_SUB_COUNTDOWN:
                    display.drawCountdownScreen(alarmMgr.countdownCurrent(), alarmMgr.countdownRunning);
                    break;
                case ALARM_SUB_STOPWATCH:
                    display.drawStopwatchScreen(alarmMgr.swCurrent(), alarmMgr.swRunning);
                    break;
            }
            break;
    }
}

// ─── Show "alarm firing" overlay ─────────────────────────────
void drawAlarmFiringOverlay() {
    display.tft.fillScreen(TFT_RED);
    display.tft.setTextColor(TFT_WHITE, TFT_RED);
    display.tft.setTextSize(3);
    display.tft.setCursor(55, 90);
    display.tft.print("ALARM!");
    display.tft.setTextSize(1);
    display.tft.setCursor(50, 145);
    display.tft.print("Short tap = snooze   Long = dismiss");
}

// ============================================================
//  setup()
// ============================================================
void setup() {
    Serial.begin(115200);
    Serial.println("\n[DeskBuddy] Booting…");

    // Storage
    storage.begin();

    // Display
    display.begin();
    display.drawLogo();

    // DHT20
    dht20.begin();

    // Buzzer
    buzzer.begin();

    // Batarya
    battery.begin();

    // WiFi
    bool wifiOK = wifiManager.begin();

    if (!wifiOK) {
        // Show AP splash before waiting
        display.drawAPSplash(wifiManager.apSSID.c_str(),
                             wifiManager.apIP.toString().c_str());
        delay(2000);
    }

    // Time (needs WiFi)
    if (wifiOK) {
        timeManager.begin();
    }

    // Web server (works in both STA and AP mode)
    webServer.begin();

    // Initial weather fetch
    if (wifiOK && storage.cfg.owmApiKey[0]) {
        weatherPrimary.fetch(storage.cfg.cityPrimary, true);
        weatherSecondary.fetch(storage.cfg.citySecondary, false);
    }

    // Wait out logo duration (already showed logo above)
    unsigned long logoStart = millis();
    while (millis() - logoStart < LOGO_DISPLAY_MS) {
        webServer.loop();
        delay(50);
    }

    // Init RoboEyesTFT for buddy mode
    eyes = new RoboEyesTFT(display.tft);
    eyes->begin();
    eyes->setAutoblinker(true, 3, 2);
    eyes->setIdleMode(true, 4, 1);
    eyes->setCuriosity(0.7f);
    applyMood(MOOD_DEFAULT);

    // Clear screen, enter buddy mode
    display.tft.fillScreen(TFT_BLACK);
    Serial.println("[DeskBuddy] Ready!");
}

// ============================================================
//  loop()
// ============================================================
void loop() {
    unsigned long now = millis();

    // ── Web server ──────────────────────────────────────────
    webServer.loop();

    // ── Touch input ─────────────────────────────────────────
    TouchInput::Events ev = touch.poll();
    handleTouch(ev);

    // ── DHT20 ───────────────────────────────────────────────
    if (now - lastDHTread > DHT_INTERVAL) {
        dht20.read();
        lastDHTread = now;
    }

    // ── Batarya ─────────────────────────────────────────────
    battery.update();

    // ── Alarm / countdown / stopwatch ───────────────────────
    struct tm localT = timeManager.getLocalTime();
    alarmMgr.update(localT);
    buzzer.update();

    // ── Alarm firing overlay ─────────────────────────────────
    if (alarmMgr.alarmFiring || alarmMgr.countdownFiring) {
        // Override screen with overlay each second
        if (now - lastRedraw > 500) {
            drawAlarmFiringOverlay();
            // RoboEyes happy popup while alarm fires
            if (eyes && appState.mode == MODE_BUDDY) {
                eyes->setMood(HAPPY);
                eyes->setColors(EYE_COLOR_HAPPY, TFT_BLACK);
            }
            lastRedraw = now;
        }
        return;  // Don't draw normal screen while alarm fires
    }

    // ── WiFi maintain ────────────────────────────────────────
    if (now - lastWifiMaintain > WIFI_CHECK_MS) {
        wifiManager.maintain();
        lastWifiMaintain = now;
        if (wifiManager.isConnected) {
            timeManager.maintain();
        }
    }

    // ── Weather update ───────────────────────────────────────
    if (wifiManager.isConnected && storage.cfg.owmApiKey[0]) {
        if (weatherPrimary.needsUpdate())   weatherPrimary.fetch(storage.cfg.cityPrimary);
        if (weatherSecondary.needsUpdate()) weatherSecondary.fetch(storage.cfg.citySecondary);
    }

    // ── Screen draw ──────────────────────────────────────────
    if (appState.mode == MODE_BUDDY) {
        // RoboEyes runs at its own frame rate
        if (eyes) eyes->update();
    } else {
        // Other screens: redraw at 1 Hz (stopwatch: 100ms)
        uint32_t interval = (appState.mode == MODE_ALARM &&
                             appState.alarmSub == ALARM_SUB_STOPWATCH) ? 100 : REDRAW_INTERVAL;
        if (now - lastRedraw > interval) {
            drawCurrentScreen();
            lastRedraw = now;
        }
    }
}
