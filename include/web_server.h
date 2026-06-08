#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "storage.h"
#include "alarm_manager.h"
#include "sleep_manager.h"
#include "wifi_manager.h"
#include "time_manager.h"

// ============================================================
//  web_server.h  -  DeskBuddy Konfigurasyon Web Sunucusu
//
//  Cozum: HTML PROGMEM'de parcalar halinde saklanir.
//  handleRoot(), parcalari birlestirir ve server.send() ile
//  Content-Length belirterek tek seferde gonderir.
//  Bu sayede ERR_CONNECTION_ABORTED ve heap tasmasi engellenir.
// ============================================================

// ---- PROGMEM HTML parcalari ----
// Her parca ~800-1200 byte, toplam ~8KB

static const char HTML_HEAD[] PROGMEM =
"<!DOCTYPE html><html lang=tr><head>"
"<meta charset=UTF-8>"
"<meta name=viewport content='width=device-width,initial-scale=1'>"
"<title>DeskBuddy</title>"
"<style>"
":root{--bg:#0a0e1a;--card:#131929;--ac:#82EEFD;--g:#39FF14;--r:#FF2400;--tx:#e8eaf0;--mu:#6b7280}"
"*{box-sizing:border-box;margin:0;padding:0}"
"body{background:var(--bg);color:var(--tx);font-family:sans-serif;padding:1.2rem}"
"h1{color:var(--ac);font-size:1.6rem;margin-bottom:.2rem}"
".sub{color:var(--mu);font-size:.8rem;margin-bottom:1.5rem}"
".card{background:var(--card);border:1px solid #1e2740;border-radius:10px;padding:1.1rem;margin-bottom:1rem}"
"h2{color:var(--ac);font-size:.9rem;margin-bottom:.8rem;text-transform:uppercase;letter-spacing:.1em}"
"input{width:100%;background:#0d1120;border:1px solid #2a3550;border-radius:7px;"
"color:var(--tx);padding:.5rem .7rem;font-size:.85rem;margin:.25rem 0 .5rem;outline:none}"
"input:focus{border-color:var(--ac)}"
".row{display:flex;gap:.4rem;margin-bottom:.4rem}"
".row input{margin:0}"
".btn{border:none;border-radius:7px;cursor:pointer;font-size:.8rem;font-weight:600;padding:.45rem .9rem}"
".bp{background:var(--ac);color:#000}.bd{background:var(--r);color:#fff}.bs{background:var(--g);color:#000}"
".bp:hover,.bd:hover,.bs:hover{opacity:.85}"
".we{background:#0d1120;border-radius:7px;padding:.55rem .7rem;margin:.25rem 0;"
"display:flex;justify-content:space-between;align-items:center;gap:.4rem}"
".ws{font-size:.85rem;flex:1}"
".st{padding:.4rem .7rem;background:#0d1120;border-radius:6px;font-size:.75rem;color:var(--mu)}"
"label{font-size:.75rem;color:var(--mu)}"
".sc-list{display:none;margin-top:.4rem;max-height:200px;overflow-y:auto;border:1px solid #1e2740;border-radius:7px}"
".sc-item{display:flex;align-items:center;justify-content:space-between;padding:.45rem .7rem;"
"cursor:pointer;border-bottom:1px solid #1a2030}"
".sc-item:hover{background:#1a2535}"
".sc-ssid{flex:1;font-size:.85rem}"
".sc-meta{display:flex;align-items:center;gap:.4rem}"
".sig{display:flex;align-items:flex-end;gap:1px;height:12px}"
".sig span{width:3px;background:#2a3550;border-radius:1px}"
".sig span.on{background:var(--ac)}"
".pw-box{display:none;margin-top:.5rem;padding:.65rem;background:#0a0f1e;border:1px solid #1e2740;border-radius:7px}"
".spin{display:none;width:13px;height:13px;border:2px solid #333;border-top-color:var(--ac);"
"border-radius:50%;animation:sp .7s linear infinite;margin-left:.4rem}"
"@keyframes sp{to{transform:rotate(360deg)}}"
"details summary{font-size:.8rem;color:var(--mu);cursor:pointer;margin-top:.5rem}"
"</style></head><body>"
"<h1>DeskBuddy</h1><p class=sub>Konfigurasyon Paneli</p>";

