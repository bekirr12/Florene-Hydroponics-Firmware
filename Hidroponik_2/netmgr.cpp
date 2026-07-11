// ============================================================
//  NETMGR.CPP - WiFi baglanti yonetimi, captive portal, NTP
//  - Event-driven baglanti (WiFi.onEvent) + exponential backoff
//  - WiFiManager non-blocking portal (Turkce arayuz + logo)
//  - ESP32-S3 dahili RTC'yi NTP (SNTP) ile senkronize eder
// ============================================================

#include <WiFi.h>
#include <WiFiManager.h>
#include "esp_mac.h"
#include <time.h>

#include "config.h"
#include "secrets.h"
#include "state.h"
#include "netmgr.h"
#include "logo.h"

char deviceID[7] = "";

static WiFiManager wifiManager;

// Baglanti/portal durumu (yalnizca NetworkTask + event context erisir)
static volatile bool s_justConnected = false;
static bool     s_portalActive  = false;
static uint32_t s_portalStart   = 0;
static uint32_t s_lastAttempt   = 0;
static uint32_t s_backoff       = WIFI_BACKOFF_MIN_MS;
static char     s_apMessage[300];

// NTP
static uint32_t s_lastNTPSync = 0;

// ==================== CIHAZ KIMLIGI ====================
static void initDeviceID() {
  uint8_t mac[6];
  esp_efuse_mac_get_default(mac);
  snprintf(deviceID, sizeof(deviceID), "%02X%02X%02X", mac[3], mac[4], mac[5]);
}

void printMAC() {
  uint8_t mac[6];
  esp_efuse_mac_get_default(mac);
  LOG("INFO", "MAC Adres: %02X:%02X:%02X:%02X:%02X:%02X",
      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

// ==================== LOGO ENDPOINT ====================
// Portal acikken /logo.png istegini PROGMEM'den stream et
static void serveLogo() {
  wifiManager.server->sendHeader("Content-Type", "image/png");
  wifiManager.server->sendHeader("Content-Length", String(logo_png_len));
  wifiManager.server->sendHeader("Cache-Control", "max-age=3600");
  wifiManager.server->setContentLength(logo_png_len);
  wifiManager.server->send(200);

  WiFiClient client = wifiManager.server->client();
  const size_t chunkSize = 1024;
  uint8_t buf[chunkSize];
  size_t sent = 0;
  while (sent < logo_png_len) {
    size_t toSend = min(chunkSize, (size_t)(logo_png_len - sent));
    memcpy_P(buf, logo_png + sent, toSend);
    client.write(buf, toSend);
    sent += toSend;
  }
  LOG("LOGO", "/logo.png servis edildi");
}

// Portal JS'inin yokladigi baglanti durumu (JSON)
static void serveStatus() {
  bool connected = (WiFi.status() == WL_CONNECTED);
  char json[192];
  snprintf(json, sizeof(json),
    "{\"connected\":%s,\"ip\":\"%s\",\"ssid\":\"%s\"}",
    connected ? "true" : "false",
    connected ? WiFi.localIP().toString().c_str() : "",
    WiFi.SSID().c_str());
  wifiManager.server->send(200, "application/json", json);
}

// Cevredeki WiFi aglarini JSON olarak dondurur (async tarama, bloklamaz).
//   {"scanning":true}                                  -> tarama devam ediyor
//   [{"ssid":"..","rssi":-60,"enc":true}, ...]         -> sonuclar hazir
static void serveScan() {
  bool refresh = wifiManager.server->hasArg("refresh");
  int n = WiFi.scanComplete();

  if (refresh && n != WIFI_SCAN_RUNNING) {
    WiFi.scanDelete();
    n = WIFI_SCAN_FAILED;  // yeni tarama tetiklensin
  }

  if (n == WIFI_SCAN_RUNNING) {
    wifiManager.server->send(200, "application/json", "{\"scanning\":true}");
    return;
  }
  if (n < 0) {                       // henuz taranmadi -> async baslat
    WiFi.scanNetworks(true);
    wifiManager.server->send(200, "application/json", "{\"scanning\":true}");
    return;
  }

  // n >= 0: sonuclar hazir -> JSON dizisi
  String out = "[";
  for (int i = 0; i < n; i++) {
    if (i) out += ",";
    String ssid = WiFi.SSID(i);
    ssid.replace("\\", "\\\\");
    ssid.replace("\"", "\\\"");
    out += "{\"ssid\":\"" + ssid + "\",\"rssi\":" + String(WiFi.RSSI(i)) +
           ",\"enc\":" + String(WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "false" : "true") + "}";
  }
  out += "]";
  wifiManager.server->send(200, "application/json", out);
}

static void onAPStarted() {
  wifiManager.server->on("/logo.png", HTTP_GET, serveLogo);
  wifiManager.server->on("/status",   HTTP_GET, serveStatus);
  wifiManager.server->on("/scan",     HTTP_GET, serveScan);
  LOG("PORTAL", "/logo.png + /status + /scan endpoint hazir");
}

// ==================== WiFi EVENT HANDLER ====================
static void onWiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP: {
      stateLock();
      g_state.net.wifiConnected = true;
      stateUnlock();
      s_justConnected = true;
      s_backoff = WIFI_BACKOFF_MIN_MS;
      LOG("WIFI", "Baglanti kuruldu | Ag: %s | IP: %s | Sinyal: %d dBm",
          WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(), WiFi.RSSI());
      break;
    }
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED: {
      stateLock();
      bool wasConn = g_state.net.wifiConnected;
      g_state.net.wifiConnected = false;
      stateUnlock();
      if (wasConn) LOG("WIFI", "Baglanti koptu (roleler son durumda korunur)");
      break;
    }
    default:
      break;
  }
}

