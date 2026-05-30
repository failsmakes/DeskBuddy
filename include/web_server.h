#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "storage.h"
#include "alarm_manager.h"

// ============================================================
//  web_server.h  -  Configuration web interface
//  NOTE: All HTML is built with String concatenation to avoid
//  raw-string-literal / multibyte-character issues in MSVC/GCC.
// ============================================================

class ConfigWebServer {
public:
    WebServer server{80};

    void begin() {
        server.on("/",             HTTP_GET,  [this]() { handleRoot(); });
        server.on("/wifi/add",     HTTP_POST, [this]() { handleWifiAdd(); });
        server.on("/wifi/update",  HTTP_POST, [this]() { handleWifiUpdate(); });
        server.on("/wifi/delete",  HTTP_POST, [this]() { handleWifiDelete(); });
        server.on("/settings",     HTTP_POST, [this]() { handleSettings(); });
        server.on("/alarm/set",    HTTP_POST, [this]() { handleAlarmSet(); });
        server.on("/countdown/set",HTTP_POST, [this]() { handleCountdownSet(); });
        server.on("/status",       HTTP_GET,  [this]() { handleStatus(); });
        server.onNotFound([this]() { server.send(404, "text/plain", "Not found"); });
        server.begin();
        Serial.println("[WebServer] Started on port 80");
    }

