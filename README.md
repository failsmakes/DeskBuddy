# DeskBuddy

Wemos Lolin D32 tabanli masaustu arkadasi. ILI9341 TFT, DHT11/22 sicaklik & nem sensoru, DS3231 RTC (ZS-042), kapasitif dokunmatik padler, buzzer, batarya olcumu ve web arayuzu ile tam ozellikli.

---

## Donanim Baglantilari

### ILI9341 TFT (SPI)
| TFT Pin | D32 Pin | Aciklama |
|---------|---------|----------|
| VCC     | 3.3V    | Guc |
| GND     | GND     | Toprak |
| CS      | GPIO 5  | Chip Select |
| RESET   | GPIO 17 | Reset |
| DC/RS   | GPIO 16 | Data/Command |
| MOSI    | GPIO 23 | SPI MOSI |
| SCK     | GPIO 18 | SPI Clock |
| LED/BL  | GPIO 32 | Arka isik (uyku modunda kapanir) |

### DHT11 / DHT22 Sicaklik & Nem Sensoru (One-Wire)
I2C KULLANILMAZ. Tek data pini yeterli.

| DHT Pin | D32 Pin | Aciklama |
|---------|---------|----------|
| VCC     | 3.3V    | Guc |
| GND     | GND     | Toprak |
| DATA    | GPIO 26 | Tek kablo veri hatti |

Onemli: DATA pini ile VCC arasina 10 kOhm pull-up direnc ekleyin.

Sensor tipini config.h icinde secin:
    #define DHT_TYPE  DHT_TYPE_DHT22   // veya DHT_TYPE_DHT11

### DS3231 RTC + AT24C32 EEPROM (ZS-042 Modulu)
RTC, internet baglantisiniz olmasa bile cihazin dogru saati gostermesini saglar.

| ZS-042 Pin | D32 Pin | Aciklama |
|------------|---------|----------|
| VCC        | 3.3V    | Guc |
| GND        | GND     | Toprak |
| SDA        | GPIO 21 | I2C SDA |
| SCL        | GPIO 22 | I2C SCL |

Calisma mantigi:
- WiFi + NTP basarili: Saat NTP'den alinir, RTC'ye yazilir
- WiFi yoksa veya NTP basarisizsa: Saat RTC'den okunur (pil destekli hafiza)
- Her saat basinda NTP ile yenilenir, RTC guncellenir
ZS-042 uzerindeki CR2032 pil ile RTC, cihaz kapali olsa dahi saati tutar.

### Kapasitif Dokunmatik Padler (ESP32 Touch Pinleri)
| Pad           | D32 Pin  | Touch Kanal |
|---------------|----------|-------------|
| Ana Etkilesim | GPIO 4   | T0 |
| + (Artir)     | GPIO 15  | T3 |
| - (Azalt)     | GPIO 14  | T6 |
| OK / Enter    | GPIO 27  | T7 |

### Buzzer (Pasif)
| Buzzer Pin | D32 Pin |
|------------|---------|
| +          | GPIO 25 |
| -          | GND     |

### Lityum Iyon Batarya
Lolin D32 dahili sarj devresi (TP4054) ve JST konektoru. 3.7V Li-Ion pil dogrudan BAT konektorune baglanir. Voltaj GPIO35 ADC ile olculur (dahili voltaj bolucusu, ekstra baglanti gerekmez).

---

## Kurulum

### Gerekli Araclar
PlatformIO (VS Code eklentisi onerilir)

### Kutuphaneler
lib_deps otomatik indirir. Sadece RoboEyesTFT manuel kurulum gerektirir:

    mkdir -p lib
    git clone https://github.com/yousseftechdev/RoboEyesTFT lib/RoboEyesTFT

### Derleme ve Yukleme
    pio run -t upload

---

## Ilk Kullanim

1. Cihazi acin - DeskBuddy logosu 3 saniye gosterilir
2. Kayitli WiFi yoksa AP moduna gecer: SSID: DeskBuddy-Setup / Sifre: deskbuddy123
3. Telefondan bu aga baglanin, tarayicida 192.168.4.1 acin
4. WiFi bilgileri, sehirler ve OWM API anahtarini girin
5. Cihazi yeniden baslatın

---

## Modlar ve Kontrol

