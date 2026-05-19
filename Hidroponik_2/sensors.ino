// ============================================================
//  SENSORS.INO - Sensör Fonksiyonları
//  Su Seviye Okuma, Medyan Filtreleme
//
//  ESP32-S3 Pin:
//  HC_SR04_TRIG_PIN → GPIO17
//  HC_SR04_ECHO_PIN → GPIO16
// ============================================================

// Medyan Filtreleme Algoritması (Kabarcık sıralama)
int getMedianNum(int bArray[], int iFilterLen) {
  int bTab[iFilterLen];
  for (byte i = 0; i < iFilterLen; i++) bTab[i] = bArray[i];
  int i, j, bTemp;
  for (j = 0; j < iFilterLen - 1; j++) {
    for (i = 0; i < iFilterLen - j - 1; i++) {
      if (bTab[i] > bTab[i + 1]) {
        bTemp    = bTab[i];
        bTab[i]  = bTab[i + 1];
        bTab[i + 1] = bTemp;
      }
    }
  }
  if ((iFilterLen & 1) > 0) {
    bTemp = bTab[(iFilterLen - 1) / 2];
  } else {
    bTemp = (bTab[iFilterLen / 2] + bTab[iFilterLen / 2 - 1]) / 2;
  }
  return bTemp;
}

// HC-SR04 Su Seviyesi (Mesafe) Okuma — 5 ölçüm medyanı
float readWaterLevel() {
  readingWaterLevel = true;
  int distances[5];

  for (int i = 0; i < 5; i++) {
    digitalWrite(HC_SR04_TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(HC_SR04_TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(HC_SR04_TRIG_PIN, LOW);

    // 30ms timeout ≈ ~5m maksimum mesafe
    long duration = pulseIn(HC_SR04_ECHO_PIN, HIGH, 30000);
    if (duration == 0) {
      distances[i] = 999;  // Okuma hatası veya su çok uzakta
    } else {
      distances[i] = (int)((duration * 0.0343f) / 2.0f);  // cm
    }
    delay(50);  // Akustik yankı sönmesini bekle
  }

  float medianDistance = (float)getMedianNum(distances, 5);
  readingWaterLevel = false;
  return medianDistance;
}