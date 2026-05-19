// ============================================================
//  FLORENE_API.INO - Düzeltilmiş Versiyon
//  Kural:
//  - ESP32 KAYIT YAPMAZ (registerDevice kaldırıldı)
//  - GET  → /api/v1/devices/:deviceId  (ayarları oku)
//  - PUT  → /api/v1/devices/:deviceId  (sensör verisini gönder)
// ============================================================

#define FLORENE_API_URL  "https://dev.florene.cloud"
#define FLORENE_API_KEY  "XYZ-Florene-Key"

// Ortak HTTP header'ları
void addFloreneHeaders(HTTPClient &http, bool withContent) {
  http.setTimeout(HTTP_TIMEOUT);
  http.addHeader("X-Florene-Key", FLORENE_API_KEY);
  if (withContent) {
    http.addHeader("Content-Type", "application/json");
  }
}

// ── GET /api/v1/devices/:deviceId ──
// Sunucudan zamanlama ayarlarını ve override'ları oku
void checkCommands() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  char url[128];
  snprintf(url, sizeof(url), "%s/api/v1/devices/%s",
           FLORENE_API_URL, deviceID);

  http.begin(url);
  addFloreneHeaders(http, false);

  int code = http.GET();
  Serial.printf("[GET] %s → HTTP %d\n", url, code);

  if (code == 200) {
    String payload = http.getString();
    Serial.printf("[GET] Body: %s\n", payload.c_str());

    StaticJsonDocument<1024> doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
      Serial.printf("[GET] JSON parse hatasi: %s\n", err.c_str());
      http.end();
      return;
    }

    // Sunucu {"status":200,"success":{...}} formatında dönüyor
    JsonObject device = doc["success"];
    if (device.isNull()) {
      Serial.println("[GET] HATA: 'success' objesi bulunamadi!");
      http.end();
      return;
    }

    // LED zamanlama
    if (!device["ledStart"].isNull())
      strncpy(ledStart, device["ledStart"] | "08:00", sizeof(ledStart) - 1);

    if (!device["ledEnd"].isNull())
      strncpy(ledEnd, device["ledEnd"] | "20:00", sizeof(ledEnd) - 1);

    // Pompa slotları
    if (!device["pumpSlots"].isNull())
      strncpy(pumpSlots, device["pumpSlots"] | "08:00", sizeof(pumpSlots) - 1);

    // LED override (null → -1, true → 1, false → 0)
    if (device["ledOverride"].isNull())
      ledOverride = -1;
    else
      ledOverride = device["ledOverride"].as<bool>() ? 1 : 0;

    // Pompa override
    if (device["pumpOverride"].isNull())
      pumpOverride = -1;
    else
      pumpOverride = device["pumpOverride"].as<bool>() ? 1 : 0;

    Serial.printf("[GET] ledStart=%s ledEnd=%s pumpSlots=%s\n",
                  ledStart, ledEnd, pumpSlots);
    Serial.printf("[GET] ledOverride=%d pumpOverride=%d\n",
                  ledOverride, pumpOverride);

    updateRelays();

  } else if (code == 404) {
    Serial.println("[GET] HATA: Cihaz DB'de bulunamadi! (deviceId yanlis veya kayit yok)");
  } else if (code < 0) {
    Serial.printf("[GET] Baglanti hatasi: %s\n", http.errorToString(code).c_str());
  } else {
    String resp = http.getString();
    Serial.printf("[GET] Beklenmeyen HTTP %d: %s\n", code, resp.c_str());
  }

  http.end();
}

// ── PUT /api/v1/devices/:deviceId ──
// Sensör verilerini ve cihaz durumunu sunucuya gönder
void sendToAPI() {
  if (WiFi.status() != WL_CONNECTED) return;

  if (!ntpSyncSuccess) {
    Serial.println("[PUT] NTP sync bekleniyor, gonderim atlandi");
    return;
  }

  // Zaman damgası
  char timestamp[26];  // 25 karakter + null terminator
  struct tm timeinfo;
  if (getRTCTime(timeinfo)) {
    long absOffset = abs(timezoneOffset);
    int tzHour = absOffset / 3600;
    int tzMin  = (absOffset % 3600) / 60;
    char tzSign = (timezoneOffset >= 0) ? '+' : '-';

    snprintf(timestamp, sizeof(timestamp),
            "%04d-%02d-%02dT%02d:%02d:%02d%c%02d:%02d",
            timeinfo.tm_year + 1900,
            timeinfo.tm_mon + 1,
            timeinfo.tm_mday,
            timeinfo.tm_hour,
            timeinfo.tm_min,
            timeinfo.tm_sec,
            tzSign, tzHour, tzMin);
  } else {
    snprintf(timestamp, sizeof(timestamp), "");
    Serial.println("[PUT] UYARI: NTP sync yok, timestamp bos gonderiliyor");
  }

  // JSON body
  StaticJsonDocument<512> doc;
  doc["clockTime"]    = timestamp;
  doc["lastSeen"]     = timestamp;
  doc["temperature"]  = dhtError ? -1.0f : (float)((int)(temperature * 10)) / 10.0f;
  doc["humidity"]     = dhtError ? -1.0f : (float)((int)(humidity * 10)) / 10.0f;
  doc["waterLevel"]   = (int)waterDistance;
  doc["tds"]          = 0;
  doc["pumpStatus"]   = pumpStatus;
  doc["ledStatus"]    = ledStatus;
  doc["powerStatus"]  = true;
  doc["wifiStatus"]   = true;
  if (ledOverride >= 0)  doc["ledOverride"]  = (bool)(ledOverride == 1);
  else                   doc["ledOverride"]  = nullptr;
  if (pumpOverride >= 0) doc["pumpOverride"] = (bool)(pumpOverride == 1);
  else                   doc["pumpOverride"] = nullptr;
  doc["ledStart"]     = ledStart;
  doc["ledEnd"]       = ledEnd;
  doc["pumpSlots"]    = pumpSlots;

  char json[512];
  serializeJson(doc, json, sizeof(json));

  char url[128];
  snprintf(url, sizeof(url), "%s/api/v1/devices/%s",
           FLORENE_API_URL, deviceID);

  Serial.printf("[PUT] %s\n", url);
  Serial.printf("[PUT] Body: %s\n", json);

  HTTPClient http;
  http.begin(url);
  addFloreneHeaders(http, true);

  int code = http.PUT(json);
  Serial.printf("[PUT] HTTP %d\n", code);

  if (code == 200 || code == 204) {
    Serial.println("[PUT] Florene API: OK");
  } else if (code == 404) {
    Serial.println("[PUT] HATA: Cihaz bulunamadi! deviceId kontrol et.");
  } else if (code < 0) {
    Serial.printf("[PUT] Baglanti hatasi: %s\n", http.errorToString(code).c_str());
  } else {
    String resp = http.getString();
    Serial.printf("[PUT] Beklenmeyen HTTP %d: %s\n", code, resp.c_str());
  }

  http.end();
}