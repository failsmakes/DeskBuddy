#pragma once

// ============================================================
//  DeskBuddy — config.h
//  Wemos Lolin D32 + ILI9341 TFT + DHT20 + Touch pads + Buzzer
// ============================================================

// ---------- Display (do NOT redefine TFT_WIDTH/TFT_HEIGHT here;
//  ILI9341_Defines.h inside TFT_eSPI already sets them) ----------
#define SCREEN_W   320
#define SCREEN_H   240

// ---------- Pin Definitions ----------
#define PIN_TOUCH_INTERACT  T0   // GPIO4  – main interaction pad
#define PIN_TOUCH_PLUS      T3   // GPIO15 – "+" pad
#define PIN_TOUCH_MINUS     T6   // GPIO14 – "-" pad
#define PIN_TOUCH_OK        T7   // GPIO27 – "OK / Enter" pad
#define PIN_BUZZER          25
#define PIN_DHT20_SDA       21   // I2C default SDA
#define PIN_DHT20_SCL       22   // I2C default SCL

// ---------- Touch Thresholds ----------
#define TOUCH_THRESHOLD        40    // below this → touched
#define TOUCH_LONG_PRESS_MS   800    // ms to qualify as long press
#define TOUCH_DEBOUNCE_MS      50

// ---------- Logo ----------
#define LOGO_DISPLAY_MS       3000   // how long to show logo on boot
#define LOGO_FG_COLOR         0x07FF // cyan-ish (565)
#define LOGO_BG_COLOR         0x0000 // black

// ---------- WiFi / EEPROM ----------
#define MAX_WIFI_NETWORKS      5
#define WIFI_SSID_LEN         32
#define WIFI_PASS_LEN         64
#define EEPROM_SIZE          512
#define EEPROM_MAGIC         0xDB  // 0xDB = DeskBuddy
#define WIFI_CONNECT_TIMEOUT 8000  // ms per network

// ---------- NTP ----------
#define NTP_SERVER1   "pool.ntp.org"
#define NTP_SERVER2   "time.nist.gov"
#define NTP_SYNC_INTERVAL_MS  3600000UL  // re-sync every hour

// ---------- Weather API (OpenWeatherMap) ----------
// Fill in your API key here or set via web UI
#define OWM_API_KEY       ""          // overridden by EEPROM value
#define OWM_BASE_URL      "http://api.openweathermap.org"
#define WEATHER_UPDATE_MS  600000UL   // 10 min

// ---------- Mood Eye Colours (RGB565) ----------
// Normal   #82EEFD  →  RGB(130,238,253)
#define EYE_COLOR_DEFAULT  0x877F
// Happy    #39FF14  →  RGB(57,255,20)
#define EYE_COLOR_HAPPY    0x3FE2
// Tired    #4682B4  →  RGB(70,130,180)
#define EYE_COLOR_TIRED    0x4416
// Angry    #FF2400  →  RGB(255,36,0)
#define EYE_COLOR_ANGRY    0xF900

// ---------- Alarm / Countdown ----------
#define ALARM_SNOOZE_MS   600000UL   // 10 minutes
#define BUZZER_FREQ_HZ    1000
#define BUZZER_ON_MS       200
#define BUZZER_OFF_MS      300
