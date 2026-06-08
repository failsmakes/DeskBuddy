#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include "storage.h"
#include "config.h"

// ============================================================
//  wifi_manager.h  -  STA baglantisi + AP fallback
// ============================================================

class WiFiManager {
public:
    bool      isConnected = false;
    bool      isAPMode    = false;
    String    apSSID      = "DeskBuddy-Setup";
    String    apPass      = "deskbuddy123";
    IPAddress apIP        = IPAddress(192, 168, 4, 1);
    IPAddress apGateway   = IPAddress(192, 168, 4, 1);
    IPAddress apSubnet    = IPAddress(255, 255, 255, 0);

    // Boot'ta cagrilir.
    // Donus degeri: true = STA baglantisi kuruldu
    //               false = AP moduna gecildi (veya AP de basarisiz)
    bool begin() {
        WiFi.persistent(false);
        WiFi.mode(WIFI_OFF);
        delay(300);

        // Kayitli ag varsa STA dene
        if (storage.cfg.wifiCount > 0) {
            WiFi.mode(WIFI_STA);
            delay(100);

            for (uint8_t i = 0; i < storage.cfg.wifiCount; i++) {
                if (strlen(storage.cfg.wifiList[i].ssid) == 0) continue;
                DLOGF("[WiFi] Trying: %s\n", storage.cfg.wifiList[i].ssid);
                WiFi.begin(storage.cfg.wifiList[i].ssid, storage.cfg.wifiList[i].pass);

                unsigned long t = millis();
                while (WiFi.status() != WL_CONNECTED
                       && millis() - t < WIFI_CONNECT_TIMEOUT) {
                    delay(200);
                }

                if (WiFi.status() == WL_CONNECTED) {
                    isConnected = true;
                    isAPMode    = false;
                    DLOGF("[WiFi] Connected! IP: %s\n",
                                  WiFi.localIP().toString().c_str());
                    return true;   // STA baglantisi OK
                }

                WiFi.disconnect(true);
                delay(500);
            }
        }

        // STA basarisiz -> AP modu
        startAP();
        return false;   // AP modunda calisiyor, STA yok
    }

    // AP modunu baslat
    void startAP() {
        DLOG("[WiFi] Starting AP...");

        WiFi.mode(WIFI_OFF);
        delay(500);

        WiFi.mode(WIFI_AP_STA);
        delay(500);

        // TX gucunu dusur — AP baslatma anindaki ani akim cekisini kisitlar.
        // Brownout'un nedeni: WiFi varsayilan 20dBm TX ile baslayinca
        // ESP32 ~350mA ceker, zayif besleme gerilimi duser, brownout tetiklenir.
        // 8.5dBm ile ~150mA'ya iner, 10-15m menzil yeterli.
        WiFi.setTxPower(WIFI_POWER_8_5dBm);
        delay(100);

        if (!WiFi.softAPConfig(apIP, apGateway, apSubnet)) {
            DLOG("[WiFi] softAPConfig failed");
        }
        delay(200);

        bool ok = false;
        for (int i = 0; i < 3 && !ok; i++) {
            ok = WiFi.softAP(apSSID.c_str(), apPass.c_str(), 6, 0, 4);
            if (!ok) {
                DLOGF("[WiFi] softAP attempt %d failed\n", i + 1);
                delay(500);
            }
        }

        if (ok) {
            isAPMode    = true;
            isConnected = false;
            delay(1000);
            DLOGF("[WiFi] AP OK  SSID:%s  IP:%s\n",
                          apSSID.c_str(),
                          WiFi.softAPIP().toString().c_str());
        } else {
            isAPMode    = false;
            isConnected = false;
            DLOG("[WiFi] AP FAILED!");
        }
    }

    void maintain() {
        if (isAPMode) return;
        if (WiFi.status() != WL_CONNECTED) {
            DLOG("[WiFi] Lost connection, retrying...");
            isConnected = false;
            begin();
        }
    }

    String localIPString() {
        if (isAPMode) return WiFi.softAPIP().toString();
        return WiFi.localIP().toString();
    }
};

extern WiFiManager wifiManager;
