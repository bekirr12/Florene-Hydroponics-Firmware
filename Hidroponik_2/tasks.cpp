// ============================================================
//  TASKS.CPP - FreeRTOS gorevleri (ControlTask + NetworkTask)
//  - ControlTask (core 1): sensor + role + GUVENLIK, gercek zamanli
//  - NetworkTask (core 0): WiFi/portal/NTP/Florene API (bloklama serbest)
//  Gorevler paylasilan g_state uzerinden haberlesir (state.h, mutex).
// ============================================================

#include <esp_task_wdt.h>

#include "config.h"
#include "state.h"
#include "sensors.h"
#include "relays.h"
#include "netmgr.h"
#include "florene_api.h"
#include "tasks.h"

static TaskHandle_t controlTaskHandle = nullptr;
static TaskHandle_t networkTaskHandle = nullptr;

// ==================== YARDIMCILAR ====================

// BOOT butonu: 3sn basili tutulunca WiFi ayarlarini sifirla + restart
static void handleResetButton() {
  static uint32_t pressStart = 0;
  static bool     pressed    = false;

  if (digitalRead(RESET_BTN_PIN) == LOW) {
    if (!pressed) {
      pressed    = true;
      pressStart = millis();
      LOG("BTN", "BOOT algilandi - 3sn basili tutun...");
    } else if (millis() - pressStart >= 3000) {
      LOG("BTN", "WiFi ayarlari sifirlaniyor, yeniden baslatiliyor...");
      for (int i = 0; i < 5; i++) {
        digitalWrite(RELAY_LED_PIN, RELAY_ON);
        vTaskDelay(pdMS_TO_TICKS(100));
        digitalWrite(RELAY_LED_PIN, RELAY_OFF);
        vTaskDelay(pdMS_TO_TICKS(100));
      }
      netResetWiFiSettings();
      vTaskDelay(pdMS_TO_TICKS(500));
      ESP.restart();
    }
  } else {
    pressed = false;
  }
}

// Seri monitor durum ozeti
static void printStatus() {
  SensorData s;
  RelayState r;
  NetState   n;
  stateLock();
  s = g_state.sensors;
  r = g_state.relays;
  n = g_state.net;
  stateUnlock();

  struct tm ti;
  if (getRTCTime(ti)) {
    Serial.printf("Tarih: %02d/%02d/%04d  Saat: %02d:%02d:%02d\n",
                  ti.tm_mday, ti.tm_mon + 1, ti.tm_year + 1900,
                  ti.tm_hour, ti.tm_min, ti.tm_sec);
  } else {
    Serial.println("[UYARI] RTC henuz NTP ile ayarlanmadi");
  }

  if (s.dhtError) Serial.println("DHT: [HATA - son bilinen deger korunuyor]");
  else            Serial.printf("Nem: %.1f%%  Sicaklik: %.1f C\n", s.humidity, s.temperature);

  Serial.printf("Su Seviyesi (Mesafe): %.1f cm\n", s.waterDistanceCm);
  Serial.printf("Pompa: %s  LED: %s  WiFi: %s  NTP: %s\n",
                r.pumpOn ? "ACIK" : "KAPALI",
                r.ledOn  ? "ACIK" : "KAPALI",
                n.wifiConnected ? "OK" : "YOK",
                n.ntpSynced ? "OK" : "YOK");
  Serial.println("-----------------------");
}

// ==================== GOREVLER ====================

// ControlTask: gercek zamanli - aga hic dokunmaz
static void controlTask(void* pv) {
  esp_task_wdt_add(NULL);
  uint32_t tSensor = 0, tSchedule = 0, tPrint = 0;

  for (;;) {
    esp_task_wdt_reset();
    uint32_t now = millis();

    if (now - tSensor >= SENSOR_READ_INTERVAL) {
      tSensor = now;
      sensorsRead();
    }
    if (now - tSchedule >= SCHEDULE_CHECK_INTERVAL) {
      tSchedule = now;
      updateRelays();
      checkMidnightOverrideReset();
    }
    handleResetButton();
    if (now - tPrint >= SERIAL_PRINT_INTERVAL) {
      tPrint = now;
      printStatus();
    }

    vTaskDelay(pdMS_TO_TICKS(CONTROL_TASK_PERIOD_MS));
  }
}

// NetworkTask: WiFi/portal/NTP/API - bloklama serbest
static void networkTask(void* pv) {
  esp_task_wdt_add(NULL);

  netStartConnectOrPortal();

  uint32_t tGet = 0, tPut = 0, tNtp = 0;

  for (;;) {
    esp_task_wdt_reset();
    netLoop();

    // Yeni baglanti -> once NTP, sonra ilk ayar cekimi
    if (netConsumeJustConnected()) {
      syncTimeWithNTP();
      checkCommands();
    }

    bool connected;
    stateLock();
    connected = g_state.net.wifiConnected;
    stateUnlock();

    if (connected) {
      uint32_t now = millis();
      if (now - tGet >= COMMAND_CHECK_INTERVAL) { tGet = now; checkCommands(); }
      if (now - tPut >= API_SEND_INTERVAL)      { tPut = now; sendToAPI(); }
      if (now - tNtp >= 60000) {
        tNtp = now;
        if (isNTPSyncNeeded()) syncTimeWithNTP();
      }
    }

    vTaskDelay(pdMS_TO_TICKS(50));  // portal/HTTP isteklerine duyarli kal
  }
}

// ==================== BASLATMA ====================
void tasksStart() {
  xTaskCreatePinnedToCore(controlTask, "ControlTask", CONTROL_TASK_STACK,
                          nullptr, CONTROL_TASK_PRIO, &controlTaskHandle, CONTROL_TASK_CORE);
  xTaskCreatePinnedToCore(networkTask, "NetworkTask", NETWORK_TASK_STACK,
                          nullptr, NETWORK_TASK_PRIO, &networkTaskHandle, NETWORK_TASK_CORE);
}
