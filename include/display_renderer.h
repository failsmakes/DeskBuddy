#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>
#include "config.h"
#include "app_state.h"
#include "weather.h"
#include "alarm_manager.h"
#include "dht_sensor.h"
#include "time_manager.h"
#include "battery.h"

// ============================================================
//  display_renderer.h  -  Sprite tabanli kırpisma-onleyici renderer
//
//  Strateji:
//  1. Her ekranin statik altyapisi sadece mod degisiminde tam cizilir
//  2. Dinamik alanlar (saat, batarya, sicaklik...) sprite ile guncellenir:
//     - Onceki degerle karsilastirilir
//     - Degismemisse sprite hic gonderilmez
//  3. Her sprite kendi arka plan rengiyle olusturulur -> fillScreen gerek yok
// ============================================================

// ---- Sprite boyutlari ----
#define SPR_TIME_W     200   // HH:MM:SS tam saat - textsize4 x 8 karakter
#define SPR_TIME_H      32
#define SPR_DATE_W     160   // DD.MM.YYYY  font2
#define SPR_DATE_H      20
#define SPR_BAT_W       72   // ikon + yuzde
#define SPR_BAT_H       18
#define SPR_SENSOR_W   200   // "23.4 C"  font2
#define SPR_SENSOR_H    20
#define SPR_CD_W       280   // countdown/stopwatch
#define SPR_CD_H        40
#define SPR_VOLT_W      60   // "3.91V"
#define SPR_VOLT_H      12

class DisplayRenderer {
public:
    TFT_eSPI    tft;

    // Sprite'lar - sadece gerekince heap'te olusturulur, sonra silinir
    TFT_eSprite sprTime   {&tft};
    TFT_eSprite sprDate   {&tft};
    TFT_eSprite sprBat    {&tft};
    TFT_eSprite sprTemp   {&tft};
    TFT_eSprite sprHum    {&tft};
    TFT_eSprite sprVolt   {&tft};
    TFT_eSprite sprCd     {&tft};

    // ---- Onceki deger cache'leri (degisimi tespit icin) ----
    int8_t  _lastHour   = -1;
    int8_t  _lastMin    = -1;
    int8_t  _lastSec    = -1;
    int16_t _lastMday   = -1;
    int8_t  _lastBatPct = -1;
    bool    _lastBatCrit= false;
    float   _lastTemp   = -999;
    float   _lastHum    = -999;
    float   _lastVolt   = -999;
    uint32_t _lastCdMs  = UINT32_MAX;
    bool    _lastCdRun  = false;
    uint32_t _lastSwMs  = UINT32_MAX;
    bool    _lastSwRun  = false;

    // Hangi ekran son cizildi (tam yeniden cizim icin)
    AppMode  _lastMode       = (AppMode)-1;
    bool     _lastShowSec    = false;
    bool     _needFullRedraw = true;

    // ---- Baslangic ----
    void begin() {
        tft.init();
        tft.setRotation(1);
        tft.fillScreen(TFT_BLACK);
        _needFullRedraw = true;
    }

    // ---- Tam yeniden cizimi zorla (mod degisiminde) ----
    void invalidate() {
        _needFullRedraw = true;
        _lastHour = _lastMin = _lastSec = -1;
        _lastMday = -1;
        _lastBatPct = -1;
        _lastTemp = _lastHum = _lastVolt = -999;
        _lastCdMs = UINT32_MAX;
        _lastSwMs = UINT32_MAX;
    }

