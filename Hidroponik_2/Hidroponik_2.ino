// ============================================================
//  TOPRAKSIZ TARIM - HIDROPONIK / AKILLI SAKSI (v6.0)
//  Hedef: ESP32-S3-WROOM-1
// ============================================================
//  Mimari: FreeRTOS 2-task (tasks.cpp)
//    - ControlTask (core 1) : sensor + role + GUVENLIK, gercek zamanli
//    - NetworkTask (core 0) : WiFi/portal/NTP/Florene API (bloklama serbest)
//  Paylasilan durum: state.h icindeki g_state (mutex korumali)
//
//  Bu dosya yalnizca orkestrasyon yapar: setup() donanimi/durumu
//  baslatir ve gorevleri olusturur; loop() bostur.
//
//  Donanim:
//    DHT11        -> GPIO11
//    HC-SR04      -> TRIG GPIO9 / ECHO GPIO10
//    Pompa rolesi -> GPIO8  (aktif HIGH)
//    LED rolesi   -> GPIO7  (aktif HIGH)
//    BOOT butonu  -> GPIO0  (3sn basili = WiFi sifirla)
// ============================================================

#include <esp_task_wdt.h>

#include "config.h"
#include "state.h"
#include "sensors.h"
#include "relays.h"
#include "netmgr.h"
#include "tasks.h"

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  vTaskDelay(pdMS_TO_TICKS(100));
  Serial.println("\n============================================");
  Serial.println("  TOPRAKSIZ TARIM - Akilli Saksi v6.0");
  Serial.println("  ESP32-S3 | FreeRTOS 2-task | Dahili RTC+NTP");
  Serial.println("============================================\n");

  // Paylasilan durum + mutex
  stateInit();

  // Ag altyapisi (WiFi modu, deviceID, event handler, WiFiManager config)
  netInit();

  Serial.println("-------- CIHAZ BILGILERI --------");
  LOG("INFO", "Cihaz ID: %s", deviceID);
  printMAC();
  Serial.println("---------------------------------");

  // Pinler + sensorler + roleler
  pinMode(RESET_BTN_PIN, INPUT_PULLUP);
  sensorsInit();
  relaysInit();

  // Onboarding LED flash (3x - cihaz acildi sinyali)
  for (int i = 0; i < 3; i++) {
    digitalWrite(RELAY_LED_PIN, RELAY_ON);
    vTaskDelay(pdMS_TO_TICKS(250));
    digitalWrite(RELAY_LED_PIN, RELAY_OFF);
    vTaskDelay(pdMS_TO_TICKS(250));
  }

  // Watchdog (her gorev kendini ekler + reset eder)
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms     = (uint32_t)WDT_TIMEOUT * 1000,
    .idle_core_mask = 0,
    .trigger_panic  = true
  };
  esp_task_wdt_reconfigure(&wdt_config);
  disableLoopWDT();  // bos loop() WDT'ye takilmasin

  // FreeRTOS gorevlerini baslat (tasks.cpp)
  tasksStart();

  Serial.println("[OK] Gorevler baslatildi. Sistem hazir.\n");
}

// FreeRTOS gorevleri isi yapar; loop bostur.
void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}
