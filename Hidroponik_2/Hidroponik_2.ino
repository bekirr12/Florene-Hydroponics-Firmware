// ============================================================
//  TOPRAKSIZ TARIM - HİDROPONİK SİSTEM (TİCARİ VERSİYON)
//  Tarih: Mart 2026
//  Hedef: ESP32-S3-WROOM-1 / 1U
// ============================================================
//  Sensörler : DHT22 (GPIO4), Su Seviye HC-SR04 (TRIG=GPIO17, ECHO=GPIO16)
//  Kontrol   : Röle — Pompa (GPIO5), LED (GPIO18) — Aktif LOW
//  Bağlantı  : WiFiManager (Captive Portal) + Otomatik Yeniden Bağlanma
//  Saat      : ESP32-S3 dahili RTC + NTP (SNTP) senkronizasyonu
//              (Harici DS1302 RTC modülü kullanılmamaktadır)
//  Veri      : Florene API (device_id bazlı)
//  Güvenlik  : Watchdog Timer, Sensör Hata Yönetimi, Bellek Optimizasyonu
// ============================================================
//
//  DOSYA YAPISI:
//  Hidroponik.ino  → Ana dosya (setup + loop)
//  config.h         → Ayarlar, pin tanımları, sabitler
//  globals.h        → Global değişkenler, nesneler, getRTCTime()
//  sensors.ino      → Su seviye, medyan hesaplama
//  relays.ino       → Röle kontrol fonksiyonu
//  florene.ino      → Florene API iletişim (komut, kayıt, veri)
//  wifi_utils.ino   → Cihaz ID, MAC adresi, NTP senkronizasyonu
//
//  ESP32-S3-WROOM-1 PIN KULLANIMI:
//  GPIO0  → BOOT/RESET butonu (strapping — pull-up dahili)
//  GPIO4  → DHT22 veri hattı
//  GPIO5  → Pompa rölesi (aktif LOW)
//  GPIO16 → HC-SR04 ECHO
//  GPIO17 → HC-SR04 TRIG
//  GPIO18 → LED rölesi (aktif LOW)
//  GPIO19 → DAHİLİ USB D- (kullanılmıyor)
//  GPIO20 → DAHİLİ USB D+ (kullanılmıyor)
//
//  KALDIRILDI:
//  ThreeWire / RtcDS1302 kütüphaneleri
//  GPIO25 (RTC_CLK), GPIO26 (RTC_DAT), GPIO27 (RTC_RST)
//  → Bu pinler ESP32-S3-WROOM-1'de mevcut değildir.
// ============================================================

#include "DHT.h"
#include <WiFi.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "esp_mac.h"
#include <esp_task_wdt.h>
#include <time.h>
#include <Preferences.h>

#include "config.h"
#include "globals.h"