static const char HTML_WIFI[] PROGMEM =
"<div class=card><h2>WiFi Aglari</h2>"
"<div id=wl>Yukleniyor...</div>"
"<hr style='border:1px solid #1e2740;margin:.6rem 0'>"
"<label>Yeni Ag Ekle</label>"
"<div style='display:flex;align-items:center;gap:.4rem;margin:.3rem 0'>"
"<button class='btn bp' onclick='scanWifi()' id=scanbtn>Ag Tara</button>"
"<div class=spin id=spin></div>"
"<span id=scan-info style='font-size:.75rem;color:var(--mu)'></span></div>"
"<div class=sc-list id=scl></div>"
"<div class=pw-box id=pwbox>"
"<div id=sel-ssid style='color:var(--ac);font-size:.85rem;margin-bottom:.3rem'></div>"
"<input id=np type=password placeholder='Sifre (acik ag icin bos birakin)'>"
"<div class=row style='margin-top:.3rem'>"
"<button class='btn bs' onclick='addWifi()'>Kaydet</button>"
"<button class=btn style='background:#1e2740;color:#aaa' onclick='cancelSel()'>Iptal</button>"
"</div></div>"
"<details><summary>Manuel SSID gir</summary>"
"<div class=row style='margin-top:.4rem'>"
"<input id=ns placeholder=SSID>"
"<input id=npm type=password placeholder=Sifre>"
"<button class='btn bp' onclick='addWifiManual()'>Ekle</button>"
"</div></details></div>";

static const char HTML_SETTINGS[] PROGMEM =
"<div class=card><h2>Genel Ayarlar</h2>"
"<label>Birincil Konum</label>"
"<div style='display:flex;gap:.4rem;align-items:center'>"
"<input id=cp placeholder='Istanbul,TR' style=margin:0 oninput=\"cc('p')\">"
"<span id=cp-st style='font-size:1rem;width:18px;flex-shrink:0'></span></div>"
"<div id=cp-msg style='font-size:.72rem;min-height:.9rem;margin-bottom:.4rem'></div>"
"<label>Ikincil Konum <span style='color:#555;font-size:.7rem'>(bos=devre disi)</span></label>"
"<div style='display:flex;gap:.4rem;align-items:center'>"
"<input id=cs placeholder='London,GB' style=margin:0 oninput=\"cc('s')\">"
"<span id=cs-st style='font-size:1rem;width:18px;flex-shrink:0'></span></div>"
"<div id=cs-msg style='font-size:.72rem;min-height:.9rem;margin-bottom:.4rem'></div>"
"<label>OWM API Key</label>"
"<div style='display:flex;gap:.4rem;align-items:center'>"
"<input id=ok placeholder='OpenWeatherMap API anahtariniz' style=margin:0>"
"<button class='btn bp' onclick='revalidate()' style='white-space:nowrap;padding:.45rem .7rem;font-size:.75rem'>Dogrula</button>"
"</div>"
"<div id=ok-msg style='font-size:.72rem;min-height:.9rem;margin-bottom:.4rem'></div>"
"<label>UTC Ofseti (saat)</label>"
"<input id=tz type=number min=-12 max=14 value=3>"
"<button id=save-btn class='btn bp' onclick='saveSettings()' style='margin-top:.4rem;opacity:.4' disabled>Kaydet</button>"
"<div id=save-msg style='font-size:.72rem;margin-top:.3rem;min-height:.9rem'></div></div>";

