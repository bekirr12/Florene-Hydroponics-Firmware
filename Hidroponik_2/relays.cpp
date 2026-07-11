// ============================================================
//  RELAYS.CPP - Role kontrol ve zamanlama motoru
//  Saat bilgisi ESP32-S3 dahili RTC'den getRTCTime() ile alinir.
//  Role polaritesi config.h'dan gelir (RELAY_ON/RELAY_OFF, varsayilan aktif-HIGH).
// ============================================================

#include "config.h"
#include "state.h"
#include "relays.h"
#include <string.h>

void relaysInit() {
  pinMode(RELAY_PUMP_PIN, OUTPUT);
  pinMode(RELAY_LED_PIN,  OUTPUT);
  digitalWrite(RELAY_PUMP_PIN, RELAY_OFF);  // baslangicta KAPALI
  digitalWrite(RELAY_LED_PIN,  RELAY_OFF);
}

int timeToMinutes(const char* hhmm) {
  int h = 0, m = 0;
  if (sscanf(hhmm, "%d:%d", &h, &m) == 2) return h * 60 + m;
  return -1;
}

// LED zamanlamasi aktif mi? (gece yarisini gecen araligi da destekler)
static bool isLedScheduleActive(int nowMin, const char* ledStart, const char* ledEnd) {
  int startMin = timeToMinutes(ledStart);
  int endMin   = timeToMinutes(ledEnd);
  if (startMin < 0 || endMin < 0) return false;

  if (startMin <= endMin) return (nowMin >= startMin && nowMin < endMin);
  return (nowMin >= startMin || nowMin < endMin);  // 20:00 - 06:00 gibi
}

// Mevcut saat herhangi bir pompa slotunun PUMP_SLOT_DURATION penceresinde mi?
static bool isPumpScheduleActive(int nowMin, const char* pumpSlots) {
  char slotsCopy[64];
  strncpy(slotsCopy, pumpSlots, sizeof(slotsCopy) - 1);
  slotsCopy[sizeof(slotsCopy) - 1] = '\0';

  char* token = strtok(slotsCopy, ",");
  while (token != NULL) {
    while (*token == ' ') token++;  // bastaki bosluklari atla
    int slotMin = timeToMinutes(token);
    if (slotMin >= 0) {
      int slotEnd = slotMin + PUMP_SLOT_DURATION;
      if (slotEnd <= 1440) {
        if (nowMin >= slotMin && nowMin < slotEnd) return true;
      } else {
        // gece yarisini gecen slot (or. 23:58 + 5dk)
        if (nowMin >= slotMin || nowMin < (slotEnd - 1440)) return true;
      }
    }
    token = strtok(NULL, ",");
  }
  return false;
}

void updateRelays() {
  struct tm timeinfo;
  bool rtcValid = getRTCTime(timeinfo);
  int  nowMin   = rtcValid ? (timeinfo.tm_hour * 60 + timeinfo.tm_min) : -1;

  // --- Paylasilan durumdan snapshot al (kilit kisa) ---
  char   ledStart[6], ledEnd[6], pumpSlots[64];
  int8_t ledOv, pumpOv;
  float  waterCm;
  bool   curLed, curPump;
  stateLock();
  strncpy(ledStart,  g_state.relays.ledStart,  sizeof(ledStart));
  strncpy(ledEnd,    g_state.relays.ledEnd,    sizeof(ledEnd));
  strncpy(pumpSlots, g_state.relays.pumpSlots, sizeof(pumpSlots));
  ledOv   = g_state.relays.ledOverride;
  pumpOv  = g_state.relays.pumpOverride;
  waterCm = g_state.sensors.waterDistanceCm;
  curLed  = g_state.relays.ledOn;
  curPump = g_state.relays.pumpOn;
  stateUnlock();

  // === LED Kontrolu ===
  bool newLed;
  if (ledOv >= 0)        newLed = (ledOv == 1);
  else if (rtcValid)     newLed = isLedScheduleActive(nowMin, ledStart, ledEnd);
  else                   newLed = curLed;  // RTC gecersiz -> son durum

  if (newLed != curLed) {
    digitalWrite(RELAY_LED_PIN, newLed ? RELAY_ON : RELAY_OFF);  // polarite config.h'dan
    LOG("RELAY", "LED: %s (%s)", newLed ? "ACIK" : "KAPALI",
        ledOv >= 0 ? "override" : "zamanlama");
  }

  // === Pompa Kontrolu ===
  bool newPump;
  bool safetyCut = (waterCm >= WATER_SAFETY_DISTANCE_CM);
  if (safetyCut) {
    newPump = false;  // su kritik -> guvenlik kapatmasi (override'i da ezer)
    if (curPump) {
      LOG("RELAY", "GUVENLIK KAPATMASI: Su kritik (%.1f cm). Pompa kapali!", waterCm);
    }
  } else if (pumpOv >= 0) {
    newPump = (pumpOv == 1);
  } else if (rtcValid) {
    newPump = isPumpScheduleActive(nowMin, pumpSlots);
  } else {
    newPump = curPump;  // RTC gecersiz -> son durum
  }

  if (newPump != curPump) {
    digitalWrite(RELAY_PUMP_PIN, newPump ? RELAY_ON : RELAY_OFF);
    LOG("RELAY", "Pompa: %s (%s)", newPump ? "ACIK" : "KAPALI",
        (pumpOv >= 0 && !safetyCut) ? "override" : "zamanlama");
  }

  // --- Durumu geri yaz ---
  stateLock();
  g_state.relays.ledOn  = newLed;
  g_state.relays.pumpOn = newPump;
  stateUnlock();
}

void checkMidnightOverrideReset() {
  struct tm t;
  if (!getRTCTime(t)) return;

  bool didReset = false;
  stateLock();
  int today = t.tm_mday;
  if (t.tm_hour == 0 && t.tm_min == 0 && g_state.relays.lastResetDay != today) {
    g_state.relays.lastResetDay = today;
    if (g_state.relays.ledOverride >= 0 || g_state.relays.pumpOverride >= 0) {
      g_state.relays.ledOverride  = -1;
      g_state.relays.pumpOverride = -1;
      didReset = true;
    }
  }
  stateUnlock();

  if (didReset) LOG("SCHEDULE", "Gece yarisi: Override sifirlandi");
}