// ==================== TIMEZONE SAVE CALLBACK ====================
void saveTimezoneCallback() {
  Serial.println("[PORTAL] Ayarlar kaydediliyor...");
}

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  Serial.println("\n============================================");
  Serial.println("  TOPRAKSIZ TARIM - Hidroponik Sistem v4.0");
  Serial.println("  Hedef: ESP32-S3-WROOM-1");
  Serial.println("  Dahili RTC + NTP | Harici DS1302 YOK");
  Serial.println("============================================\n");

  // Device ID ve MAC oluştur
  WiFi.mode(WIFI_STA);
  initDeviceID();

  // Timezone'u bellekten yükle
  loadTimezoneFromMemory();

  // Türkiye saati sabit (GMT+3)
  timezoneOffset = 10800;
  strncpy(timezoneLabel, "Turkiye (GMT+3)", sizeof(timezoneLabel) - 1);

  Serial.println("-------- CİHAZ BİLGİLERİ --------");
  Serial.printf("[INFO] Cihaz ID : %s\n", deviceID);
  printMAC();
  Serial.println("----------------------------------");

  // BOOT butonu (GPIO0)
  pinMode(RESET_BTN_PIN, INPUT_PULLUP);

  // WiFiManager ayarları
  char apName[24];
  snprintf(apName, sizeof(apName), "Florene-%s", deviceID);

  wifiManager.setConfigPortalBlocking(false);
  wifiManager.setConfigPortalTimeout(AP_TIMEOUT);
  wifiManager.setConnectTimeout(15);

  wifiManager.setTitle("Florene");
  wifiManager.setDarkMode(false);
  std::vector<const char*> menu = {"wifi", "exit"};
  wifiManager.setMenu(menu);

  wifiManager.setCustomHeadElement(
    "<style>"
    "body{font-family:'Segoe UI',Arial,sans-serif;background:#FEF8EE!important;color:#000!important;}"
    ".wrap{max-width:380px;padding:0 16px;}"
    "h1{font-size:1.6em;text-align:center;margin:16px 0 8px;color:#2E7D32;}"
    "h3{text-align:center;font-weight:400;color:#000;margin:0 0 16px;font-size:0.85em;}"
    "h3 strong{font-size:1.2em;display:block;margin-top:12px;}"
    ".msg{background:#F5F1E8;border-left:4px solid #2E7D32;padding:12px 16px;"
    "border-radius:0 8px 8px 0;margin:12px 0;font-size:0.9em;}"
    ".msg b{color:#2E7D32;}"
    "button,input[type='submit']{background:#2E7D32!important;border:none!important;"
    "border-radius:20px!important;padding:12px!important;font-size:1em!important;"
    "font-weight:600!important;letter-spacing:0.5px;cursor:pointer;color:#fff!important;"
    "transition:background 0.3s!important;}"
    "button:hover,input[type='submit']:hover{background:#1B5E20!important;}"
    "input[type='text'],input[type='password'],select{border:2px solid #DDD7CC!important;"
    "border-radius:10px!important;padding:10px 14px!important;font-size:0.95em!important;"
    "background:#FAF6EF!important;color:#000!important;}"
    "input[type='text']:focus,input[type='password']:focus,select:focus{border-color:#2E7D32!important;"
    "outline:none!important;}"
    "a{color:#2E7D32!important;text-decoration:none!important;}"
    "#wifi_scan{border-radius:12px;overflow:hidden;}"
    ".q{color:#2E7D32!important;}"
    "label{display:block;margin:12px 0 6px;color:#000;font-size:0.9em;}"
    ".tz-info{background:#F5F1E8;border-left:4px solid #2E7D32;padding:10px 14px;"
    "border-radius:0 8px 8px 0;margin:12px 0 0;font-size:0.85em;color:#000;}"
    ".wifi-list li{border-bottom:1px solid #D8CFBC;}"
    ".wifi-list li:hover{background:#EDE8DF;}"
    ".wifi-ssid{color:#000;}.wifi-signal{display:none!important;}"
    "nav{display:none!important;}.selected-net{display:none!important;}"
    ".logo{display:none!important;}"
    ".device-badge{display:inline-block;background:#F2F8F2;color:#4CAF50;"
    "padding:6px 16px;border-radius:999px;font-size:0.85em;font-weight:600;"
    "border:1.5px solid #C8E6C9;margin:8px 0;}"
    "</style>"
    "<script>"
    "document.addEventListener('DOMContentLoaded',function(){"
    "  var w=document.createTreeWalker(document.body,NodeFilter.SHOW_TEXT);"
    "  var m={'Configure WiFi':'WiFi Ayarla','Exit':'Çıkış',"
    "  'Refresh':'Yenile','SSID':'WiFi Adı',"
    "  'Password':'Şifre','Show Password':'Şifreyi Göster',"
    "  'WiFi Scan':'WiFi Tara'};"
    "  var n;while(n=w.nextNode()){"
    "    for(var k in m){n.nodeValue=n.nodeValue.replace(k,m[k]);}"
    "    if(n.nodeValue.indexOf('Not connected to')>-1) n.nodeValue=n.nodeValue.replace('Not connected to','Bağlantı Başarısız:');"
    "    if(n.nodeValue.indexOf('Connected to')>-1) n.nodeValue=n.nodeValue.replace('Connected to','Bağlantı Başarılı:');"
    "  }"
    "  var btns=document.querySelectorAll('button[type=submit], input[type=submit], input[type=button]');"
    "  btns.forEach(function(b){"
    "    if(b.value==='Save') {"
    "      b.style.color='transparent';"
    "      b.style.position='relative';"
    "      var span=document.createElement('span');"
    "      span.innerText='Bağlan';"
    "      span.style.position='absolute'; span.style.left='50%'; span.style.top='50%'; span.style.transform='translate(-50%, -50%)'; span.style.color='#fff';"
    "      b.appendChild(span);"
    "    }"
    "  });"
    "});"
    "</script>"
  );

  // ── DEĞIŞIKLIK: Önce hızlı bağlantı denemesi, sonra AP mesajı ──
  Serial.println("\n[WIFI] Kayıtlı WiFi kontrol ediliyor...");
  WiFi.begin();
  unsigned long connectStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - connectStart < 10000) {
    delay(200);
    Serial.print(".");
  }
  Serial.println();

  bool isConnected = (WiFi.status() == WL_CONNECTED);

  if (isConnected) {
    Serial.printf("[WIFI] Kayıtlı ağ bulundu!\n");
    Serial.printf("[WIFI] Ağ: %s | IP: %s\n",
                  WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
    wasConnected = true;
    syncTimeWithNTP();
    checkCommands();  // Açılışta ayarları sunucudan çek
  } else {
    Serial.println("[WIFI] Kayıtlı ağ yok veya bağlanılamadı.");
  }

  // AP mesajını bağlantı sonucuna göre oluştur (waitForConnectResult() YOK)
  static char apMessage[256];
  if (isConnected) {
    snprintf(apMessage, sizeof(apMessage),
             "<div class='msg'>&#127793; <b>Florene</b><br>"
             "&#128246; Bağlı ağ: <b>%s</b></div>",
             WiFi.SSID().c_str());
  } else {
    snprintf(apMessage, sizeof(apMessage),
             "<div class='msg'>WiFi ağınızı seçin ve şifreyi girin.</div>");
  }
  static WiFiManagerParameter custom_text(apMessage);
  wifiManager.addParameter(&custom_text);

  static WiFiManagerParameter hint_text(
    "<div class='msg' style='font-size:0.82em;margin:10px 0;'>"
    "WiFi adını ve şifrenizi girerken büyük ve küçük harfe dikkat edin.<br>"
    "Yalnızca 2.4 GHz ağlar desteklenir."
    "</div>"
  );
  wifiManager.addParameter(&hint_text);

  wifiManager.setSaveParamsCallback(saveTimezoneCallback);

  // Portal kararı
  if (isConnected) {
    Serial.println("[WIFI] İnternet bağlantısı hazır! Portal atlanıyor...");
    portalActive = false;
  } else {
    Serial.println("[WIFI] Kayıtlı ağa bağlanılamadı! Ayar portalı başlatılıyor...");
    wifiManager.startConfigPortal(apName, "florene123");
    Serial.println("[WIFI] Portal: http://192.168.4.1");
    Serial.println("[WIFI] Portal işlemi tamamlandı, ana döngüye geçiliyor...");
    portalActive = true;
    portalStartTime = millis();
  }

  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);

  // ==================== PIN AYARLARI ====================

  // HC-SR04 Ultrasonik Sensör
  pinMode(HC_SR04_TRIG_PIN, OUTPUT);
  pinMode(HC_SR04_ECHO_PIN, INPUT);

  // Röleler (Aktif LOW — başlangıçta HIGH = KAPALI)
  pinMode(RELAY_PUMP_PIN, OUTPUT);
  pinMode(RELAY_LED_PIN,  OUTPUT);
  digitalWrite(RELAY_PUMP_PIN, HIGH);
  digitalWrite(RELAY_LED_PIN,  LOW);

  // Onboarding LED Flash (3x)
  Serial.println("[LED] Onboarding flash...");
  for (int i = 0; i < 3; i++) {
    digitalWrite(RELAY_LED_PIN, HIGH);
    delay(300);
    digitalWrite(RELAY_LED_PIN, LOW);
    delay(300);
  }
  Serial.println("[LED] Onboarding tamamlandı");

  // DHT22
  dht.begin();

  // ESP32-S3 ADC atenuasyonu
  analogSetAttenuation(ADC_11db);

  Serial.println("[OK] Sensörler hazır");
  Serial.println("[OK] Röleler hazır");
  Serial.println("[OK] Dahili RTC aktif (NTP ile senkronize edilecek)");

  // Watchdog Timer
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms     = WDT_TIMEOUT * 1000,
    .idle_core_mask = 0,
    .trigger_panic  = true
  };
  esp_task_wdt_reconfigure(&wdt_config);
  esp_err_t wdt_err = esp_task_wdt_add(NULL);
  if (wdt_err == ESP_OK || wdt_err == ESP_ERR_INVALID_ARG) {
    Serial.printf("[OK] Watchdog aktif (%d sn)\n", WDT_TIMEOUT);
  } else {
    Serial.printf("[UYARI] Watchdog eklenemedi: %d\n", wdt_err);
  }

  Serial.println("\n========== SİSTEM HAZIR ==========\n");
}

