// ============================================================
//  GLOBALS.H - Global Değişkenler ve Nesneler
//  Tüm sensör değişkenleri, durum bayrakları ve nesneler burada
// ============================================================

#ifndef GLOBALS_H
#define GLOBALS_H

// ==================== GLOBAL DEĞİŞKENLER ====================
// Cihaz Kimliği (MAC son 6 hex = 6 karakter + null)
char deviceID[7] = "";




// Sensör değişkenleri (NAN = henüz okunmadı, -1 gönderilir)
float temperature = NAN;
float humidity = NAN;
float waterDistance = 0.0;
bool readingWaterLevel = false;

// Röle durumları
bool pumpStatus = false;
bool ledStatus = false;

// Zamanlama değişkenleri (API'den okunur)
char    ledStart[6]   = "19:08";          // LED açılma saati (HH:MM)
char    ledEnd[6]     = "19:09";          // LED kapanma saati (HH:MM)
int8_t  ledOverride   = -1;               // -1=zamanlama, 0=zorla kapat, 1=zorla aç
char    pumpSlots[64] = "21:28,20:00";    // Pompa çalışma saatleri (virgülle ayrılmış)
int8_t  pumpOverride  = -1;               // -1=zamanlama, 0=zorla kapat, 1=zorla aç
int     lastResetDay  = -1;               // Override gece yarısı sıfırlama için son gün

// Sensör hata durumları
bool dhtError = true;  // Başlangıçta true, ilk başarılı okumada false

// WiFi bağlantı durumu
bool wasConnected = false;
int wifiReconnectAttempts = 0;

// WiFi Portal durumu
bool          portalActive    = false;
unsigned long portalStartTime = 0;
//bool          deviceRegistered = false;

// NTP ve Timezone değişkenleri
// ESP32-S3 dahili RTC: time_t / struct tm / esp-idf SNTP kullanılır.
Preferences   preferences;
long          timezoneOffset  = 10800;          // Türkiye GMT+3 (saniye)
unsigned long lastNTPSync     = 0;
bool          ntpSyncSuccess  = false;
char          timezoneLabel[48] = "Türkiye (GMT+3)";

// DHT22 Sensörü
DHT dht(DHT_PIN, DHT_TYPE);
 
// WiFiManager
WiFiManager wifiManager;

// ==================== YARDIMCI FONKSİYON ====================
// ESP32-S3 dahili RTC'den anlık struct tm al.
// configTime() çağrısından sonra getLocalTime() doğrudan çalışır.
// Yıl geçerlilik kontrolü: 2025'ten küçükse NTP sync henüz yapılmamış.
inline bool getRTCTime(struct tm &timeinfo) {
  if (!getLocalTime(&timeinfo)) return false;
  if (timeinfo.tm_year + 1900 < 2025) return false;
  return true;
}
 
// Saat bilgisini "HH:MM:SS" olarak doldurur (timestamp için)
inline void formatTimestamp(char* buf, size_t len) {
  struct tm t;
  if (getRTCTime(t)) {
    snprintf(buf, len, "%04d-%02d-%02dT%02d:%02d:%02d",
             t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
             t.tm_hour, t.tm_min, t.tm_sec);
  } else {
    snprintf(buf, len, "1970-01-01T00:00:00");
  }
}

// RTC nesneleri kaldırıldı
// ThreeWire rtcWire(RTC_DAT_PIN, RTC_CLK_PIN, RTC_RST_PIN);
// RtcDS1302<ThreeWire> rtc(rtcWire);

// WiFiManager
//WiFiManager wifiManager;

#endif