    void loop() { server.handleClient(); }

private:
    // ---- build page in chunks to stay under stack limit ----
    void handleRoot() {
        String p = "";
        p += "<!DOCTYPE html><html lang=\"en\"><head>";
        p += "<meta charset=\"UTF-8\">";
        p += "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">";
        p += "<title>DeskBuddy Config</title>";
        p += "<style>";
        p += ":root{--bg:#0a0e1a;--card:#131929;--accent:#82EEFD;--acc2:#39FF14;";
        p += "--danger:#FF2400;--text:#e8eaf0;--muted:#6b7280}";
        p += "*{box-sizing:border-box;margin:0;padding:0}";
        p += "body{background:var(--bg);color:var(--text);font-family:'Segoe UI',sans-serif;";
        p += "min-height:100vh;padding:1.5rem}";
        p += "h1{color:var(--accent);font-size:1.8rem;margin-bottom:.25rem}";
        p += ".sub{color:var(--muted);font-size:.85rem;margin-bottom:2rem}";
        p += ".card{background:var(--card);border:1px solid #1e2740;border-radius:12px;";
        p += "padding:1.25rem;margin-bottom:1.25rem}";
        p += "h2{color:var(--accent);font-size:1rem;margin-bottom:1rem;";
        p += "text-transform:uppercase;letter-spacing:.1em}";
        p += "input,select{width:100%;background:#0d1120;border:1px solid #2a3550;";
        p += "border-radius:8px;color:var(--text);padding:.55rem .75rem;font-size:.9rem;";
        p += "margin:.3rem 0 .6rem;outline:none}";
        p += "input:focus{border-color:var(--accent)}";
        p += ".row{display:flex;gap:.5rem;margin-bottom:.4rem}";
        p += ".row input{margin:0}";
        p += ".btn{border:none;border-radius:8px;cursor:pointer;font-size:.85rem;";
        p += "font-weight:600;padding:.5rem 1rem;transition:.15s}";
        p += ".bp{background:var(--accent);color:#000}";
        p += ".bd{background:var(--danger);color:#fff}";
        p += ".bs{background:var(--acc2);color:#000}";
        p += ".bp:hover,.bd:hover,.bs:hover{opacity:.85}";
        p += ".we{background:#0d1120;border-radius:8px;padding:.6rem .75rem;";
        p += "margin:.3rem 0;display:flex;justify-content:space-between;align-items:center;gap:.5rem}";
        p += ".ws{font-size:.9rem;flex:1}";
        p += ".st{padding:.4rem .8rem;background:#0d1120;border-radius:6px;";
        p += "font-size:.8rem;color:var(--muted);margin-top:.5rem}";
        p += "label{font-size:.8rem;color:var(--muted)}";
        p += "</style></head><body>";

        p += "<h1>DeskBuddy</h1>";
        p += "<p class=\"sub\">Configuration Panel</p>";

        // WiFi
        p += "<div class=\"card\"><h2>WiFi Networks</h2>";
        p += "<div id=\"wl\">Loading...</div>";
        p += "<hr style=\"border:1px solid #1e2740;margin:.75rem 0\">";
        p += "<label>Add New Network</label>";
        p += "<div class=\"row\">";
        p += "<input id=\"ns\" placeholder=\"SSID\" />";
        p += "<input id=\"np\" type=\"password\" placeholder=\"Password\" />";
        p += "<button class=\"btn bp\" onclick=\"addWifi()\">Add</button>";
        p += "</div></div>";

        // Settings
        p += "<div class=\"card\"><h2>General Settings</h2>";
        p += "<label>Primary City (e.g. Istanbul,TR)</label><input id=\"cp\" value=\"\" />";
        p += "<label>Secondary City (e.g. London,GB)</label><input id=\"cs\" value=\"\" />";
        p += "<label>OpenWeatherMap API Key</label>";
        p += "<input id=\"ok\" placeholder=\"Enter your OWM API key\" />";
        p += "<label>UTC Offset hours (e.g. 3 for UTC+3)</label>";
        p += "<input id=\"tz\" type=\"number\" min=\"-12\" max=\"14\" value=\"3\" />";
        p += "<button class=\"btn bp\" onclick=\"saveSettings()\" style=\"margin-top:.5rem\">";
        p += "Save Settings</button></div>";

        // Alarm
        p += "<div class=\"card\"><h2>Alarm</h2>";
        p += "<div class=\"row\">";
        p += "<div style=\"flex:1\"><label>Hour</label>";
        p += "<input id=\"ah\" type=\"number\" min=\"0\" max=\"23\" value=\"7\"></div>";
        p += "<div style=\"flex:1\"><label>Minute</label>";
        p += "<input id=\"am\" type=\"number\" min=\"0\" max=\"59\" value=\"0\"></div>";
        p += "</div><div class=\"row\">";
        p += "<button class=\"btn bs\" onclick=\"setAlarm(true)\">Enable Alarm</button>";
        p += "<button class=\"btn bd\" onclick=\"setAlarm(false)\">Disable</button>";
        p += "</div></div>";

        // Countdown
        p += "<div class=\"card\"><h2>Countdown</h2>";
        p += "<div class=\"row\">";
        p += "<div style=\"flex:1\"><label>Hours</label>";
        p += "<input id=\"ch\" type=\"number\" min=\"0\" max=\"23\" value=\"0\"></div>";
        p += "<div style=\"flex:1\"><label>Minutes</label>";
        p += "<input id=\"cm\" type=\"number\" min=\"0\" max=\"59\" value=\"5\"></div>";
        p += "<div style=\"flex:1\"><label>Seconds</label>";
        p += "<input id=\"cs2\" type=\"number\" min=\"0\" max=\"59\" value=\"0\"></div>";
        p += "</div><div class=\"row\">";
        p += "<button class=\"btn bs\" onclick=\"startCountdown()\">Start</button>";
        p += "<button class=\"btn bp\" onclick=\"stopCountdown()\">Stop</button>";
        p += "<button class=\"btn bd\" onclick=\"resetCountdown()\">Reset</button>";
        p += "</div></div>";

        // Status
        p += "<div class=\"card\"><h2>Device Status</h2>";
        p += "<div id=\"sb\" class=\"st\">Loading...</div></div>";

        // Script
        p += "<script>";
        p += "async function api(u,m,b){";
        p += "const o={method:m||'GET',headers:{'Content-Type':'application/x-www-form-urlencoded'}};";
        p += "if(b)o.body=new URLSearchParams(b).toString();";
        p += "const r=await fetch(u,o);return r.text();}";

        p += "async function loadStatus(){";
        p += "const r=await fetch('/status');const d=await r.json();";
        p += "document.getElementById('wl').innerHTML=";
        p += "d.wifi.map((w,i)=>\"<div class='we'><span class='ws'>\"+w+\"</span>\"";
        p += "+\"<button class='btn bd' onclick='delWifi(\"+i+\")'>X</button></div>\").join('')";
        p += "||'<span style=color:gray>No networks saved</span>';";
        p += "document.getElementById('cp').value=d.cityP;";
        p += "document.getElementById('cs').value=d.cityS;";
        p += "document.getElementById('ok').value=d.owmKey;";
        p += "document.getElementById('tz').value=d.tzH;";
        p += "document.getElementById('sb').innerHTML=";
        p += "'IP: '+d.ip+' | WiFi: '+(d.connected?'<b style=color:lime>Connected</b>':'<b style=color:red>AP Mode</b>')";
        p += "+'  Heap: '+d.heap+'B  Uptime: '+d.uptime+'s';}";

        p += "async function addWifi(){";
        p += "const s=document.getElementById('ns').value.trim();";
        p += "const pw=document.getElementById('np').value;";
        p += "if(!s)return alert('SSID required');";
        p += "await api('/wifi/add','POST',{ssid:s,pass:pw});";
        p += "document.getElementById('ns').value='';";
        p += "document.getElementById('np').value='';loadStatus();}";

        p += "async function delWifi(i){";
        p += "if(!confirm('Remove this network?'))return;";
        p += "await api('/wifi/delete','POST',{idx:i});loadStatus();}";

        p += "async function saveSettings(){";
        p += "const cP=document.getElementById('cp').value.trim();";
        p += "const cS=document.getElementById('cs').value.trim();";
        p += "const oK=document.getElementById('ok').value.trim();";
        p += "const tZ=document.getElementById('tz').value;";
        p += "await api('/settings','POST',{cityP:cP,cityS:cS,owmKey:oK,tzH:tZ});";
        p += "alert('Settings saved!');}";

        p += "async function setAlarm(e){";
        p += "const h=document.getElementById('ah').value;";
        p += "const m=document.getElementById('am').value;";
        p += "await api('/alarm/set','POST',{h:h,m:m,enable:e?1:0});}";

        p += "async function startCountdown(){";
        p += "const h=parseInt(document.getElementById('ch').value)||0;";
        p += "const m=parseInt(document.getElementById('cm').value)||0;";
        p += "const s=parseInt(document.getElementById('cs2').value)||0;";
        p += "const ms=(h*3600+m*60+s)*1000;";
        p += "await api('/countdown/set','POST',{ms:ms,action:'start'});}";

        p += "async function stopCountdown(){";
        p += "await api('/countdown/set','POST',{ms:0,action:'stop'});}";

        p += "async function resetCountdown(){";
        p += "await api('/countdown/set','POST',{ms:0,action:'reset'});}";

        p += "loadStatus();setInterval(loadStatus,5000);";
        p += "</script></body></html>";

        server.send(200, "text/html", p);
    }

