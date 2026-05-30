# DeskBuddy

Wemos Lolin D32 tabanlı masaüstü arkadaş. ILI9341 TFT, DHT20 sensör, dokunmatik padler, buzzer ve web arayüzü ile tam özellikli.

---

## Donanım Bağlantıları

### ILI9341 TFT (SPI)
| TFT Pin | D32 Pin | Açıklama |
|---------|---------|----------|
| VCC     | 3.3V    | Güç |
| GND     | GND     | Toprak |
| CS      | GPIO 5  | Chip Select |
| RESET   | GPIO 17 | Reset |
| DC/RS   | GPIO 16 | Data/Command |
| MOSI    | GPIO 23 | SPI MOSI |
| SCK     | GPIO 18 | SPI Clock |
| LED/BL  | GPIO 32 | Arka ışık (PWM) |
| MISO    | GPIO 19 | (opsiyonel, okuma için) |

### DHT20 (I2C)
| DHT20 Pin | D32 Pin | Açıklama |
|-----------|---------|----------|
| VCC       | 3.3V    | Güç |
| GND       | GND     | Toprak |
| SDA       | GPIO 21 | I2C SDA |
| SCL       | GPIO 22 | I2C SCL |

### Dokunmatik Padler (ESP32 Kapasitif Touch)
| Pad           | D32 Pin  | Touch Kanal |
|---------------|----------|-------------|
| Ana Etkileşim | GPIO 4   | T0 |
| + (Artır)     | GPIO 15  | T3 |
| - (Azalt)     | GPIO 14  | T6 |
| OK / Enter    | GPIO 27  | T7 |

> Padler için 3–5 cm² alüminyum folyo veya bakır bant kullanın.  
> Her pad ile ilgili GPIO arasına **1MΩ pull-down direnç** ekleyin (opsiyonel ama kararlılık için önerilir).

### Buzzer (Passive)
| Buzzer Pin | D32 Pin  |
|------------|----------|
| +          | GPIO 25  |
| -          | GND      |

### Lityum İyon Batarya
Wemos Lolin D32 üzerinde dahili şarj devresi (TP4054) ve JST konnektörü mevcuttur.  
3.7V Li-Ion pil doğrudan **BAT** konnektörüne bağlanır.

---

## Kurulum

