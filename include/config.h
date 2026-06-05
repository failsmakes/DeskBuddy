#pragma once

// ============================================================
//  DeskBuddy - config.h
//  Wemos Lolin D32 + ILI9341 TFT + DHT11/22 + DS3231 RTC
//  + ZS-042 module + Touch pads + Buzzer
// ============================================================

// ---------- Display (do NOT redefine TFT_WIDTH/TFT_HEIGHT here;
//  ILI9341_Defines.h inside TFT_eSPI already sets them) ----------
#define SCREEN_W   320
#define SCREEN_H   240

// ---------- TFT Backlight Pin ----------
// Lolin D32 GPIO32 genellikle TFT BL icin kullanilir
#define PIN_TFT_BL          32

// ---------- Touch Pad Pins (ESP32 Capacitive Touch) ----------
#define PIN_TOUCH_INTERACT  T0   // GPIO4  - ana etkilesim padi
#define PIN_TOUCH_PLUS      T3   // GPIO15 - "+" padi
#define PIN_TOUCH_MINUS     T6   // GPIO14 - "-" padi
#define PIN_TOUCH_OK        T7   // GPIO27 - "OK / Enter" padi

// GPIO numaralari (deep sleep wakeup icin gerekli)
#define PIN_TOUCH_INTERACT_GPIO  4

// ---------- Buzzer ----------
#define PIN_BUZZER          25

// ---------- DHT11 / DHT22 Sensor (One-Wire) ----------
// I2C KULLANILMIYOR. Tek data pini yeterli.
// DATA pini ile VCC arasina 10kOhm pull-up direnc ekleyin.
#define PIN_DHT_DATA        26    // GPIO26 - DHT data pini
#define DHT_TYPE            DHT_TYPE_DHT22   // DHT11 veya DHT22

// ---------- DS3231 RTC + AT24C32 EEPROM (ZS-042 Modulu) ----------
// ZS-042 modulu I2C kullanir. DHT sensor ile ayni hat paylasimaz.
// RTC icin ayri I2C pinleri kullaniyoruz (Wire1 yerine Wire kullanilir
// cunku ESP32'de tek Wire objesi yeterli, adres catismasi yok).
#define PIN_RTC_SDA         21   // GPIO21 - I2C SDA (DS3231 + AT24C32)
#define PIN_RTC_SCL         22   // GPIO22 - I2C SCL (DS3231 + AT24C32)
// Not: DS3231 adresi 0x68, AT24C32 adresi 0x57

// ---------- Touch Thresholds ----------
#define TOUCH_THRESHOLD         40    // bu degerden dusuk = dokunuldu
#define TOUCH_SLEEP_THRESHOLD   20    // deep sleep wakeup esigi (daha duyarli)
#define TOUCH_LONG_PRESS_MS    800    // ms - uzun basma suresi
#define TOUCH_DEBOUNCE_MS       50

// ---------- Logo ----------
#define LOGO_DISPLAY_MS       3000   // logo gosterme suresi (ms)
#define LOGO_FG_COLOR         0x07FF // cyan (RGB565)
#define LOGO_BG_COLOR         0x0000 // siyah

// ---- DeskBuddy logo (16x16 monochrome 1-bit bitmap) ----
// image2cpp ile olusturuldu (javl.github.io/image2cpp)
// Format: Horizontal, 1 bit/pixel, LSB first
// Kendi logonuzla degistirin.
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
#define LOGO_SCALE 8   // 128x128 piksel olarak ciz

// ---------- WiFi / EEPROM ----------
#define MAX_WIFI_NETWORKS      5
#define WIFI_SSID_LEN         32
#define WIFI_PASS_LEN         64
#define EEPROM_MAGIC         0xDB  // 0xDB = DeskBuddy
#define WIFI_CONNECT_TIMEOUT 8000  // ms - ag basina baglanti zaman asimi

// ---------- NTP ----------
#define NTP_SERVER1   "pool.ntp.org"
#define NTP_SERVER2   "time.nist.gov"
#define NTP_SYNC_INTERVAL_MS  3600000UL  // 1 saatte bir yenile

// ---------- Weather API (OpenWeatherMap) ----------
#define OWM_API_KEY       ""          // web arayuzunden girilebilir
#define OWM_BASE_URL      "http://api.openweathermap.org"
#define WEATHER_UPDATE_MS  600000UL   // 10 dakika

// ---------- Mood Eye Colors (RGB565) ----------
// Normal  #82EEFD -> RGB(130,238,253)
#define EYE_COLOR_DEFAULT  0x877F
// Happy   #39FF14 -> RGB(57,255,20)
#define EYE_COLOR_HAPPY    0x3FE2
// Tired   #4682B4 -> RGB(70,130,180)
#define EYE_COLOR_TIRED    0x4416
// Angry   #FF2400 -> RGB(255,36,0)
#define EYE_COLOR_ANGRY    0xF900

// ---------- Alarm / Countdown ----------
#define ALARM_SNOOZE_MS   600000UL   // 10 dakika erteleme
#define BUZZER_FREQ_HZ    1000
#define BUZZER_ON_MS       200
#define BUZZER_OFF_MS      300

// ---------- Sleep / Uyku Modu ----------
#define SLEEP_TIMEOUT_DEFAULT_S  300  // 5 dakika varsayilan
