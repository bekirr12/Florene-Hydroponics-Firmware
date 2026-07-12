// ============================================================
//  SENSORS.CPP - DHT11 + HC-SR04 su seviyesi (medyan filtre)
// ============================================================

#include "DHT.h"
#include "config.h"
#include "state.h"
#include "sensors.h"

static DHT dht(DHT_PIN, DHT_TYPE);

void sensorsInit() {
  pinMode(HC_SR04_TRIG_PIN, OUTPUT);
  pinMode(HC_SR04_ECHO_PIN, INPUT);
  digitalWrite(HC_SR04_TRIG_PIN, LOW);
  dht.begin();
  analogSetAttenuation(ADC_11db);  // ESP32-S3 ADC atenuasyonu
}

// Medyan filtre (kabarcik siralama) - aykiri okumalari eler
static int getMedianNum(int arr[], int len) {
  int tmp[WATER_READ_SAMPLES];
  for (int i = 0; i < len; i++) tmp[i] = arr[i];
  for (int j = 0; j < len - 1; j++) {
    for (int i = 0; i < len - j - 1; i++) {
      if (tmp[i] > tmp[i + 1]) {
        int t = tmp[i]; tmp[i] = tmp[i + 1]; tmp[i + 1] = t;
      }
    }
  }
  if (len & 1) return tmp[len / 2];
  return (tmp[len / 2] + tmp[len / 2 - 1]) / 2;
}

static float readWaterLevelCm() {
  int distances[WATER_READ_SAMPLES];

  for (int i = 0; i < WATER_READ_SAMPLES; i++) {
    digitalWrite(HC_SR04_TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(HC_SR04_TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(HC_SR04_TRIG_PIN, LOW);

    // 30ms timeout ~= 5m maksimum mesafe
    long duration = pulseIn(HC_SR04_ECHO_PIN, HIGH, 30000);
    if (duration == 0) {
      distances[i] = WATER_READ_ERROR_CM;  // okuma hatasi -> guvenli tarafta (uzak = az su)
    } else {
      distances[i] = (int)((duration * 0.0343f) / 2.0f);  // cm
    }
    vTaskDelay(pdMS_TO_TICKS(50));  // akustik yankinin sonmesini bekle
  }

  return (float)getMedianNum(distances, WATER_READ_SAMPLES);
}

void sensorsRead() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  float w = readWaterLevelCm();

  stateLock();
  if (isnan(h) || isnan(t)) {
    g_state.sensors.dhtError = true;   // son bilinen deger korunur
  } else {
    g_state.sensors.dhtError   = false;
    g_state.sensors.temperature = t;
    g_state.sensors.humidity    = h;
  }
  g_state.sensors.waterDistanceCm = w;
  stateUnlock();
}