    // ===========================================================
    //  LOGO
    // ===========================================================
    void drawLogo() {
        tft.fillScreen(LOGO_BG_COLOR);
        const int bytesPerRow = (LOGO_W + 7) / 8;
        int16_t x0 = (SCREEN_W - LOGO_W * LOGO_SCALE) / 2;
        int16_t y0 = (SCREEN_H - LOGO_H * LOGO_SCALE) / 2;
        if (x0 < 0) x0 = 0;
        if (y0 < 0) y0 = 0;

        if (LOGO_SCALE == 1) {
            uint16_t lineBuf[LOGO_W];
            for (int row = 0; row < LOGO_H; row++) {
                for (int col = 0; col < LOGO_W; col++) {
                    uint8_t b = pgm_read_byte(&LOGO_BITMAP[row * bytesPerRow + (col >> 3)]);
                    lineBuf[col] = ((b >> (7 - (col & 7))) & 1) ? LOGO_FG_COLOR : LOGO_BG_COLOR;
                }
                tft.setAddrWindow(x0, y0 + row, x0 + LOGO_W - 1, y0 + row);
                tft.pushColors(lineBuf, LOGO_W, true);
            }
        } else {
            for (int row = 0; row < LOGO_H; row++)
                for (int col = 0; col < LOGO_W; col++) {
                    uint8_t b = pgm_read_byte(&LOGO_BITMAP[row * bytesPerRow + (col >> 3)]);
                    bool px = (b >> (7 - (col & 7))) & 1;
                    tft.fillRect(x0 + col * LOGO_SCALE, y0 + row * LOGO_SCALE,
                                 LOGO_SCALE, LOGO_SCALE, px ? LOGO_FG_COLOR : LOGO_BG_COLOR);
                }
        }
        int16_t textY = y0 + LOGO_H * LOGO_SCALE + 4;
        if (textY + 16 < SCREEN_H) {
            tft.setTextColor(LOGO_FG_COLOR, LOGO_BG_COLOR);
            tft.setTextSize(2);
            tft.setCursor(SCREEN_W / 2 - 58, textY);
            tft.print("DeskBuddy");
        }
    }