static const char HTML_COUNTDOWN[] PROGMEM =
"<div class=card><h2>Geri Sayim</h2>"
"<div class=row>"
"<div style=flex:1><label>Saat</label><input id=ch type=number min=0 max=23 value=0></div>"
"<div style=flex:1><label>Dakika</label><input id=cm type=number min=0 max=59 value=5></div>"
"<div style=flex:1><label>Saniye</label><input id=cs2 type=number min=0 max=59 value=0></div>"
"</div><div class=row>"
"<button class='btn bs' onclick='startCountdown()'>Baslat</button>"
"<button class='btn bp' onclick='stopCountdown()'>Durdur</button>"
"<button class='btn bd' onclick='resetCountdown()'>Sifirla</button>"
"</div></div>";

static const char HTML_SLEEP[] PROGMEM =
"<div class=card><h2>Uyku Modu</h2>"
"<label>Hareketsizlik suresi (saniye, 0=devre disi)</label>"
"<input id=sl type=number min=0 max=3600 value=300>"
"<button class='btn bp' onclick='saveSleep()' style=margin-top:.4rem>Kaydet</button></div>";

static const char HTML_STATUS[] PROGMEM =
"<div class=card><h2>Cihaz Durumu</h2>"
"<div id=sb class=st>Yukleniyor...</div>"
"<button class='btn bd' onclick='reboot()' style='margin-top:.5rem;font-size:.75rem'>"
"Yeniden Baslat</button></div>";

static const char HTML_JS1[] PROGMEM =
"<script>"
"async function api(u,m,b){"
"const o={method:m||'GET',headers:{'Content-Type':'application/x-www-form-urlencoded'}};"
"if(b)o.body=new URLSearchParams(b).toString();"
"const r=await fetch(u,o);return r.text();}"
"function sigBar(rssi){"
"const l=rssi>=-50?4:rssi>=-65?3:rssi>=-75?2:rssi>=-85?1:0;"
"const ht=['4px','7px','10px','13px'];"
"let h='<div class=sig>';"
"for(let i=0;i<4;i++)h+='<span style=height:'+ht[i]+(i<l?' class=on':'')+'></span>';"
"return h+'</div>';}"
"function lockIco(e){return e?'&#128274;':'<span style=color:#555>&#128275;</span>';}"
"async function scanWifi(){"
"document.getElementById('scanbtn').disabled=true;"
"document.getElementById('spin').style.display='block';"
"document.getElementById('scan-info').textContent='Taranıyor...';"
"document.getElementById('scl').style.display='none';cancelSel();"
"try{"
"const nets=await(await fetch('/wifi/scan')).json();"
"const ul=document.getElementById('scl');"
"ul.innerHTML=nets.length?nets.map(n=>"
"\"<div class='sc-item' onclick='selNet(\\\"\" +n.ssid.replace(/\\\"/g,'&quot;')+ \"\\\",\"+n.enc+\")'>\""
"+\"<span class='sc-ssid'>\"+n.ssid+\"</span>\""
"+\"<span class='sc-meta'>\"+sigBar(n.rssi)+lockIco(n.enc)+\"</span></div>\").join('')"
":'<div style=padding:.5rem;color:gray>Ag bulunamadi</div>';"
"ul.style.display='block';"
"document.getElementById('scan-info').textContent=nets.length+' ag bulundu';"
"}catch(e){document.getElementById('scan-info').textContent='Hata:'+e.message;}"
"document.getElementById('scanbtn').disabled=false;"
"document.getElementById('spin').style.display='none';}"
"let _ss='';"
"function selNet(s,e){_ss=s;"
"document.getElementById('sel-ssid').textContent=s+(e?' (sifre gerekli)':' (acik ag)');"
"document.getElementById('np').value='';"
"document.getElementById('pwbox').style.display='block';"
"document.getElementById('np').focus();}"
"function cancelSel(){_ss='';"
"document.getElementById('pwbox').style.display='none';"
"document.getElementById('np').value='';}"
"async function addWifi(){if(!_ss)return alert('Once bir ag secin');"
"const r=await api('/wifi/add','POST',{ssid:_ss,pass:document.getElementById('np').value});"
"if(r==='OK'){cancelSel();document.getElementById('scl').style.display='none';"
"document.getElementById('scan-info').textContent='';loadStatus();}else alert('Hata:'+r);}"
"async function addWifiManual(){"
"const s=document.getElementById('ns').value.trim();"
"if(!s)return alert('SSID gerekli');"
"const r=await api('/wifi/add','POST',{ssid:s,pass:document.getElementById('npm').value});"
"if(r==='OK'){document.getElementById('ns').value='';document.getElementById('npm').value='';loadStatus();}"
"else alert('Hata:'+r);}"
"async function delWifi(i){if(!confirm('Bu agi silmek istiyor musunuz?'))return;"
"await api('/wifi/delete','POST',{idx:i});loadStatus();}";