// ==================== LOOP ====================
void loop() {
  esp_task_wdt_reset();

  // 2. Sensör Okuma + Seri Monitör (Her 2 saniye)
  static unsigned long printTime = millis();
  if (millis() - printTime > SERIAL_PRINT_INTERVAL) {
    printTime = millis();

    // Tarih/Saat — ESP32-S3 dahili RTC
    struct tm timeinfo;
    if (getRTCTime(timeinfo)) {
      Serial.printf("Tarih: %02d/%02d/%04d  Saat: %02d:%02d:%02d\n",
                    timeinfo.tm_mday,
                    timeinfo.tm_mon + 1,
                    timeinfo.tm_year + 1900,
                    timeinfo.tm_hour,
                    timeinfo.tm_min,
                    timeinfo.tm_sec);
    } else {
      Serial.println("[UYARI] Dahili RTC henüz NTP ile ayarlanmadı");
    }

    // Sıcaklık/Nem
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    if (isnan(h) || isnan(t)) {
      dhtError = true;
      Serial.println("[HATA] DHT okunamıyor! Son bilinen değer kullanılıyor");
    } else {
      dhtError    = false;
      temperature = t;
      humidity    = h;
      Serial.printf("Nem: %.1f%%  Sıcaklık: %.1f C\n", humidity, temperature);
    }

    // Su Seviyesi
    waterDistance = readWaterLevel();
    Serial.printf("Su Seviyesi (Mesafe): %.1f cm\n", waterDistance);

    // Durum Özeti
    Serial.printf("Pompa: %s  LED: %s",
                  pumpStatus ? "AÇIK" : "KAPALI",
                  ledStatus  ? "AÇIK" : "KAPALI");
    if (dhtError) Serial.print("  [!DHT]");
    if (!ntpSyncSuccess) Serial.print("  [!NTP]");
    Serial.printf("  WiFi: %s\n", WiFi.status() == WL_CONNECTED ? "OK" : "YOK");
    Serial.println("-----------------------");
  }

  // 3. Zamanlama Ayarlarını Oku (Her 5 saniye)
  static unsigned long commandTime = millis();
  if (millis() - commandTime > COMMAND_CHECK_INTERVAL) {
    commandTime = millis();
    checkCommands();
  }

  // 4. Florene API Gönderimi (Her 10 saniye)
  static unsigned long apiSendTime = millis();
  if (millis() - apiSendTime > API_SEND_INTERVAL) {
    apiSendTime = millis();
    sendToAPI();
  }

  // 5. Zamanlama Motoru (Her 10 saniye — röleleri iç RTC saatine göre güncelle)
  static unsigned long scheduleTime = millis();
  if (millis() - scheduleTime > SCHEDULE_CHECK_INTERVAL) {
    scheduleTime = millis();
    updateRelays();

    // Gece yarısı override sıfırlama (00:00–00:01 arası, günde 1 kez)
    struct tm t;
    if (getRTCTime(t)) {
      int today = t.tm_mday;
      if (t.tm_hour == 0 && t.tm_min == 0 && lastResetDay != today) {
        lastResetDay = today;
        if (ledOverride >= 0 || pumpOverride >= 0) {
          ledOverride  = -1;
          pumpOverride = -1;
          Serial.println("[SCHEDULE] Gece yarısı: Override sıfırlandı → zamanlama aktif");
        }
      }
    }
  }

  // 6. WiFi Yeniden Bağlanma (Her 30 saniye — portal aktifken devre dışı)
  static unsigned long wifiCheckTime = millis();
  if (!portalActive && millis() - wifiCheckTime > WIFI_RECONNECT_INTERVAL) {
    wifiCheckTime = millis();

    if (WiFi.status() != WL_CONNECTED) {
      if (wasConnected) {
        Serial.println("[WIFI] Bağlantı koptu!");
        Serial.printf("[OFFLINE] Röleler son durumda — Pompa: %s, LED: %s\n",
                      pumpStatus ? "AÇIK" : "KAPALI",
                      ledStatus  ? "AÇIK" : "KAPALI");
        wasConnected          = false;
        wifiReconnectAttempts = 0;
      }

      wifiReconnectAttempts++;
      Serial.printf("[WIFI] Yeniden bağlanma denemesi #%d...\n", wifiReconnectAttempts);

      WiFi.disconnect(false, false);
      delay(200);
      WiFi.begin();

      if (wifiReconnectAttempts > MAX_RECONNECT_ATTEMPTS) {
        Serial.println("[WIFI] Bağlantı başarısız — denemeye devam ediliyor");
        wifiReconnectAttempts = 0;
      }
    } else {
      if (!wasConnected) {
        Serial.println("[WIFI] Bağlantı kuruldu!");
        Serial.printf("[WIFI] Ağ: %s | IP: %s | Sinyal: %d dBm\n",
                      WiFi.SSID().c_str(),
                      WiFi.localIP().toString().c_str(),
                      WiFi.RSSI());
        wasConnected          = true;
        wifiReconnectAttempts = 0;

        //if (!deviceRegistered) {
        //  deviceRegistered = registerDevice();
        //}

        // Yeniden bağlandığında NTP sync
        if (!ntpSyncSuccess) {
          syncTimeWithNTP();
        }

        checkCommands(); // Ayarları sunucudan çek
      }
    }
  }

  // 7. WiFi Portal Kontrolü (5dk sonra kapat)
  if (portalActive) {
    wifiManager.process();

    if (WiFi.status() == WL_CONNECTED && !wasConnected) {
      Serial.println("[WIFI] Portal üzerinden bağlantı kuruldu!");
      Serial.printf("[WIFI] Ağ: %s | IP: %s\n",
                    WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
      wasConnected = true;
      syncTimeWithNTP();
      checkCommands();  // Ayarları çek
      wifiManager.stopConfigPortal();
      portalActive = false;
    }

    if (portalActive && (millis() - portalStartTime > (unsigned long)AP_TIMEOUT * 1000)) {
      wifiManager.stopConfigPortal(); // YENİ: Hem yayını keser hem HTTP sunucusunu kapatıp RAM'i temizler
      WiFi.mode(WIFI_STA);
      WiFi.setAutoReconnect(false);  
      portalActive = false;           // State makinemizi güncelliyoruz
      Serial.println("[WIFI] Ayar portalı kapatıldı (5dk doldu, cihaz normal döngüye dönüyor)");

      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WIFI] Kayıtlı WiFi'ye bağlanılıyor...");
        WiFi.begin();
      }
      /*
      if (WiFi.status() == WL_CONNECTED && !wasConnected) {
        wasConnected = true;
        syncTimeWithNTP();
        checkCommands();
        }
      */
    }
  }

  // 8. Periyodik NTP Senkronizasyonu (Her 12 saatte bir — dahili RTC'yi kalibre eder)
  static unsigned long ntpCheckTime = millis();
  if (millis() - ntpCheckTime > 60000) {
    ntpCheckTime = millis();

    if (WiFi.status() == WL_CONNECTED && isNTPSyncNeeded()) {
      syncTimeWithNTP();
    }
  }

  // 9. BOOT Butonu: 3sn basılı tut = WiFi sıfırla + restart
  static unsigned long btnPressStart = 0;
  static bool          btnPressed    = false;
  if (digitalRead(RESET_BTN_PIN) == LOW) {
    if (!btnPressed) {
      btnPressed    = true;
      btnPressStart = millis();
      Serial.println("[BTN] BOOT butonu algılandı — 3sn basılı tutun...");
    } else if (millis() - btnPressStart >= 3000) {
      Serial.println("[BTN] WiFi ayarları sıfırlandı! Yeniden başlatılıyor...");
      wifiManager.resetSettings();
      delay(1000);
      ESP.restart();
    }
  } else {
    if (btnPressed && millis() - btnPressStart < 3000) {
      Serial.println("[BTN] Buton erken bırakıldı");
    }
    btnPressed = false;
  }
}