    // ===========================================================
    //  AP SPLASH
    // ===========================================================
    void drawAPSplash(const char* apSSID, const char* apIP) {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_YELLOW, TFT_BLACK); tft.setTextSize(2);
        tft.setCursor(20, 20); tft.print("WiFi Setup Mode");
        tft.setTextColor(TFT_WHITE, TFT_BLACK); tft.setTextSize(1);
        tft.setCursor(10, 60); tft.print("Connect to WiFi:");
        tft.setTextColor(TFT_CYAN, TFT_BLACK); tft.setTextSize(2);
        tft.setCursor(10, 80); tft.print(apSSID);
        tft.setTextColor(TFT_WHITE, TFT_BLACK); tft.setTextSize(1);
        tft.setCursor(10, 115); tft.print("Pass: deskbuddy123");
        tft.setCursor(10, 135); tft.print("Then open browser:");
        tft.setTextColor(TFT_GREEN, TFT_BLACK); tft.setTextSize(2);
        tft.setCursor(10, 155); tft.print(apIP);
    }

    // ===========================================================
    //  DATE/TIME ekrani - sprite tabanli guncelleme
    // ===========================================================
    void drawDateTime(struct tm& t, bool showSec = false,
                      const char* secCity = nullptr,
                      const WeatherDay* secWeather = nullptr) {

        bool modeChanged = _needFullRedraw || (_lastShowSec != showSec);

        if (modeChanged) {
            // Tam ekrani bir kez ciz (statik elemanlar)
            tft.fillScreen(TFT_BLACK);
            tft.drawFastHLine(0, 118, SCREEN_W, TFT_DARKGREY);

            if (!showSec) {
                // Statik etiketler (degismez)
                // "Temp:" ve "Hum:" etiketlerini sadece ilk cizimde yaz
                tft.setTextSize(2);
                tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
                tft.setCursor(10, 130); tft.print("Temp:");
                tft.setCursor(10, 158); tft.print("Hum: ");
            } else {
                tft.setTextSize(2);
                tft.setTextColor(TFT_CYAN, TFT_BLACK);
                tft.setCursor(10, 10);
                tft.print(secCity ? secCity : "Secondary");
            }

            _needFullRedraw = false;
            _lastShowSec    = showSec;
            // Cache'leri sifirla - hepsini yeniden ciz
            _lastHour = _lastMin = _lastSec = -1;
            _lastMday = -1;
            _lastBatPct = -1;
            _lastTemp = _lastHum = _lastVolt = -999;
        }

        if (!showSec) {
            _updateTimeSprite(t);
            _updateDateSprite(t);
            _updateBatterySprite();
            _updateTempSprite();
            _updateHumSprite();
            _updateVoltSprite();
        } else {
            _updateSecondaryTimeSprite(t);
            _updateBatterySprite();
            if (secWeather) _updateSecWeatherText(secWeather);
        }
    }

    // ===========================================================
    //  HAVA DURUMU ekrani
    // ===========================================================
    void drawWeatherCurrent(const WeatherDay& w, const char* city) {
        if (_needFullRedraw) {
            tft.fillScreen(TFT_NAVY);
            tft.setTextColor(TFT_CYAN, TFT_NAVY); tft.setTextSize(2);
            tft.setCursor(8, 8); tft.print(city);
            // Statik bilgiler
            tft.setTextSize(4); tft.setTextColor(TFT_WHITE, TFT_NAVY);
            tft.setCursor(100, 40); tft.printf("%.0f", w.tempCurrent);
            tft.setTextSize(2); tft.setCursor(175, 38); tft.print("C");
            tft.setTextSize(2); tft.setTextColor(TFT_LIGHTGREY, TFT_NAVY);
            tft.setCursor(100, 90); tft.print(w.desc);
            tft.setTextSize(2); tft.setTextColor(TFT_YELLOW, TFT_NAVY);
            tft.setCursor(100, 130);
            tft.printf("D:%.0f  Y:%.0f", w.tempMin, w.tempMax);
            tft.setTextSize(2); tft.setTextColor(TFT_WHITE, TFT_NAVY);
            tft.setCursor(100, 158); tft.printf("Nem: %d%%", w.humidity);
            _needFullRedraw = false;
        }
        // Hava ikonu her frame guncellenir (animasyon icin) ama sprite ile
        _updateWeatherIconSprite(w.conditionId, 8, 35, 80, 80, TFT_NAVY);
    }

    void drawForecast(const WeatherDay forecast[5]) {
        if (!_needFullRedraw) return;  // Forecast statik, degismiyorsa yeniden cizme
        _needFullRedraw = false;

        tft.fillScreen(TFT_NAVY);
        tft.setTextSize(1); tft.setTextColor(TFT_CYAN, TFT_NAVY);
        tft.setCursor(100, 4); tft.print("5-Day Forecast");

        uint16_t colW = SCREEN_W / 5;
        const char* dayNames[] = {"Pzt","Sal","Car","Per","Cum"};

        for (int i = 0; i < 5; i++) {
            int x = i * colW;
            if (i > 0) tft.drawFastVLine(x, 20, SCREEN_H - 20, TFT_DARKGREY);
            drawWeatherIcon(forecast[i].conditionId, x + 8, 24, colW - 16, 60);
            tft.setTextSize(1);
            tft.setTextColor(TFT_WHITE, TFT_NAVY);
            tft.setCursor(x + 4, 92); tft.printf("Y%.0f", forecast[i].tempMax);
            tft.setTextColor(TFT_CYAN, TFT_NAVY);
            tft.setCursor(x + 4, 106); tft.printf("D%.0f", forecast[i].tempMin);
            tft.setTextColor(TFT_YELLOW, TFT_NAVY);
            tft.setCursor(x + 4, 120); tft.print(dayNames[i]);
        }
    }

    // ===========================================================
    //  ALARM ekrani
    // ===========================================================
    void drawAlarmScreen(const AlarmEntry alarms[], uint8_t selectedIdx) {
        if (!_needFullRedraw) return;
        _needFullRedraw = false;

        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_RED, TFT_BLACK); tft.setTextSize(2);
        tft.setCursor(10, 6); tft.print("ALARM");

        for (uint8_t i = 0; i < MAX_ALARMS; i++) {
            const AlarmEntry& a = alarms[i];
            int16_t y = 38 + i * 52;
            bool selected = (i == selectedIdx);

            if (selected)
                tft.fillRect(0, y - 4, SCREEN_W, 48, 0x1082);

            tft.setTextSize(1);
            tft.setTextColor(selected ? TFT_CYAN : TFT_DARKGREY,
                             selected ? 0x1082 : TFT_BLACK);
            tft.setCursor(8, y); tft.printf("ALM%d", i + 1);

            tft.setTextSize(3);
            uint16_t tc = a.firing ? TFT_RED : (a.enabled ? TFT_WHITE : 0x4208);
            tft.setTextColor(tc, selected ? 0x1082 : TFT_BLACK);
            tft.setCursor(48, y - 4); tft.printf("%02d:%02d", a.hour, a.minute);

            tft.setTextSize(1);
            if (a.firing) {
                tft.setTextColor(TFT_RED, selected ? 0x1082 : TFT_BLACK);
                tft.setCursor(200, y); tft.print("CALIYOR!");
            } else if (a.snoozed) {
                tft.setTextColor(TFT_YELLOW, selected ? 0x1082 : TFT_BLACK);
                tft.setCursor(200, y); tft.print("ERTELENDI");
            } else {
                int16_t bx = 200, by = y - 2, bw = 44, bh = 18;
                if (a.enabled) {
                    tft.fillRoundRect(bx, by, bw, bh, 9, TFT_GREEN);
                    tft.setTextColor(TFT_BLACK, TFT_GREEN);
                    tft.setCursor(bx + 8, by + 5); tft.print("ACIK");
                } else {
                    tft.drawRoundRect(bx, by, bw, bh, 9, TFT_DARKGREY);
                    tft.setTextColor(TFT_DARKGREY, selected ? 0x1082 : TFT_BLACK);
                    tft.setCursor(bx + 4, by + 5); tft.print("KAPALI");
                }
            }
            if (i < MAX_ALARMS - 1)
                tft.drawFastHLine(0, y + 44, SCREEN_W, 0x2104);
        }
        tft.setTextSize(1); tft.setTextColor(0x4208, TFT_BLACK);
        tft.setCursor(4, 224); tft.print("+/-:saat  OK:ac/kapat  UZUN:alt ekran");
    }

    // ===========================================================
    //  COUNTDOWN ekrani - sprite ile sadece sayac guncellenir
    // ===========================================================
    void drawCountdownScreen(uint32_t leftMs, bool running) {
        if (_needFullRedraw) {
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(TFT_ORANGE, TFT_BLACK); tft.setTextSize(2);
            tft.setCursor(10, 10); tft.print("GERI SAYIM");
            tft.setTextSize(1); tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
            tft.setCursor(10, 200); tft.print("+/- ayarla  OK baslat/durdur  UZUN sifirla");
            _needFullRedraw = false;
            _lastCdMs  = UINT32_MAX;  // yeniden ciz
            _lastCdRun = !running;    // zorla guncelle
        }

        // Sadece deger veya durum degistiyse guncelle
        uint32_t cdSec = leftMs / 1000;
        uint32_t lastSec = _lastCdMs / 1000;
        if (cdSec != lastSec || running != _lastCdRun) {
            _lastCdMs  = leftMs;
            _lastCdRun = running;

            sprCd.createSprite(SPR_CD_W, SPR_CD_H);
            sprCd.fillSprite(TFT_BLACK);
            sprCd.setTextSize(4);
            sprCd.setTextColor(running ? TFT_GREEN : TFT_WHITE);
            sprCd.setCursor(0, 4);
            sprCd.printf("%02lu:%02lu:%02lu",
                (unsigned long)(cdSec / 3600),
                (unsigned long)((cdSec % 3600) / 60),
                (unsigned long)(cdSec % 60));
            sprCd.pushSprite(20, 70);
            sprCd.deleteSprite();
        }
    }

    // ===========================================================
    //  STOPWATCH ekrani - sprite ile sadece sayac guncellenir
    // ===========================================================
    void drawStopwatchScreen(uint32_t elapsedMs, bool running) {
        if (_needFullRedraw) {
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(TFT_CYAN, TFT_BLACK); tft.setTextSize(2);
            tft.setCursor(10, 10); tft.print("KRONOMETRE");
            tft.setTextSize(1); tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
            tft.setCursor(10, 200); tft.print("OK baslat/durdur  UZUN sifirla");
            _needFullRedraw = false;
            _lastSwMs  = UINT32_MAX;
            _lastSwRun = !running;
        }

        // 100ms hassasiyette guncelle
        uint32_t cs = elapsedMs / 10;   // centisaniye
        uint32_t lastCs = _lastSwMs / 10;
        if (cs != lastCs || running != _lastSwRun) {
            _lastSwMs  = elapsedMs;
            _lastSwRun = running;

            uint32_t s  = elapsedMs / 1000;
            uint32_t ms = (elapsedMs % 1000) / 10;

            sprCd.createSprite(SPR_CD_W, SPR_CD_H);
            sprCd.fillSprite(TFT_BLACK);
            sprCd.setTextSize(3);
            sprCd.setTextColor(running ? TFT_GREEN : TFT_WHITE);
            sprCd.setCursor(0, 6);
            sprCd.printf("%02lu:%02lu.%02lu",
                (unsigned long)(s / 60),
                (unsigned long)(s % 60),
                (unsigned long)ms);
            sprCd.pushSprite(20, 80);
            sprCd.deleteSprite();
        }
    }

    // ===========================================================
    //  WEATHER ICON (dogrudan tft'ye - statik ekranlarda)
    // ===========================================================
    void drawWeatherIcon(uint8_t conditionId, int16_t x, int16_t y,
                         int16_t w, int16_t h) {
        tft.fillRect(x, y, w, h, TFT_NAVY);
        int16_t cx = x + w/2, cy = y + h/2, r = min(w,h)/3;
        uint32_t phase = (millis() / 100) % 20;

        switch (conditionId) {
            case 8:
                tft.fillCircle(cx, cy, r, TFT_YELLOW);
                for (int a = 0; a < 360; a += 45) {
                    float rad = (a + phase*4) * PI / 180.0f;
                    tft.drawLine(cx+(r+3)*cos(rad), cy+(r+3)*sin(rad),
                                 cx+(r+8)*cos(rad), cy+(r+8)*sin(rad), TFT_YELLOW);
                }
                break;
            case 7:
                for (int i = 0; i < 4; i++)
                    tft.drawFastHLine(x+5, y+15+i*12+(phase%4), w-10, TFT_LIGHTGREY);
                break;
            case 6:
                tft.fillCircle(cx, cy-5, r-5, TFT_WHITE);
                for (int i = 0; i < 5; i++) {
                    int sx=x+(i*w/5)+4, sy=y+10+((phase+i*3)%(h-20));
                    tft.drawPixel(sx, sy, TFT_WHITE);
                    tft.drawPixel(sx+1, sy, TFT_WHITE);
                }
                break;
            case 5: case 3:
                tft.fillCircle(cx, cy-10, r-5, TFT_LIGHTGREY);
                for (int i = 0; i < 5; i++) {
                    int rx=x+(i*w/5)+4, ry=y+20+((phase*3+i*5)%(h-30));
                    tft.drawLine(rx, ry, rx-2, ry+6, TFT_CYAN);
                }
                break;
            case 2:
                tft.fillCircle(cx, cy-10, r-5, TFT_DARKGREY);
                tft.drawLine(cx, cy-5, cx-6, cy+5, TFT_YELLOW);
                tft.drawLine(cx-6, cy+5, cx+2, cy+5, TFT_YELLOW);
                tft.drawLine(cx+2, cy+5, cx-4, cy+18, TFT_YELLOW);
                break;
            default:
                tft.fillCircle(cx-8, cy+5, r-8, TFT_LIGHTGREY);
                tft.fillCircle(cx+5, cy+5, r-5, TFT_LIGHTGREY);
                tft.fillCircle(cx,   cy-3,  r-7, TFT_WHITE);
                break;
        }
    }

