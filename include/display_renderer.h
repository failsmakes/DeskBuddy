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
//  display_renderer.h  –  All screen drawing routines
//  Uses TFT_eSPI (double-buffered where needed via sprites)
// ============================================================

class DisplayRenderer {
public:
    TFT_eSPI   tft;
    TFT_eSprite spr{&tft};

    void begin() {
        tft.init();
        tft.setRotation(1);  // landscape 320×240
        tft.fillScreen(TFT_BLACK);
    }

    // ─── Logo screen ──────────────────────────────────────────
    // Format: yatay, 1 bit/piksel, MSB first (image2cpp varsayilani)
    // LOGO_SCALE=1 icin en hizli yontem:
    //   Her satiri okuyup 2 renk buffer'a yaz, setWindow+pushColors ile gonder
    //   40000 piksel icin ~50ms (drawPixel ile ~400ms olurdu)
    void drawLogo() {
        tft.fillScreen(LOGO_BG_COLOR);

        const int bytesPerRow = (LOGO_W + 7) / 8;  // 200px -> 25 byte/satir

        // Ekranda ortala, sinir disi giderse klamp et
        int16_t x0 = (SCREEN_W  - LOGO_W * LOGO_SCALE) / 2;
        int16_t y0 = (SCREEN_H - LOGO_H * LOGO_SCALE) / 2;
        if (x0 < 0) x0 = 0;
        if (y0 < 0) y0 = 0;

        if (LOGO_SCALE == 1) {
            // --- Hizli yol: satir basina setWindow + pushColor ---
            // Her satir icin: bitmap'ten 25 byte oku, 200 RGB565 piksele donustur, tek seferde gonder
            uint16_t lineBuf[LOGO_W];

            for (int row = 0; row < LOGO_H; row++) {
                // Satirin 200 pikselini RGB565 buffer'a donustur
                for (int col = 0; col < LOGO_W; col++) {
                    int     byteIdx = row * bytesPerRow + (col >> 3);
                    uint8_t b       = pgm_read_byte(&LOGO_BITMAP[byteIdx]);
                    bool    pixel   = (b >> (7 - (col & 7))) & 0x01;
                    lineBuf[col]    = pixel ? LOGO_FG_COLOR : LOGO_BG_COLOR;
                }
                // TFT_eSPI: setWindow sonrasi pushColors ile tum satiri tek seferde yaz
                tft.setAddrWindow(x0, y0 + row, x0 + LOGO_W - 1, y0 + row);
                tft.pushColors(lineBuf, LOGO_W, true);
            }
        } else {
            // Buyutme modu: fillRect ile (yavas ama buyutme nadiren gerekli)
            for (int row = 0; row < LOGO_H; row++) {
                for (int col = 0; col < LOGO_W; col++) {
                    int     byteIdx = row * bytesPerRow + (col >> 3);
                    uint8_t b       = pgm_read_byte(&LOGO_BITMAP[byteIdx]);
                    bool    pixel   = (b >> (7 - (col & 7))) & 0x01;
                    uint16_t colour = pixel ? LOGO_FG_COLOR : LOGO_BG_COLOR;
                    tft.fillRect(x0 + col * LOGO_SCALE,
                                 y0 + row * LOGO_SCALE,
                                 LOGO_SCALE, LOGO_SCALE, colour);
                }
            }
        }

        // Proje adini logonun altina yaz (yer varsa)
        int16_t textY = y0 + LOGO_H * LOGO_SCALE + 4;
        if (textY + 16 < SCREEN_H) {
            tft.setTextColor(LOGO_FG_COLOR, LOGO_BG_COLOR);
            tft.setTextSize(2);
            tft.setCursor(SCREEN_W / 2 - 58, textY);
            tft.print("DeskBuddy");
        }
    }

