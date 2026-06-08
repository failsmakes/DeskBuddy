// ============================================================
//  DeskBuddy - main.cpp
//
//  Donanim: Wemos Lolin D32 + ILI9341 TFT + DHT11/22 + DS3231 RTC
//           ZS-042 Modulu + Kapasitif Touch padler + Buzzer
//
//  Gerekli Kutuphaneler:
//    - TFT_eSPI          (Bodmer) - PlatformIO: bodmer/TFT_eSPI
//    - RoboEyesTFT       (yousseftechdev) - manuel: lib/ klasorune kopyala
//    - ArduinoJson       (Benoit Blanchon) - PlatformIO: bblanchon/ArduinoJson
//    - WebServer         (ESP32 dahili)
//    - EEPROM            (ESP32 dahili)
//    - Wire              (ESP32 dahili)
//
//  TFT_eSPI platformio.ini build_flags ile yapilandirilir.
//  User_Setup.h duzenlemeye GEREK YOKTUR.
// ============================================================

#include <Arduino.h>
#include <Wire.h>
#include "soc/soc.h"           // WRITE_PERI_REG
#include "soc/rtc_cntl_reg.h"  // RTC_CNTL_BROWN_OUT_REG

#include "config.h"
#include "storage.h"
#include "rtc_manager.h"
#include "sleep_manager.h"
#include "wifi_manager.h"
#include "time_manager.h"
#include "dht_sensor.h"
#include "touch_input.h"
#include "buzzer.h"
#include "weather.h"
#include "alarm_manager.h"
#include "app_state.h"
#include "display_renderer.h"
#include "web_server.h"
#include "battery.h"

// RoboEyesTFT - Arduino.h DEFAULT macro catismasini once temizle
#ifdef DEFAULT
  #undef DEFAULT
#endif
#include "RoboEyesTFT_eSPI.h"

// ============================================================
//  Global singleton'lar
// ============================================================
Storage          storage;
RTCManager       rtcManager;
SleepManager     sleepMgr;
WiFiManager      wifiManager;
TimeManager      timeManager;
DHTSensor        dhtSensor(DHT_TYPE);
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

// ============================================================
//  Zamanlama degiskenleri
// ============================================================
unsigned long lastDHTread      = 0;
unsigned long lastWifiMaintain = 0;
unsigned long lastRedraw       = 0;
const unsigned long DHT_INTERVAL    = 10000UL;  // 10 sn
const unsigned long WIFI_CHECK_MS   = 30000UL;  // 30 sn
const unsigned long REDRAW_INTERVAL = 1000UL;   // 1 sn

// ============================================================
//  Yardimci fonksiyonlar
// ============================================================

uint16_t moodColour() {
    switch (appState.buddyMood) {
        case MOOD_HAPPY: return EYE_COLOR_HAPPY;
        case MOOD_TIRED: return EYE_COLOR_TIRED;
        case MOOD_ANGRY: return EYE_COLOR_ANGRY;
        default:         return EYE_COLOR_DEFAULT;
    }
}

void applyMood(BuddyMood mood) {
    if (!eyes) return;
    eyes->setColors(moodColour(), TFT_BLACK);
    switch (mood) {
        case MOOD_DEFAULT: eyes->setMood(DEFAULT); break;
        case MOOD_HAPPY:   eyes->setMood(HAPPY);   break;
        case MOOD_TIRED:   eyes->setMood(TIRED);   break;
        case MOOD_ANGRY:   eyes->setMood(ANGRY);   break;
    }
}

void advanceMood() {
    appState.moodSeqIdx = (appState.moodSeqIdx + 1) % MOOD_SEQUENCE_LEN;
    appState.buddyMood  = MOOD_SEQUENCE[appState.moodSeqIdx];
    applyMood(appState.buddyMood);
}

// ============================================================
//  Mod gecisi - ikincil konum yoksa MODE_WEATHER_S atlanir
// ============================================================
void nextMode() {
    bool hasSecondary = storage.hasSecondaryCity();
    appState.mode = nextModeSkipSecondary(appState.mode, hasSecondary);
    appState.showSecondary = false;
    display.invalidate();   // yeni mod icin tam yeniden cizimi zorla

    if (appState.mode == MODE_BUDDY) {
        display.tft.fillScreen(TFT_BLACK);
        applyMood(appState.buddyMood);
    } else {
        display.tft.fillScreen(TFT_BLACK);
    }
}

