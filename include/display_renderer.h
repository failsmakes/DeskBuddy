#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>
#include "config.h"
#include "app_state.h"
#include "weather.h"
#include "alarm_manager.h"
#include "dht20_sensor.h"
#include "time_manager.h"
#include "battery.h"

// ============================================================
//  display_renderer.h  –  All screen drawing routines
//  Uses TFT_eSPI (double-buffered where needed via sprites)
// ============================================================

// ---- Minimal DeskBuddy logo (16×16 monochrome C-array) ------
// Replace this with your actual 1-bit logo XBM/C-array.
// Format: 1 = foreground pixel, 0 = background pixel
// Each byte = 8 pixels, LSB first
static const uint8_t LOGO_BITMAP[] PROGMEM = {
    0b00111100, 0b00000000,
    0b01111110, 0b00000000,
    0b11000011, 0b00000000,
    0b11011011, 0b00000000,
    0b11111111, 0b00000000,
    0b11011011, 0b00000000,
    0b11000011, 0b00000000,
    0b01111110, 0b00000000,
    0b00111100, 0b00000000,
    0b00011000, 0b00000000,
    0b00111100, 0b00000000,
    0b01111110, 0b00000000,
    0b01111110, 0b00000000,
    0b00111100, 0b00000000,
    0b00011000, 0b00000000,
    0b00000000, 0b00000000
};
#define LOGO_W 16
#define LOGO_H 16
#define LOGO_SCALE 8   // render at 128×128

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
    void drawLogo() {
        tft.fillScreen(LOGO_BG_COLOR);
        int16_t x0 = (SCREEN_W  - LOGO_W * LOGO_SCALE) / 2;
        int16_t y0 = (SCREEN_H - LOGO_H * LOGO_SCALE) / 2;
        for (int row = 0; row < LOGO_H; row++) {
            uint8_t byte0 = pgm_read_byte(&LOGO_BITMAP[row * 2]);
            uint8_t byte1 = pgm_read_byte(&LOGO_BITMAP[row * 2 + 1]);
            uint16_t rowBits = ((uint16_t)byte1 << 8) | byte0;
            for (int col = 0; col < LOGO_W; col++) {
                uint16_t colour = (rowBits & (1 << col)) ? LOGO_FG_COLOR : LOGO_BG_COLOR;
                tft.fillRect(x0 + col * LOGO_SCALE, y0 + row * LOGO_SCALE,
                             LOGO_SCALE, LOGO_SCALE, colour);
            }
        }
        // Centered project name
        tft.setTextColor(LOGO_FG_COLOR, LOGO_BG_COLOR);
        tft.setTextSize(2);
        tft.setCursor(SCREEN_W / 2 - 58, SCREEN_H / 2 + LOGO_H * LOGO_SCALE / 2 + 8);
        tft.print("DeskBuddy");
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
        else if (pct > 50) barColor = TFT_GREEN;
        else if (pct > 20) barColor = TFT_YELLOW;
        else               barColor = TFT_RED;

        // Dis cerceve
        tft.drawRect(x, y, w, h, TFT_WHITE);

        // Ucundaki kucuk kutup tokmagi (+)
        int16_t tipW = 4;
        int16_t tipH = h / 3;
        int16_t tipX = x + w;
        int16_t tipY = y + (h - tipH) / 2;
        tft.fillRect(tipX, tipY, tipW, tipH, TFT_WHITE);

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
        else if (pct > 50) col = TFT_GREEN;
        else if (pct > 20) col = TFT_YELLOW;
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
            if (dht20.valid) {
                tft.setTextSize(2);
                tft.setTextColor(TFT_YELLOW, TFT_BLACK);
                tft.setCursor(10, 130);
                tft.printf("Temp: %.1f C", dht20.temperature);
                tft.setCursor(10, 158);
                tft.printf("Hum:  %.0f%%", dht20.humidity);
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

    // ─── Alarm sub-screens ────────────────────────────────────
    void drawAlarmScreen(uint8_t h, uint8_t m, bool enabled) {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.setTextSize(2);
        tft.setCursor(10, 10);
        tft.print("ALARM");
        if (enabled) {
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.print(" [ON]");
        } else {
            tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
            tft.print(" [OFF]");
        }
        tft.setTextSize(4);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setCursor(60, 80);
        tft.printf("%02d:%02d", h, m);
        tft.setTextSize(1);
        tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
        tft.setCursor(10, 200);
        tft.print("+/- adjust  OK enable  Long=dismiss");
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