// ==================== WiFiManager KURULUMU ====================
void netInit() {
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);   // yeniden baglanmayi biz yonetiriz (backoff)
  WiFi.onEvent(onWiFiEvent);

  initDeviceID();

  wifiManager.setConfigPortalBlocking(false);
  // Baglanti kurulunca AP'yi WiFiManager KENDISI kapatmasin; telefonun
  // /status yoklamasi 'basarili' overlay'i gorebilsin diye biz kapatacagiz.
  wifiManager.setDisableConfigPortal(false);
  wifiManager.setConfigPortalTimeout(AP_TIMEOUT);
  wifiManager.setConnectTimeout(WIFI_CONNECT_TIMEOUT_S);
  wifiManager.setTitle("Florene");
  wifiManager.setDarkMode(false);

  // "wifinoscan" -> /0wifi: sayfa taramasiz, ANINDA acilir. Ag listesini
  // kendi /scan endpoint'imizle async doldururuz (bloklayan tarama yok).
  std::vector<const char*> menu = {"wifinoscan", "exit"};
  wifiManager.setMenu(menu);

  // Turkce arayuz + logo inject + basari overlay (CSS/JS)
  wifiManager.setCustomHeadElement(
    "<style>"
    "body{font-family:'Segoe UI',Arial,sans-serif;background:#FEF8EE!important;color:#000!important;}"
    ".wrap{max-width:380px;padding:0 16px;}"
    "h1{font-size:1.6em;text-align:center;margin:8px 0 4px;color:#2E7D32;}"
    "h3{text-align:center;font-weight:400;color:#000;margin:0 0 12px;font-size:0.85em;}"
    ".msg{background:#F5F1E8;border-left:4px solid #2E7D32;padding:12px 16px;border-radius:0 8px 8px 0;margin:12px 0;font-size:0.9em;}"
    ".msg b{color:#2E7D32;}"
    "button,input[type='submit']{background:#2E7D32!important;border:none!important;border-radius:20px!important;padding:12px!important;font-size:1em!important;font-weight:600!important;cursor:pointer;color:#fff!important;width:100%;margin-top:8px;}"
    "button:hover,input[type='submit']:hover{background:#1B5E20!important;}"
    "input[type='text'],input[type='password']{border:2px solid #DDD7CC!important;border-radius:10px!important;padding:10px 14px!important;font-size:0.95em!important;background:#FAF6EF!important;color:#000!important;width:100%;box-sizing:border-box;}"
    "input:focus{border-color:#2E7D32!important;outline:none!important;}"
    "a{color:#2E7D32!important;text-decoration:none!important;}"
    ".q{color:#2E7D32!important;}"
    "label{display:block;margin:12px 0 4px;color:#000;font-size:0.9em;font-weight:600;}"
    ".wifi-list li{border-bottom:1px solid #D8CFBC;padding:10px 4px;}"
    ".wifi-list li:hover{background:#EDE8DF;}"
    ".wifi-ssid{color:#000;font-weight:500;}"
    ".wifi-signal{display:none!important;}"
    "nav{display:none!important;}"
    ".logo{display:none!important;}"
    "#success-overlay{display:none;position:fixed;top:0;left:0;width:100%;height:100%;"
    "background:#FEF8EE;z-index:9999;justify-content:center;align-items:center;flex-direction:column;}"
    "#success-overlay.show{display:flex;}"
    ".success-icon{font-size:4em;margin-bottom:16px;}"
    ".success-title{font-size:1.4em;font-weight:700;color:#2E7D32;margin-bottom:8px;}"
    ".success-sub{font-size:0.9em;color:#555;text-align:center;}"
    ".florene-logo{width:140px;display:block;margin:0 auto 12px;}"
    ".spinner{width:44px;height:44px;border:5px solid #DDD7CC;border-top-color:#2E7D32;border-radius:50%;animation:spin 1s linear infinite;margin:0 auto;}"
    "@keyframes spin{to{transform:rotate(360deg);}}"
    ".spinner-sm{width:18px;height:18px;border-width:3px;margin:0;}"
    ".scanbox{margin:10px 0;}"
    ".scan-status{display:flex;align-items:center;gap:8px;justify-content:center;font-size:0.85em;color:#555;margin:8px 0;}"
    ".scan-status a{font-weight:600;}"
    ".scanlist{list-style:none;padding:0;margin:6px 0;border:1px solid #D8CFBC;border-radius:10px;overflow:hidden;}"
    ".scanlist li{padding:12px 14px;border-bottom:1px solid #EDE8DF;cursor:pointer;color:#000;display:flex;justify-content:space-between;align-items:center;}"
    ".scanlist li:last-child{border-bottom:none;}"
    ".scanlist li:hover,.scanlist li:active{background:#EDE8DF;}"
    ".scanlist .sig{font-size:0.8em;color:#888;}"
    "</style>"

    "<script>"
    "var TR={"
    "'Configure WiFi (No scan)':'WiFi Ayarla',"
    "'Configure WiFi':'WiFi Ayarla',"
    "'Exit':'Cikis',"
    "'Refresh':'Yenile',"
    "'Scanning':'Taraniyor...',"
    "'SSID':'WiFi Adi',"
    "'Show Password':'Sifreyi Goster',"
    "'Password':'Sifre',"
    "'WiFi Scan':'WiFi Tara',"
    "'Save':'Baglan',"
    "'Back':'Geri',"
    "'Delete':'Sil',"
    "'Update':'Guncelle',"
    "'Info':'Bilgi',"
    "'Close':'Kapat',"
    "'Connect':'Baglan',"
    "'Disconnect':'Baglantiyi Kes',"
    "'No networks found.':'Ag bulunamadi.',"
    "'Scan done':'Tarama tamamlandi',"
    "'networks found':'ag bulundu',"
    "'Click here to configure WiFi':'WiFi ayarlamak icin tiklayin',"
    "'Not connected to':'Baglanti Basarisiz:',"
    "'Connected to':'Baglanti Basarili:',"
    "'No AP set':'Ag secilmedi',"
    "'Saving Credentials':'Bilgiler kaydediliyor',"
    "'Trying to connect ESP to network.':'Cihaz aga baglaniyor.',"
    "'If it fails reconnect to AP to try again':'Basarisiz olursa AP agina tekrar baglanip deneyin',"
    "'IP address':'IP adresi',"
    "'Restart':'Yeniden Baslat',"
    "'Erase':'Sil'"
    "};"
    "function translateText(root){"
    "  var w=document.createTreeWalker(root||document.body,NodeFilter.SHOW_TEXT);"
    "  var n;"
    "  while(n=w.nextNode()){"
    "    for(var k in TR){"
    "      if(n.nodeValue.indexOf(k)>-1) n.nodeValue=n.nodeValue.replace(k,TR[k]);"
    "    }"
    "  }"
    "}"
    "function translateAttrs(root){"
    "  var el=root||document;"
    "  el.querySelectorAll('input[type=submit],input[type=button],button').forEach(function(b){"
    "    if(b.value&&TR[b.value]) b.value=TR[b.value];"
    "    if(b.innerText&&TR[b.innerText.trim()]) b.innerText=TR[b.innerText.trim()];"
    "  });"
    "  el.querySelectorAll('input[placeholder]').forEach(function(i){"
    "    if(TR[i.placeholder]) i.placeholder=TR[i.placeholder];"
    "  });"
    "}"
    "function injectLogo(){"
    "  var wrap=document.querySelector('.wrap')||document.querySelector('form')||document.body;"
    "  if(document.querySelector('.florene-logo')) return;"
    "  var img=document.createElement('img');"
    "  img.src='/logo.png';"
    "  img.className='florene-logo';"
    "  img.onerror=function(){this.style.display='none';};"
    "  wrap.insertBefore(img,wrap.firstChild);"
    "}"
    "function setOverlay(icon,title,sub){"
    "  var ov=document.getElementById('success-overlay');"
    "  ov.innerHTML='<img src=\"/logo.png\" class=\"florene-logo\">'"
    "    +'<div class=\"success-icon\">'+icon+'</div>'"
    "    +'<div class=\"success-title\">'+title+'</div>'"
    "    +'<div class=\"success-sub\">'+sub+'</div>';"
    "  ov.className='show';"
    "}"
    "function pollStatus(tries){"
    "  fetch('/status').then(function(r){return r.json();}).then(function(d){"
    "    if(d.connected){"
    "      setOverlay('&#10003;','Baglanti Basarili!',(d.ssid||'')+'<br>IP: '+d.ip+'<br>Cihaz hazirlaniyor...');"
    "    }else if(tries>0){setTimeout(function(){pollStatus(tries-1);},1000);}"
    "    else{setOverlay('&#9888;','Baglanilamadi','Sifre yanlis olabilir.<br><a href=\"/\">Tekrar dene</a>');}"
    "  }).catch(function(){if(tries>0)setTimeout(function(){pollStatus(tries-1);},1000);});"
    "}"
    // ── Async ag listesi (/scan) ──
    "function sigLabel(r){return r>=-60?'Guclu':(r>=-75?'Orta':'Zayif');}"
    "function bindRescan(){var r=document.getElementById('rescan');if(r)r.addEventListener('click',function(e){e.preventDefault();startScan(true);});}"
    "function scanFail(){var b=document.getElementById('scanbox');if(b)b.innerHTML='<div class=\"scan-status\">Aglar taranamadi. <a href=\"#\" id=\"rescan\">Yenile</a></div>';bindRescan();}"
    "function renderScan(list){"
    "  var box=document.getElementById('scanbox'); if(!box) return;"
    "  var seen={},uniq=[];"
    "  list.forEach(function(a){if(a.ssid&&!(a.ssid in seen)){seen[a.ssid]=1;uniq.push(a);}});"
    "  uniq.sort(function(x,y){return y.rssi-x.rssi;});"
    "  if(!uniq.length){box.innerHTML='<div class=\"scan-status\">Ag bulunamadi. <a href=\"#\" id=\"rescan\">Yenile</a></div>';bindRescan();return;}"
    "  var h='<div class=\"scan-status\">Yakindaki aglar (dokunun):</div><ul class=\"scanlist\">';"
    "  uniq.forEach(function(a){var s=a.ssid.replace(/</g,'&lt;');h+='<li data-ssid=\"'+a.ssid.replace(/\"/g,'&quot;')+'\">'+(a.enc?'&#128274; ':'')+s+'<span class=\"sig\">'+sigLabel(a.rssi)+'</span></li>';});"
    "  h+='</ul><div class=\"scan-status\"><a href=\"#\" id=\"rescan\">Yenile</a></div>';"
    "  box.innerHTML=h;"
    "  box.querySelectorAll('.scanlist li').forEach(function(li){li.addEventListener('click',function(){var s=document.getElementById('s');if(s)s.value=li.getAttribute('data-ssid');var p=document.getElementById('p');if(p)p.focus();});});"
    "  bindRescan();"
    "}"
    "function pollScan(tries){"
    "  fetch('/scan').then(function(r){return r.json();}).then(function(d){"
    "    if(d&&d.scanning){if(tries>0)setTimeout(function(){pollScan(tries-1);},1500);else scanFail();}"
    "    else renderScan(d||[]);"
    "  }).catch(function(){if(tries>0)setTimeout(function(){pollScan(tries-1);},1500);else scanFail();});"
    "}"
    "function startScan(refresh){"
    "  var box=document.getElementById('scanbox');if(box)box.innerHTML='<div class=\"scan-status\"><div class=\"spinner spinner-sm\"></div>Aglar taraniyor...</div>';"
    "  fetch('/scan'+(refresh?'?refresh=1':'')).then(function(){pollScan(12);}).catch(function(){pollScan(12);});"
    "}"
    "function initScanList(){"
    "  if(!document.getElementById('s')) return;"
    "  var form=document.querySelector('form'); if(!form) return;"
    "  var box=document.createElement('div'); box.id='scanbox'; box.className='scanbox';"
    "  form.parentNode.insertBefore(box,form);"
    "  startScan(false);"
    "}"
    "var obs=new MutationObserver(function(muts){"
    "  muts.forEach(function(m){"
    "    m.addedNodes.forEach(function(n){"
    "      if(n.nodeType===1){ translateText(n); translateAttrs(n); }"
    "    });"
    "  });"
    "});"
    "document.addEventListener('DOMContentLoaded',function(){"
    "  translateText(); translateAttrs(); injectLogo();"
    "  obs.observe(document.body,{childList:true,subtree:true});"
    // Kaydet sonrasi sonuc sayfasi (/wifisave): tam ekran Turkce overlay ile
    // WiFiManager'in Ingilizce sayfasini ort ve gercek durumu /status'tan yokla.
    "  if(location.pathname.indexOf('wifisave')>-1){"
    "    setOverlay('<div class=\"spinner\"></div>','Baglaniliyor...','WiFi aginiza baglanmaya calisiliyor.<br>Lutfen bekleyin.');"
    "    pollStatus(15);"
    "  } else if(document.getElementById('s')){"
    "    initScanList();"  // /0wifi form sayfasi -> ag listesini async doldur
    "  }"
    "});"
    "</script>"
    "<div id='success-overlay'></div>"
  );

  wifiManager.setWebServerCallback(onAPStarted);
}

