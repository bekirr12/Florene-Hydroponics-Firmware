// ============================================================
//  RELAYS.H - Role kontrol ve zamanlama motoru arayuzu
// ============================================================

#ifndef RELAYS_H
#define RELAYS_H

// Role pinlerini baslat (KAPALI konumda)
void relaysInit();

// "HH:MM" -> dakika (0..1439), gecersizse -1
int timeToMinutes(const char* hhmm);

// Zamanlama motorunu calistir: zamanlama/override/guvenlik -> role GPIO.
// ControlTask tarafindan periyodik cagrilir. Kilidi kisa tutar.
void updateRelays();

// Gece yarisi (00:00) override'lari sifirla (gunde 1 kez)
void checkMidnightOverrideReset();

#endif // RELAYS_H
