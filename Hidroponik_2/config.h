// ============================================================
//  CONFIG.H - Ayarlar ve Pin Tanımlamaları
//  Tüm sabitler, zamanlama, pin ayarları ve kalibrasyon burada
// ============================================================

#ifndef CONFIG_H
#define CONFIG_H

// ==================== ZAMANLAMA AYARLARI ====================
#define SERIAL_PRINT_INTERVAL 2000     // Seri monitör yazdırma (ms)
#define API_SEND_INTERVAL 10000        // Veri gönderimi (ms)
#define COMMAND_CHECK_INTERVAL 5000    // Komut kontrolü (ms)

#define AP_TIMEOUT 300                 // WiFi AP süresi (saniye) = 5 dakika
#define WIFI_RECONNECT_INTERVAL 30000  // WiFi yeniden bağlanma kontrolü (ms)
#define WDT_TIMEOUT 60                 // Watchdog süresi (saniye)
#define HTTP_TIMEOUT 10000             // HTTP timeout (ms)
#define MAX_RECONNECT_ATTEMPTS 3       // WiFi.reconnect() deneme sayısı
#define SCHEDULE_CHECK_INTERVAL 10000  // Zamanlama motoru kontrolü (ms)
#define PUMP_SLOT_DURATION 5           // Pompa çalışma süresi (dakika/slot)

// ==================== PIN TANIMLAMALARI ====================
// ESP32-S3-WROOM-1 Pin Haritası
// Not: GPIO26, GPIO25, GPIO27 bu modelde YOKTUR.
// Not: GPIO35, GPIO36, GPIO37 PSRAM içeren versiyonlarda kullanılamaz.
// Not: GPIO19, GPIO20 dahili USB PHY için ayrılmıştır.
// Not: GPIO0 Boot strapping pinidir, butona bağlanabilir (pull-up ile).

#define RESET_BTN_PIN 0     // WiFi reset butonu (BOOT)



// DHT22 Sıcaklık/Nem Sensörü
// GPIO4 → RTC_GPIO4, TOUCH4, ADC1_CH3 — genel I/O için uygun
#define DHT_PIN 4
#define DHT_TYPE DHT11

// HC-SR04 Ultrasonik Su Seviye Sensörü
// GPIO16 → RTC_GPIO16, ADC2_CH5 — ECHO girişi
// GPIO17 → RTC_GPIO17, U1TXD, ADC2_CH6 — TRIG çıkışı
// Not: GPIO17 aynı zamanda UART1 TX'tir; Serial1 kullanılmıyorsa güvenli.
#define HC_SR04_TRIG_PIN 17
#define HC_SR04_ECHO_PIN 16

// Şimdilik harici RTC modülünü kaldırıyoruz.
/*
// RTC Modülü (DS1302)
#define RTC_DAT_PIN 26
#define RTC_CLK_PIN 25
#define RTC_RST_PIN 27
*/

// Röle Pinleri (Aktif LOW — LOW=Açık, HIGH=Kapalı)
// GPIO5  → RTC_GPIO5, TOUCH5, ADC1_CH4 — Pompa rölesi
// GPIO18 → RTC_GPIO18, U1RXD, ADC2_CH7 — LED rölesi
#define RELAY_PUMP_PIN 5    // Pompa rölesi
#define RELAY_LED_PIN 18    // LED rölesi

// ==================== SABİTLER ====================
#define VREF 3.3

// ==================== NTP AYARLARI ====================
// ESP32-S3 dahili RTC + NTP ile saat yönetimi
// Harici DS1302 RTC modülü kullanılmamaktadır.
const char* NTP_SERVER_1 = "pool.ntp.org";
const char* NTP_SERVER_2 = "time.google.com";
const char* NTP_SERVER_3 = "time.cloudflare.com";
const int NTP_TIMEOUT = 10000;                    // NTP timeout (ms)
const unsigned long NTP_SYNC_INTERVAL = 43200000; // 12 saat (ms)

// ==================== TIMEZONE LİSTESİ ====================
struct TimezoneOption {
  const char* label;
  const char* value;
  long offsetSec;
};

const TimezoneOption TIMEZONES[] = {
  {"Türkiye (GMT+3)",           "10800",   10800},
  {"Almanya (GMT+1)",           "3600",    3600},
  {"Fransa (GMT+1)",            "3600",    3600},
  {"İngiltere (GMT+0)",         "0",       0},
  {"İspanya (GMT+1)",           "3600",    3600},
  {"İtalya (GMT+1)",            "3600",    3600},
  {"Hollanda (GMT+1)",          "3600",    3600},
  {"Belçika (GMT+1)",           "3600",    3600},
  {"Yunanistan (GMT+2)",        "7200",    7200},
  {"Polonya (GMT+1)",           "3600",    3600},
  {"Rusya Moskova (GMT+3)",     "10800",   10800},
  {"BAE Dubai (GMT+4)",         "14400",   14400},
  {"Suudi Arabistan (GMT+3)",   "10800",   10800},
  {"Hindistan (GMT+5:30)",      "19800",   19800},
  {"Çin (GMT+8)",               "28800",   28800},
  {"Japonya (GMT+9)",           "32400",   32400},
  {"Avustralya Sydney (GMT+10)","36000",   36000},
  {"ABD Doğu (GMT-5)",          "-18000", -18000},
  {"ABD Merkez (GMT-6)",        "-21600", -21600},
  {"ABD Batı (GMT-8)",          "-28800", -28800},
  {"Brezilya (GMT-3)",          "-10800", -10800},
  {"Arjantin (GMT-3)",          "-10800", -10800},
  {"Meksika (GMT-6)",           "-21600", -21600}
};

const int TIMEZONE_COUNT = sizeof(TIMEZONES) / sizeof(TIMEZONES[0]);

#endif