// ============================================================
//  STATE.CPP - Paylasilan durum tanimi + baslatma
// ============================================================

#include "state.h"
#include "config.h"
#include <string.h>

SystemState      g_state;
SemaphoreHandle_t g_stateMutex = nullptr;

void stateInit() {
  g_stateMutex = xSemaphoreCreateMutex();

  // Zamanlama varsayilanlarini config'ten yukle (tek dogruluk kaynagi)
  strncpy(g_state.relays.ledStart,  DEFAULT_LED_START,  sizeof(g_state.relays.ledStart)  - 1);
  strncpy(g_state.relays.ledEnd,    DEFAULT_LED_END,    sizeof(g_state.relays.ledEnd)    - 1);
  strncpy(g_state.relays.pumpSlots, DEFAULT_PUMP_SLOTS, sizeof(g_state.relays.pumpSlots) - 1);
  g_state.relays.ledStart[sizeof(g_state.relays.ledStart)   - 1] = '\0';
  g_state.relays.ledEnd[sizeof(g_state.relays.ledEnd)       - 1] = '\0';
  g_state.relays.pumpSlots[sizeof(g_state.relays.pumpSlots) - 1] = '\0';
}