private:
    // ===========================================================
    //  SPRITE GUNCELLEME YARDIMCILARI
    //  Her biri: onceki degerle karsilastir, fark yoksa return
    // ===========================================================

    // HH:MM:SS - tek sprite, tum saat satirini gunceller
    // textSize(4): her karakter 24x32px
    // "HH:MM:SS" = 8 karakter x 24px = 192px genislik, 32px yukseklik
    // Ayirici ":" 12px oldugu icin: 2*24 + 12 + 2*24 + 12 + 2*24 = 144px
    // Gercek olcum: printf ile font2 textsize4 = karakter basi ~24px, ":" ~14px
    // Guvenli genislik: SCREEN_W - 16 - 4 = 300px (tasmaz)
    void _updateTimeSprite(struct tm& t) {
        bool changed = (t.tm_hour != _lastHour ||
                        t.tm_min  != _lastMin  ||
                        t.tm_sec  != _lastSec);
        if (!changed) return;

        _lastHour = t.tm_hour;
        _lastMin  = t.tm_min;
        _lastSec  = t.tm_sec;

        // Tum saati tek sprite'ta ciz - hizalama sorunu olmaz
        // textSize(4) ile "00:00:00" = yaklasik 192px x 32px
        const int16_t W = 200;
        const int16_t H = 32;

        sprTime.createSprite(W, H);
        sprTime.fillSprite(TFT_BLACK);
        sprTime.setTextSize(4);
        sprTime.setTextColor(TFT_CYAN);  // tum saat ayni renk
        sprTime.setCursor(0, 0);
        sprTime.printf("%02d:%02d:%02d",
                       t.tm_hour, t.tm_min, t.tm_sec);
        sprTime.pushSprite(16, 30);
        sprTime.deleteSprite();
    }

    // Ikincil konum saati (HH:MM, daha buyuk)
    void _updateSecondaryTimeSprite(struct tm& t) {
        if (t.tm_hour != _lastHour || t.tm_min != _lastMin) {
            _lastHour = t.tm_hour;
            _lastMin  = t.tm_min;

            sprTime.createSprite(200, 40);
            sprTime.fillSprite(TFT_BLACK);
            sprTime.setTextSize(4);
            sprTime.setTextColor(TFT_WHITE);
            sprTime.setCursor(0, 0);
            sprTime.printf("%02d:%02d", t.tm_hour, t.tm_min);
            sprTime.pushSprite(40, 50);
            sprTime.deleteSprite();
        }
    }

    // Tarih (gun degisince guncellenir - genellikle hic degismez)
    void _updateDateSprite(struct tm& t) {
        if (t.tm_mday == _lastMday) return;
        _lastMday = t.tm_mday;

        sprDate.createSprite(SPR_DATE_W, SPR_DATE_H);
        sprDate.fillSprite(TFT_BLACK);
        sprDate.setTextSize(2);
        sprDate.setTextColor(TFT_WHITE);
        sprDate.setCursor(0, 0);
        sprDate.printf("%02d.%02d.%04d",
            t.tm_mday, t.tm_mon + 1, t.tm_year + 1900);
        sprDate.pushSprite(72, 92);
        sprDate.deleteSprite();
    }

    // Batarya widget (yuzde degisince guncellenir - 30sn aralik)
    void _updateBatterySprite() {
        if (!battery.valid) return;
        if (battery.percent == _lastBatPct &&
            battery.critical == _lastBatCrit) return;

        _lastBatPct  = battery.percent;
        _lastBatCrit = battery.critical;

        uint16_t col = battery.critical ? TFT_RED :
                       (battery.percent > 50 ? 0x877F :
                       (battery.percent > 20 ? TFT_YELLOW : TFT_RED));

        sprBat.createSprite(SPR_BAT_W, SPR_BAT_H);
        sprBat.fillSprite(TFT_BLACK);

        // Pil ikonu (36x16)
        sprBat.drawRect(0, 1, 36, 14, col);
        sprBat.fillRect(36, 5, 4, 6, col);  // kutup tokmagi
        sprBat.fillRect(2, 3, 32, 10, TFT_BLACK);  // ic siyah
        int16_t fw = (32 * battery.percent) / 100;
        if (fw > 0) sprBat.fillRect(2, 3, fw, 10, col);

        // Kritik yanip sonme
        if (battery.critical && ((millis() / 500) % 2 == 0))
            sprBat.fillRect(2, 3, 32, 10, TFT_BLACK);

        // Yuzde metni
        sprBat.setTextSize(1);
        sprBat.setTextColor(col);
        sprBat.setCursor(42, 5);
        sprBat.printf("%3d%%", battery.percent);

        sprBat.pushSprite(SCREEN_W - SPR_BAT_W - 4, 3);
        sprBat.deleteSprite();
    }

    // Sicaklik (0.1 derece hassasiyette degisince)
    void _updateTempSprite() {
        if (!dhtSensor.valid) return;
        // 0.1 hassasiyette karsilastir
        int16_t cur = (int16_t)(dhtSensor.temperature * 10);
        int16_t lst = (int16_t)(_lastTemp * 10);
        if (cur == lst) return;
        _lastTemp = dhtSensor.temperature;

        sprTemp.createSprite(SPR_SENSOR_W, SPR_SENSOR_H);
        sprTemp.fillSprite(TFT_BLACK);
        sprTemp.setTextSize(2);
        sprTemp.setTextColor(TFT_YELLOW);
        sprTemp.setCursor(0, 0);
        sprTemp.printf("%.1f C", dhtSensor.temperature);
        sprTemp.pushSprite(78, 130);
        sprTemp.deleteSprite();
    }

    // Nem (1% degisince)
    void _updateHumSprite() {
        if (!dhtSensor.valid) return;
        int16_t cur = (int16_t)dhtSensor.humidity;
        int16_t lst = (int16_t)_lastHum;
        if (cur == lst) return;
        _lastHum = dhtSensor.humidity;

        sprHum.createSprite(SPR_SENSOR_W, SPR_SENSOR_H);
        sprHum.fillSprite(TFT_BLACK);
        sprHum.setTextSize(2);
        sprHum.setTextColor(TFT_YELLOW);
        sprHum.setCursor(0, 0);
        sprHum.printf("%.0f%%", dhtSensor.humidity);
        sprHum.pushSprite(78, 158);
        sprHum.deleteSprite();
    }

    // Voltaj (0.01V degisince)
    void _updateVoltSprite() {
        if (!battery.valid) return;
        int16_t cur = (int16_t)(battery.voltage * 100);
        int16_t lst = (int16_t)(_lastVolt * 100);
        if (cur == lst) return;
        _lastVolt = battery.voltage;

        sprVolt.createSprite(SPR_VOLT_W, SPR_VOLT_H);
        sprVolt.fillSprite(TFT_BLACK);
        sprVolt.setTextSize(1);
        sprVolt.setTextColor(battery.critical ? TFT_RED : 0x4208);
        sprVolt.setCursor(0, 0);
        sprVolt.printf("%.2fV", battery.voltage);
        sprVolt.pushSprite(SCREEN_W - SPR_VOLT_W - 4, 200);
        sprVolt.deleteSprite();

        if (battery.critical) {
            tft.setTextSize(1);
            tft.setTextColor(TFT_RED, TFT_BLACK);
            tft.setCursor(SCREEN_W - 80, 215);
            tft.print("LOW BATTERY!");
        }
    }

    // Ikincil konum hava metni
    void _updateSecWeatherText(const WeatherDay* w) {
        if (!w) return;
        tft.setTextSize(2);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setCursor(10, 130);
        tft.printf("%.0f C  %-16s", w->tempCurrent, w->desc);
    }

    // Hava ikonu - sprite ile (animasyon icin her frame)
    void _updateWeatherIconSprite(uint8_t condId,
                                   int16_t x, int16_t y,
                                   int16_t w, int16_t h,
                                   uint16_t bgCol) {
        TFT_eSprite s{&tft};
        s.createSprite(w, h);
        s.fillSprite(bgCol);

        int16_t cx = w/2, cy = h/2, r = min(w,h)/3;
        uint32_t phase = (millis() / 100) % 20;

        switch (condId) {
            case 8:
                s.fillCircle(cx, cy, r, TFT_YELLOW);
                for (int a = 0; a < 360; a += 45) {
                    float rad = (a + phase*4) * PI / 180.0f;
                    s.drawLine(cx+(r+3)*cos(rad), cy+(r+3)*sin(rad),
                               cx+(r+8)*cos(rad), cy+(r+8)*sin(rad), TFT_YELLOW);
                }
                break;
            case 7:
                for (int i = 0; i < 4; i++)
                    s.drawFastHLine(5, 15+i*12+(phase%4), w-10, TFT_LIGHTGREY);
                break;
            case 6:
                s.fillCircle(cx, cy-5, r-5, TFT_WHITE);
                for (int i = 0; i < 5; i++) {
                    int sx=(i*w/5)+4, sy=10+((phase+i*3)%(h-20));
                    s.drawPixel(sx, sy, TFT_WHITE);
                    s.drawPixel(sx+1, sy, TFT_WHITE);
                }
                break;
            case 5: case 3:
                s.fillCircle(cx, cy-10, r-5, TFT_LIGHTGREY);
                for (int i = 0; i < 5; i++) {
                    int rx=(i*w/5)+4, ry=20+((phase*3+i*5)%(h-30));
                    s.drawLine(rx, ry, rx-2, ry+6, TFT_CYAN);
                }
                break;
            case 2:
                s.fillCircle(cx, cy-10, r-5, TFT_DARKGREY);
                s.drawLine(cx, cy-5, cx-6, cy+5, TFT_YELLOW);
                s.drawLine(cx-6, cy+5, cx+2, cy+5, TFT_YELLOW);
                break;
            default:
                s.fillCircle(cx-8, cy+5, r-8, TFT_LIGHTGREY);
                s.fillCircle(cx+5, cy+5, r-5, TFT_LIGHTGREY);
                s.fillCircle(cx,   cy-3,  r-7, TFT_WHITE);
                break;
        }
        s.pushSprite(x, y);
        s.deleteSprite();
    }
};

extern DisplayRenderer display;
