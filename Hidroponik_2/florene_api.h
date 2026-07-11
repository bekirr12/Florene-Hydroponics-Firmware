// ============================================================
//  FLORENE_API.H - Florene bulut API (GET ayar / PUT veri)
//  ESP32 KAYIT YAPMAZ; yalnizca device_id bazli GET + PUT.
// ============================================================

#ifndef FLORENE_API_H
#define FLORENE_API_H

// GET /api/v1/devices/:deviceId -> zamanlama ayarlari + override'lari oku
void checkCommands();

// PUT /api/v1/devices/:deviceId -> sensor verisi + cihaz durumu gonder
void sendToAPI();

#endif // FLORENE_API_H
