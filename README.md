# Florene — Akıllı Saksı / Hidroponik Sistem (ESP32-S3)

Topraksız tarım için ESP32-S3-WROOM-1 tabanlı üretime hazır firmware. Sıcaklık/nem ve su
seviyesi ölçer, LED ve su pompasını zamanlamaya göre sürer, Florene bulut API'si ile senkron
çalışır ve WiFi kurulumunu telefon üzerinden captive portal ile yapar.

> **Versiyon 6.0** — FreeRTOS 2-task mimarisi, modüler `.cpp/.h` yapı, event-driven WiFi.

---

## İçindekiler
- [Donanım](#donanım)
- [Pin Haritası](#pin-haritası)
- [Mimari (FreeRTOS 2-task)](#mimari-freertos-2-task)
- [Dosya Yapısı](#dosya-yapısı)
- [Kurulum ve Derleme](#kurulum-ve-derleme)
- [secrets.h Şablonu](#secretsh-şablonu)
- [WiFi Kurulumu (Captive Portal)](#wifi-kurulumu-captive-portal)
- [Florene API Sözleşmesi](#florene-api-sözleşmesi)
- [Kalibrasyon](#kalibrasyon)
- [Sorun Giderme](#sorun-giderme)
- [Değişiklik Geçmişi](#değişiklik-geçmişi)

---

## Donanım

| Bileşen | Model | Not |
|---------|-------|-----|
| MCU | ESP32-S3-WROOM-1 | 2.4 GHz WiFi |
| Sıcaklık/Nem | **DHT11** | min 1 sn okuma aralığı |
| Su Seviyesi | HC-SR04 ultrasonik | mesafe ölçer (uzak = az su) |
| Pompa Rölesi | Aktif HIGH | HIGH=açık, LOW=kapalı |
| LED Rölesi | Aktif HIGH | HIGH=açık, LOW=kapalı |
| Buton | BOOT (GPIO0) | 3 sn basılı = WiFi sıfırla |

## Pin Haritası

| Fonksiyon | GPIO |
|-----------|------|
| DHT11 veri | 11 |
| HC-SR04 TRIG | 9 |
| HC-SR04 ECHO | 10 |
| Pompa rölesi | 8 |
| LED rölesi | 7 |
| Reset butonu (BOOT) | 0 |

Tüm pinler ve zamanlama/kalibrasyon sabitleri [`config.h`](Hidroponik_2/config.h) içindedir.

---

## Mimari (FreeRTOS 2-task)

Güvenlik-kritik röle mantığı, bloklayan ağ çağrılarından ayrılmıştır. Böylece su seviyesi
güvenlik kapatması bir HTTP isteği beklerken **gecikmez**.

```
                  ┌──────────────────────────────────────┐
                  │        Paylaşılan Durum (g_state)      │
                  │   SensorData · RelayState · NetState   │
                  │        Mutex korumalı (state.h)        │
                  └───────▲───────────────────────▲────────┘
                          │ oku/yaz               │ oku/yaz
        ┌─────────────────┴──────┐      ┌─────────┴──────────────────┐
        │  ControlTask (core 1)  │      │   NetworkTask (core 0)     │
        │  öncelik: yüksek       │      │   öncelik: normal          │
        │                        │      │                            │
        │  • DHT + su seviyesi   │      │  • WiFi event + backoff    │
        │  • zamanlama motoru    │      │  • WiFiManager portal      │
        │  • GÜVENLİK kapatması  │      │  • NTP senkronizasyonu     │
        │  • BOOT butonu         │      │  • Florene GET (5sn)       │
        │  • watchdog reset      │      │  • Florene PUT (10sn)      │
        │  (aga hiç dokunmaz)    │      │  (bloklama serbest)        │
        └────────────────────────┘      └────────────────────────────┘
```

- **ControlTask** ~1 sn periyotla döner; sensör okur, röleleri sürer, güvenlik kapatmasını
  gerçek zamanlı uygular. Ağ işlemleri onu asla bloklamaz.
- **NetworkTask** WiFi bağlantısını (event-driven + exponential backoff), captive portalı,
  NTP saatini ve Florene API GET/PUT çağrılarını yürütür.
- Paylaşılan durum tek bir mutex ile korunur; kritik bölümler kısa tutulur (snapshot alınır).

---

## Dosya Yapısı

```
Hidroponik_2/
├── Hidroponik_2.ino   # yalnızca setup() (init + tasksStart) + boş loop()
├── tasks.h / .cpp     # ControlTask + NetworkTask (FreeRTOS gövdeleri) + tasksStart()
├── config.h           # pin, zamanlama, kalibrasyon (constexpr) + LOG makrosu
├── secrets.h          # API URL/anahtar + portal şifresi (git'e girmez)
├── state.h / .cpp     # SystemState (gruplu durum) + mutex + zaman yardımcısı
├── sensors.h / .cpp   # DHT11 + HC-SR04 (medyan filtre)
├── relays.h / .cpp    # zamanlama motoru + override + güvenlik kapatması
├── netmgr.h / .cpp    # WiFi (event-driven) + portal + NTP
├── florene_api.h/.cpp # Florene bulut API (GET/PUT, HTTPS)
└── logo.h             # portal logosu (PROGMEM PNG)
```

> **Neden `netmgr` ve `Network` değil?** ESP32 çekirdeği kendi `Network.h` dosyasını dahil eder.
> Windows dosya sistemi büyük/küçük harf duyarsız olduğundan `network.h` adlı bir dosya çekirdeğin
> `Network.h`'ını gölgeler ve derleme kırılır. Bu yüzden modül `netmgr` olarak adlandırıldı.

---

## Kurulum ve Derleme

### Gereksinimler
- **ESP32 board paketi:** `esp32:esp32` (3.x)
- **Kütüphaneler:** DHT sensor library, Adafruit Unified Sensor, ArduinoJson (7.x), WiFiManager (tzapu, 2.x)

### arduino-cli ile

```bash
# ESP32 board paketi
arduino-cli config add board_manager.additional_urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32

# Kütüphaneler
arduino-cli lib install "DHT sensor library" "ArduinoJson" "WiFiManager"

# Derleme
arduino-cli compile --fqbn esp32:esp32:esp32s3 Hidroponik_2

# Yükleme (COM portunu kendinize göre değiştirin)
arduino-cli upload -p COM5 --fqbn esp32:esp32:esp32s3 Hidroponik_2
```

### Arduino IDE ile
1. Kart: **ESP32S3 Dev Module**
2. Yukarıdaki kütüphaneleri Library Manager'dan kurun.
3. `Hidroponik_2` klasörünü açıp Yükle'ye basın.

> **Not (bellek):** Firmware varsayılan partition şemasında program alanının ~%94'ünü kullanır.
> OTA veya ek özellik planlıyorsanız **Partition Scheme → "Huge APP (3MB No OTA)"** seçmeniz önerilir.

---

## secrets.h Şablonu

`secrets.h` git'e dahil **edilmez** (`.gitignore`). Aşağıdaki şablonu kendi değerlerinizle doldurun:

```cpp
#ifndef SECRETS_H
#define SECRETS_H

#define FLORENE_API_URL    "https://dev.florene.cloud"
#define FLORENE_API_KEY    "GERÇEK-API-ANAHTARINIZ"
#define PORTAL_AP_PASSWORD "florene123"   // captive portal WiFi şifresi

#endif
```

---

## WiFi Kurulumu (Captive Portal)

1. Cihaz kayıtlı WiFi'ye 10 sn içinde bağlanamazsa (ya da hiç kayıtlı ağ yoksa **hemen**) **`Florene-XXXXXX`** adlı bir WiFi ağı açar.
2. Telefonla bu ağa bağlanın (şifre: `secrets.h`'daki `PORTAL_AP_PASSWORD`).
3. Açılan Türkçe portalda ağınızı seçip şifreyi girin (yalnızca **2.4 GHz** desteklenir).
4. Portal cihazın gerçek durumunu (`/status`) yoklar: bağlanınca **"Bağlantı Başarılı ✓ + IP"**, şifre yanlışsa **"Bağlanılamadı"** gösterir. Tüm arayüz Türkçedir.

**WiFi'yi sıfırlamak:** BOOT butonunu **3 saniye** basılı tutun; kayıtlı ağ silinir ve cihaz yeniden başlar.

---

## Florene API Sözleşmesi

Cihaz **kayıt yapmaz**; yalnızca `device_id` bazlı GET + PUT kullanır. `device_id`, MAC adresinin
son 6 hex hanesidir. Header: `X-Florene-Key: <API_KEY>`.

### GET `/api/v1/devices/:deviceId` — ayarları oku
Yanıt (`success` objesi):
```json
{ "success": {
  "ledStart": "08:00", "ledEnd": "20:00",
  "pumpSlots": "08:00,20:00",
  "ledOverride": null, "pumpOverride": null
}}
```
`ledOverride`/`pumpOverride`: `null`=zamanlama, `true`=zorla aç, `false`=zorla kapat.

### PUT `/api/v1/devices/:deviceId` — sensör verisi gönder
```json
{
  "clockTime": "2026-07-04T14:23:05+03:00",
  "lastSeen":  "2026-07-04T14:23:05+03:00",
  "temperature": 24.3, "humidity": 55.1,
  "waterLevel": 8, "tds": 0,
  "pumpStatus": false, "ledStatus": true,
  "powerStatus": true, "wifiStatus": true,
  "ledOverride": null, "pumpOverride": null,
  "ledStart": "08:00", "ledEnd": "20:00", "pumpSlots": "08:00,20:00"
}
```
- `temperature`/`humidity`: DHT hatası varsa `-1` gönderilir.
- `waterLevel`: HC-SR04 mesafesi (cm, tam sayı).
- `tds`: sensör yok → şimdilik `0` (gelecekte eklenecek).

---

## Kalibrasyon

Tüm değerler [`config.h`](Hidroponik_2/config.h):

| Sabit | Varsayılan | Anlam |
|-------|-----------|-------|
| `WATER_SAFETY_DISTANCE_CM` | 16.0 | Bu mesafe ve üzeri = su kritik → **pompa güvenlik kapatması** |
| `WATER_TANK_FULL_CM` | 3.0 | Dolu tankta sensör–su yüzeyi mesafesi |
| `WATER_TANK_EMPTY_CM` | 20.0 | Boş tankta mesafe |
| `PUMP_SLOT_DURATION` | 5 | Her pompa slotunun çalışma süresi (dakika) |

**Su güvenliği:** Mesafe `WATER_SAFETY_DISTANCE_CM` eşiğine ulaşırsa pompa, override dahil her
koşulda kapatılır. HC-SR04 okuma hatası da "uzak" (999 cm) sayılır → güvenli tarafta kalır.

---

## Sorun Giderme

| Belirti | Olası neden / çözüm |
|---------|---------------------|
| `arduino_event_id_t has not been declared` derleme hatası | Sketch klasöründe `network.h` gibi çekirdek başlığını gölgeleyen dosya. (Bu projede `netmgr` kullanılır.) |
| Program alanı %100'ü aştı | Partition Scheme'i "Huge APP (3MB No OTA)" yapın. |
| `[PUT] NTP sync bekleniyor` | Saat henüz senkronize değil; WiFi bağlanınca NTP otomatik çalışır. |
| Portal açılıyor ama ağ görünmüyor | Yalnızca 2.4 GHz desteklenir; 5 GHz ağlar listelenmez. |
| Pompa hiç çalışmıyor | Su seviyesi kritik olabilir (mesafe ≥ 16 cm) → güvenlik kapatması aktiftir. |
| LED/pompa ters çalışıyor | Röle aktif-HIGH varsayılır; farklı bir modül kullanıyorsanız `config.h`'daki `RELAY_ON`/`RELAY_OFF` değerlerini takas edin. |

Seri monitör: **115200 baud**. `ControlTask` ve `NetworkTask` logları `[RELAY]`, `[WIFI]`, `[GET]`, `[PUT]`, `[NTP]` etiketleriyle akar.

---

## Değişiklik Geçmişi

### v6.0 (mevcut)
- **FreeRTOS 2-task mimarisi:** güvenlik kapatması ağ bloklamalarından bağımsız.
- **Modüler `.cpp/.h` yapı:** `globals.h` → `state.h/.cpp` (mutex korumalı `SystemState`).
- **Kritik bug fix:** Röle polaritesi tek kaynaktan yönetilir (`RELAY_ON`/`RELAY_OFF`, varsayılan aktif-HIGH); DHT tip tanımı tutarlı hale getirildi (DHT11).
- **Event-driven WiFi** (`WiFi.onEvent`) + exponential backoff (polling kaldırıldı).
- **Portal:** kayıtlı ağ yoksa anında açılır (bekleme yok); gerçek bağlantı onayı (`/status` endpoint'i, başarılı/başarısız), tam Türkçe arayüz.
- **HTTPS:** `WiFiClientSecure` + `setInsecure()`.
- **ArduinoJson 7** API'ye geçiş; sıcaklık/nem yuvarlama.
- Ölü timezone altyapısı kaldırıldı (sabit GMT+3); kalibrasyon `config.h`'a taşındı; API anahtarı `secrets.h`'a ayrıldı.

### v5.0 (önceki)
- Non-blocking captive portal, Türkçe arayüz, logo endpoint, dahili RTC + NTP.
