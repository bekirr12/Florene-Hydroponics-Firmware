// ============================================================
//  STATE.H - Paylasilan Sistem Durumu (FreeRTOS mutex korumali)
//  Onceki globals.h yerine gecer. Header'da EXTERN bildirim,
//  tanim state.cpp icinde -> coklu-tanim (ODR) hatasi olmaz.
//
//  Erisim kurali: g_state alanlarina her zaman stateLock()/
//  stateUnlock() arasinda erisilir (ControlTask ve NetworkTask
//  ayni veriyi paylasir). Kritik bolumler kisa tutulur.
// ============================================================

#ifndef STATE_H
#define STATE_H

#include <Arduino.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// ---- Sensor verileri (ControlTask yazar, NetworkTask okur) ----
struct SensorData {
  float temperature   = NAN;   // C (NAN = henuz okunmadi)
  float humidity      = NAN;   // %
  float waterDistanceCm = 0.0f;// HC-SR04 mesafe (buyuk = az su)
  bool  dhtError      = true;  // ilk basarili okumada false
};

// ---- Role durumu + zamanlama ayarlari ----
// pumpOn/ledOn: ControlTask yazar. Zamanlama alanlari: NetworkTask (GET) yazar.
struct RelayState {
  bool   pumpOn       = false;
  bool   ledOn        = false;
  char   ledStart[6]  = "08:00";
  char   ledEnd[6]    = "20:00";
  char   pumpSlots[64]= "08:00,20:00";
  int8_t ledOverride  = -1;    // -1=zamanlama, 0=zorla kapat, 1=zorla ac
  int8_t pumpOverride = -1;
  int    lastResetDay = -1;    // gece yarisi override sifirlama icin
};

// ---- Ag / saat durumu (NetworkTask yazar) ----
struct NetState {
  bool wifiConnected = false;
  bool portalActive  = false;
  bool ntpSynced     = false;
};

struct SystemState {
  SensorData sensors;
  RelayState relays;
  NetState   net;
};

extern SystemState      g_state;
extern SemaphoreHandle_t g_stateMutex;

// Mutex olustur + config varsayilanlarini yukle (setup icinde cagrilir)
void stateInit();

inline bool stateLock(TickType_t timeout = portMAX_DELAY) {
  return xSemaphoreTake(g_stateMutex, timeout) == pdTRUE;
}
inline void stateUnlock() {
  xSemaphoreGive(g_stateMutex);
}

// ---- Zaman yardimcisi ----
// ESP32-S3 dahili RTC'den yerel saati al. configTime() sonrasi calisir.
// 2025'ten kucuk yil = NTP henuz senkronize olmadi -> gecersiz.
inline bool getRTCTime(struct tm &timeinfo) {
  if (!getLocalTime(&timeinfo)) return false;
  if (timeinfo.tm_year + 1900 < 2025) return false;
  return true;
}

#endif // STATE_H