    // =========================================================
    //  Batarya ikonu + yuzde gosterimi
    //
    //  Ikon anatomisi (klasik pil sembolu):
    //   +--[+]-+
    //   |      |   <-- dis cerceve
    //   | #### |   <-- doluluk cubugu
    //   +------+
    //
    //  x,y: ikonun sol ust kosesi
    //  w,h: dis cercevenin genisligi ve yuksekligi
    // =========================================================
    void drawBatteryIcon(int16_t x, int16_t y, int16_t w, int16_t h,
                         uint8_t pct, bool critical) {
        // Renk secimi
        uint16_t barColor;
        if (critical)    barColor = TFT_RED;
        else if (pct > 50) barColor = 0x877F; //TFT_GREEN;
        else if (pct > 20) barColor = 0x877F; //TFT_YELLOW;
        else               barColor = TFT_RED;

        // Dis cerceve
        tft.drawRect(x, y, w, h, 0x877F); //TFT_WHITE);

        // Ucundaki kucuk kutup tokmagi (+)
        int16_t tipW = 4;
        int16_t tipH = h / 3;
        int16_t tipX = x + w;
        int16_t tipY = y + (h - tipH) / 2;
        tft.fillRect(tipX, tipY, tipW, tipH, 0x877F); //TFT_WHITE);

        // Ic doluluk alani
        int16_t innerX = x + 2;
        int16_t innerY = y + 2;
        int16_t innerW = w - 4;
        int16_t innerH = h - 4;

        // Once ici siyah yap
        tft.fillRect(innerX, innerY, innerW, innerH, TFT_BLACK);

        // Dolu kismi ciz
        int16_t fillW = (int16_t)((innerW * pct) / 100);
        if (fillW > 0) {
            tft.fillRect(innerX, innerY, fillW, innerH, barColor);
        }

        // Kritik modda yanip sonme efekti (millis cift/tek)
        if (critical && ((millis() / 500) % 2 == 0)) {
            tft.fillRect(innerX, innerY, innerW, innerH, TFT_BLACK);
        }
    }

    // Batarya yuzdesini metin olarak cizer (%XX)
    void drawBatteryText(int16_t x, int16_t y, uint8_t pct, bool critical) {
        uint16_t col;
        if (critical)      col = TFT_RED;
        else if (pct > 50) col = 0x877F; //TFT_GREEN;
        else if (pct > 20) col = 0x877F; //TFT_YELLOW;
        else               col = TFT_RED;

        tft.setTextSize(1);
        tft.setTextColor(col, TFT_BLACK);
        char buf[7];
        snprintf(buf, sizeof(buf), "%3d%%", pct);
        tft.setCursor(x, y);
        tft.print(buf);
    }

    // Tam batarya widget'i: ikon + yuzde metni yan yana
    // x,y: sol ust, iconW/iconH: ikon boyutu
    void drawBatteryWidget(int16_t x, int16_t y,
                           int16_t iconW = 36, int16_t iconH = 16) {
        if (!battery.valid) return;
        drawBatteryIcon(x, y, iconW, iconH, battery.percent, battery.critical);
        drawBatteryText(x + iconW + 6, y + (iconH / 2) - 4,
                        battery.percent, battery.critical);
    }

    // ─── Date/Time + DHT20 + Batarya ekrani ──────────────────
    void drawDateTime(struct tm& t, bool showSec = false,
                      const char* secCity = nullptr,
                      const WeatherDay* secWeather = nullptr) {
        tft.fillScreen(TFT_BLACK);

        if (!showSec) {
            // ── Ana ekran: Saat | Tarih | Sicaklik/Nem | Batarya ──

            // Batarya widget'i -- sag ust kose
            // Ikon: 36x16 px, pozisyon: sag ust koseden 4px bosluk
            // Yuzde metni yaninda (1px textsize = 6px karakter genisligi)
            drawBatteryWidget(SCREEN_W - 36 - 6 - 24 - 4, 4, 36, 16);

            // Saat -- buyuk font, ortada
            char timeBuf[9];
            snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d",
                     t.tm_hour, t.tm_min, t.tm_sec);
            tft.setTextColor(TFT_CYAN, TFT_BLACK);
            tft.setTextSize(4);
            tft.setCursor(16, 30);
            tft.print(timeBuf);

            // Tarih -- orta buyuklukte, saatin altinda
            char dateBuf[11];
            snprintf(dateBuf, sizeof(dateBuf), "%02d.%02d.%04d",
                     t.tm_mday, t.tm_mon + 1, t.tm_year + 1900);
            tft.setTextSize(2);
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            tft.setCursor(72, 94);
            tft.print(dateBuf);

            // Yatay ayirici cizgi
            tft.drawFastHLine(0, 118, SCREEN_W, TFT_DARKGREY);

            // Sicaklik ve nem -- alt sol
            if (dhtSensor.valid) {
                tft.setTextSize(2);
                tft.setTextColor(TFT_YELLOW, TFT_BLACK);
                tft.setCursor(10, 130);
                tft.printf("Temp: %.1f C", dhtSensor.temperature);
                tft.setCursor(10, 158);
                tft.printf("Hum:  %.0f%%", dhtSensor.humidity);
            }

            // Batarya voltaji -- alt sag
            if (battery.valid) {
                tft.setTextSize(1);
                uint16_t col = battery.critical ? TFT_RED : TFT_DARKGREY;
                tft.setTextColor(col, TFT_BLACK);
                tft.setCursor(SCREEN_W - 60, 200);
                tft.printf("%.2fV", battery.voltage);

                // Kritik uyari
                if (battery.critical) {
                    tft.setTextSize(1);
                    tft.setTextColor(TFT_RED, TFT_BLACK);
                    tft.setCursor(SCREEN_W - 80, 215);
                    tft.print("LOW BATTERY!");
                }
            }

        } else {
            // ── Ikincil konum ekrani: Sehir | Saat | Hava + Batarya ──

            // Batarya widget'i -- sag ust kose (kucuk)
            drawBatteryWidget(SCREEN_W - 36 - 6 - 24 - 4, 4, 30, 14);

            tft.setTextColor(TFT_CYAN, TFT_BLACK);
            tft.setTextSize(2);
            tft.setCursor(10, 10);
            tft.print(secCity ? secCity : "Secondary");

            char timeBuf[9];
            snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d",
                     t.tm_hour, t.tm_min);
            tft.setTextSize(4);
            tft.setCursor(40, 50);
            tft.print(timeBuf);