static const char HTML_JS2[] PROGMEM =
"async function loadStatus(){"
"try{"
"const d=await(await fetch('/status')).json();"
"document.getElementById('wl').innerHTML="
"d.wifi.map((w,i)=>\"<div class='we'><span class='ws'>\"+w"
"+\"</span><button class='btn bd' onclick='delWifi(\"+i+\")' style='padding:.25rem .5rem'>Sil</button></div>\").join('')"
"||'<span style=color:gray;font-size:.8rem>Kayitli ag yok</span>';"
// Sadece odakta olmayan alanlari guncelle - kullanici yazarken ezme
"const act=document.activeElement;"
"if(act!==document.getElementById('cp'))document.getElementById('cp').value=d.cityP||'';"
"if(act!==document.getElementById('cs'))document.getElementById('cs').value=d.cityS||'';"
"if(act!==document.getElementById('ok'))document.getElementById('ok').value=d.owmKey||'';"
"if(act!==document.getElementById('tz'))document.getElementById('tz').value=d.tzH||3;"
"const sl=document.getElementById('sl');"
"if(sl&&act!==sl)sl.value=d.sleepS||300;"
"if(d.alarms)d.alarms.forEach((a,i)=>{"
"const ah=document.getElementById('ah'+i);"
"const am=document.getElementById('am'+i);"
// Dirty flag: kullanici bu alani degistirdiyse sunucudan gelen deger ezilmez
"if(ah&&!ah.dataset.dirty)ah.value=a.h;"
"if(am&&!am.dataset.dirty)am.value=a.m;"
"const st=document.getElementById('alst'+i);"
"if(st)st.innerHTML=a.enabled"
"?'<span style=color:#39ff14>AKTIF '+String(a.h).padStart(2,'0')+':'+String(a.m).padStart(2,'0')+'</span>'"
":'<span style=color:#555>Kapali</span>';});"
"document.getElementById('sb').innerHTML="
"'IP:'+d.ip+' WiFi:'+(d.connected?'<b style=color:lime>Bagli</b>':'<b style=color:red>AP</b>')"
"+'  Heap:'+d.heap+' Uptime:'+d.uptime+'s';"
// Ilk yuklemede: once API key'i yaz, sonra sehirleri dogrula
"if(!_loaded){"
"_loaded=true;"
// API key'i once yaz - dogrulama buna bagli
"const okEl=document.getElementById('ok');"
"if(d.owmKey)okEl.value=d.owmKey;"
// Sehirleri yaz ve API key varsa dogrula
"if(d.cityP){document.getElementById('cp').value=d.cityP;cc('p');}"
"else{_cv.p={ok:false};upBtn();}"
"if(d.cityS){document.getElementById('cs').value=d.cityS;cc('s');}"
"else{_cv.s={ok:true};upBtn();}}"
"}catch(e){document.getElementById('sb').textContent='Baglanti hatasi:'+e.message;}}"
"let _loaded=false;"
"const _cv={p:{ok:false},s:{ok:true}},_ct={p:null,s:null};"
// Sehir alani degisince tetiklenir
"function cc(k){"
"clearTimeout(_ct[k]);"
"const v=document.getElementById(k==='p'?'cp':'cs').value.trim();"
"const st=document.getElementById(k==='p'?'cp-st':'cs-st');"
"const mg=document.getElementById(k==='p'?'cp-msg':'cs-msg');"
"if(k==='s'&&!v){_cv.s={ok:true};st.textContent='';mg.textContent='';upBtn();return;}"
"if(v.length<2){_cv[k]={ok:false};st.textContent='';mg.textContent='';upBtn();return;}"
"st.textContent='...';mg.textContent='API key girilince dogrulanacak';mg.style.color='#888';"
"_cv[k]={ok:false};upBtn();"
// API key varsa hemen dogrula, yoksa Dogrula butonunu bekle
"const key=document.getElementById('ok').value.trim();"
"if(key.length>10)_ct[k]=setTimeout(()=>dv(k,v),800);}"
// Dogrula butonu: her iki sehri de API key ile dogrula
"function revalidate(){"
"const key=document.getElementById('ok').value.trim();"
"const om=document.getElementById('ok-msg');"
"if(key.length<10){om.style.color='#ff8c00';om.textContent='Gecerli bir API key girin (en az 10 karakter)';return;}"
"om.style.color='#888';om.textContent='Dogrulanıyor...';"
"const cp=document.getElementById('cp').value.trim();"
"const cs=document.getElementById('cs').value.trim();"
"let pending=0;"
"if(cp.length>=2){pending++;dv('p',cp).then(()=>{if(!--pending)om.textContent='';});}"
"else{_cv.p={ok:false};upBtn();}"
"if(cs.length>=2){pending++;dv('s',cs).then(()=>{if(!--pending)om.textContent='';});}"
"else{_cv.s={ok:true};upBtn();}"
"if(!pending)om.textContent='Sehir alanlari bos';}"
"async function dv(k,city){"
// Her zaman sayfadaki API key alanindaki degeri kullan
"const key=document.getElementById('ok').value.trim();"
"const st=document.getElementById(k==='p'?'cp-st':'cs-st');"
"const mg=document.getElementById(k==='p'?'cp-msg':'cs-msg');"
"if(!key||key.length<10){"
"st.textContent='';"
"mg.style.color='#ff8c00';"
"mg.textContent='Once API key girin, sonra Dogrula butonuna basin';"
"_cv[k]={ok:false};upBtn();return Promise.resolve();}"
"st.textContent='...';mg.textContent='Sorgulanıyor...';mg.style.color='#888';"
"try{"
"const url='/city/validate?city='+encodeURIComponent(city)+'&key='+encodeURIComponent(key);"
"const d=await(await fetch(url)).json();"
"if(d.found){"
"_cv[k]={ok:true};"
"st.innerHTML='<span style=color:#39ff14>&#10003;</span>';"
"mg.style.color='#39ff14';"
"mg.textContent=d.name+', '+d.country+' ('+d.lat.toFixed(2)+', '+d.lon.toFixed(2)+')';}"
"else{"
"_cv[k]={ok:false};"
"st.innerHTML='<span style=color:#ff2400>&#10007;</span>';"
"mg.style.color='#ff2400';"
"mg.textContent=d.error==='no_key'?'Gecersiz API key':'Bulunamadi. Ornek: Istanbul,TR';}"
"}catch(e){"
"st.textContent='';"
"mg.style.color='#ff8c00';"
"mg.textContent='Baglanti hatasi: '+e.message;"
"_cv[k]={ok:false};}"
"upBtn();}"
"function upBtn(){const ok=_cv.p.ok&&_cv.s.ok;const b=document.getElementById('save-btn');"
"b.disabled=!ok;b.style.opacity=ok?'1':'0.4';}";