// ============================================================
//  Touch olaylarini isle
// ============================================================
void handleTouch(TouchInput::Events& ev) {
    bool anyTouch = (ev.interact != TOUCH_NONE ||
                     ev.plus     != TOUCH_NONE ||
                     ev.minus_   != TOUCH_NONE ||
                     ev.ok       != TOUCH_NONE);
    if (anyTouch) {
        sleepMgr.resetTimer();
        DLOGF("[handleTouch] EVENT interact=%d plus=%d minus=%d ok=%d  mode=%d",
              (int)ev.interact, (int)ev.plus,
              (int)ev.minus_,   (int)ev.ok,
              (int)appState.mode);
    }

    // ── Alarm veya geri sayim calarken: her turlu dokunusa tepki ver ───
    // Bu kontrol MOD'dan bagimsiz calisir - overlay aktifken her zaman gecerli
    if (alarmMgr.alarmFiring) {
        if (ev.interact == TOUCH_SHORT) {
            DLOG("[handleTouch] -> snoozeAlarm (short)");
            alarmMgr.snoozeAlarm();
            display.invalidate();
            return;
        }
        if (ev.interact == TOUCH_LONG) {
            DLOG("[handleTouch] -> dismissAlarm (long)");
            alarmMgr.dismissAlarm();
            display.invalidate();
            return;
        }
        return;  // diger padler alarm modunda islem yapmaz
    }
    if (alarmMgr.countdownFiring) {
        if (ev.interact != TOUCH_NONE) {
            DLOG("[handleTouch] -> countdownDismiss");
            alarmMgr.countdownDismiss();
            display.invalidate();
            return;
        }
        return;  // diger padler countdown modunda islem yapmaz
    }

    // ── Kisa dokunma: mod degis ──────────────────────────────
    if (ev.interact == TOUCH_SHORT) {
        if (!storage.hasSecondaryCity() && appState.showSecondary) {
            appState.showSecondary = false;
            DLOGF("[handleTouch] -> nextMode (skip secondary), new mode=%d", (int)appState.mode);
            nextMode();
            buzzer.beep(50);
            return;
        }
        DLOGF("[handleTouch] -> nextMode, current=%d", (int)appState.mode);
        nextMode();
        DLOGF("[handleTouch]    new mode=%d", (int)appState.mode);
        buzzer.beep(50);
        return;
    }

    // ── Uzun basma: moda ozgu islem ─────────────────────────
    if (ev.interact == TOUCH_LONG) {
        // Ikincil konum yoksa uzun basma ikincil ekrana gecmemeli
        switch (appState.mode) {
            case MODE_BUDDY:
                advanceMood();
                buzzer.beep(80);
                break;

            case MODE_DATETIME:
                if (storage.hasSecondaryCity()) {
                    appState.showSecondary = !appState.showSecondary;
                    display.invalidate();
                }
                // Ikincil konum yoksa uzun basma buddy'de bir sonraki moda gider
                else {
                    nextMode();
                    buzzer.beep(50);
                }
                break;

            case MODE_WEATHER_P:
                // Ikincil konum varsa 5 gunluk tahmine gec; yoksa sonraki moda gec
                if (!appState.showSecondary) {
                    appState.showSecondary = true;
                    display.invalidate();
                } else {
                    appState.showSecondary = false;
                    display.invalidate();
                    nextMode();
                    buzzer.beep(50);
                }
                break;

            case MODE_WEATHER_S:
                // Bu moda sadece ikincil konum tanimli ise gelinir
                appState.showSecondary = !appState.showSecondary;
                display.invalidate();
                break;

            case MODE_ALARM:
                appState.alarmSub = (AlarmSubScreen)
                    (((int)appState.alarmSub + 1) % (int)ALARM_SUB_COUNT);
                display.invalidate();
                break;

            default: break;
        }
        return;
    }

    // ── +, -, OK padleri (alarm modunda) ────────────────────
    if (appState.mode != MODE_ALARM) return;

    bool alarmChanged = false;  // herhangi bir degisiklik oldu mu?

    switch (appState.alarmSub) {
        case ALARM_SUB_ALARM: {
            uint8_t idx = appState.selectedAlarm;
            // + kisa: dakika +1
            if (ev.plus   == TOUCH_SHORT) {
                alarmMgr.alarms[idx].minute = (alarmMgr.alarms[idx].minute + 1) % 60;
                alarmChanged = true;
            }
            // - kisa: dakika -1
            if (ev.minus_ == TOUCH_SHORT) {
                alarmMgr.alarms[idx].minute = (alarmMgr.alarms[idx].minute + 59) % 60;
                alarmChanged = true;
            }
            // + uzun: saat +1
            if (ev.plus   == TOUCH_LONG) {
                alarmMgr.alarms[idx].hour = (alarmMgr.alarms[idx].hour + 1) % 24;
                alarmChanged = true;
            }
            // - uzun: saat -1
            if (ev.minus_ == TOUCH_LONG) {
                alarmMgr.alarms[idx].hour = (alarmMgr.alarms[idx].hour + 23) % 24;
                alarmChanged = true;
            }
            // OK kisa: ac/kapat
            if (ev.ok == TOUCH_SHORT) {
                if (alarmMgr.alarms[idx].enabled) alarmMgr.disableAlarm(idx);
                else                               alarmMgr.enableAlarm(idx);
                alarmChanged = true;
            }
            // OK uzun: sonraki alarm yuvasina gec
            if (ev.ok == TOUCH_LONG) {
                alarmMgr.saveToStorage();
                appState.selectedAlarm = (appState.selectedAlarm + 1) % MAX_ALARMS;
                buzzer.beep(40);
                alarmChanged = true;
            }
            // Degisiklik olduysa kaydet ve ekrani guncelle
            if (alarmChanged) {
                alarmMgr.saveToStorage();
                display.invalidate();
            }
            break;
        }

        case ALARM_SUB_COUNTDOWN: {
            uint32_t step = 60000;
            if (ev.plus   == TOUCH_SHORT) {
                alarmMgr.countdownSet(alarmMgr.countdownSetMs + step);
                alarmChanged = true;
            }
            if (ev.minus_ == TOUCH_SHORT && alarmMgr.countdownSetMs >= step) {
                alarmMgr.countdownSet(alarmMgr.countdownSetMs - step);
                alarmChanged = true;
            }
            if (ev.ok == TOUCH_SHORT) {
                if (alarmMgr.countdownRunning) alarmMgr.countdownStop();
                else                           alarmMgr.countdownStart();
                alarmChanged = true;
            }
            if (ev.ok == TOUCH_LONG) {
                alarmMgr.countdownReset();
                alarmChanged = true;
            }
            if (alarmChanged) display.invalidate();
            break;
        }

        case ALARM_SUB_STOPWATCH:
            if (ev.ok == TOUCH_SHORT) {
                if (alarmMgr.swRunning) alarmMgr.swStop();
                else                    alarmMgr.swStart();
                alarmChanged = true;
            }
            if (ev.ok == TOUCH_LONG) {
                alarmMgr.swReset();
                alarmChanged = true;
            }
            if (alarmChanged) display.invalidate();
            break;

        default: break;
    }
}

