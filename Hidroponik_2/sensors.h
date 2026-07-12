// ============================================================
//  SENSORS.H - Sensor arayuzu (DHT11 + HC-SR04)
// ============================================================

#ifndef SENSORS_H
#define SENSORS_H

// Pin modlari + DHT baslat (setup icinde bir kez)
void sensorsInit();

// DHT + su seviyesi oku ve g_state.sensors'u guncelle (kilit iceride).
// ControlTask tarafindan periyodik cagrilir.
void sensorsRead();

#endif // SENSORS_H