// Portali baslat (yardimci) - baglanti durumuna gore mesaj gosterir
static void startPortal() {
  bool connected = (WiFi.status() == WL_CONNECTED);
  if (connected) {
    snprintf(s_apMessage, sizeof(s_apMessage),
      "<div class='msg'>&#127793; <b>Florene</b> aktif<br>"
      "&#128246; Bagli: <b>%s</b></div>", WiFi.SSID().c_str());
  } else {
    snprintf(s_apMessage, sizeof(s_apMessage),
      "<div class='msg'>WiFi aginizi secin ve sifreyi girin.<br>"
      "<small>Yalnizca 2.4 GHz desteklenir.</small></div>");
  }
  static WiFiManagerParameter custom_text(s_apMessage);
  wifiManager.addParameter(&custom_text);

  char apName[24];
  snprintf(apName, sizeof(apName), "Florene-%s", deviceID);

  wifiManager.startConfigPortal(apName, PORTAL_AP_PASSWORD);
  // Not: burada tarama YAPMIYORUZ (AP yeni acilirken telefonun baglanmasini
  // bozmasin). Ag listesi /scan endpoint'i ile sayfa acilinca async doldurulur.

  s_portalActive = true;
  s_portalStart  = millis();
  stateLock(); g_state.net.portalActive = true; stateUnlock();

  LOG("WIFI", "Portal: http://192.168.4.1 | Sifre: %s | %d sn aktif",
      PORTAL_AP_PASSWORD, AP_TIMEOUT);
}

