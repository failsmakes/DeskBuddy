#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include "storage.h"
#include "config.h"

// ============================================================
//  wifi_manager.h  –  STA connection + AP fallback
// ============================================================

class WiFiManager {
public:
    bool isConnected  = false;
    bool isAPMode     = false;
    String apSSID     = "DeskBuddy-Setup";
    String apPass     = "deskbuddy123";
    IPAddress apIP    = IPAddress(192, 168, 4, 1);

    // Called once on boot. Returns true if STA connected.
    bool begin() {
        WiFi.mode(WIFI_STA);
        WiFi.disconnect(true);
        delay(100);

        for (uint8_t i = 0; i < storage.cfg.wifiCount; i++) {
            Serial.printf("[WiFi] Trying SSID: %s\n", storage.cfg.wifiList[i].ssid);
            WiFi.begin(storage.cfg.wifiList[i].ssid, storage.cfg.wifiList[i].pass);

            unsigned long t = millis();
            while (WiFi.status() != WL_CONNECTED && millis() - t < WIFI_CONNECT_TIMEOUT) {
                delay(200);
            }
            if (WiFi.status() == WL_CONNECTED) {
                isConnected = true;
                isAPMode    = false;
                Serial.printf("[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
                return true;
            }
            WiFi.disconnect(true);
            delay(100);
        }

        // All networks failed (or none stored) → start AP
        startAP();
        return false;
    }

    void startAP() {
        Serial.println("[WiFi] Starting AP mode...");
        WiFi.mode(WIFI_AP);
        WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
        WiFi.softAP(apSSID.c_str(), apPass.c_str());
        isAPMode    = true;
        isConnected = false;
        Serial.printf("[WiFi] AP IP: %s\n", WiFi.softAPIP().toString().c_str());
    }

    // Call periodically to detect disconnection and retry / re-AP
    void maintain() {
        if (!isAPMode && WiFi.status() != WL_CONNECTED) {
            Serial.println("[WiFi] Connection lost. Retrying...");
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
