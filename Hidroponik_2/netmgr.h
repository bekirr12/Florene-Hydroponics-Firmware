// ============================================================
//  NETMGR.H - WiFi (event-driven + backoff), portal, NTP
//  Dosya adi bilerek "netmgr": ESP32 cekirdeginin Network.h dosyasi
//  ile (Windows'ta buyuk/kucuk harf duyarsiz) cakismayi onler.
// ============================================================

#ifndef HIDRO_NETMGR_H
#define HIDRO_NETMGR_H

#include <Arduino.h>

// MAC son 6 hane -> global cihaz kimligi
extern char deviceID[7];

// WiFi modu, deviceID, event handler, WiFiManager yapilandirmasi (setup)
void netInit();

// Kayitli WiFi'ye baglanmayi dene; basarisizsa captive portal ac
void netStartConnectOrPortal();

// Portal isle + backoff yeniden baglanma + portal timeout (NetworkTask her tur)
void netLoop();

// Yeni baglanti kuruldu mu? (bir kez true doner, sonra sifirlanir)
// NetworkTask bunu görünce NTP sync + ilk GET yapar.
bool netConsumeJustConnected();

// BOOT butonu 3sn: WiFi ayarlarini sifirla
void netResetWiFiSettings();

void printMAC();

// ---- NTP ----
bool syncTimeWithNTP();
bool isNTPSyncNeeded();

#endif // HIDRO_NETMGR_H