static const char HTML_JS3[] PROGMEM =
"async function saveSettings(){"
"if(!_cv.p.ok||!_cv.s.ok){"
"alert('Lutfen once sehirleri dogrulayin (Dogrula butonuna basin).');return;}"
"const r=await api('/settings','POST',{"
"cityP:document.getElementById('cp').value.trim(),"
"cityS:document.getElementById('cs').value.trim(),"
"owmKey:document.getElementById('ok').value.trim(),"
"tzH:document.getElementById('tz').value});"
"const m=document.getElementById('save-msg');"
"if(r==='OK'){m.style.color='#39ff14';m.textContent='Ayarlar kaydedildi!';}"
"else{m.style.color='#ff2400';m.textContent='Hata: '+r;}}"
"async function setAlarm(idx,en){"
"const ah=document.getElementById('ah'+idx);"
"const am=document.getElementById('am'+idx);"
// Aralik kontrolu: sadece 0-23 ve 0-59
"const h=Math.min(23,Math.max(0,parseInt(ah.value)||0));"
"const m=Math.min(59,Math.max(0,parseInt(am.value)||0));"
"ah.value=h;am.value=m;"
"const r=await api('/alarm/set','POST',{idx,h,m,enable:en?1:0});"
"if(r==='OK'){"
// Basarili: dirty flag temizle, loadStatus artik bu alanlari guncelleyebilir
"ah.dataset.dirty='';"
"am.dataset.dirty='';"
"loadStatus();}"
"else alert('Hata: '+r);}"
"async function startCountdown(){"
"const h=parseInt(document.getElementById('ch').value)||0;"
"const m=parseInt(document.getElementById('cm').value)||0;"
"const s=parseInt(document.getElementById('cs2').value)||0;"
"await api('/countdown/set','POST',{ms:(h*3600+m*60+s)*1000,action:'start'});}"
"async function stopCountdown(){await api('/countdown/set','POST',{ms:0,action:'stop'});}"
"async function resetCountdown(){await api('/countdown/set','POST',{ms:0,action:'reset'});}"
"async function saveSleep(){"
"await api('/sleep/set','POST',{seconds:parseInt(document.getElementById('sl').value)||0});"
"alert('Kaydedildi!');}"
"async function reboot(){if(!confirm('Cihaz yeniden baslatilacak. Emin misiniz?'))return;"
"await api('/reboot','POST');alert('Yeniden baslatiliyor...');}"
"loadStatus();setInterval(loadStatus,6000);"
"</script></body></html>";

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
        server.on("/sleep/set",    HTTP_POST, [this]() { handleSleepSet(); });
        server.on("/status",       HTTP_GET,  [this]() { handleStatus(); });
        server.on("/reboot",       HTTP_POST, [this]() { handleReboot(); });
        server.onNotFound([this]() { server.send(404, "text/plain", "Not found"); });
        server.begin();
        Serial.println("[WebServer] Started on port 80");
    }

    void loop() { server.handleClient(); }