### 1. Gerekli Araçlar
- [PlatformIO](https://platformio.org/) (VS Code eklentisi önerilir)
- veya Arduino IDE 2.x (ESP32 board paketi yüklü)

### 2. Kütüphaneler

**PlatformIO** kullanıyorsanız `platformio.ini` içindeki `lib_deps` otomatik indirir.  
Sadece **RoboEyesTFT** manuel kurulum gerektirir:

```bash
# Projenin kök dizinine git
cd deskbuddy

# lib/ klasörü yoksa oluştur
mkdir -p lib

# GitHub'dan indir
git clone https://github.com/yousseftechdev/RoboEyesTFT lib/RoboEyesTFT
```

**Arduino IDE** kullanıyorsanız şu kütüphaneleri Library Manager'dan kurun:
- `TFT_eSPI` (Bodmer)
- `ArduinoJson` (Benoit Blanchon)
- RoboEyesTFT'yi GitHub'dan zip olarak indirip "Add ZIP Library" ile ekleyin

### 3. TFT_eSPI Yapılandırması

PlatformIO kullanıyorsanız `platformio.ini` içindeki `build_flags` pin tanımlarını içerir — ekstra bir şey gerekmez.

Arduino IDE kullanıyorsanız `TFT_eSPI` kütüphane klasöründeki `User_Setup.h` dosyasını düzenleyin:

```cpp
#define ILI9341_DRIVER
#define TFT_MOSI  23
#define TFT_SCLK  18
#define TFT_CS     5
#define TFT_DC    16
#define TFT_RST   17
#define TFT_BL    32
#define TFT_BACKLIGHT_ON 1
#define SPI_FREQUENCY  40000000
```

### 4. OpenWeatherMap API Anahtarı

1. [openweathermap.org](https://openweathermap.org) adresinden ücretsiz hesap açın
2. API Keys bölümünden bir anahtar oluşturun
3. İlk boot'tan sonra web arayüzünden girin (bkz. aşağıdaki "Web Arayüzü" bölümü)

### 5. Derleme ve Yükleme

```bash
# PlatformIO CLI ile
pio run -t upload

# veya VS Code PlatformIO eklentisinden Upload butonuna tıklayın
```

---

## İlk Kullanım

1. Cihazı açın → **DeskBuddy** logosu 3 saniye görünür
2. Kayıtlı WiFi yoksa cihaz **AP moduna** geçer:
   - SSID: `DeskBuddy-Setup`
   - Şifre: `deskbuddy123`
3. Telefonunuzdan bu ağa bağlanın
4. Tarayıcıda `192.168.4.1` adresini açın
5. WiFi bilgilerinizi, şehirleri ve OWM API anahtarınızı girin
6. Cihazı yeniden başlatın → normal moda geçer

---

## Modlar ve Kontrol

| Kısa Tık (Ana Pad) | Mod değiştirme: Buddy → Tarih/Saat → Hava (Ana) → Hava (İkincil) → Alarm → ... |
|---|---|
| **Uzun Basış (Ana Pad)** | Moda özgü fonksiyon (aşağıya bak) |

### Buddy Modu
- Uzun basış ile mood sıralaması:  
  `Default → Happy → Default → Happy → Default → Happy → Default → Tired → Angry → Default → ...`
- Göz renkleri mood'a göre otomatik değişir

### Tarih/Saat Modu
- Ekran: Saat, tarih, oda sıcaklığı (DHT20), nem
- Uzun basış: İkincil şehir saati + anlık hava durumu

### Hava Durumu Modu (Ana / İkincil)
- Ekran: Anlık hava, ikon animasyonu, min/max sıcaklık
- Uzun basış: 5 günlük tahmin (sütun grafik)

### Alarm / Geri Sayım / Kronometre Modu
- Uzun basış ile alt ekranlar arası geçiş
- **+/−** padleri değer ayarlama
- **OK** padi başlatma/durdurma
- OK uzun basış: sıfırlama
- Alarm çalarken: Kısa tık = 10 dk ertele, Uzun = iptal

---

## Web Arayüzü

Cihaz STA veya AP modunda iken `http://<cihaz-ip>` adresini tarayıcıda açın.

- WiFi ağları ekleme/düzenleme/silme (maks. 5 ağ)
- Şehir tanımları (ana ve ikincil)
- OWM API anahtarı
- Saat dilimi (UTC offset)
- Alarm kurma / devreye alma
- Geri sayım başlatma/durdurma/sıfırlama

---

## Logo Değiştirme

`include/display_renderer.h` içindeki `LOGO_BITMAP` dizisini kendi 16×16 1-bit bitmap'inizle değiştirin.

Bitmap oluşturmak için:
- [image2cpp](https://javl.github.io/image2cpp/) — görüntüyü C array'e çevirir
- Format: **Horizontal**, **1 bit per pixel**, **LSB first**

Büyüklük ve ölçek için `LOGO_W`, `LOGO_H`, `LOGO_SCALE` sabitlerini ayarlayın.  
Renk için `LOGO_FG_COLOR` ve `LOGO_BG_COLOR` (RGB565) değerlerini `config.h`'den değiştirin.

---

## Proje Yapısı

```
deskbuddy/
├── platformio.ini
├── src/
│   └── main.cpp              ← setup() ve loop()
├── include/
│   ├── config.h              ← Tüm sabitler ve pin tanımları
│   ├── storage.h             ← EEPROM okuma/yazma
│   ├── wifi_manager.h        ← STA/AP modu yönetimi
│   ├── time_manager.h        ← NTP senkronizasyonu
│   ├── dht20_sensor.h        ← DHT20 I2C sürücüsü
│   ├── battery.h             ← Batarya voltaj/yüzde okuyucu (GPIO35)
│   ├── touch_input.h         ← Kapasitif touch yönetimi
│   ├── buzzer.h              ← Non-blocking buzzer
│   ├── weather.h             ← OpenWeatherMap API
│   ├── alarm_manager.h       ← Alarm/geri sayım/kronometre
│   ├── app_state.h           ← Mod ve durum tanımları
│   ├── display_renderer.h    ← TFT ekran çizim rutinleri
│   └── web_server.h          ← Konfigurasyon web sunucusu
└── lib/
    └── RoboEyesTFT/          ← Manuel kurulum (GitHub)
```

---

## Notlar

- Lolin D32 üzerinde GPIO 34, 35, 36, 39 pinleri **sadece giriş** (input-only) olduğundan touch için kullanılmaz.
- Touch pinleri (T0–T9) GPIO 0,2,4,12,13,14,15,27,32,33 karşılık gelir. Yukarıdaki pin seçimi bunları baz alır.
- Batarya voltajını okumak için D32 üzerindeki dahili bölücü **GPIO 35** pinini kullanır (`analogRead(35) * 2 * 3.3 / 4095`).