    // ---- API endpoints ----
    void handleWifiAdd() {
        String ssid = server.arg("ssid");
        String pass = server.arg("pass");
        if (ssid.length() == 0) { server.send(400, "text/plain", "No SSID"); return; }
        if (storage.cfg.wifiCount >= MAX_WIFI_NETWORKS) {
            server.send(400, "text/plain", "Max networks reached"); return;
        }
        storage.addWifi(ssid.c_str(), pass.c_str());
        server.send(200, "text/plain", "OK");
    }

    void handleWifiUpdate() {
        uint8_t idx = (uint8_t)server.arg("idx").toInt();
        String ssid = server.arg("ssid");
        String pass = server.arg("pass");
        storage.updateWifi(idx, ssid.c_str(), pass.c_str());
        server.send(200, "text/plain", "OK");
    }

    void handleWifiDelete() {
        uint8_t idx = (uint8_t)server.arg("idx").toInt();
        storage.deleteWifi(idx);
        server.send(200, "text/plain", "OK");
    }

    void handleSettings() {
        String cityP  = server.arg("cityP");
        String cityS  = server.arg("cityS");
        String owmKey = server.arg("owmKey");
        int8_t tzH    = (int8_t)server.arg("tzH").toInt();
        if (cityP.length())  strncpy(storage.cfg.cityPrimary,   cityP.c_str(),  32);
        if (cityS.length())  strncpy(storage.cfg.citySecondary, cityS.c_str(),  32);
        if (owmKey.length()) strncpy(storage.cfg.owmApiKey,     owmKey.c_str(), 32);
        storage.cfg.tzOffsetHours = tzH;
        storage.save();
        server.send(200, "text/plain", "OK");
    }

    void handleAlarmSet() {
        uint8_t h   = (uint8_t)server.arg("h").toInt();
        uint8_t m   = (uint8_t)server.arg("m").toInt();
        bool enable = server.arg("enable").toInt() != 0;
        alarmMgr.setAlarm(h, m);
        if (enable) alarmMgr.enableAlarm();
        else        alarmMgr.disableAlarm();
        server.send(200, "text/plain", "OK");
    }

    void handleCountdownSet() {
        String action = server.arg("action");
        uint32_t ms   = (uint32_t)server.arg("ms").toInt();
        if (action == "start") {
            if (ms > 0) alarmMgr.countdownSet(ms);
            alarmMgr.countdownStart();
        } else if (action == "stop") {
            alarmMgr.countdownStop();
        } else if (action == "reset") {
            alarmMgr.countdownReset();
        }
        server.send(200, "text/plain", "OK");
    }

    void handleStatus() {
        JsonDocument doc;
        JsonArray wifi = doc["wifi"].to<JsonArray>();
        for (uint8_t i = 0; i < storage.cfg.wifiCount; i++) {
            wifi.add(storage.cfg.wifiList[i].ssid);
        }
        doc["cityP"]     = storage.cfg.cityPrimary;
        doc["cityS"]     = storage.cfg.citySecondary;
        doc["owmKey"]    = storage.cfg.owmApiKey;
        doc["tzH"]       = storage.cfg.tzOffsetHours;
        doc["connected"] = (WiFi.status() == WL_CONNECTED);
        doc["ip"]        = WiFi.status() == WL_CONNECTED
                           ? WiFi.localIP().toString()
                           : WiFi.softAPIP().toString();
        doc["heap"]      = ESP.getFreeHeap();
        doc["uptime"]    = millis() / 1000;
        String out;
        serializeJson(doc, out);
        server.send(200, "application/json", out);
    }
};

extern ConfigWebServer webServer;
