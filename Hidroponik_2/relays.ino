// ============================================================
//  RELAYS.INO - Röle Kontrol ve Zamanlama Motoru
//  LED ve Pompa rölelerini zamanlama/override'a göre günceller
//
//  ESP32-S3 Notu:
//  Harici DS1302 RTC kaldırıldı. Saat bilgisi doğrudan
//  getLocalTime() ile ESP32-S3 dahili RTC'den alınır.
// ============================================================

// "HH:MM" formatını dakikaya çevir (00:00=0, 23:59=1439)
int timeToMinutes(const char* hhmm) {
  int h = 0, m = 0;
  if (sscanf(hhmm, "%d:%d", &h, &m) == 2) {
    return h * 60 + m;
  }
  return -1;  // Geçersiz format
}

// Mevcut RTC saatinin LED zamanlaması içinde olup olmadığını kontrol et
bool isLedScheduleActive(int nowMin) {
  int startMin = timeToMinutes(ledStart);
  int endMin   = timeToMinutes(ledEnd);
  if (startMin < 0 || endMin < 0) return false;

  if (startMin <= endMin) {
    // Normal aralık: ör. 08:00 – 20:00
    return (nowMin >= startMin && nowMin < endMin);
  } else {
    // Gece yarısını geçen aralık: ör. 20:00 – 06:00
    return (nowMin >= startMin || nowMin < endMin);
  }
}

// Mevcut saatin herhangi bir pompa slotunun PUMP_SLOT_DURATION
// dakikalık penceresi içinde olup olmadığını kontrol et
bool isPumpScheduleActive(int nowMin) {
  char slotsCopy[64];
  strncpy(slotsCopy, pumpSlots, sizeof(slotsCopy) - 1);
  slotsCopy[sizeof(slotsCopy) - 1] = '\0';

  char* token = strtok(slotsCopy, ",");
  while (token != NULL) {
    while (*token == ' ') token++;  // Baştaki boşlukları atla

    int slotMin = timeToMinutes(token);
    if (slotMin >= 0) {
      int slotEnd = slotMin + PUMP_SLOT_DURATION;
      if (slotEnd <= 1440) {
        if (nowMin >= slotMin && nowMin < slotEnd) return true;
      } else {
        // Gece yarısını geçen slot (ör. 23:58 + 5dk = 00:03)
        if (nowMin >= slotMin || nowMin < (slotEnd - 1440)) return true;
      }
    }
    token = strtok(NULL, ",");
  }
  return false;
}

// Zamanlama motorunu çalıştır ve röleleri güncelle
void updateRelays() {
  // ESP32-S3 dahili RTC'den yerel saati al
  struct tm timeinfo;
  bool rtcValid = getRTCTime(timeinfo);  // globals.h'daki inline fonksiyon
  int  nowMin   = rtcValid ? (timeinfo.tm_hour * 60 + timeinfo.tm_min) : -1;

  // === LED Kontrolü ===
  bool newLedStatus;
  if (ledOverride >= 0) {
    newLedStatus = (ledOverride == 1);
  } else if (rtcValid) {
    newLedStatus = isLedScheduleActive(nowMin);
  } else {
    Serial.println("[RELAY] RTC geçersiz - LED son durumda");
    newLedStatus = ledStatus;
  }

  if (newLedStatus != ledStatus) {
    ledStatus = newLedStatus;
    digitalWrite(RELAY_LED_PIN, ledStatus ? HIGH : LOW);  // Aktif HIGH
    Serial.printf("[RELAY] LED: %s (%s)\n",
                  ledStatus ? "ACIK" : "KAPALI",
                  ledOverride >= 0 ? "override" : "zamanlama");
  }

  // === Pompa Kontrolü ===
  bool newPumpStatus;
  if (waterDistance >= 16.0) {
    // 16 cm veya daha uzaktaysa su 6 litrenin altına inmiş → GÜVENLİK KAPATMASI
    newPumpStatus = false;
    Serial.printf("[RELAY] GÜVENLIK KAPATMASI: Su seviyesi kritik (Mesafe: %.1f cm). Pompa kapalı!\n",
                  waterDistance);
  } else if (pumpOverride >= 0) {
    newPumpStatus = (pumpOverride == 1);
  } else if (rtcValid) {
    newPumpStatus = isPumpScheduleActive(nowMin);
  } else {
    Serial.println("[RELAY] RTC geçersiz - Pompa son durumda");
    newPumpStatus = pumpStatus;
  }

  if (newPumpStatus != pumpStatus) {
    pumpStatus = newPumpStatus;
    digitalWrite(RELAY_PUMP_PIN, pumpStatus ? LOW : HIGH);  // Aktif LOW
    Serial.printf("[RELAY] Pompa: %s (%s)\n",
                  pumpStatus ? "ACIK" : "KAPALI",
                  pumpOverride >= 0 ? "override" : "zamanlama");
  }
}