void netStartConnectOrPortal() {
  // Kayitli ag YOKSA beklemenin anlami yok -> portali hemen ac
  if (!wifiManager.getWiFiIsSaved()) {
    LOG("WIFI", "Kayitli ag yok -> portal hemen aciliyor.");
    startPortal();
    return;
  }

  LOG("WIFI", "Kayitli WiFi'ye baglaniliyor (en fazla %d sn)...", WIFI_CONNECT_TIMEOUT_S);
  WiFi.begin();

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED &&
         millis() - start < (uint32_t)WIFI_CONNECT_TIMEOUT_S * 1000) {
    vTaskDelay(pdMS_TO_TICKS(200));
  }

  if (WiFi.status() == WL_CONNECTED) {
    LOG("WIFI", "Baglanti basarili (kayitli ag).");
    // GOT_IP event'i wifiConnected + justConnected'i zaten set eder
  } else {
    LOG("WIFI", "Kayitli aga ulasilamadi -> portal aciliyor.");
    startPortal();
  }
}

void netLoop() {
  if (s_portalActive) {
    wifiManager.process();

    bool connected;
    stateLock(); connected = g_state.net.wifiConnected; stateUnlock();

    // Portal uzerinden baglanti kuruldu -> basari overlay'i gorunsun, sonra kapat
    if (connected) {
      LOG("WIFI", "Portal uzerinden baglanti kuruldu, basari ekrani gosteriliyor...");
      // AP'yi kapatmadan once ~6 sn boyunca web sunucusunu servis etmeye
      // DEVAM et; boylece telefonun /status yoklamasi 'basarili' overlay'i
      // gorebilir. (Bloklamak yerine surekli process() cagiriyoruz.)
      uint32_t hold = millis();
      while (millis() - hold < 6000) {
        wifiManager.process();
        vTaskDelay(pdMS_TO_TICKS(20));
      }
      WiFi.softAPdisconnect(true);
      WiFi.mode(WIFI_STA);
      s_portalActive = false;
      stateLock(); g_state.net.portalActive = false; stateUnlock();
      LOG("WIFI", "Portal kapatildi (baglanti basarili).");
      return;
    }

    // Portal timeout
    if (millis() - s_portalStart >= (uint32_t)AP_TIMEOUT * 1000) {
      LOG("WIFI", "Portal suresi doldu, kapatiliyor.");
      WiFi.softAPdisconnect(true);
      WiFi.mode(WIFI_STA);
      s_portalActive = false;
      stateLock(); g_state.net.portalActive = false; stateUnlock();
      WiFi.begin();  // kayitli aga tekrar dene
      s_lastAttempt = millis();
    }
    return;
  }

  // --- Portal kapali: backoff ile yeniden baglanma ---
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - s_lastAttempt >= s_backoff) {
      LOG("WIFI", "Yeniden baglanma denemesi (backoff %lu ms)...", (unsigned long)s_backoff);
      WiFi.disconnect(false, false);
      vTaskDelay(pdMS_TO_TICKS(100));
      WiFi.begin();
      s_lastAttempt = millis();
      s_backoff = min(s_backoff * 2, (uint32_t)WIFI_BACKOFF_MAX_MS);
    }
  }
}

