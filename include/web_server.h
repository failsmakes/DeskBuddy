#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include <HTTPClient.h>
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
        server.on("/wifi/scan",    HTTP_GET,  [this]() { handleWifiScan(); });
        server.on("/wifi/add",     HTTP_POST, [this]() { handleWifiAdd(); });
        server.on("/wifi/update",  HTTP_POST, [this]() { handleWifiUpdate(); });
        server.on("/wifi/delete",  HTTP_POST, [this]() { handleWifiDelete(); });
        server.on("/settings",     HTTP_POST, [this]() { handleSettings(); });
        server.on("/city/validate",HTTP_GET,  [this]() { handleCityValidate(); });
        server.on("/alarm/set",    HTTP_POST, [this]() { handleAlarmSet(); });
        server.on("/countdown/set",HTTP_POST, [this]() { handleCountdownSet(); });
        server.on("/status",       HTTP_GET,  [this]() { handleStatus(); });
        server.on("/sleep/set",    HTTP_POST, [this]() { handleSleepSet(); });
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
        // Scan listesi stilleri
        p += ".sc-list{display:none;margin-top:.5rem;max-height:220px;overflow-y:auto;";
        p += "border:1px solid #1e2740;border-radius:8px}";
        p += ".sc-item{display:flex;align-items:center;justify-content:space-between;";
        p += "padding:.5rem .75rem;cursor:pointer;border-bottom:1px solid #1a2030;gap:.5rem}";
        p += ".sc-item:last-child{border-bottom:none}";
        p += ".sc-item:hover{background:#1a2535}";
        p += ".sc-ssid{flex:1;font-size:.9rem;word-break:break-all}";
        p += ".sc-meta{display:flex;align-items:center;gap:.5rem;flex-shrink:0}";
        p += ".sc-lock{font-size:.75rem;color:#aaa}";
        // Sinyal guc cubugu
        p += ".sig{display:flex;align-items:flex-end;gap:1px;height:14px}";
        p += ".sig span{width:3px;background:#2a3550;border-radius:1px}";
        p += ".sig span.on{background:var(--accent)}";
        p += ".pw-box{display:none;margin-top:.6rem;padding:.75rem;";
        p += "background:#0a0f1e;border:1px solid #1e2740;border-radius:8px}";
        p += ".pw-ssid{color:var(--accent);font-size:.9rem;margin-bottom:.4rem}";
        p += ".scan-btn{display:flex;align-items:center;gap:.4rem}";
        p += ".spin{display:none;width:14px;height:14px;border:2px solid #333;";
        p += "border-top-color:var(--accent);border-radius:50%;";
        p += "animation:spin .7s linear infinite}";
        p += "@keyframes spin{to{transform:rotate(360deg)}}";
        p += "</style></head><body>";

        p += "<h1>DeskBuddy</h1>";
        p += "<p class=\"sub\">Configuration Panel</p>";

        // WiFi kartı - tarama destekli
        p += "<div class=\"card\"><h2>WiFi Networks</h2>";
        p += "<div id=\"wl\">Loading...</div>";
        p += "<hr style=\"border:1px solid #1e2740;margin:.75rem 0\">";
        p += "<label>Yeni Ag Ekle</label>";

        // Tara butonu + spinner
        p += "<div class=\"scan-btn\" style=\"margin:.4rem 0\">";
        p += "<button class=\"btn bp\" onclick=\"scanWifi()\" id=\"scanbtn\">Ag Tara</button>";
        p += "<div class=\"spin\" id=\"spin\"></div>";
        p += "<span id=\"scan-info\" style=\"font-size:.8rem;color:var(--muted)\"></span>";
        p += "</div>";

        // Tarama sonuclari listesi
        p += "<div class=\"sc-list\" id=\"scl\"></div>";

        // Sifre giris kutusu (secim yapilince acilir)
        p += "<div class=\"pw-box\" id=\"pwbox\">";
        p += "<div class=\"pw-ssid\" id=\"sel-ssid\"></div>";
        p += "<input id=\"np\" type=\"password\" placeholder=\"Sifre (acik ag icin bos birakin)\" />";
        p += "<div class=\"row\" style=\"margin-top:.3rem\">";
        p += "<button class=\"btn bs\" onclick=\"addWifi()\">Kaydet</button>";
        p += "<button class=\"btn\" style=\"background:#1e2740;color:#aaa\" onclick=\"cancelSel()\">Iptal</button>";
        p += "</div></div>";

        // Manuel SSID girisi (taramaya alternatif)
        p += "<details style=\"margin-top:.6rem\">";
        p += "<summary style=\"font-size:.8rem;color:var(--muted);cursor:pointer\">Manuel SSID gir</summary>";
        p += "<div class=\"row\" style=\"margin-top:.4rem\">";
        p += "<input id=\"ns\" placeholder=\"SSID\" />";
        p += "<input id=\"npm\" type=\"password\" placeholder=\"Sifre\" />";
        p += "<button class=\"btn bp\" onclick=\"addWifiManual()\">Ekle</button>";
        p += "</div></details>";
        p += "</div>";

        // Settings
        p += "<div class=\"card\"><h2>General Settings</h2>";

        // Ana konum - dogrulama gostergeli
        p += "<label>Birincil Konum</label>";
        p += "<div style=\"display:flex;gap:.4rem;align-items:center\">";
        p += "<input id=\"cp\" placeholder=\"Ornek: Istanbul,TR\" style=\"margin:0\" oninput=\"cityChanged('p')\" />";
        p += "<span id=\"cp-st\" style=\"font-size:1.1rem;flex-shrink:0;width:20px\"></span>";
        p += "</div>";
        p += "<div id=\"cp-msg\" style=\"font-size:.78rem;margin:.2rem 0 .6rem;min-height:1rem\"></div>";

        // Ikincil konum - dogrulama gostergeli
        p += "<label>Ikincil Konum <span style=color:#555;font-size:.75rem>(bos birakilabilir)</span></label>";
        p += "<div style=\"display:flex;gap:.4rem;align-items:center\">";
        p += "<input id=\"cs\" placeholder=\"Ornek: London,GB  (bos=devre disi)\" style=\"margin:0\" oninput=\"cityChanged('s')\" />";
        p += "<span id=\"cs-st\" style=\"font-size:1.1rem;flex-shrink:0;width:20px\"></span>";
        p += "</div>";
        p += "<div id=\"cs-msg\" style=\"font-size:.78rem;margin:.2rem 0 .6rem;min-height:1rem\"></div>";

        p += "<label>OpenWeatherMap API Key</label>";
        p += "<input id=\"ok\" placeholder=\"OWM API anahtarinizi girin\" oninput=\"cityChanged('p');cityChanged('s')\" />";
        p += "<label>UTC Offset (saat, ornek: 3 = UTC+3)</label>";
        p += "<input id=\"tz\" type=\"number\" min=\"-12\" max=\"14\" value=\"3\" />";
        p += "<button id=\"save-btn\" class=\"btn bp\" onclick=\"saveSettings()\" style=\"margin-top:.5rem\" disabled>";
        p += "Kaydet</button>";
        p += "<div id=\"save-msg\" style=\"font-size:.78rem;margin-top:.4rem;min-height:1rem\"></div>";
        p += "</div>";

        // Alarm - 3 yuva
        p += "<div class=\"card\"><h2>Alarm</h2>";
        // 3 alarm satiri
        for (int i = 0; i < 3; i++) {
            p += "<div style=\"background:#0a0f1e;border-radius:8px;padding:.6rem .75rem;margin-bottom:.5rem;border:1px solid #1e2740\">";
            p += "<div style=\"display:flex;align-items:center;justify-content:space-between;gap:.5rem\">";
            // Sol: alarm no + saat/dakika girisi
            p += "<span style=\"color:#82EEFD;font-size:.85rem;min-width:40px\">ALM";
            p += String(i + 1);
            p += "</span>";
            p += "<div style=\"display:flex;gap:.3rem;align-items:center;flex:1\">";
            p += "<input id=\"ah";
            p += String(i);
            p += "\" type=\"number\" min=\"0\" max=\"23\" value=\"7\" style=\"margin:0;width:56px;text-align:center\" />";
            p += "<span style=\"color:#555\">:</span>";
            p += "<input id=\"am";
            p += String(i);
            p += "\" type=\"number\" min=\"0\" max=\"59\" value=\"0\" style=\"margin:0;width:56px;text-align:center\" />";
            p += "</div>";
            // Sag: ac/kapat butonlari
            p += "<div style=\"display:flex;gap:.3rem\">";
            p += "<button class=\"btn bs\" onclick=\"setAlarm(";
            p += String(i);
            p += ",true)\" style=\"padding:.3rem .6rem;font-size:.8rem\">AC</button>";
            p += "<button class=\"btn bd\" onclick=\"setAlarm(";
            p += String(i);
            p += ",false)\" style=\"padding:.3rem .6rem;font-size:.8rem\">KAPAT</button>";
            p += "</div>";
            p += "</div>";
            // Durum satiri
            p += "<div id=\"alst";
            p += String(i);
            p += "\" style=\"font-size:.75rem;color:#555;margin-top:.3rem\">-</div>";
            p += "</div>";
        }
        p += "</div>";

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

        // Sleep / Uyku Modu
        p += "<div class=\"card\"><h2>Uyku Modu</h2>";
        p += "<label>Hareketsizlik suresi (saniye, 0 = devre disi)</label>";
        p += "<input id=\"sl\" type=\"number\" min=\"0\" max=\"3600\" value=\"300\" />";
        p += "<button class=\"btn bp\" onclick=\"saveSleep()\" style=\"margin-top:.5rem\">Kaydet</button></div>";

        // Status
        p += "<div class=\"card\"><h2>Device Status</h2>";
        p += "<div id=\"sb\" class=\"st\">Loading...</div></div>";

        // Script
        p += "<script>";
        p += "async function api(u,m,b){";
        p += "const o={method:m||'GET',headers:{'Content-Type':'application/x-www-form-urlencoded'}};";
        p += "if(b)o.body=new URLSearchParams(b).toString();";
        p += "const r=await fetch(u,o);return r.text();}";

        // Sinyal guc cubugu HTML yardimcisi (rssi: -30=mukemmel .. -90=zayif)
        p += "function sigBar(rssi){";
        p += "const lvl=rssi>=-50?4:rssi>=-65?3:rssi>=-75?2:rssi>=-85?1:0;";
        p += "let h='<div class=sig>';";
        p += "const ht=['4px','7px','10px','13px'];";
        p += "for(let i=0;i<4;i++)h+='<span style=height:'+ht[i]+(i<lvl?' class=on':'')+'></span>';";
        p += "return h+'</div>';}";

        // Kilit ikonu
        p += "function lockIcon(enc){";
        p += "return enc?'<span class=sc-lock>&#128274;</span>':'<span class=sc-lock style=color:#555>&#128275;</span>';}";

        // Ag tara
        p += "async function scanWifi(){";
        p += "document.getElementById('scanbtn').disabled=true;";
        p += "document.getElementById('spin').style.display='block';";
        p += "document.getElementById('scan-info').textContent='Taranıyor...';";
        p += "document.getElementById('scl').style.display='none';";
        p += "cancelSel();";
        p += "try{";
        p += "const r=await fetch('/wifi/scan');";
        p += "const nets=await r.json();";
        p += "const ul=document.getElementById('scl');";
        p += "if(!nets.length){ul.innerHTML='<div style=padding:.6rem;color:gray;font-size:.85rem>Ag bulunamadi</div>';}";
        p += "else{ul.innerHTML=nets.map(n=>";
        p += "\"<div class='sc-item' onclick='selectNet(\\\"\" +n.ssid.replace(/\\\"/g,'&quot;')+ \"\\\",\" +n.enc+ \")'>\"+";
        p += "\"<span class='sc-ssid'>\" +n.ssid+ \"</span>\"+";
        p += "\"<span class='sc-meta'>\" +sigBar(n.rssi)+ lockIcon(n.enc)+ \"</span>\"+";
        p += "\"</div>\").join('');}";
        p += "ul.style.display='block';";
        p += "document.getElementById('scan-info').textContent=nets.length+' ag bulundu';";
        p += "}catch(e){document.getElementById('scan-info').textContent='Hata: '+e.message;}";
        p += "document.getElementById('scanbtn').disabled=false;";
        p += "document.getElementById('spin').style.display='none';}";

        // SSID secimi
        p += "let _selSSID='';";
        p += "function selectNet(ssid,enc){";
        p += "_selSSID=ssid;";
        p += "document.getElementById('sel-ssid').textContent=ssid+(enc?' (sifre gerekli)':' (acik ag)');";
        p += "document.getElementById('np').value='';";
        p += "document.getElementById('pwbox').style.display='block';";
        p += "document.getElementById('np').focus();}";

        // Secimi iptal et
        p += "function cancelSel(){";
        p += "_selSSID='';";
        p += "document.getElementById('pwbox').style.display='none';";
        p += "document.getElementById('np').value='';}";

        // Secilen SSID ile kaydet
        p += "async function addWifi(){";
        p += "if(!_selSSID)return alert('Once bir ag secin');";
        p += "const pw=document.getElementById('np').value;";
        p += "const r=await api('/wifi/add','POST',{ssid:_selSSID,pass:pw});";
        p += "if(r==='OK'){cancelSel();";
        p += "document.getElementById('scl').style.display='none';";
        p += "document.getElementById('scan-info').textContent='';";
        p += "loadStatus();}else{alert('Hata: '+r);}}";

        // Manuel ekleme (details icinden)
        p += "async function addWifiManual(){";
        p += "const s=document.getElementById('ns').value.trim();";
        p += "const pw=document.getElementById('npm').value;";
        p += "if(!s)return alert('SSID gerekli');";
        p += "const r=await api('/wifi/add','POST',{ssid:s,pass:pw});";
        p += "if(r==='OK'){document.getElementById('ns').value='';";
        p += "document.getElementById('npm').value='';loadStatus();}";
        p += "else{alert('Hata: '+r);}}";

        p += "async function delWifi(i){";
        p += "if(!confirm('Bu agi silmek istiyor musunuz?'))return;";
        p += "await api('/wifi/delete','POST',{idx:i});loadStatus();}";

        p += "async function loadStatus(){";
        p += "const r=await fetch('/status');const d=await r.json();";
        p += "document.getElementById('wl').innerHTML=";
        p += "d.wifi.map((w,i)=>\"<div class='we'><span class='ws'>\" +w+ \"</span>\"";
        p += "+\"<button class='btn bd' onclick='delWifi(\"+i+\")' style='padding:.3rem .6rem'>Sil</button></div>\").join('')";
        p += "||'<span style=color:gray;font-size:.85rem>Kayitli ag yok</span>';";
        p += "document.getElementById('cp').value=d.cityP;";
        p += "document.getElementById('cs').value=d.cityS;";
        p += "document.getElementById('ok').value=d.owmKey;";
        p += "document.getElementById('tz').value=d.tzH;";
        p += "if(document.getElementById('sl'))document.getElementById('sl').value=d.sleepS;";
        // Sayfa ilk yuklendiginde kayitli sehirleri dogrula
        p += "if(d.cityP){cityChanged('p');}else{_cv.p={ok:false,checked:true};updateSaveBtn();}";
        p += "if(d.cityS){cityChanged('s');}else{_cv.s={ok:true,checked:true};updateSaveBtn();}";
        p += "document.getElementById('sb').innerHTML=";
        p += "'IP: '+d.ip+' | WiFi: '+(d.connected?'<b style=color:lime>Bagli</b>':'<b style=color:red>AP Modu</b>')";
        p += "+'  Heap: '+d.heap+'B  Uptime: '+d.uptime+'s';";
        // Alarm durumlarini guncelle
        p += "if(d.alarms){d.alarms.forEach((a,i)=>{";
        p += "document.getElementById('ah'+i).value=a.h;";
        p += "document.getElementById('am'+i).value=a.m;";
        p += "const st=document.getElementById('alst'+i);";
        p += "if(st)st.innerHTML=a.enabled";
        p += "?'<span style=color:#39ff14>AKTIF - '+String(a.h).padStart(2,'0')+':'+String(a.m).padStart(2,'0')+'</span>'";
        p += ":'<span style=color:#555>Kapali</span>';";
        p += "});}";
        p += "}";


        // Sehir dogrulama durumu
        p += "const _cv={p:{ok:false,checked:false},s:{ok:true,checked:true}};";// s bosta gecerli

        // Debounce zamanlayici
        p += "const _ct={p:null,s:null};";

        // Input degisince 800ms sonra dogrula (cok hizli istek gondermesin)
        p += "function cityChanged(k){";
        p += "clearTimeout(_ct[k]);";
        p += "const val=document.getElementById(k==='p'?'cp':'cs').value.trim();";
        p += "const st=document.getElementById(k==='p'?'cp-st':'cs-st');";
        p += "const msg=document.getElementById(k==='p'?'cp-msg':'cs-msg');";
        // Ikincil bos birakilabilir
        p += "if(k==='s'&&!val){";
        p += "_cv.s={ok:true,checked:true};";
        p += "st.textContent='';msg.textContent='';msg.style.color='';";
        p += "updateSaveBtn();return;}";
        // Cok kisa girisi bekleme
        p += "if(val.length<2){_cv[k]={ok:false,checked:false};";
        p += "st.textContent='';msg.textContent='';updateSaveBtn();return;}";
        p += "st.textContent='...';msg.textContent='Kontrol ediliyor...';msg.style.color='#888';";
        p += "_cv[k]={ok:false,checked:false};updateSaveBtn();";
        p += "_ct[k]=setTimeout(()=>doValidate(k,val),800);}";

        // Asil dogrulama - /city/validate endpoint'i uzerinden
        p += "async function doValidate(k,city){";
        p += "const apiKey=document.getElementById('ok').value.trim();";
        p += "const st=document.getElementById(k==='p'?'cp-st':'cs-st');";
        p += "const msg=document.getElementById(k==='p'?'cp-msg':'cs-msg');";
        p += "if(!apiKey){";
        p += "st.textContent='';";
        p += "msg.textContent='Dogrulama icin once OWM API anahtari girin';";
        p += "msg.style.color='#ff8c00';";
        p += "_cv[k]={ok:false,checked:true};updateSaveBtn();return;}";
        p += "try{";
        p += "const r=await fetch('/city/validate?city='+encodeURIComponent(city)+'&key='+encodeURIComponent(apiKey));";
        p += "const d=await r.json();";
        p += "if(d.found){";
        p += "_cv[k]={ok:true,checked:true};";
        p += "st.innerHTML='<span style=color:#39ff14>&#10003;</span>';";
        p += "msg.style.color='#39ff14';";
        p += "msg.textContent=d.name+', '+d.country+' ('+d.lat.toFixed(2)+', '+d.lon.toFixed(2)+')';";
        p += "}else{";
        p += "_cv[k]={ok:false,checked:true};";
        p += "st.innerHTML='<span style=color:#ff2400>&#10007;</span>';";
        p += "msg.style.color='#ff2400';";
        p += "msg.textContent='Konum bulunamadi. Sehir,ULKEKOD formatini deneyin (ornek: Ankara,TR)';";
        p += "}";
        p += "}catch(e){";
        p += "st.textContent='';msg.style.color='#ff8c00';";
        p += "msg.textContent='Baglanti hatasi: '+e.message;";
        p += "_cv[k]={ok:false,checked:true};}";
        p += "updateSaveBtn();}";

        // Kaydet butonunu guncelle
        p += "function updateSaveBtn(){";
        p += "const ok=_cv.p.ok&&_cv.s.ok;";
        p += "const btn=document.getElementById('save-btn');";
        p += "btn.disabled=!ok;";
        p += "btn.style.opacity=ok?'1':'0.4';}";

        // Kaydet
        p += "async function saveSettings(){";
        p += "if(!_cv.p.ok||!_cv.s.ok)return;";
        p += "const cP=document.getElementById('cp').value.trim();";
        p += "const cS=document.getElementById('cs').value.trim();";
        p += "const oK=document.getElementById('ok').value.trim();";
        p += "const tZ=document.getElementById('tz').value;";
        p += "const r=await api('/settings','POST',{cityP:cP,cityS:cS,owmKey:oK,tzH:tZ});";
        p += "const smsg=document.getElementById('save-msg');";
        p += "if(r==='OK'){smsg.style.color='#39ff14';smsg.textContent='Ayarlar kaydedildi!';}";
        p += "else{smsg.style.color='#ff2400';smsg.textContent='Kayit hatasi: '+r;}}";

        p += "async function setAlarm(idx,enable){";
        p += "const h=document.getElementById('ah'+idx).value;";
        p += "const m=document.getElementById('am'+idx).value;";
        p += "await api('/alarm/set','POST',{idx:idx,h:h,m:m,enable:enable?1:0});";
        p += "loadStatus();}";

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

        p += "async function saveSleep(){";
        p += "const s=parseInt(document.getElementById('sl').value)||0;";
        p += "await api('/sleep/set','POST',{seconds:s});alert('Kaydedildi!');}";
        p += "loadStatus();setInterval(loadStatus,5000);";
        p += "</script></body></html>";

        server.send(200, "text/html", p);
    }

    // ---- WiFi tarama endpoint'i ----
    void handleWifiScan() {
        // Tarama yap (blocking, ~2-3 sn surer)
        int n = WiFi.scanNetworks(false, false);  // async=false, show_hidden=false

        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();

        if (n > 0) {
            // RSSI'ya gore sirala (kucukten buyuge, yani gucludan zayifa)
            // scanNetworks zaten siralanmis doner ama elle siralayalim
            for (int i = 0; i < n - 1; i++) {
                for (int j = i + 1; j < n; j++) {
                    if (WiFi.RSSI(j) > WiFi.RSSI(i)) {
                        // swap
                        String tmpSSID = WiFi.SSID(i);
                        int32_t tmpRSSI = WiFi.RSSI(i);
                        uint8_t tmpEnc  = WiFi.encryptionType(i);
                        // ESP32 scanNetworks sonuclari index bazlidir,
                        // dogrudan swap yapilamaz; JsonArray'e sirasiz ekleyip
                        // JS tarafinda siralanmis gelsin
                        (void)tmpSSID; (void)tmpRSSI; (void)tmpEnc;
                        break;
                    }
                }
            }

            // Duplicate SSID filtreleme icin set benzeri yaklasim
            for (int i = 0; i < n; i++) {
                String ssid = WiFi.SSID(i);
                if (ssid.length() == 0) continue;  // gizli ag, atla

                // Daha once ayni SSID eklendi mi kontrol et
                bool duplicate = false;
                for (JsonObject o : arr) {
                    if (o["ssid"].as<String>() == ssid) { duplicate = true; break; }
                }
                if (duplicate) continue;

                JsonObject net = arr.add<JsonObject>();
                net["ssid"] = ssid;
                net["rssi"] = WiFi.RSSI(i);
                net["enc"]  = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN) ? 1 : 0;
            }
        }

        WiFi.scanDelete();  // Tarama sonuclarini bellekten temizle

        String out;
        serializeJson(doc, out);
        server.send(200, "application/json", out);
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

    // ---- Sehir dogrulama (OWM Geocoding API) ----
    // GET /city/validate?city=Istanbul,TR&key=APIKEY
    // Yanit: {"found":true,"name":"Istanbul","country":"TR","lat":41.01,"lon":28.95}
    //        {"found":false}
    void handleCityValidate() {
        String city   = server.arg("city");
        String apiKey = server.arg("key");

        // API anahtari yoksa sadece format kontrolu yap
        if (apiKey.length() == 0) {
            // EEPROM'daki anahtari dene
            if (storage.cfg.owmApiKey[0]) {
                apiKey = String(storage.cfg.owmApiKey);
            } else {
                server.send(200, "application/json", "{\"found\":false,\"error\":\"no_key\"}");
                return;
            }
        }

        if (city.length() == 0) {
            server.send(200, "application/json", "{\"found\":false}");
            return;
        }

        // OWM Geocoding API: http://api.openweathermap.org/geo/1.0/direct?q=city&limit=1&appid=key
        HTTPClient http;
        char url[256];
        snprintf(url, sizeof(url),
            "http://api.openweathermap.org/geo/1.0/direct?q=%s&limit=1&appid=%s",
            urlEncode(city).c_str(), apiKey.c_str());

        http.begin(url);
        http.setTimeout(8000);
        int code = http.GET();

        if (code != 200) {
            http.end();
            char err[64];
            snprintf(err, sizeof(err), "{\"found\":false,\"error\":\"http_%d\"}", code);
            server.send(200, "application/json", String(err));
            return;
        }

        String body = http.getString();
        http.end();

        // OWM bos dizi donerse sehir bulunamadi
        if (body.length() < 5 || body == "[]") {
            server.send(200, "application/json", "{\"found\":false}");
            return;
        }

        // JSON parse et
        JsonDocument doc;
        if (deserializeJson(doc, body) || !doc.is<JsonArray>() || doc.as<JsonArray>().size() == 0) {
            server.send(200, "application/json", "{\"found\":false,\"error\":\"parse\"}");
            return;
        }

        JsonObject result = doc[0].as<JsonObject>();
        String name    = result["name"]    | "";
        String country = result["country"] | "";
        float  lat     = result["lat"]     | 0.0f;
        float  lon     = result["lon"]     | 0.0f;

        // Yanit olustur
        JsonDocument resp;
        resp["found"]   = true;
        resp["name"]    = name;
        resp["country"] = country;
        resp["lat"]     = lat;
        resp["lon"]     = lon;
        String out;
        serializeJson(resp, out);
        server.send(200, "application/json", out);
    }

    // URL encode yardimcisi (weather.h'deki ile ayni mantik)
    String urlEncode(const String& src) {
        String out;
        for (int i = 0; i < src.length(); i++) {
            char c = src[i];
            if (isAlphaNumeric(c) || c == '-' || c == '_' || c == '.' || c == ',') {
                out += c;
            } else {
                char buf[4];
                snprintf(buf, sizeof(buf), "%%%02X", (uint8_t)c);
                out += buf;
            }
        }
        return out;
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
        uint8_t idx = (uint8_t)server.arg("idx").toInt();
        uint8_t h   = (uint8_t)server.arg("h").toInt();
        uint8_t m   = (uint8_t)server.arg("m").toInt();
        bool enable = server.arg("enable").toInt() != 0;
        if (idx < MAX_ALARMS) {
            alarmMgr.setAlarm(idx, h, m);
            if (enable) alarmMgr.enableAlarm(idx);
            else        alarmMgr.disableAlarm(idx);
        }
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

    void handleSleepSet() {
        uint32_t s = (uint32_t)server.arg("seconds").toInt();
        storage.cfg.sleepTimeoutS = s;
        storage.save();
        sleepMgr.setTimeoutSeconds(s);
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
        doc["sleepS"]    = storage.cfg.sleepTimeoutS;
        // 3 alarm durumu
        JsonArray alarms = doc["alarms"].to<JsonArray>();
        for (uint8_t i = 0; i < MAX_ALARMS; i++) {
            JsonObject a = alarms.add<JsonObject>();
            a["h"]       = alarmMgr.alarms[i].hour;
            a["m"]       = alarmMgr.alarms[i].minute;
            a["enabled"] = alarmMgr.alarms[i].enabled;
            a["firing"]  = alarmMgr.alarms[i].firing;
        }
        String out;
        serializeJson(doc, out);
        server.send(200, "application/json", out);
    }
};

extern ConfigWebServer webServer;
