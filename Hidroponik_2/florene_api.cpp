// ============================================================
//  FLORENE_API.CPP - Florene bulut API istemcisi
//  - HTTPS: WiFiClientSecure + setInsecure() (sertifika dogrulamasi yok)
//  - ArduinoJson 7 (JsonDocument)
//  - Paylasilan durum kilitli snapshot ile okunur/yazilir
//  Not: GET sonrasi role dogrudan surulmez; ControlTask bir sonraki
//  dongude (<=1sn) yeni ayarlari uygular (GPIO yaris kosulu onlenir).
// ============================================================

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

#include "config.h"
#include "secrets.h"
#include "state.h"
#include "netmgr.h"
#include "florene_api.h"

static void addFloreneHeaders(HTTPClient &http, bool withContent) {
  http.setTimeout(HTTP_TIMEOUT);
  http.addHeader("X-Florene-Key", FLORENE_API_KEY);
  if (withContent) http.addHeader("Content-Type", "application/json");
}

static void buildUrl(char* url, size_t len) {
  snprintf(url, len, "%s/api/v1/devices/%s", FLORENE_API_URL, deviceID);
}

// ── GET: ayarlari oku ──
void checkCommands() {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  char url[128];
  buildUrl(url, sizeof(url));
  http.begin(client, url);
  addFloreneHeaders(http, false);

  int code = http.GET();
  LOG("GET", "%s -> HTTP %d", url, code);

  if (code == 200) {
    String payload = http.getString();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
      LOG("GET", "JSON parse hatasi: %s", err.c_str());
      http.end();
      return;
    }

    JsonObject device = doc["success"];
    if (device.isNull()) {
      LOG("GET", "HATA: 'success' objesi bulunamadi!");
      http.end();
      return;
    }

    // Yerel degiskenlere parse et
    char   ledStart[6], ledEnd[6], pumpSlots[64];
    int8_t ledOv, pumpOv;
    strncpy(ledStart,  device["ledStart"]  | DEFAULT_LED_START,  sizeof(ledStart)  - 1);
    strncpy(ledEnd,    device["ledEnd"]    | DEFAULT_LED_END,    sizeof(ledEnd)    - 1);
    strncpy(pumpSlots, device["pumpSlots"] | DEFAULT_PUMP_SLOTS, sizeof(pumpSlots) - 1);
    ledStart[sizeof(ledStart)   - 1] = '\0';
    ledEnd[sizeof(ledEnd)       - 1] = '\0';
    pumpSlots[sizeof(pumpSlots) - 1] = '\0';

    ledOv  = device["ledOverride"].isNull()  ? -1 : (device["ledOverride"].as<bool>()  ? 1 : 0);
    pumpOv = device["pumpOverride"].isNull() ? -1 : (device["pumpOverride"].as<bool>() ? 1 : 0);

    // Paylasilan duruma yaz (kilit kisa)
    stateLock();
    strncpy(g_state.relays.ledStart,  ledStart,  sizeof(g_state.relays.ledStart));
    strncpy(g_state.relays.ledEnd,    ledEnd,    sizeof(g_state.relays.ledEnd));
    strncpy(g_state.relays.pumpSlots, pumpSlots, sizeof(g_state.relays.pumpSlots));
    g_state.relays.ledOverride  = ledOv;
    g_state.relays.pumpOverride = pumpOv;
    stateUnlock();

    LOG("GET", "ledStart=%s ledEnd=%s pumpSlots=%s ledOv=%d pumpOv=%d",
        ledStart, ledEnd, pumpSlots, ledOv, pumpOv);

  } else if (code == 404) {
    LOG("GET", "HATA: Cihaz DB'de bulunamadi (deviceId=%s)", deviceID);
  } else if (code < 0) {
    LOG("GET", "Baglanti hatasi: %s", http.errorToString(code).c_str());
  } else {
    LOG("GET", "Beklenmeyen HTTP %d", code);
  }

  http.end();
}

// ── PUT: sensor verisi gonder ──
void sendToAPI() {
  if (WiFi.status() != WL_CONNECTED) return;

  // Snapshot al
  SensorData s;
  RelayState r;
  bool ntp;
  stateLock();
  s   = g_state.sensors;
  r   = g_state.relays;
  ntp = g_state.net.ntpSynced;
  stateUnlock();

  if (!ntp) {
    LOG("PUT", "NTP sync bekleniyor, gonderim atlandi");
    return;
  }

  // Zaman damgasi (RFC3339, timezone offsetli)
  char timestamp[26];
  struct tm timeinfo;
  if (getRTCTime(timeinfo)) {
    long absOffset = labs(TIMEZONE_OFFSET_SEC);
    int  tzHour = absOffset / 3600;
    int  tzMin  = (absOffset % 3600) / 60;
    char tzSign = (TIMEZONE_OFFSET_SEC >= 0) ? '+' : '-';
    snprintf(timestamp, sizeof(timestamp),
             "%04d-%02d-%02dT%02d:%02d:%02d%c%02d:%02d",
             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
             tzSign, tzHour, tzMin);
  } else {
    timestamp[0] = '\0';
    LOG("PUT", "UYARI: RTC gecersiz, timestamp bos gonderiliyor");
  }

  // JSON gövdesi
  JsonDocument doc;
  doc["clockTime"]   = timestamp;
  doc["lastSeen"]    = timestamp;
  doc["temperature"] = s.dhtError ? -1.0f : roundf(s.temperature * 10.0f) / 10.0f;
  doc["humidity"]    = s.dhtError ? -1.0f : roundf(s.humidity * 10.0f) / 10.0f;
  doc["waterLevel"]  = (int)s.waterDistanceCm;
  doc["tds"]         = 0;  // TDS sensoru yok (gelecekte eklenecek)
  doc["pumpStatus"]  = r.pumpOn;
  doc["ledStatus"]   = r.ledOn;
  doc["powerStatus"] = true;
  doc["wifiStatus"]  = true;
  if (r.ledOverride >= 0)  doc["ledOverride"]  = (bool)(r.ledOverride == 1);
  else                     doc["ledOverride"]  = nullptr;
  if (r.pumpOverride >= 0) doc["pumpOverride"] = (bool)(r.pumpOverride == 1);
  else                     doc["pumpOverride"] = nullptr;
  doc["ledStart"]    = r.ledStart;
  doc["ledEnd"]      = r.ledEnd;
  doc["pumpSlots"]   = r.pumpSlots;

  char json[512];
  serializeJson(doc, json, sizeof(json));

  char url[128];
  buildUrl(url, sizeof(url));

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.begin(client, url);
  addFloreneHeaders(http, true);

  int code = http.PUT((uint8_t*)json, strlen(json));
  LOG("PUT", "%s -> HTTP %d", url, code);

  if (code == 200 || code == 204) {
    LOG("PUT", "Florene API: OK");
  } else if (code == 404) {
    LOG("PUT", "HATA: Cihaz bulunamadi (deviceId=%s)", deviceID);
  } else if (code < 0) {
    LOG("PUT", "Baglanti hatasi: %s", http.errorToString(code).c_str());
  } else {
    LOG("PUT", "Beklenmeyen HTTP %d", code);
  }

  http.end();
}