bool netConsumeJustConnected() {
  if (s_justConnected) {
    s_justConnected = false;
    return true;
  }
  return false;
}

void netResetWiFiSettings() {
  wifiManager.resetSettings();
}

// ==================== NTP ====================
bool syncTimeWithNTP() {
  if (WiFi.status() != WL_CONNECTED) {
    LOG("NTP", "WiFi bagli degil, sync atlandi");
    return false;
  }

  LOG("NTP", "Saat senkronizasyonu baslatiliyor...");
  configTime(TIMEZONE_OFFSET_SEC, 0, NTP_SERVER_1, NTP_SERVER_2, NTP_SERVER_3);

  struct tm timeinfo;
  uint32_t start = millis();
  bool success = false;
  while (millis() - start < NTP_TIMEOUT_MS) {
    if (getLocalTime(&timeinfo)) {
      int year = timeinfo.tm_year + 1900;
      if (year >= 2025 && year <= 2099) { success = true; break; }
    }
    vTaskDelay(pdMS_TO_TICKS(500));
  }

  if (!success) {
    LOG("NTP", "HATA: Sunucuya ulasilamiyor veya gecersiz tarih!");
    return false;
  }

  s_lastNTPSync = millis();
  stateLock(); g_state.net.ntpSynced = true; stateUnlock();

  LOG("NTP", "Senkronize: %02d/%02d/%04d %02d:%02d:%02d (%s)",
      timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900,
      timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, TIMEZONE_LABEL);
  return true;
}

bool isNTPSyncNeeded() {
  bool synced;
  stateLock(); synced = g_state.net.ntpSynced; stateUnlock();
  if (!synced) return true;
  return (millis() - s_lastNTPSync > NTP_SYNC_INTERVAL);
}
