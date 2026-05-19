// ============================================================
//  WIFI_UTILS.INO - WiFi Yardımcı Fonksiyonları
//  Cihaz ID, MAC Adresi ve NTP Senkronizasyonu
// ============================================================

// Device ID oluştur (MAC son 6 hane → global char deviceID[7])
void initDeviceID() {
  uint8_t mac[6];
  esp_efuse_mac_get_default(mac);
  snprintf(deviceID, sizeof(deviceID), "%02X%02X%02X", mac[3], mac[4], mac[5]);
}

// Tam MAC Adresini yazdır (heap alloc yok, direkt Serial'e)
void printMAC() {
  uint8_t mac[6];
  esp_efuse_mac_get_default(mac);
  Serial.printf("[INFO] MAC Adres: %02X:%02X:%02X:%02X:%02X:%02X\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

// ==================== NTP FONKSİYONLARI ====================

// EEPROM'dan timezone oku
void loadTimezoneFromMemory() {
  preferences.begin("hidroponik", false);
  timezoneOffset = preferences.getLong("tz_offset", 10800);
  preferences.getString("tz_label", timezoneLabel, sizeof(timezoneLabel));
  preferences.end();

  // İlk başlatmada label boşsa varsayılan ata
  if (strlen(timezoneLabel) == 0) {
    strncpy(timezoneLabel, "Türkiye (GMT+3)", sizeof(timezoneLabel) - 1);
    timezoneLabel[sizeof(timezoneLabel) - 1] = '\0';
  }
  
  Serial.printf("[NTP] Kaydedilmis timezone: %s (%ld sn)\n", timezoneLabel, timezoneOffset);
}

// EEPROM'a timezone kaydet
void saveTimezoneToMemory(long offset, const char* label) {
  preferences.begin("hidroponik", false);
  preferences.putLong("tz_offset", offset);
  preferences.putString("tz_label", label);
  preferences.end();
  
  Serial.printf("[NTP] Timezone kaydedildi: %s (%ld sn)\n", label, offset);
}

// NTP'den saat çek ve RTC'ye yaz
bool syncTimeWithNTP() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[NTP] WiFi bagli degil, sync atlanıyor");
    return false;
  }

  Serial.println("[NTP] Saat senkronizasyonu baslatiliyor...");
  
  configTime(timezoneOffset, 0, NTP_SERVER_1, NTP_SERVER_2, NTP_SERVER_3);
  
  struct tm timeinfo;
  unsigned long startTime = millis();
  bool success = false;
  
  while (millis() - startTime < (unsigned long)NTP_TIMEOUT) {
    if (getLocalTime(&timeinfo)) {
      // Geçerlilik kontrolü
      if (timeinfo.tm_year + 1900 >= 2025 && timeinfo.tm_year + 1900 <= 2099) {
        success = true;
        break;
      }
    }
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (!success) {
    Serial.println("[NTP] HATA: Sunucuya ulaşılamıyor veya geçersiz tarih!");
    return false;
  }

  // Başarılı: sistem saati artık ayarlı, getLocalTime() her zaman çalışır
  Serial.println("[NTP] Saat başarıyla senkronize edildi!");
  Serial.printf("[NTP] Tarih: %02d/%02d/%04d  Saat: %02d:%02d:%02d (%s)\n",
                timeinfo.tm_mday,
                timeinfo.tm_mon + 1,
                timeinfo.tm_year + 1900,
                timeinfo.tm_hour,
                timeinfo.tm_min,
                timeinfo.tm_sec,
                timezoneLabel);

  lastNTPSync    = millis();
  ntpSyncSuccess = true;

  
  return true;
}

// Periyodik NTP güncellemesi gerekli mi?
bool isNTPSyncNeeded() {
  if (!ntpSyncSuccess) return true;
  
  if (millis() - lastNTPSync > NTP_SYNC_INTERVAL) return true;
  
  return false;
}