// ============================================================
//  Ekran cizim
// ============================================================
void drawCurrentScreen() {
    struct tm t = timeManager.getLocalTime();

    switch (appState.mode) {
        case MODE_BUDDY:
            if (eyes) eyes->update();
            break;

        case MODE_DATETIME:
            if (!appState.showSecondary) {
                display.drawDateTime(t, false);
            } else {
                display.drawDateTime(t, true,
                    storage.cfg.citySecondary,
                    weatherSecondary.valid ? &weatherSecondary.current : nullptr);
            }
            break;

        case MODE_WEATHER_P:
            if (!appState.showSecondary)
                display.drawWeatherCurrent(weatherPrimary.current, storage.cfg.cityPrimary);
            else
                display.drawForecast(weatherPrimary.forecast);
            break;

        case MODE_WEATHER_S:
            if (!appState.showSecondary)
                display.drawWeatherCurrent(weatherSecondary.current, storage.cfg.citySecondary);
            else
                display.drawForecast(weatherSecondary.forecast);
            break;

        case MODE_ALARM:
            switch (appState.alarmSub) {
                case ALARM_SUB_ALARM:
                    display.drawAlarmScreen(alarmMgr.alarms, appState.selectedAlarm);
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
    // Brownout detector'i devre disi birak.
    // WiFi AP baslatmasi ani yuksek akim ceker (~350mA).
    // Zayif besleme (USB kablo, hub) ile gerilim dusup brownout
    // reset dongusune girilir. Bu satir bunu onler.
    // NOT: Gercek dusuk batarya durumunda koruma kalmaz,
    //      bu nedenle batarya voltaj izleme (battery.h) onemlidir.
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

    // CPU frekansini 240MHz'de sabitle.
    // Bataryadan beslenirken ESP32 otomatik olarak 80MHz'e dusurebilir,
    // bu touch algilama hassasiyetini ve WiFi hizini olumsuz etkiler.
    setCpuFrequencyMhz(240);

    #if DEBUG_SERIAL
    Serial.begin(115200);
    #endif
    DLOG("\n[DeskBuddy] Booting...");

    // Uyku modundan uyanilip uyanilmadigini kontrol et
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    bool wokenFromSleep = (cause == ESP_SLEEP_WAKEUP_TOUCHPAD ||
                           cause == ESP_SLEEP_WAKEUP_EXT0);
    if (wokenFromSleep) DLOG("[DeskBuddy] Woke from deep sleep");

    // 1. EEPROM / Konfigurasyonu yukle
    storage.begin();
    // Alarm verilerini EEPROM'dan yukle
    alarmMgr.loadFromStorage();

    // 2. Ekran
    display.begin();
    display.drawLogo();

    // 3. RTC (Wire baslatir)
    rtcManager.begin();

    // 4. Uyku yoneticisi
    sleepMgr.begin();
    sleepMgr.setTimeoutSeconds(storage.cfg.sleepTimeoutS);

    // 5. Buzzer
    buzzer.begin();

    // 6. Batarya ADC
    battery.begin();

    // 7. WiFi - DHT'den ONCE baslatilmali
    bool wifiOK = wifiManager.begin();

    if (!wifiOK) {
        // AP modunda: SSID ve IP ekranda goster
        // softAPIP() bazi durumlarda 0.0.0.0 donebilir,
        // 1sn bekleyip tekrar sorgula
        delay(1000);
        String apIPStr = WiFi.softAPIP().toString();
        if (apIPStr == "0.0.0.0") apIPStr = "192.168.4.1";
        display.drawAPSplash(wifiManager.apSSID.c_str(), apIPStr.c_str());
        DLOGF("[Setup] AP splash shown: %s  %s\n",
                      wifiManager.apSSID.c_str(), apIPStr.c_str());
    }

    // 8. DHT sensoru (2sn delay icerir - WiFi basladiktan sonra)
    dhtSensor.begin();

    // 9. Zaman (NTP > RTC fallback)
    timeManager.begin();

    // 10. Web sunucu
    webServer.begin();

    // 11. Hava durumu ilk cekim (WiFi varsa)
    if (wifiOK && storage.cfg.owmApiKey[0]) {
        weatherPrimary.fetch(storage.cfg.cityPrimary, true);
        if (storage.hasSecondaryCity()) {
            weatherSecondary.fetch(storage.cfg.citySecondary, false);
        }
    }

    // 12. Logo sure tamamlanana kadar bekle
    // AP modundaysa logo yerine AP splash gosteriliyor, beklemeye gerek yok
    if (wifiOK) {
        unsigned long logoStart = millis();
        while (millis() - logoStart < LOGO_DISPLAY_MS) {
            webServer.loop();
            delay(50);
        }
    } else {
        // AP modunda: splash ekranda kalsin, web sunucusu calissin
        delay(100);
    }

    // 13. RoboEyesTFT - sadece STA modunda baslatilir
    // AP modunda buddy gosterilmez, ekranda AP splash kalir
    if (wifiOK) {
        eyes = new RoboEyesTFT(display.tft);
        eyes->begin();
        eyes->setAutoblinker(true, 3, 2);
        eyes->setIdleMode(true, 4, 1);
        eyes->setCuriosity(0.7f);
        applyMood(MOOD_DEFAULT);
        display.tft.fillScreen(TFT_BLACK);
    }

    DLOG("[DeskBuddy] Ready!");
}

// ============================================================
//  loop()
// ============================================================
void loop() {
    unsigned long now = millis();

    // Web sunucu - her modda calisir
    webServer.loop();

    // AP modunda sadece web sunucusu ve uyku zamanlayicisi calisir
    // Ekrana dokunulursa uyku sayaci sifirlanir, baska bir sey yapilmaz
    if (wifiManager.isAPMode) {
        TouchInput::Events ev = touch.poll();
        if (ev.interact != TOUCH_NONE) sleepMgr.resetTimer();
        sleepMgr.update();
        return;
    }

    // ---- Normal (STA) modu ----

    // Touch okuma
    TouchInput::Events ev = touch.poll();
    handleTouch(ev);

    // DHT sensoru guncelleme (sensor hazirsa)
    if (dhtSensor.isReady() && now - lastDHTread > DHT_INTERVAL) {
        dhtSensor.read();
        lastDHTread = now;
    }

    // Batarya guncelleme
    battery.update();

    // Alarm / geri sayim / kronometre
    struct tm localT = timeManager.getLocalTime();
    bool wasFiring = alarmMgr.alarmFiring || alarmMgr.countdownFiring;
    alarmMgr.update(localT);
    buzzer.update();
    bool isFiring = alarmMgr.alarmFiring || alarmMgr.countdownFiring;

    // Auto-stop ile alarm bitti: ekrani temizle
    if (wasFiring && !isFiring) {
        display.invalidate();
    }

    // Alarm calisiyor overlay
    if (isFiring) {
        if (now - lastRedraw > 500) {
            drawAlarmFiringOverlay();
            if (eyes && appState.mode == MODE_BUDDY) {
                eyes->setMood(HAPPY);
                eyes->setColors(EYE_COLOR_HAPPY, TFT_BLACK);
            }
            lastRedraw = now;
        }
        // Alarm calarken uyku moduna girme
        sleepMgr.resetTimer();
        return;
    }

    // WiFi baglanti kontrolu
    if (now - lastWifiMaintain > WIFI_CHECK_MS) {
        wifiManager.maintain();
        lastWifiMaintain = now;
        if (wifiManager.isConnected) {
            timeManager.maintain();
        }
    }

    // Hava durumu guncelleme - BLOCKING HTTP isteği touch'tan önce değil,
    // touch polling bittikten sonra ve sadece gerekirse yapılır.
    // Bataryada WiFi yavaş çalışabilir, fetch 1-5sn bloklayabilir.
    // Çözüm: fetch'i loop başında touch'tan SONRA yap,
    // ama her loop iterasyonunda değil - sadece needsUpdate() true ise.
    static bool _fetchNeeded = false;
    if (wifiManager.isConnected && storage.cfg.owmApiKey[0]) {
        if (weatherPrimary.needsUpdate() || 
            (storage.hasSecondaryCity() && weatherSecondary.needsUpdate())) {
            _fetchNeeded = true;
        }
    }
    // Fetch'i sadece herhangi bir touch aktivitesi yoksa yap
    // Bu sayede touch gecikmesi yaşanmaz
    if (_fetchNeeded) {
        bool anyActive = touch.interact.isHeld() || touch.plus.isHeld() ||
                         touch.minus_.isHeld() || touch.ok.isHeld();
        if (!anyActive) {
            if (weatherPrimary.needsUpdate())
                weatherPrimary.fetch(storage.cfg.cityPrimary);
            if (storage.hasSecondaryCity() && weatherSecondary.needsUpdate())
                weatherSecondary.fetch(storage.cfg.citySecondary);
            _fetchNeeded = false;
        }
    }

    // Ekran guncelleme
    if (appState.mode == MODE_BUDDY) {
        if (eyes) eyes->update();
    } else {
        uint32_t interval = (appState.mode == MODE_ALARM &&
                             appState.alarmSub == ALARM_SUB_STOPWATCH)
                            ? 100 : REDRAW_INTERVAL;
        if (now - lastRedraw > interval) {
            drawCurrentScreen();
            lastRedraw = now;
        }
    }

    // Uyku modu kontrolu (en sona - her sey tamamlandiktan sonra)
    sleepMgr.update();
}