Kisa tik (Ana Pad): Bir sonraki moda gec
Uzun basma (Ana Pad): Moda ozgu islem

Mod Sirasi:
Buddy -> Tarih/Saat -> Hava (Ana) -> [Hava (Ikincil)]* -> Alarm -> Buddy...

* Ikincil konum web arayuzunde tanimlanmamissa bu mod tamamen atlanir.
  Kisa veya uzun basma ikincil konum ekranini tamamen atlar, bir sonraki moda gecer.

### Buddy Modu
Uzun basma ile mood: Default -> Happy -> Default -> Happy -> Default -> Happy -> Default -> Tired -> Angry -> Default...

### Tarih/Saat Modu
Saat, tarih, DHT sicaklik/nem, batarya gostergesi.
Uzun basma: Ikincil sehir saati + hava durumu (SADECE ikincil konum tanimli ise)

### Hava Durumu Modu (Ana / Ikincil)
Uzun basma: 5 gunluk tahmin

### Alarm / Geri Sayim / Kronometre
Uzun basma: Alt ekranlar arasi gecis
+/- padleri: Deger ayarla
OK padi: Baslat/durdur
OK uzun: Sifirla
Alarm calinirken: Kisa tik = 10 dk ertele, Uzun = iptal

---

## Uyku Modu (Derin Uyku)

- Son pad dokunusundan sonra ayarlanan sure gecince cihaz derin uykuya girer
- Ekran (GPIO32 BL) kapanir, ESP32 minimum guc tuketerek uyur
- Ana etkilesim padine (GPIO4 / T0) dokunulursa cihaz uyanir
- Uyanma sonrasi normal acilis rutini calisir; RTC sayesinde saat korunur
- Web arayuzunden ayarlanir: "Sleep / Uyku Modu" bolumu (saniye, 0 = devre disi)

---

## Web Arayuzu

http://<cihaz-ip> adresine tarayicidan erisin.

- WiFi Networks: Ag ekle/duzenle/sil (max 5)
- General Settings: Ana sehir, ikincil sehir (bos birakilabilir), OWM API anahtari, UTC ofseti
- Sleep: Hareketsizlik sonrasi uyku suresi (saniye, 0=devre disi)
- Alarm: Alarm saati ayarla / devreye al
- Countdown: Geri sayim baslat/durdur/sifirla
- Device Status: IP, WiFi durumu, heap, uptime

---

## Proje Yapisi

    DeskBuddy/
    +-- platformio.ini
    +-- src/
    |   +-- main.cpp
    +-- include/
    |   +-- config.h              <- Pin tanimlari, sabitler
    |   +-- storage.h             <- EEPROM okuma/yazma
    |   +-- rtc_manager.h         <- DS3231 RTC surucusu (ZS-042)
    |   +-- sleep_manager.h       <- ESP32 derin uyku yonetimi
    |   +-- wifi_manager.h        <- STA/AP mod yonetimi
    |   +-- time_manager.h        <- NTP + RTC fallback
    |   +-- dht_sensor.h          <- DHT11/DHT22 one-wire surucusu
    |   +-- touch_input.h         <- Kapasitif touch yonetimi
    |   +-- buzzer.h              <- Non-blocking buzzer
    |   +-- battery.h             <- Batarya voltaj olcumu
    |   +-- weather.h             <- OpenWeatherMap API
    |   +-- alarm_manager.h       <- Alarm/geri sayim/kronometre
    |   +-- app_state.h           <- Mod ve durum yonetimi
    |   +-- display_renderer.h    <- TFT ekran cizim rutinleri
    |   +-- web_server.h          <- Konfigurasyon web sunucusu
    |   +-- RoboEyesTFT_eSPI.h    <- RoboEyes (lib/'den kopyalanmis)
    +-- lib/
        +-- RoboEyesTFT/          <- Manuel kurulum

---

## Notlar

- GPIO 34, 35, 36, 39: sadece giris (input-only), touch icin kullanilamaz
- Batarya okuma: GPIO35 (ekstra baglanti gerekmez)
- DS3231 I2C adresi: 0x68 / AT24C32: 0x57
- DHT okuma araligi: DHT11 min 1sn, DHT22 min 2sn (kod otomatik handle eder)
- Deep sleep'te touch wakeup icin: touchSleepWakeUpEnable(4, 20) kullanilir
