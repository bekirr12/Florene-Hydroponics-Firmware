// ============================================================
//  RELAYS.H - Role kontrol ve zamanlama motoru arayuzu
// ============================================================

#ifndef RELAYS_H
#define RELAYS_H

// Role pinlerini baslat (KAPALI konumda)
void relaysInit();

// Zamanlama motorunu calistir: zamanlama/override/guvenlik -> role GPIO.
// ControlTask tarafindan periyodik cagrilir. Kilidi kisa tutar.
void updateRelays();

// Gece yarisi (00:00) override'lari sifirla (gunde 1 kez)
void checkMidnightOverrideReset();

#endif // RELAYS_H
