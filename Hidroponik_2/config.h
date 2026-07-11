// ============================================================
//  CONFIG.H - Ayarlar, Pin Tanimlamalari ve Kalibrasyon
//  Tum sabitler, zamanlama, pin ayarlari ve kalibrasyon burada.
//  Not: constexpr kullanildi -> header birden fazla .cpp icine
//  dahil edilse bile coklu-tanim (ODR) hatasi olusmaz.
// ============================================================

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ==================== LOG MAKROLARI ====================
// Tutarli seri cikti. Kullanim: LOG("WIFI", "Bagli: %s", ssid);
// Tag ve format string derleme zamaninda birlestirilir (hepsi literal).
#define LOG(tag, fmt, ...)  Serial.printf("[" tag "] " fmt "\n", ##__VA_ARGS__)

// ==================== ZAMANLAMA AYARLARI ====================
constexpr uint32_t SERIAL_PRINT_INTERVAL   = 2000;   // Seri monitor ozeti (ms)
constexpr uint32_t API_SEND_INTERVAL       = 10000;  // Veri gonderimi / PUT (ms)
constexpr uint32_t COMMAND_CHECK_INTERVAL  = 5000;   // Komut kontrolu / GET (ms)
constexpr uint32_t SENSOR_READ_INTERVAL    = 2000;   // Sensor okuma (ms) - DHT11 min 1sn (2sn guvenli)
constexpr uint32_t SCHEDULE_CHECK_INTERVAL = 2000;   // Zamanlama motoru + guvenlik (ms)

constexpr uint16_t AP_TIMEOUT              = 300;    // WiFi AP suresi (saniye) = 5 dakika
constexpr uint16_t WDT_TIMEOUT             = 60;     // Watchdog suresi (saniye)
constexpr uint32_t HTTP_TIMEOUT            = 10000;  // HTTP timeout (ms)
constexpr uint8_t  WIFI_CONNECT_TIMEOUT_S  = 10;     // Kayitli aga baglanma ust siniri (sn); ag yoksa hic beklenmez
constexpr uint8_t  PUMP_SLOT_DURATION      = 5;      // Pompa calisma suresi (dakika/slot)

// WiFi yeniden baglanma - exponential backoff (event-driven)
constexpr uint32_t WIFI_BACKOFF_MIN_MS     = 1000;   // ilk deneme araligi
constexpr uint32_t WIFI_BACKOFF_MAX_MS     = 60000;  // tavan (1 dk)

// ==================== FreeRTOS GOREV AYARLARI ====================
constexpr uint32_t CONTROL_TASK_STACK      = 4096;   // ControlTask stack (bytes)
constexpr uint32_t NETWORK_TASK_STACK      = 12288;  // NetworkTask stack (HTTP/TLS icin genis)
constexpr UBaseType_t CONTROL_TASK_PRIO    = 3;      // yuksek oncelik (guvenlik-kritik)
constexpr UBaseType_t NETWORK_TASK_PRIO    = 2;
constexpr BaseType_t  CONTROL_TASK_CORE    = 1;      // APP CPU
constexpr BaseType_t  NETWORK_TASK_CORE    = 0;      // PRO CPU
constexpr uint32_t CONTROL_TASK_PERIOD_MS  = 50;   // ControlTask dongu periyodu

// ==================== PIN TANIMLAMALARI ====================
// ESP32-S3-WROOM-1 Pin Haritasi
// Not: GPIO26/25/27 bu modelde YOKTUR. GPIO19/20 dahili USB PHY.
// Not: GPIO0 Boot strapping pinidir (pull-up ile butona baglanabilir).

#define RESET_BTN_PIN 0     // WiFi reset butonu (BOOT)

// DHT11 Sicaklik/Nem Sensoru
#define DHT_PIN 11
#define DHT_TYPE DHT11      // donanim DHT11

// HC-SR04 Ultrasonik Su Seviye Sensoru
#define HC_SR04_TRIG_PIN 9 // TRIG cikisi (U1TXD - Serial1 kullanilmiyorsa guvenli)
#define HC_SR04_ECHO_PIN 10 // ECHO girisi

// Role Pinleri - HER IKISI DE AKTIF HIGH (HIGH=Acik, LOW=Kapali)
#define RELAY_PUMP_PIN 8    // Pompa rolesi
#define RELAY_LED_PIN 7    // LED rolesi

// Role mantik seviyeleri
constexpr uint8_t RELAY_ON  = HIGH;   // Aktif HIGH roleyi acar
constexpr uint8_t RELAY_OFF = LOW;

// ==================== KALIBRASYON ====================
// Su seviyesi HC-SR04 ile mesafe (cm) olarak olculur. Sensor su yuzeyine
// yakin (kucuk mesafe) = tank dolu; uzak (buyuk mesafe) = tank bos.
constexpr float WATER_SAFETY_DISTANCE_CM = 16.0f;  // >= bu ise su kritik -> pompa GUVENLIK KAPATMASI
constexpr float WATER_TANK_FULL_CM       = 3.0f;   // dolu tankta su yuzeyine mesafe
constexpr float WATER_TANK_EMPTY_CM      = 20.0f;  // bos tankta mesafe
constexpr int   WATER_READ_SAMPLES       = 5;      // medyan icin olcum sayisi
constexpr int   WATER_READ_ERROR_CM      = 999;    // okuma hatasi sentinel degeri

// ==================== NTP / SAAT AYARLARI ====================
// ESP32-S3 dahili RTC + NTP (SNTP). Harici DS1302 kullanilmaz.
constexpr const char* NTP_SERVER_1 = "pool.ntp.org";
constexpr const char* NTP_SERVER_2 = "time.google.com";
constexpr const char* NTP_SERVER_3 = "time.cloudflare.com";
constexpr uint32_t NTP_TIMEOUT_MS     = 10000;     // NTP timeout (ms)
constexpr uint32_t NTP_SYNC_INTERVAL  = 43200000;  // 12 saat (ms)

// Turkiye sabit GMT+3, yaz saati (DST) yok. Timezone secim listesi kaldirildi.
constexpr long  TIMEZONE_OFFSET_SEC = 10800;
constexpr const char* TIMEZONE_LABEL = "Turkiye (GMT+3)";

// ==================== ZAMANLAMA VARSAYILANLARI ====================
// API'den ayar gelene kadar kullanilan makul production defaultlari.
constexpr const char* DEFAULT_LED_START  = "08:00";
constexpr const char* DEFAULT_LED_END    = "20:00";
constexpr const char* DEFAULT_PUMP_SLOTS = "08:00,20:00";

#endif // CONFIG_H