private:
    // ---- Alarm HTML dinamik olarak uretilir (3 yuva) ----
    String buildAlarmCard() {
        String s = "<div class=card><h2>Alarm</h2>";
        for (int i = 0; i < 3; i++) {
            s += "<div style='background:#0a0f1e;border-radius:7px;padding:.55rem .7rem;"
                 "margin-bottom:.45rem;border:1px solid #1e2740'>"
                 "<div style='display:flex;align-items:center;justify-content:space-between;gap:.4rem'>"
                 "<span style='color:#82EEFD;font-size:.8rem;min-width:38px'>ALM";
            s += String(i + 1);
            s += "</span>"
                 "<div style='display:flex;gap:.25rem;align-items:center;flex:1'>"
                 // Saat: 0-23, sadece aralik kontrolu, baska dogrulama yok
                 // oninput ile dirty flag set edilir - loadStatus ezmeyi durdurur
                 "<input id=ah";
            s += String(i);
            s += " type=number min=0 max=23 value=7 "
                 "style='margin:0;width:52px;text-align:center' "
                 "oninput='this.dataset.dirty=1'>"
                 "<span style=color:#555>:</span>"
                 // Dakika: 0-59
                 "<input id=am";
            s += String(i);
            s += " type=number min=0 max=59 value=0 "
                 "style='margin:0;width:52px;text-align:center' "
                 "oninput='this.dataset.dirty=1'>"
                 "</div>"
                 "<div style='display:flex;gap:.25rem'>"
                 "<button class='btn bs' onclick='setAlarm(";
            s += String(i);
            s += ",true)' style='padding:.25rem .5rem;font-size:.75rem'>AC</button>"
                 "<button class='btn bd' onclick='setAlarm(";
            s += String(i);
            s += ",false)' style='padding:.25rem .5rem;font-size:.75rem'>KAPAT</button>"
                 "</div></div>"
                 "<div id=alst";
            s += String(i);
            s += " style='font-size:.7rem;color:#555;margin-top:.25rem'>-</div>"
                 "</div>";
        }
        s += "</div>";
        return s;
    }

    // ---- Ana sayfa - Content-Length ile gonderim ----
    void handleRoot() {
        // Tum statik parcalari PROGMEM'den String'e kopyala
        String page;

        // Tahmini boyut: ~9KB - reserve ile heap parcalanmasini onle
        page.reserve(9500);

        page += FPSTR(HTML_HEAD);
        page += FPSTR(HTML_WIFI);
        page += FPSTR(HTML_SETTINGS);
        page += buildAlarmCard();   // dinamik (3 yuva)
        page += FPSTR(HTML_COUNTDOWN);
        page += FPSTR(HTML_SLEEP);
        page += FPSTR(HTML_STATUS);
        page += FPSTR(HTML_JS1);
        page += FPSTR(HTML_JS2);
        page += FPSTR(HTML_JS3);

        // Content-Length ile tek seferde gonder - tarayici beklemiyor
        server.send(200, "text/html; charset=utf-8", page);
    }

    // ---- WiFi tarama ----
    void handleWifiScan() {
        int n = WiFi.scanNetworks(false, false);
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();
        if (n > 0) {
            for (int i = 0; i < n; i++) {
                String ssid = WiFi.SSID(i);
                if (ssid.length() == 0) continue;
                bool dup = false;
                for (JsonObject o : arr)
                    if (o["ssid"].as<String>() == ssid) { dup = true; break; }
                if (dup) continue;
                JsonObject net = arr.add<JsonObject>();
                net["ssid"] = ssid;
                net["rssi"] = WiFi.RSSI(i);
                net["enc"]  = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN) ? 1 : 0;
            }
        }
        WiFi.scanDelete();
        String out; serializeJson(doc, out);
        server.send(200, "application/json", out);
    }

    // ---- API endpoints ----
    void handleWifiAdd() {
        String ssid = server.arg("ssid");
        String pass = server.arg("pass");
        if (ssid.length() == 0)                    { server.send(400, "text/plain", "No SSID"); return; }
        if (storage.cfg.wifiCount >= MAX_WIFI_NETWORKS) { server.send(400, "text/plain", "Max 5 network"); return; }
        storage.addWifi(ssid.c_str(), pass.c_str());
        server.send(200, "text/plain", "OK");
    }

    void handleWifiUpdate() {
        uint8_t idx = (uint8_t)server.arg("idx").toInt();
        storage.updateWifi(idx, server.arg("ssid").c_str(), server.arg("pass").c_str());
        server.send(200, "text/plain", "OK");
    }

    void handleWifiDelete() {
        storage.deleteWifi((uint8_t)server.arg("idx").toInt());
        server.send(200, "text/plain", "OK");
    }

    void handleCityValidate() {
        String city   = server.arg("city");
        String apiKey = server.arg("key");
        if (apiKey.length() == 0 && storage.cfg.owmApiKey[0])
            apiKey = String(storage.cfg.owmApiKey);
        if (city.length() == 0 || apiKey.length() == 0) {
            server.send(200, "application/json", "{\"found\":false}"); return;
        }
        HTTPClient http;
        char url[256];
        snprintf(url, sizeof(url),
            "http://api.openweathermap.org/geo/1.0/direct?q=%s&limit=1&appid=%s",
            urlEncode(city).c_str(), apiKey.c_str());
        http.begin(url); http.setTimeout(8000);
        int code = http.GET();
        if (code != 200) { http.end();
            char err[48]; snprintf(err, sizeof(err), "{\"found\":false,\"error\":\"http_%d\"}", code);
            server.send(200, "application/json", String(err)); return;
        }
        String body = http.getString(); http.end();
        if (body.length() < 5 || body == "[]") {
            server.send(200, "application/json", "{\"found\":false}"); return;
        }
        JsonDocument doc;
        if (deserializeJson(doc, body) || !doc.is<JsonArray>() || doc.as<JsonArray>().size() == 0) {
            server.send(200, "application/json", "{\"found\":false,\"error\":\"parse\"}"); return;
        }
        JsonObject r = doc[0].as<JsonObject>();
        JsonDocument resp;
        resp["found"]   = true;
        resp["name"]    = r["name"]    | "";
        resp["country"] = r["country"] | "";
        resp["lat"]     = r["lat"]     | 0.0f;
        resp["lon"]     = r["lon"]     | 0.0f;
        String out; serializeJson(resp, out);
        server.send(200, "application/json", out);
    }

    void handleSettings() {
        String cityP  = server.arg("cityP");
        String cityS  = server.arg("cityS");
        String owmKey = server.arg("owmKey");
        int8_t tzH    = (int8_t)server.arg("tzH").toInt();
        if (cityP.length())  strncpy(storage.cfg.cityPrimary,   cityP.c_str(),  32);
        if (cityS.length())  strncpy(storage.cfg.citySecondary, cityS.c_str(),  32);
        else                 memset(storage.cfg.citySecondary, 0, 33);
        if (owmKey.length()) strncpy(storage.cfg.owmApiKey, owmKey.c_str(), 64);
        storage.cfg.tzOffsetHours = tzH;
        storage.save();
        // Timezone degistiyse yeniden uygula
        timeManager.applyTimezone();
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
        if (action == "start") { if (ms > 0) alarmMgr.countdownSet(ms); alarmMgr.countdownStart(); }
        else if (action == "stop")  alarmMgr.countdownStop();
        else if (action == "reset") alarmMgr.countdownReset();
        server.send(200, "text/plain", "OK");
    }

    void handleSleepSet() {
        uint32_t s = (uint32_t)server.arg("seconds").toInt();
        storage.cfg.sleepTimeoutS = s;
        storage.save();
        sleepMgr.setTimeoutSeconds(s);
        server.send(200, "text/plain", "OK");
    }

    void handleReboot() {
        server.send(200, "text/plain", "Rebooting...");
        delay(500);
        ESP.restart();
    }

    void handleStatus() {
        JsonDocument doc;
        JsonArray wifi = doc["wifi"].to<JsonArray>();
        for (uint8_t i = 0; i < storage.cfg.wifiCount; i++)
            wifi.add(storage.cfg.wifiList[i].ssid);
        doc["cityP"]     = storage.cfg.cityPrimary;
        doc["cityS"]     = storage.cfg.citySecondary;
        doc["owmKey"]    = storage.cfg.owmApiKey;
        doc["tzH"]       = storage.cfg.tzOffsetHours;
        doc["connected"] = (WiFi.status() == WL_CONNECTED);
        doc["ip"]        = wifiManager.localIPString();
        doc["heap"]      = ESP.getFreeHeap();
        doc["uptime"]    = millis() / 1000;
        doc["sleepS"]    = storage.cfg.sleepTimeoutS;
        JsonArray alarms = doc["alarms"].to<JsonArray>();
        for (uint8_t i = 0; i < MAX_ALARMS; i++) {
            JsonObject a = alarms.add<JsonObject>();
            a["h"]       = alarmMgr.alarms[i].hour;
            a["m"]       = alarmMgr.alarms[i].minute;
            a["enabled"] = alarmMgr.alarms[i].enabled;
        }
        String out; serializeJson(doc, out);
        server.send(200, "application/json", out);
    }

    String urlEncode(const String& src) {
        String out;
        for (int i = 0; i < (int)src.length(); i++) {
            char c = src[i];
            if (isAlphaNumeric(c) || c=='-' || c=='_' || c=='.' || c==',') out += c;
            else { char buf[4]; snprintf(buf, sizeof(buf), "%%%02X", (uint8_t)c); out += buf; }
        }
        return out;
    }
};

extern ConfigWebServer webServer;