            if (secWeather) {
                tft.setTextSize(2);
                tft.setTextColor(TFT_WHITE, TFT_BLACK);
                tft.setCursor(10, 130);
                tft.printf("%.0f C  %s", secWeather->tempCurrent, secWeather->desc);
            }
        }
    }

    // ─── Current weather screen ───────────────────────────────
    void drawWeatherCurrent(const WeatherDay& w, const char* city) {
        tft.fillScreen(TFT_NAVY);

        // City name
        tft.setTextColor(TFT_CYAN, TFT_NAVY);
        tft.setTextSize(2);
        tft.setCursor(8, 8);
        tft.print(city);

        // Animated icon drawn in a sprite
        drawWeatherIcon(w.conditionId, 8, 35, 80, 80);

        // Temperature
        tft.setTextSize(4);
        tft.setTextColor(TFT_WHITE, TFT_NAVY);
        tft.setCursor(100, 40);
        tft.printf("%.0f", w.tempCurrent);
        tft.setTextSize(2);
        tft.setCursor(175, 38);
        tft.print("°C");

        // Description
        tft.setTextSize(2);
        tft.setTextColor(TFT_LIGHTGREY, TFT_NAVY);
        tft.setCursor(100, 90);
        tft.print(w.desc);

        // Min/Max
        tft.setTextSize(2);
        tft.setTextColor(TFT_YELLOW, TFT_NAVY);
        tft.setCursor(100, 130);
        tft.printf("L:%.0f  H:%.0f", w.tempMin, w.tempMax);

        // Humidity
        tft.setTextSize(2);
        tft.setTextColor(TFT_WHITE, TFT_NAVY);
        tft.setCursor(100, 158);
        tft.printf("Hum: %d%%", w.humidity);
    }

    // ─── 5-day forecast ───────────────────────────────────────
    void drawForecast(const WeatherDay forecast[5]) {
        tft.fillScreen(TFT_NAVY);
        tft.setTextSize(1);
        tft.setTextColor(TFT_CYAN, TFT_NAVY);
        tft.setCursor(100, 4);
        tft.print("5-Day Forecast");

        uint16_t colW = SCREEN_W / 5;  // 64px each
        const char* dayNames[] = { "Day1","Day2","Day3","Day4","Day5" };

        for (int i = 0; i < 5; i++) {
            int x = i * colW;
            // Divider
            if (i > 0) tft.drawFastVLine(x, 20, SCREEN_H - 20, TFT_DARKGREY);

            // Small weather icon
            drawWeatherIcon(forecast[i].conditionId, x + 8, 24, colW - 16, 60);

            // Temps
            tft.setTextSize(1);
            tft.setTextColor(TFT_WHITE, TFT_NAVY);
            tft.setCursor(x + 4, 92);
            tft.printf("H%.0f", forecast[i].tempMax);
            tft.setCursor(x + 4, 106);
            tft.setTextColor(TFT_CYAN, TFT_NAVY);
            tft.printf("L%.0f", forecast[i].tempMin);

            // Day label
            tft.setTextColor(TFT_YELLOW, TFT_NAVY);
            tft.setCursor(x + 4, 120);
            tft.print(dayNames[i]);
        }
    }

    // ─── Alarm ekrani - 3 alarm yuvasi ───────────────────────
    // selectedIdx: su an duzenlenen yuva (0-2), vurgulu gosterilir
    void drawAlarmScreen(const AlarmEntry alarms[], uint8_t selectedIdx) {
        tft.fillScreen(TFT_BLACK);

        // Baslik
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.setTextSize(2);
        tft.setCursor(10, 6);
        tft.print("ALARM");

        // Her alarm yuvasi icin satir
        // 3 satir: y = 38, 90, 142  (52px aralik)
        for (uint8_t i = 0; i < MAX_ALARMS; i++) {
            const AlarmEntry& a = alarms[i];
            int16_t y = 38 + i * 52;
            bool selected = (i == selectedIdx);

            // Secili satirin arkaplanini vurgula
            if (selected) {
                tft.fillRect(0, y - 4, SCREEN_W, 48, 0x1082);  // koyu mavi
            }

            // Alarm no
            tft.setTextSize(1);
            tft.setTextColor(selected ? TFT_CYAN : TFT_DARKGREY, selected ? 0x1082 : TFT_BLACK);
            tft.setCursor(8, y);
            tft.printf("ALM%d", i + 1);

            // Saat:Dakika - buyuk font
            tft.setTextSize(3);
            uint16_t timeCol;
            if (a.firing)        timeCol = TFT_RED;
            else if (a.enabled)  timeCol = TFT_WHITE;
            else                 timeCol = 0x4208;  // koyu gri
            tft.setTextColor(timeCol, selected ? 0x1082 : TFT_BLACK);
            tft.setCursor(48, y - 4);
            tft.printf("%02d:%02d", a.hour, a.minute);

            // Durum badge
            tft.setTextSize(1);
            if (a.firing) {
                tft.setTextColor(TFT_RED, selected ? 0x1082 : TFT_BLACK);
                tft.setCursor(200, y);
                tft.print("CALIYOR!");
            } else if (a.snoozed) {
                tft.setTextColor(TFT_YELLOW, selected ? 0x1082 : TFT_BLACK);
                tft.setCursor(200, y);
                tft.print("ERTELENDI");
            } else {
                // ON/OFF toggle gostergesi
                int16_t bx = 200, by = y - 2;
                int16_t bw = 44, bh = 18;
                if (a.enabled) {
                    tft.fillRoundRect(bx, by, bw, bh, 9, TFT_GREEN);
                    tft.setTextColor(TFT_BLACK, TFT_GREEN);
                    tft.setCursor(bx + 8, by + 5);
                    tft.print("ACIK");
                } else {
                    tft.drawRoundRect(bx, by, bw, bh, 9, TFT_DARKGREY);
                    tft.setTextColor(TFT_DARKGREY, selected ? 0x1082 : TFT_BLACK);
                    tft.setCursor(bx + 4, by + 5);
                    tft.print("KAPALI");
                }
            }

            // Satir alt cizgisi (son satir haric)
            if (i < MAX_ALARMS - 1) {
                tft.drawFastHLine(0, y + 44, SCREEN_W, 0x2104);
            }
        }

        // Alt yardim satiri
        tft.setTextSize(1);
        tft.setTextColor(0x4208, TFT_BLACK);
        tft.setCursor(4, 224);
        tft.print("+/-:saat  OK:ac/kapat  UZUN:alt ekran");
    }

    void drawCountdownScreen(uint32_t leftMs, bool running) {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_ORANGE, TFT_BLACK);
        tft.setTextSize(2);
        tft.setCursor(10, 10);
        tft.print("COUNTDOWN");
        tft.setTextSize(4);
        tft.setTextColor(running ? TFT_GREEN : TFT_WHITE, TFT_BLACK);
        tft.setCursor(20, 80);
        uint32_t s = leftMs / 1000;
        tft.printf("%02lu:%02lu:%02lu", (unsigned long)(s/3600),
                   (unsigned long)((s%3600)/60), (unsigned long)(s%60));
        tft.setTextSize(1);
        tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
        tft.setCursor(10, 200);
        tft.print("+/- set  OK start/stop  Long=reset");
    }

    void drawStopwatchScreen(uint32_t elapsedMs, bool running) {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.setTextSize(2);
        tft.setCursor(10, 10);
        tft.print("STOPWATCH");
        tft.setTextSize(3);
        tft.setTextColor(running ? TFT_GREEN : TFT_WHITE, TFT_BLACK);
        uint32_t s = elapsedMs / 1000;
        uint32_t ms = (elapsedMs % 1000) / 10;
        tft.setCursor(20, 85);
        tft.printf("%02lu:%02lu.%02lu", (unsigned long)(s/60),
                   (unsigned long)(s%60), (unsigned long)ms);
        tft.setTextSize(1);
        tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
        tft.setCursor(10, 200);
        tft.print("OK start/stop  Long=reset");
    }

    // ─── AP mode splash ───────────────────────────────────────
    void drawAPSplash(const char* apSSID, const char* apIP) {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.setTextSize(2);
        tft.setCursor(20, 20);
        tft.print("WiFi Setup Mode");
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextSize(1);
        tft.setCursor(10, 60);
        tft.print("Connect to WiFi:");
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.setTextSize(2);
        tft.setCursor(10, 80);
        tft.print(apSSID);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextSize(1);
        tft.setCursor(10, 115);
        tft.print("Pass: deskbuddy123");
        tft.setCursor(10, 135);
        tft.print("Then open browser:");
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setTextSize(2);
        tft.setCursor(10, 155);
        tft.print(apIP);
    }

    // ─── Simple animated weather icon ────────────────────────
    // conditionId is first digit of OWM id (2=storm,3=drizzle,
    //   5=rain,6=snow,7=fog,8=cloud/clear)
    // Drawn as simple geometric animation-ready shapes
    void drawWeatherIcon(uint8_t conditionId, int16_t x, int16_t y,
                         int16_t w, int16_t h) {
        tft.fillRect(x, y, w, h, TFT_NAVY);  // clear area
        int16_t cx = x + w / 2;
        int16_t cy = y + h / 2;
        int16_t r  = min(w, h) / 3;

        // Animate with millis phase
        uint32_t phase = (millis() / 100) % 20;

        switch (conditionId) {
            case 8:  // Clear (800) or clouds (801-804)
                // Sun
                tft.fillCircle(cx, cy, r, TFT_YELLOW);
                // Rays
                for (int a = 0; a < 360; a += 45) {
                    float rad = (a + phase * 4) * PI / 180.0f;
                    int x1 = cx + (r + 3) * cos(rad);
                    int y1 = cy + (r + 3) * sin(rad);
                    int x2 = cx + (r + 8) * cos(rad);
                    int y2 = cy + (r + 8) * sin(rad);
                    tft.drawLine(x1, y1, x2, y2, TFT_YELLOW);
                }
                break;

            case 7:  // Fog
                for (int i = 0; i < 4; i++) {
                    tft.drawFastHLine(x + 5, y + 15 + i * 12 + (phase % 4),
                                      w - 10, TFT_LIGHTGREY);
                }
                break;

            case 6:  // Snow
                tft.fillCircle(cx, cy - 5, r - 5, TFT_WHITE);
                for (int i = 0; i < 5; i++) {
                    int sx = x + (i * w / 5) + 4;
                    int sy = y + 10 + ((phase + i * 3) % (h - 20));
                    tft.drawPixel(sx, sy, TFT_WHITE);
                    tft.drawPixel(sx + 1, sy, TFT_WHITE);
                    tft.drawPixel(sx, sy + 1, TFT_WHITE);
                }
                break;

            case 5:  // Rain
            case 3:  // Drizzle
                tft.fillCircle(cx, cy - 10, r - 5, TFT_LIGHTGREY);
                for (int i = 0; i < 5; i++) {
                    int rx = x + (i * w / 5) + 4;
                    int ry = y + 20 + ((phase * 3 + i * 5) % (h - 30));
                    tft.drawLine(rx, ry, rx - 2, ry + 6, TFT_CYAN);
                }
                break;

            case 2:  // Thunderstorm
                tft.fillCircle(cx, cy - 10, r - 5, TFT_DARKGREY);
                // Lightning bolt
                tft.drawLine(cx, cy - 5, cx - 6, cy + 5, TFT_YELLOW);
                tft.drawLine(cx - 6, cy + 5, cx + 2, cy + 5, TFT_YELLOW);
                tft.drawLine(cx + 2, cy + 5, cx - 4, cy + 18, TFT_YELLOW);
                break;

            default: // Clouds
                tft.fillCircle(cx - 8, cy + 5, r - 8, TFT_LIGHTGREY);
                tft.fillCircle(cx + 5, cy + 5, r - 5, TFT_LIGHTGREY);
                tft.fillCircle(cx,     cy - 3, r - 7, TFT_WHITE);
                break;
        }
    }
};

extern DisplayRenderer display;
