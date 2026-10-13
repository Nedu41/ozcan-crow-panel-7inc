/*
 * ============================================================
 *  ESP01 Röle Kontrol Firmware
 *  Versiyon : v2.9.0  (TX gucu ayari /txpower; push 15sn; saat EEPROM kaydi saatte bir; UDP durum push; bloklamayan WiFi/MQTT/NTP, /status'ta kopma teshisi; MQTT cihaz basina ac/kapat - /mqtt; olay tabanli zamanlayici; mDNS+OTA; derleme: PlatformIO, src/)
 *  Tarih     : Şubat 2026
 * ============================================================
 *
 *  DONANIM
 *  -------
 *  Kart     : AI-Thinker ESP-01 (ESP8266)
 *  Flash    : 1MB
 *  Röle 1   : GPIO0 (RELAY_1_PIN)
 *  Röle 2   : GPIO2 (RELAY_2_PIN)
 *  Loji     : RELAY_ON = HIGH / RELAY_OFF = LOW
 *
 *  ÖZELLİKLER
 *  ----------
 *  [+] 2 bağımsız röle HTTP üzerinden anlık kontrol
 *  [+] Enerji kesintisi sonrası röle konumu EEPROM'dan geri yükleme
 *  [+] MQTT (PubSubClient) ile uzaktan röle kontrol ve durum takibi
 *  [+] MQTT Boot Guard: açılışta broker'dan gelen retain mesajları
 *      5 sn boyunca yoksayılır — eski "OFF" mesajı röleyi sıfırlamaz
 *  [+] MAC adresine göre otomatik benzersiz MQTT topic üretimi
 *        esp01/<MAC>/r1/set|state
 *        esp01/<MAC>/r2/set|state
 *        esp01/<MAC>/status
 *  [+] Gün/saat bazlı zamanlama (Schedule) — 7 gün bağımsız
 *  [+] Olay tabanli zamanlayici: sadece baslangic/bitis aninda islem (v2.5)
 *  [+] NTP ile otomatik zaman senkronizasyonu (pool.ntp.org, UTC+3)
 *  [+] WiFi kesilirse son bilinen zaman EEPROM'dan devam eder
 *  [+] WiFiManager ile captive portal üzerinden ağ kurulumu
 *  [+] MQTT sunucu/port/kullanıcı/şifre WiFiManager portal üzerinden ayarlanır
 *  [+] /status endpoint'i wifi_rssi (dBm) değeri döner — merkezi HUB göstergesi
 *  [+] /mac endpoint'i — merkezi HUB cihaz tanımlama için
 *  [+] /eeprom_clr — EEPROM sıfırlama (fabrika reset)
 *  [+] /wifi_clr   — WiFi ayarları silme ve yeniden portal aç
 *
 *  EEPROM HARİTASI
 *  ---------------
 *   0 -  31 : Schedule Röle 1 (struct Schedule)
 *  32 -  63 : Schedule Röle 2 (struct Schedule)
 * 100 - 163 : MQTT Sunucu adresi (64 byte, char[])
 * 200 - 201 : MQTT Port (2 byte, high-low)
 * 220 - 251 : MQTT Kullanıcı adı (32 byte)
 * 300 - 331 : MQTT Şifre (32 byte)
 * 380 - 383 : Son bilinen Unix zaman damgası (uint32)
 * 400       : Röle 1 son durumu (0=OFF, 1=ON)
 * 401       : Röle 2 son durumu (0=OFF, 1=ON)
 *
 *  HTTP API
 *  --------
 *  GET /control?id=<1|2>&action=<on|off>  Röle anlık kontrol
 *  GET /save?relay_id=<1|2>&start_h=&start_m=&end_h=&end_m=&days=&active=
 *                                          Zamanlama kaydet
 *  GET /status                             JSON durum (röle, schedule, wifi_rssi…)
 *  GET /mac                                MAC adresi (düz metin)
 *  GET /topics                             MQTT topic listesi
 *  GET /relay_status                       EEPROM ve anlık röle durumu karşılaştırma
 *  GET /eeprom_clr                         EEPROM'u sıfırla
 *  GET /wifi_clr                           WiFi ayarlarını sil, portal aç
 *
 *  MQTT API
 *  --------
 *  Subscribe : esp01/<MAC>/r1/set    → "1"/"ON"  veya "0"/"OFF"
 *  Subscribe : esp01/<MAC>/r2/set    → "1"/"ON"  veya "0"/"OFF"
 *  Publish   : esp01/<MAC>/r1/state  → "1" veya "0"  (retain)
 *  Publish   : esp01/<MAC>/r2/state  → "1" veya "0"  (retain)
 *  Publish   : esp01/<MAC>/status    → JSON (relay_1, relay_2, wifi_rssi, uptime…)
 *
 *  KÜTÜPHANELERİ
 *  -------------
 *  ESP8266 Arduino Core  : 3.1.x
 *  ESP8266WebServer      : (core içinde)
 *  WiFiManager           : tzapu/WiFiManager
 *  ArduinoJson           : bblanchon/ArduinoJson  v6.x
 *  PubSubClient          : knolleary/PubSubClient
 *  NTPClient             : arduino-libraries/NTPClient
 *  EEPROM                : (core içinde)
 *
 *  GELİŞTİRİCİ NOTLARI
 *  --------------------
 *  - GPIO0 ve GPIO2 boot sırasında HIGH olmalıdır.
 *    Röle bağlantısını NC (Normalde Kapalı) üzerinden yapın;
 *    LOW=Aktif röle modülü kullanıyorsanız boot'ta kısa kapanma olabilir.
 *  - MQTT Boot Guard süresi MQTT_BOOT_GUARD_MS sabiti ile ayarlanır (varsayılan 5000 ms).
 *  - İlk kurulumda "ESP01_SETUP" AP'sine bağlanıp portal üzerinden
 *    WiFi ve MQTT bilgilerini girin.
 *
 * ============================================================
 */
// Ayni kod iki kartta: ESP-01 (ESP8266) ve ESP32-C3 SuperMini. HUB'a ayni
// HTTP arayuzunu (/status /control /mac /save ...) sunar, HUB fark gormez.
#ifdef ESP32
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include "esp32_compat.h"
typedef WebServer WebServerT;
#if defined(BOARD_ESP32DEV)
// ESP32 DevKit (WROOM-32): 26/27 serbest, acilista guvenli (strapping/flas/UART pini degil)
#define HOST_PREFIX "esp32-"
#define SETUP_AP "ESP32_SETUP"
#define RELAY_1_PIN 26
#define RELAY_2_PIN 27
#elif defined(BOARD_ESP32S3)
// ESP32-S3 DevKitC-1: 4/5 serbest (0/3/45/46 strapping, 19/20 USB, 26-37 flas/PSRAM kullanilmaz)
#define HOST_PREFIX "s3-"
#define SETUP_AP "S3_SETUP"
#define RELAY_1_PIN 4
#define RELAY_2_PIN 5
#else
#define HOST_PREFIX "c3-"
#define SETUP_AP "C3_SETUP"
// SuperMini: GPIO2/8/9 acilis (strapping) pini, 8 kart LED'i, 18/19 USB,
// 20/21 seri - hicbiri kullanilmaz. 3 ve 4 serbest.
#define RELAY_1_PIN 3
#define RELAY_2_PIN 4
#endif
#else
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPClient.h>
#include <Hash.h>
typedef ESP8266WebServer WebServerT;
#define HOST_PREFIX "esp01-"
#define SETUP_AP "ESP01_SETUP"
#ifdef BOARD_ESP07
// ESP-07: GPIO0/2/15 acilis (strapping), 1/3 seri, 6-11 flas, 16 deep-sleep pini
// - hicbiri kullanilmaz. 4 ve 5 serbest, acilista rolesiz (glitch yok).
#define RELAY_1_PIN 4
#define RELAY_2_PIN 5
#elif defined(BOARD_NODEMCU)
// NodeMCU / ESP-12E/F / Wemos D1 mini: D1 = GPIO5, D2 = GPIO4 (acilista guvenli, strapping degil)
#define RELAY_1_PIN 5
#define RELAY_2_PIN 4
#elif defined(BOARD_SONOFF)
// Sonoff Basic (ESP8285): role GPIO12. Ikinci role yok: GPIO14 (baslik pini, bos) yer tutucu
#define RELAY_1_PIN 12
#define RELAY_2_PIN 14
#else
#define RELAY_1_PIN 0
#define RELAY_2_PIN 2
#endif
#endif
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>

#define FW_VERSION "2.9.0"  // /status "fw" alani

// Varsayilan WiFi (sifre koda gomulmesin diye ayri dosyada, paylasilmaz)
#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef DEFAULT_WIFI_SSID
#define DEFAULT_WIFI_SSID ""
#endif
#ifndef DEFAULT_WIFI_PASS
#define DEFAULT_WIFI_PASS ""
#endif

// Ag etiketi: genel IP'nin 16 bitlik ozeti. MQTT sayfasi kendi genel IP'sinin
// ozetiyle karsilastirip sadece ayni evdeki (ayni internet baglantisi) cihazlari
// listeler. IP'nin kendisi genel broker'a yayinlanmaz.
// (updateNetTag() tanimi publishStatus() yaninda - .ino prototip siralamasi icin)
String netTag = "";
unsigned long lastNetCheck = 0;

// ---------------------- EEPROM AYARLARI ----------------------
// DÜZELTİLDİ: Adres haritası netleştirildi, çakışma olmadığı doğrulandı
#define EEPROM_SIZE          512
#define SCHED_R1_ADDR        0    //  0-31: Schedule R1 (sizeof(Schedule) = ~24 byte)
#define SCHED_R2_ADDR        32   // 32-63: Schedule R2
// 64-99: Rezerve
#define MQTT_SERVER_ADDR     100  // 100-163: MQTT server (64 byte)
#define MQTT_PORT_ADDR       200  // 200-201: Port (2 byte, high-low)
#define MQTT_USER_ADDR       220  // 220-251: Username (32 byte)
#define MQTT_PASS_ADDR       300  // 300-331: Password (32 byte)
#define LAST_KNOWN_TIME_ADDR 380  // 380-383: Son bilinen unix time (4 byte)
#define RELAY_1_STATE_ADDR   400  // 400: Röle 1 son durumu
#define RELAY_2_STATE_ADDR   401  // 401: Röle 2 son durumu
#define MQTT_ENABLED_ADDR    402  // 402: MQTT_DISABLED_MAGIC = kapali, baska her deger = acik
#define MQTT_DISABLED_MAGIC  0xA5 // bos/silinmis EEPROM (0x00/0xFF) varsayilan olarak ACIK kalsin
#define TX_POWER_ADDR        403  // 403: WiFi gonderme gucu, 0.25 dBm biriminde; aralik disi = varsayilan

// v2.9: gonderme gucu HUB Cihazlar sayfasindan ayarlanir (0.25 dBm birimi,
// ESP32 esp_wifi_set_max_tx_power ile ayni). Guc sadece GONDERIMI etkiler;
// yuksek guc zayif beslemede brownout'a yol acabilir.
#ifdef ESP32
// SuperMini'nin zayif anteninde tam guc bazi kartlarda baglanmayi bozuyor
// (bilinen sorun) - varsayilan dusuk, build_flags ile degistirilebilir.
#ifndef C3_TX_POWER
#define C3_TX_POWER WIFI_POWER_8_5dBm
#endif
#define TX_POWER_DEFAULT_Q ((uint8_t)C3_TX_POWER)
#define TX_POWER_MAX_Q     84   // 21 dBm
#else
#define TX_POWER_DEFAULT_Q 82   // 20.5 dBm = ESP8266 maksimumu
#define TX_POWER_MAX_Q     82
#endif
#define TX_POWER_MIN_Q     8    // 2 dBm
uint8_t txPowerQ = TX_POWER_DEFAULT_Q;

// ---------------------- MQTT DEĞİŞKENLERİ ----------------------
char mqtt_server[64];
char mqtt_port_str[6] = "1883";
char mqtt_username[32];
char mqtt_password[32];

int mqtt_port = 1883;
// v2.6: HUB Cihazlar sayfasindan cihaz basina ac/kapat. Kapaliyken genel
// broker'a hic baglanmaz - disaridan MQTT ile komut alinamaz.
bool mqttEnabled = true;
unsigned long lastMqttAttempt = 0;
// v2.7: basarisiz denemede aralik 5sn -> 60sn'ye kadar ikiye katlanir
unsigned long mqttRetryInterval = 5000;

WiFiClient espClient;
PubSubClient mqttClient(espClient);

String topicSetR1;
String topicSetR2;
String topicStateR1;
String topicStateR2;
String topicStatus;
String topicEvent;   // v2.5: her role degisimi + sebebi (HUB olay kaydi icin)

// ---------------------- DİĞER AYARLAR ----------------------
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 3 * 3600, 60000);

// v2.8: durum push - HUB'daki STATUS_PUSH_PORT ile ayni olmali
#define STATUS_PUSH_PORT     4211
#define STATUS_PUSH_INTERVAL 15000  // v2.9: 30 -> 15sn, kapanan cihaz HUB ekraninda daha cabuk kopuk gorunsun
WiFiUDP pushUdp;
IPAddress hubIp;  // bilinmiyorsa (0.0.0.0) push yerel aga yayin olarak gider
volatile bool pushPending = true;  // acilista hemen bildir
unsigned long lastStatusPush = 0;

#ifdef RELAY_ACTIVE_LOW   // cogu 2 kanalli optokuplorlu role modulu LOW ile calisir (-DRELAY_ACTIVE_LOW)
#define RELAY_ON LOW
#define RELAY_OFF HIGH
#else
#define RELAY_ON HIGH
#define RELAY_OFF LOW
#endif

#define SCHED_R1_ADDR 0   
#define SCHED_R2_ADDR 32  

WebServerT server(80);
WiFiManager wm;

// ---------------------- BAĞLANTI DURUMU DEĞİŞKENLERİ ----------------------
unsigned long lastWiFiCheck = 0;
unsigned long lastNTPUpdate = 0;
unsigned long wifiReconnectAttempt = 0;
// v2.7: kopma teshisi (/status'ta wifi_drops / wifi_reason). Kopma sayaci sadece
// bagli -> kopuk gecisinde artar; yeniden baglanma denemelerinin hatalari sayilmaz.
volatile uint16_t wifiDrops = 0;
volatile uint16_t wifiLastReason = 0;
volatile bool wifiUp = false;
unsigned long wifiDownSince = 0;
bool ntpSynced = false;
unsigned long lastKnownEpochTime = 0;
unsigned long lastEpochUpdateMillis = 0;

// ---------------------- ZAMANLAMA YAPISI ----------------------
struct Schedule {
    uint8_t relay_id;   
    uint8_t start_h;    
    uint8_t start_m;    
    uint8_t end_h;      
    uint8_t end_m;      
    uint8_t days;       
    bool active;        
    bool override_active;
    uint32_t override_end_time;
};

Schedule scheduleR1 = {1, 8, 0, 18, 0, 127, false, false, 0};
Schedule scheduleR2 = {2, 8, 0, 18, 0, 127, false, false, 0};

// v2.5: zamanlayici OLAY TABANLI - sadece pencereye girerken/cikarken islem
// yapar, aradaki zamanda roleye karismaz. Eskiden durum tabanliydi (pencere
// disinda surekli kapatiyordu) ve HUB otomasyonlariyla (gunbatimi, bulut)
// kavga ediyordu; bunu yamayan 1 saatlik "override" da kapanis saatini
// atlatabiliyordu. override_* alanlari EEPROM yerlesimi bozulmasin diye
// struct'ta duruyor ama artik kullanilmiyor.
// -1: henuz baz alinmadi (acilis/kayit sonrasi ilk kontrol islem yapmaz)
int8_t lastWindowState[3] = {-1, -1, -1};

// Forward declarations - struct Schedule tanımından SONRA olmalı
void checkRelaySchedule(uint8_t id, Schedule& sched, unsigned long currentEpoch);
void publishStatus();
void reconnectMQTT();

// =====================================================================
// MQTT BOOT GUARD
// Boot sonrası broker'dan gelen "retain" replay mesajları röleleri
// EEPROM'daki son konumun üzerine yazar → röleler hep OFF açılır.
// Çözüm: setup tamamlanana kadar MQTT komutlarını yoksay.
// =====================================================================
static bool mqttBootGuardActive = true;    // true = komutları yoksay
static unsigned long mqttBootGuardEnd = 0; // guard bitiş zamanı (ms)
#define MQTT_BOOT_GUARD_MS  5000           // 5 saniye yeterli

// ---------------------- WiFiManager PARAMETRELERİ ----------------------
WiFiManagerParameter custom_mqtt_server("server", "MQTT Server", mqtt_server, 64);
WiFiManagerParameter custom_mqtt_port("port", "MQTT Port", mqtt_port_str, 6);
WiFiManagerParameter custom_mqtt_user("user", "MQTT Username (opsiyonel)", mqtt_username, 32);
WiFiManagerParameter custom_mqtt_pass("pass", "MQTT Password (opsiyonel)", mqtt_password, 32, "type='password'");

// ---------------------- RÖLE DURUM KAYDETME FONKSİYONLARI ----------------------
// KRİTİK DÜZELTİLDİ: EEPROM.begin() olmadan read/write çalışmaz (ESP8266)
void saveRelayState(uint8_t relayId, bool state) {
    int addr = (relayId == 1) ? RELAY_1_STATE_ADDR : RELAY_2_STATE_ADDR;
    EEPROM.begin(EEPROM_SIZE);          // ← ZORUNLU: her işlem öncesi
    EEPROM.write(addr, state ? 1 : 0);
    EEPROM.commit();
    EEPROM.end();                        // ← Belleği serbest bırak
    Serial.println("Relay " + String(relayId) + " EEPROM'a kaydedildi: " + String(state ? "ON" : "OFF"));
}

bool loadRelayState(uint8_t relayId) {
    int addr = (relayId == 1) ? RELAY_1_STATE_ADDR : RELAY_2_STATE_ADDR;
    EEPROM.begin(EEPROM_SIZE);          // ← ZORUNLU: her işlem öncesi
    uint8_t savedState = EEPROM.read(addr);
    EEPROM.end();

    // İlk kullanımda geçersiz değer (0xFF=255) → varsayılan OFF
    if (savedState != 0 && savedState != 1) {
        savedState = 0;
        saveRelayState(relayId, false); // Temiz değer yaz
    }

    Serial.println("Relay " + String(relayId) + " EEPROM'dan yuklendi: " + String(savedState ? "ON" : "OFF"));
    return (savedState == 1);
}

// ---------------------- ZAMAN FONKSİYONLARI ----------------------
unsigned long getReliableEpochTime() {
    if (WiFi.status() == WL_CONNECTED && ntpSynced) {
        unsigned long currentEpoch = timeClient.getEpochTime();
        if (currentEpoch > 1000000000UL) {
            lastKnownEpochTime    = currentEpoch;
            lastEpochUpdateMillis = millis();
            // Periyodik kayıt - her çağrıda EEPROM yazmak yerine saveLastKnownTime() kullan
            return currentEpoch;
        }
    }
    if (lastKnownEpochTime > 0) {
        unsigned long elapsedSeconds = (millis() - lastEpochUpdateMillis) / 1000;
        return lastKnownEpochTime + elapsedSeconds;
    }
    return 0;
}

void saveLastKnownTime() {
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.put(LAST_KNOWN_TIME_ADDR, lastKnownEpochTime);
    EEPROM.commit();
    EEPROM.end();
}

void loadLastKnownTime() {
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.get(LAST_KNOWN_TIME_ADDR, lastKnownEpochTime);
    EEPROM.end();
    if (lastKnownEpochTime > 1000000000UL) {
        lastEpochUpdateMillis = millis();
        Serial.println("Son bilinen zaman EEPROM'dan yuklendi: " + String(lastKnownEpochTime));
    } else {
        lastKnownEpochTime = 0;
    }
}

// ---------------------- KURULUM FONKSİYONLARI ----------------------
void setupRelays() {
    pinMode(RELAY_1_PIN, OUTPUT);
    pinMode(RELAY_2_PIN, OUTPUT);
    
    // KRİTİK: loadRelayState içinde EEPROM.begin/end yapılıyor
    bool relay1State = loadRelayState(1);
    bool relay2State = loadRelayState(2);
    
    // ESP8266 GPIO0/GPIO2 boot kısıtlaması:
    // Boot sırasında GPIO0=LOW → Programming Mode, GPIO2=LOW → sorun
    // Röle "NC" (Normalde Kapalı) konumda çalışıyorsa bu kritik değil
    // Ama "NO" (Normalde Açık) + LOW=Aktif kullanıyorsanız dikkat edin
    digitalWrite(RELAY_1_PIN, relay1State ? RELAY_ON : RELAY_OFF);
    digitalWrite(RELAY_2_PIN, relay2State ? RELAY_ON : RELAY_OFF);
    
    Serial.println("Roleler son durumlarindan baslatildi:");
    Serial.println("  Relay 1: " + String(relay1State ? "ON" : "OFF"));
    Serial.println("  Relay 2: " + String(relay2State ? "ON" : "OFF"));
}

void setRelayState(uint8_t relayId, bool state) {
    int pin = (relayId == 1) ? RELAY_1_PIN : RELAY_2_PIN;
    
    bool isCurrentlyOn = (digitalRead(pin) == RELAY_ON);
    
    // Fiziksel pin değişimi
    if (isCurrentlyOn != state) {
        digitalWrite(pin, state ? RELAY_ON : RELAY_OFF);
        Serial.println("Relay " + String(relayId) + " degisti: " + String(state ? "ON" : "OFF"));
    }
    
    // EEPROM'a her zaman yaz (durum aynı olsa bile garantilemek için)
    saveRelayState(relayId, state);
    pushPending = true;  // HUB'a loop'ta bildirilir (v2.8)

    // MQTT state topic'e yayınla
    if (mqttClient.connected()) {
        String stateTopic = (relayId == 1) ? topicStateR1 : topicStateR2;
        mqttClient.publish(stateTopic.c_str(), state ? "1" : "0", true); // retain=true
        Serial.println("MQTT Publish: " + stateTopic + " = " + String(state ? "1" : "0"));
    }
}

// Role komutu/degisimi + sebebi: "zaman", "mqtt", ya da HTTP ile gelen src
// (HUB: gunbatimi, gundogumu, bulut, bulut-geri, web). Retain yok - anlik olay.
void publishEvent(uint8_t relayId, bool state, const String& src) {
    if (!mqttClient.connected() || topicEvent.length() == 0) return;
    String p = "{\"relay\":" + String(relayId) + ",\"state\":" + String(state ? 1 : 0) +
               ",\"src\":\"" + src + "\",\"t\":" + String(getReliableEpochTime()) + "}";
    mqttClient.publish(topicEvent.c_str(), p.c_str(), false);
}

void saveSchedule(const Schedule& sched, int addr) {
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.put(addr, sched);
    EEPROM.commit();
    EEPROM.end();
}

// Yeni flaslanan modulun EEPROM'u rastgele dolu gelebiliyor (gozlem 2026-09-28:
// 253:253-252:95 "aktif") - gecersiz kaydi bos/pasif varsayilana cevir.
static void sanitizeSchedule(Schedule& s, uint8_t id, int addr) {
    if (s.start_h > 23 || s.start_m > 59 || s.end_h > 23 || s.end_m > 59 || s.days > 127) {
        s = {id, 8, 0, 18, 0, 127, false, false, 0};
        saveSchedule(s, addr);
        Serial.printf("Gecersiz zamanlayici R%d sifirlandi\n", id);
    }
}

void loadSchedule() {
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.get(SCHED_R1_ADDR, scheduleR1);
    EEPROM.get(SCHED_R2_ADDR, scheduleR2);
    EEPROM.end();
    sanitizeSchedule(scheduleR1, 1, SCHED_R1_ADDR);
    sanitizeSchedule(scheduleR2, 2, SCHED_R2_ADDR);
    // Röle durumları setupRelays() tarafından yüklenir
}

// ---------------------- WiFi YÖNETİMİ ----------------------
// v2.7: artik bloklamiyor. Eskiden 10sn delay dongusunde bekliyordu - o sure
// boyunca web sunucu/OTA/zamanlayici duruyordu. Yeniden baglanmayi SDK'nin
// otomatik baglanmasi yapiyor; takilirsa 60sn sonra elle durtulur.
void checkWiFiConnection() {
    static bool wasConnected = true;
    if (WiFi.status() == WL_CONNECTED) {
        if (!wasConnected) {
            Serial.print("WiFi yeniden baglandi, IP: ");
            Serial.println(WiFi.localIP());
            ntpSynced = false;  // saati tazele
        }
        wasConnected = true;
        wifiDownSince = 0;
        return;
    }
    if (wasConnected) {
        Serial.printf("WiFi baglantisi kesildi (sebep %u)\n", (unsigned)wifiLastReason);
        wasConnected = false;
        wifiDownSince = millis();
    } else if (millis() - wifiDownSince > 60000) {
        Serial.println("WiFi 60sn'dir kopuk, yeniden baglanma tetikleniyor");
        WiFi.reconnect();
        wifiDownSince = millis();
    }
}

// ---------------------- HTTP HANDLER'LAR ----------------------
void handleControl() {
    if (!server.hasArg("id") || !server.hasArg("action")) {
        server.send(400, "text/plain", "Eksik parametre.");
        return;
    }

    uint8_t id = server.arg("id").toInt();
    String action = server.arg("action");

    if (id != 1 && id != 2) {
        server.send(400, "text/plain", "Gecersiz ID.");
        return;
    }

    if (action != "on" && action != "off") {
        server.send(400, "text/plain", "Gecersiz aksiyon.");
        return;
    }
    bool on = (action == "on");
    setRelayState(id, on);
    // v2.5: override (1 saat askiya alma) YOK - olay tabanli zamanlayici
    // bir sonraki baslangic/bitis anina kadar bu komuta dokunmaz.

    // Kaynak (olay kaydi icin): JSON'a yazilacagi icin sadece [a-z0-9-]
    String src = server.hasArg("src") ? server.arg("src") : "http";
    String clean = "";
    for (size_t i = 0; i < src.length() && clean.length() < 16; i++) {
        char ch = src[i];
        if ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '-') clean += ch;
    }
    publishEvent(id, on, clean.length() ? clean : String("http"));
    server.send(200, "text/plain", "OK");
}

void handleSave() {
    if (!server.hasArg("relay_id") || !server.hasArg("days")) {
        server.send(400, "text/plain", "Eksik parametre.");
        return;
    }
    
    uint8_t id = server.arg("relay_id").toInt();
    Schedule* currentSched = (id == 1) ? &scheduleR1 : &scheduleR2;
    
    currentSched->days = server.arg("days").toInt();
    int active_status = server.hasArg("active") ? server.arg("active").toInt() : 1; 
    currentSched->active = (active_status == 1);
    
    if (currentSched->days > 0 && currentSched->active) {
        currentSched->start_h = server.arg("start_h").toInt();
        currentSched->start_m = server.arg("start_m").toInt();
        currentSched->end_h = server.arg("end_h").toInt();
        currentSched->end_m = server.arg("end_m").toInt();
    } 

    saveSchedule(*currentSched, (id == 1) ? SCHED_R1_ADDR : SCHED_R2_ADDR);
    // Yeni programi yeniden baz al: kaydetme aninda roleyi degistirme, sadece
    // bir sonraki baslangic/bitis aninda islem yap
    if (id == 1 || id == 2) lastWindowState[id] = -1;
    pushPending = true;
    server.send(200, "text/plain", "OK");
}

String buildStatusJson() {
    StaticJsonDocument<512> doc;
    
    unsigned long currentEpoch = getReliableEpochTime();
    bool timeValid = (currentEpoch > 0);
    
    // Röle durumları - sunucunun beklediği format
    bool r1on = (digitalRead(RELAY_1_PIN) == RELAY_ON);
    bool r2on = (digitalRead(RELAY_2_PIN) == RELAY_ON);
    doc["r1_state"] = r1on ? "ON" : "OFF";
    doc["r2_state"] = r2on ? "ON" : "OFF";
    doc["relay_1"]  = r1on ? 1 : 0;   // geriye dönük uyumluluk
    doc["relay_2"]  = r2on ? 1 : 0;

    // DÜZELTİLDİ: wifi_rssi eklendi - sunucu dBm gösterimi için gerekli
    doc["wifi_rssi"] = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : 0;
    
    JsonObject r1 = doc.createNestedObject("schedule_r1");
    r1["start_h"]  = scheduleR1.start_h;
    r1["start_m"]  = scheduleR1.start_m;
    r1["end_h"]    = scheduleR1.end_h;
    r1["end_m"]    = scheduleR1.end_m;
    r1["days"]     = scheduleR1.days;
    r1["active"]   = scheduleR1.active;
    r1["override"] = scheduleR1.override_active;
    
    JsonObject r2 = doc.createNestedObject("schedule_r2");
    r2["start_h"]  = scheduleR2.start_h;
    r2["start_m"]  = scheduleR2.start_m;
    r2["end_h"]    = scheduleR2.end_h;
    r2["end_m"]    = scheduleR2.end_m;
    r2["days"]     = scheduleR2.days;
    r2["active"]   = scheduleR2.active;
    r2["override"] = scheduleR2.override_active;
    
    doc["wifi_connected"] = (WiFi.status() == WL_CONNECTED);
    doc["mqtt_connected"] = mqttClient.connected();
    doc["mqtt_enabled"]   = mqttEnabled;
    doc["time_synced"]    = ntpSynced;
    doc["time_valid"]     = timeValid;
    // v2.7: HUB ayrica /mac istegi atmasin (cihaz basina 2 yerine 1 HTTP)
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    mac.toLowerCase();
    doc["mac"]         = mac;
    doc["fw"]          = FW_VERSION;
    doc["uptime"]      = millis() / 1000;
    doc["wifi_drops"]  = wifiDrops;
    doc["wifi_reason"] = wifiLastReason;
    doc["tx_power"]    = txPowerQ / 4.0f;
    doc["tx_max"]      = TX_POWER_MAX_Q / 4.0f;

    if (timeValid) {
        doc["current_time"] = currentEpoch;
    }
    
    String output;
    serializeJson(doc, output);
    return output;
}

void handleStatus() {
    // HUB yoklarken ?hub=1 ekler - push'lar bundan sonra ona dogrudan gider
    if (server.hasArg("hub")) hubIp = server.client().remoteIP();
    server.send(200, "application/json", buildStatusJson());
}

// v2.8: durum push (Sonoff tarzi). Role/zamanlama degisince ve 30sn'de bir
// /status JSON'u yerel aga UDP yayini olarak gider; HUB beklemeden gunceller,
// push'u gelen cihazi HTTP ile yoklamaz. Broker/HUB IP ayari gerekmez.
void sendStatusPush() {
    if (WiFi.status() != WL_CONNECTED) return;
    String json = buildStatusJson();
    // Tekil adres: WiFi katmani ACK + yeniden deneme yapar. Broadcast'te ikisi de
    // yok - zayif sinyalde (-80 dBm) paketler sessizce kayboluyordu.
    IPAddress dst = ((uint32_t)hubIp != 0) ? hubIp : WiFi.broadcastIP();
    pushUdp.beginPacket(dst, STATUS_PUSH_PORT);
    pushUdp.write((const uint8_t*)json.c_str(), json.length());
    pushUdp.endPacket();
}

// ---------------------- MQTT FONKSİYONLARI ----------------------
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    // ===================== BOOT GUARD =====================
    // Boot sonrası broker'dan gelen retain replay mesajları
    // EEPROM'daki konumu ezer → röleler hep OFF açılır.
    // Guard süresi dolana kadar tüm MQTT komutlarını yoksay.
    if (mqttBootGuardActive) {
        if (millis() < mqttBootGuardEnd) {
            Serial.println("[MQTT] Boot guard aktif - komut yoksayildi: " + String(topic));
            return;  // ← Röleye dokunma
        } else {
            mqttBootGuardActive = false;
            Serial.println("[MQTT] Boot guard sona erdi, komutlar isleniyor.");
        }
    }
    // ======================================================

    String topicStr = String(topic);
    String message  = "";
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    Serial.println("MQTT Mesaj Alindi: " + topicStr + " = " + message);
    
    // v2.5: override yok (bkz. handleControl)
    uint8_t id = (topicStr == topicSetR1) ? 1 : (topicStr == topicSetR2) ? 2 : 0;
    if (id == 0) return;
    if (message == "1" || message == "ON") {
        setRelayState(id, true);
        publishEvent(id, true, "mqtt");
    } else if (message == "0" || message == "OFF") {
        setRelayState(id, false);
        publishEvent(id, false, "mqtt");
    }
}

void reconnectMQTT() {
    if (strlen(mqtt_server) == 0) {
        Serial.println("MQTT sunucu adresi tanimli degil.");
        return;
    }
    
    String clientId = "ESP01-" + WiFi.macAddress();
    clientId.replace(":", "");
    
    Serial.print("MQTT sunucuya baglaniliyor: ");
    Serial.print(mqtt_server);
    Serial.print(":");
    Serial.println(mqtt_port);
    
    bool connected = false;
    if (strlen(mqtt_username) > 0 && strlen(mqtt_password) > 0) {
        connected = mqttClient.connect(clientId.c_str(), mqtt_username, mqtt_password);
    } else {
        connected = mqttClient.connect(clientId.c_str());
    }
    
    if (connected) {
        Serial.println("MQTT baglandi!");
        
        // Subscribe - bu noktada callback hala boot guard altında
        mqttClient.subscribe(topicSetR1.c_str());
        mqttClient.subscribe(topicSetR2.c_str());
        Serial.println("MQTT Subscribe: " + topicSetR1 + ", " + topicSetR2);
        
        // ÖNEMLİ: Mevcut röle durumunu HEMEN retain ile yayınla.
        // Bu, broker'daki eski "OFF" retain mesajının üzerine yazar.
        // Böylece bir sonraki cihaz yeniden başlatmada broker doğru
        // durumu gönderir (ve boot guard zaten onu da yoksayar).
        bool r1 = (digitalRead(RELAY_1_PIN) == RELAY_ON);
        bool r2 = (digitalRead(RELAY_2_PIN) == RELAY_ON);
        mqttClient.publish(topicStateR1.c_str(), r1 ? "1" : "0", true);
        mqttClient.publish(topicStateR2.c_str(), r2 ? "1" : "0", true);
        Serial.printf("[MQTT] Mevcut durum yayinlandi: R1=%s R2=%s\n",
                      r1 ? "ON" : "OFF", r2 ? "ON" : "OFF");
        
        publishStatus();
    } else {
        Serial.print("MQTT baglanti basarisiz, rc=");
        Serial.println(mqttClient.state());
    }
}

void updateNetTag() {
    if (WiFi.status() != WL_CONNECTED) return;
    WiFiClient client;
    HTTPClient http;
    http.setTimeout(2000);
    if (!http.begin(client, "http://api.ipify.org")) return;
    if (http.GET() == 200) {
        String ip = http.getString();
        ip.trim();
        bool valid = ip.length() >= 7 && ip.length() <= 15;
        for (size_t i = 0; valid && i < ip.length(); i++) valid = isdigit(ip[i]) || ip[i] == '.';
        if (valid) netTag = sha1("ev-lamba:" + ip).substring(0, 4);
    }
    http.end();
}

void publishStatus() {
    if (!mqttClient.connected()) return;
    
    StaticJsonDocument<256> doc;
    doc["relay_1"] = digitalRead(RELAY_1_PIN) == RELAY_ON ? 1 : 0;
    doc["relay_2"] = digitalRead(RELAY_2_PIN) == RELAY_ON ? 1 : 0;
    doc["wifi_rssi"] = WiFi.RSSI();
    doc["uptime"] = millis() / 1000;
    // Uzaktan teshis: cihaz kendi kendine yeniden basliyorsa (besleme dususu,
    // watchdog) sebebi broker'dan gorulebilsin
#ifdef ESP32
    doc["reset"] = (int)esp_reset_reason();
#else
    doc["reset"] = ESP.getResetReason();
#endif
    if (netTag.length()) doc["net"] = netTag;
    
    String output;
    serializeJson(doc, output);
    mqttClient.publish(topicStatus.c_str(), output.c_str(), true);
    
    Serial.println("MQTT Status Publish: " + output);
}

// ---------------------- MQTT AYARLARI ----------------------
void saveMQTTConfig() {
    Serial.println("MQTT Ayarlari kaydediliyor...");
    
    strcpy(mqtt_server, custom_mqtt_server.getValue());
    strcpy(mqtt_port_str, custom_mqtt_port.getValue());
    strcpy(mqtt_username, custom_mqtt_user.getValue());
    strcpy(mqtt_password, custom_mqtt_pass.getValue());
    
    mqtt_port = atoi(mqtt_port_str);
    
    EEPROM.begin(EEPROM_SIZE);
    
    for (int i = 0; i < 64; i++) {
        EEPROM.write(MQTT_SERVER_ADDR + i, mqtt_server[i]);
    }
    
    EEPROM.write(MQTT_PORT_ADDR, mqtt_port >> 8);
    EEPROM.write(MQTT_PORT_ADDR + 1, mqtt_port & 0xFF);
    
    for (int i = 0; i < 32; i++) {
        EEPROM.write(MQTT_USER_ADDR + i, mqtt_username[i]);
    }
    
    for (int i = 0; i < 32; i++) {
        EEPROM.write(MQTT_PASS_ADDR + i, mqtt_password[i]);
    }
    
    EEPROM.commit();
    EEPROM.end();
    Serial.println("MQTT Ayarlari kaydedildi.");
}

void loadMQTTConfig() {
    EEPROM.begin(EEPROM_SIZE);
    
    // Server string
    EEPROM.get(MQTT_SERVER_ADDR, mqtt_server);
    mqtt_server[63] = '\0';  // Null terminator garantisi
    
    // DÜZELTİLDİ: Port 2 byte olarak high-low şeklinde kaydedildi, aynı şekilde oku
    uint8_t portHigh = EEPROM.read(MQTT_PORT_ADDR);
    uint8_t portLow  = EEPROM.read(MQTT_PORT_ADDR + 1);
    mqtt_port = (portHigh << 8) | portLow;
    
    EEPROM.get(MQTT_USER_ADDR, mqtt_username);
    mqtt_username[31] = '\0';
    EEPROM.get(MQTT_PASS_ADDR, mqtt_password);
    mqtt_password[31] = '\0';
    
    // Geçerlilik kontrolleri
    if (strlen(mqtt_server) == 0 || mqtt_server[0] == 0xFF) {
        strcpy(mqtt_server, "broker.hivemq.com");
    }
    if (mqtt_port == 0 || mqtt_port == 0xFFFF) {
        mqtt_port = 1883;
    }
    
    sprintf(mqtt_port_str, "%d", mqtt_port);
    EEPROM.end();
    
    custom_mqtt_server.setValue(mqtt_server, 64);
    custom_mqtt_port.setValue(mqtt_port_str, 6);
    custom_mqtt_user.setValue(mqtt_username, 32);
    custom_mqtt_pass.setValue(mqtt_password, 32);
    
    Serial.println("MQTT Ayarlari yuklendi:");
    Serial.println("Server: " + String(mqtt_server));
    Serial.println("Port: " + String(mqtt_port));
}

void loadMqttEnabled() {
    EEPROM.begin(EEPROM_SIZE);
    mqttEnabled = EEPROM.read(MQTT_ENABLED_ADDR) != MQTT_DISABLED_MAGIC;
    EEPROM.end();
    Serial.println(String("MQTT: ") + (mqttEnabled ? "ACIK" : "KAPALI (HUB ayari)"));
}

void saveMqttEnabled(bool on) {
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.write(MQTT_ENABLED_ADDR, on ? 0x01 : MQTT_DISABLED_MAGIC);
    EEPROM.commit();
    EEPROM.end();
}

void setupServer() {
    server.on("/control", HTTP_GET, handleControl);
    server.on("/save", HTTP_GET, handleSave);
    server.on("/status", HTTP_GET, handleStatus);

    // /mqtt -> durumu dondurur; /mqtt?enable=0|1 -> kalici olarak kapatir/acar
    server.on("/mqtt", HTTP_GET, []() {
        if (server.hasArg("enable")) {
            bool on = server.arg("enable") == "1";
            if (on != mqttEnabled) {
                mqttEnabled = on;
                saveMqttEnabled(on);
                if (!on && mqttClient.connected()) mqttClient.disconnect();
                lastMqttAttempt = 0;  // aciliyorsa hemen baglanmayi dene
                mqttRetryInterval = 5000;
                Serial.println(String("MQTT HUB tarafindan ") + (on ? "ACILDI" : "KAPATILDI"));
            }
        }
        String out = String("{\"mqtt_enabled\":") + (mqttEnabled ? "true" : "false") +
                     ",\"mqtt_connected\":" + (mqttClient.connected() ? "true" : "false") + "}";
        server.send(200, "application/json", out);
    });

    // v2.9: /txpower?dbm=15 -> kaydet + hemen uygula; argumansiz sadece okur
    server.on("/txpower", HTTP_GET, []() {
        if (server.hasArg("dbm")) {
            int q = (int)lroundf(server.arg("dbm").toFloat() * 4);
            if (q < TX_POWER_MIN_Q || q > TX_POWER_MAX_Q) {
                server.send(400, "application/json", "{\"error\":\"aralik disi\"}");
                return;
            }
            if (q != txPowerQ) {
                txPowerQ = q;
                saveTxPower();
                applyTxPower();
                Serial.printf("TX gucu: %.2f dBm\n", txPowerQ / 4.0f);
            }
        }
        server.send(200, "application/json", String("{\"tx_power\":") + String(txPowerQ / 4.0f, 2) + "}");
    });

    server.on("/eeprom_clr", HTTP_GET, []() {
        Serial.println("EEPROM Temizleniyor...");
        EEPROM.begin(EEPROM_SIZE);
        for (int i = 0; i < EEPROM_SIZE; i++) {
            EEPROM.write(i, 0);
        }
        EEPROM.commit();
        EEPROM.end();
        // RAM'deki kopyalar eski kaldiginda zamanlayici yeniden baslatmaya kadar
        // cop degerlerle calismaya devam ediyordu - yeniden baslat.
        server.send(200, "text/plain", "EEPROM Temizlendi. Cihaz yeniden baslatiliyor...");
        Serial.println("EEPROM Temizlendi, yeniden baslatiliyor.");
        delay(500);
        ESP.restart();
    });

    server.on("/wifi_clr", HTTP_GET, []() {
        server.send(200, "text/plain", "WiFi Ayarlari Silindi. Cihaz Resetleniyor...");
        Serial.println("WiFi Ayarlari Siliniyor ve Reset Atiliyor...");
        delay(1000);
        wm.resetSettings();
        ESP.restart();
    });
    
    server.on("/mac", HTTP_GET, []() {
        String mac = WiFi.macAddress();
        mac.replace(":", "");
        mac.toLowerCase();
        server.send(200, "text/plain", mac);
    });
    
    server.on("/topics", HTTP_GET, []() {
        String response = "R1 Set: " + topicSetR1 + "\n";
        response += "R1 State: " + topicStateR1 + "\n";
        response += "R2 Set: " + topicSetR2 + "\n";
        response += "R2 State: " + topicStateR2 + "\n";
        response += "Status: " + topicStatus;
        server.send(200, "text/plain", response);
    });
    
    // YENİ: Röle durumlarını gösterme endpoint'i
    server.on("/relay_status", HTTP_GET, []() {
        String response = "Relay 1: " + String(digitalRead(RELAY_1_PIN) == RELAY_ON ? "ON" : "OFF") + "\n";
        response += "Relay 2: " + String(digitalRead(RELAY_2_PIN) == RELAY_ON ? "ON" : "OFF") + "\n";
        response += "Saved Relay 1: " + String(loadRelayState(1) ? "ON" : "OFF") + "\n";
        response += "Saved Relay 2: " + String(loadRelayState(2) ? "ON" : "OFF");
        server.send(200, "text/plain", response);
    });
    
    server.onNotFound([]() {
        server.send(404, "text/plain", "Gecersiz istek.");
    });
    
    server.begin();
    Serial.println("Web sunucu baslatildi.");
}

// ---------------------- ZAMANLAMA KONTROL ----------------------
void checkSchedule() {
    unsigned long currentEpoch = getReliableEpochTime();
    
    if (currentEpoch == 0) {
        Serial.println("Zaman bilgisi yok, zamanlama kontrol edilemiyor!");
        return;
    }
    
    checkRelaySchedule(1, scheduleR1, currentEpoch);
    checkRelaySchedule(2, scheduleR2, currentEpoch);
}

void checkRelaySchedule(uint8_t id, Schedule& sched, unsigned long currentEpoch) {
    if (!sched.active || sched.days == 0) { lastWindowState[id] = -1; return; }
    sched.override_active = false;  // v2.5: kullanilmiyor (bkz. lastWindowState)

    // Gün hesaplama
    int daysSinceEpoch = currentEpoch / 86400;
    int currentDayOfWeek = (daysSinceEpoch + 4) % 7; // Epoch 1 Ocak 1970 Perşembe (4); 0=Pazar

    // BUGFIX: web arayuzundeki gun maskesi Pazartesi'den basliyor (bit0=Pt ...
    // bit6=Pz), burasi Pazar=0 sayiyordu -> secili gunler bir gun kayik calisirdi
    uint8_t dayBit = 1 << ((currentDayOfWeek + 6) % 7);
    bool dayOk = (sched.days & dayBit);

    // Saat hesaplama
    unsigned long secondsSinceMidnight = currentEpoch % 86400;
    int currentH = secondsSinceMidnight / 3600;
    int currentM = (secondsSinceMidnight % 3600) / 60;
    
    int currentTimeMinutes = currentH * 60 + currentM;
    int startTimeMinutes = sched.start_h * 60 + sched.start_m;
    int endTimeMinutes = sched.end_h * 60 + sched.end_m;

    bool inWindow = false;
    if (startTimeMinutes <= endTimeMinutes) {
        inWindow = (currentTimeMinutes >= startTimeMinutes && currentTimeMinutes < endTimeMinutes);
    } else {
        inWindow = (currentTimeMinutes >= startTimeMinutes || currentTimeMinutes < endTimeMinutes);
    }
    int8_t now = (dayOk && inWindow) ? 1 : 0;

    // Ilk kontrol (acilis / program kaydi): sadece baz al, roleye dokunma
    if (lastWindowState[id] < 0) {
        lastWindowState[id] = now;
        return;
    }
    // Sadece pencereye GIRIS (baslangic) ve CIKIS (bitis) aninda islem yap.
    // Aradaki zamanda elle/otomasyonla yapilan degisikliklere karisma.
    if (now != lastWindowState[id]) {
        lastWindowState[id] = now;
        setRelayState(id, now == 1);
        publishEvent(id, now == 1, "zaman");
        Serial.println("Relay " + String(id) + " zamanlama ile degistirildi: " + String(now ? "ON" : "OFF"));
    }
}

// v2.7: kopma sebebi (802.11 reason kodu) - ornek: 200 beacon timeout (sinyal
// zayif/parazit), 201 AP bulunamadi, 2/4/8 AP tarafindan dusuruldu
#ifdef ESP32
void setupWiFiEvents() {
    WiFi.onEvent([](WiFiEvent_t, WiFiEventInfo_t info) {
        wifiLastReason = info.wifi_sta_disconnected.reason;
        if (wifiUp) { wifiUp = false; wifiDrops++; }
    }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    WiFi.onEvent([](WiFiEvent_t, WiFiEventInfo_t) { wifiUp = true; pushPending = true; },
                 ARDUINO_EVENT_WIFI_STA_GOT_IP);
}
#else
WiFiEventHandler wifiDisconnectHandler, wifiGotIpHandler;  // silinirse olay gelmez
void setupWiFiEvents() {
    wifiDisconnectHandler = WiFi.onStationModeDisconnected(
        [](const WiFiEventStationModeDisconnected& e) {
            wifiLastReason = e.reason;
            if (wifiUp) { wifiUp = false; wifiDrops++; }
        });
    wifiGotIpHandler = WiFi.onStationModeGotIP(
        [](const WiFiEventStationModeGotIP&) { wifiUp = true; pushPending = true; });
}
#endif

void applyTxPower() {
#ifdef ESP32
    WiFi.setTxPower((wifi_power_t)txPowerQ);
#else
    WiFi.setOutputPower(txPowerQ / 4.0f);
#endif
}

void loadTxPower() {
    EEPROM.begin(EEPROM_SIZE);
    uint8_t q = EEPROM.read(TX_POWER_ADDR);
    EEPROM.end();
    txPowerQ = (q >= TX_POWER_MIN_Q && q <= TX_POWER_MAX_Q) ? q : TX_POWER_DEFAULT_Q;
}

void saveTxPower() {
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.write(TX_POWER_ADDR, txPowerQ);
    EEPROM.commit();
    EEPROM.end();
}

// ---------------------- SETUP & LOOP ----------------------
void setup() {
    Serial.begin(115200);
    loadTxPower();
#ifdef ESP32
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
#else
    WiFi.setSleepMode(WIFI_NONE_SLEEP);
#endif
    applyTxPower();
    WiFi.setAutoReconnect(true);
    setupWiFiEvents();
    delay(1000);
    Serial.println("\n\n=== ESP01 Baslatiliyor (Son Durum Kayit v3.1) ===");

    loadSchedule();
    loadMQTTConfig();
    loadMqttEnabled();
    loadLastKnownTime(); // Son bilinen zamanı yükle
    setupRelays(); // YENİ: Röleleri son durumlarından başlat
    
// EĞER MQTT SUNUCU BOŞ İSE, MANUEL AYARLA (İLK KURULUM)
    if (strlen(mqtt_server) == 0) {
        strcpy(mqtt_server, "192.168.1.100");  // ← MQTT broker IP'nizi yazın
        mqtt_port = 1883;
        strcpy(mqtt_username, "");  // Kullanıcı adı varsa yazın
        strcpy(mqtt_password, "");  // Şifre varsa yazın
        
        saveMQTTConfig();  // EEPROM'a kaydet
        Serial.println("Varsayilan MQTT ayarlari kaydedildi!");
    }

    wm.addParameter(&custom_mqtt_server);
    wm.addParameter(&custom_mqtt_port);
    wm.addParameter(&custom_mqtt_user);
    wm.addParameter(&custom_mqtt_pass);
    
    wm.setSaveConfigCallback(saveMQTTConfig);
    wm.setConfigPortalTimeout(180);
    
    Serial.println("WiFi baglaniliyor...");
    // Kayitli ag yoksa (yeni flaslanmis modul) once varsayilan ev agini dene -
    // baglanirsa bilgiler kalici kaydedilir, portal hic acilmaz.
    if (!wm.getWiFiIsSaved() && strlen(DEFAULT_WIFI_PASS) > 0) {
        Serial.printf("Varsayilan aga deneniyor: %s\n", DEFAULT_WIFI_SSID);
        WiFi.mode(WIFI_STA);
        WiFi.begin(DEFAULT_WIFI_SSID, DEFAULT_WIFI_PASS);
        for (int i = 0; i < 30 && WiFi.status() != WL_CONNECTED; i++) delay(500);
    }
    if (WiFi.status() != WL_CONNECTED && !wm.autoConnect(SETUP_AP)) {
        Serial.println("WiFi baglanti basarisiz, offline modda devam ediliyor...");
        // WiFi bağlanamasa bile devam et
    } else {
        Serial.println("WiFi baglandi!");
        Serial.print("IP Adresi: ");
        Serial.println(WiFi.localIP());
        
        long rssi = WiFi.RSSI();
        Serial.print("WiFi RSSI: ");
        Serial.print(rssi);
        Serial.println(" dBm");
    }
    applyTxPower();  // WiFiManager/baglanma sirasinda sifirlanmis olabilir

    String mac = WiFi.macAddress();
    mac.replace(":", "");
    mac.toLowerCase();

    // YENİ: mDNS (WLED tarzı otomatik keşif) + OTA (kablosuz güncelleme)
    // Sadece WiFi bağlıyken anlamlı; offline modda atlanır.
    if (WiFi.status() == WL_CONNECTED) {
        String mdnsHost = HOST_PREFIX + mac.substring(6);  // son 3 byte (6 hex karakter)
        if (MDNS.begin(mdnsHost.c_str())) {
            MDNS.addService("http", "tcp", 80);
            MDNS.addServiceTxt("http", "tcp", "mac", mac);
            Serial.println("mDNS baslatildi: " + mdnsHost + ".local");
        } else {
            Serial.println("mDNS baslatma HATASI");
        }

        ArduinoOTA.setHostname(mdnsHost.c_str());
        ArduinoOTA.begin();
        Serial.println("ArduinoOTA baslatildi (hostname: " + mdnsHost + ")");
    }

    topicSetR1 = "esp01/" + mac + "/r1/set";
    topicSetR2 = "esp01/" + mac + "/r2/set";
    topicStateR1 = "esp01/" + mac + "/r1/state";
    topicStateR2 = "esp01/" + mac + "/r2/state";
    topicStatus = "esp01/" + mac + "/status";
    topicEvent = "esp01/" + mac + "/event";
    
    Serial.println("\n=== CIHAZ BILGILERI ===");
    Serial.println("MAC: " + mac);
    Serial.println("R1 Set Topic: " + topicSetR1);
    Serial.println("R1 State Topic: " + topicStateR1);
    Serial.println("R2 Set Topic: " + topicSetR2);
    Serial.println("R2 State Topic: " + topicStateR2);
    Serial.println("Status Topic: " + topicStatus);
    Serial.println("======================\n");
    
    mqttClient.setServer(mqtt_server, mqtt_port);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setKeepAlive(60);
    // v2.7: 30 yerine 3sn - broker yavassa connect() loop'u 30sn kilitliyordu
    mqttClient.setSocketTimeout(3);
    
    setupServer();
    timeClient.begin();
    
    // MQTT Boot Guard'ı başlat:
    // setup() bittikten sonra MQTT_BOOT_GUARD_MS süre boyunca
    // gelen retain mesajları yoksayılır.
    mqttBootGuardEnd    = millis() + MQTT_BOOT_GUARD_MS;
    mqttBootGuardActive = true;
    Serial.println("MQTT Boot Guard baslatildi (" + String(MQTT_BOOT_GUARD_MS/1000) + "sn)");
    
    Serial.println("Sistem hazir!");
}

unsigned long lastScheduleCheck = 0;
unsigned long lastStatusPublish = 0;

void loop() {
    // DÜZELTİLDİ: Web sunucuyu sadece WiFi bağlıyken çalıştır
    if (WiFi.status() == WL_CONNECTED) {
        server.handleClient();
        // YENİ: LEAmDNS (ESP8266) periyodik tetikleme ister - ESP32'nin aksine
        // kendi kendine çalışmıyor, atlanırsa mDNS birkaç saniye sonra sessizce ölür.
#ifndef ESP32
        MDNS.update();
#endif
        ArduinoOTA.handle();

        // Ag etiketi: bilinmiyorsa dakikada bir dene, sonra saatte bir tazele
        // (genel IP degisebilir)
        unsigned long netInterval = netTag.length() ? 3600000UL : 60000UL;
        if (lastNetCheck == 0 || millis() - lastNetCheck > netInterval) {
            lastNetCheck = millis();
            updateNetTag();
        }
    }
    
    unsigned long now = millis();
    
    // WiFi bağlantısını kontrol et (30 saniyede bir)
    if (now - lastWiFiCheck > 30000) {
        lastWiFiCheck = now;
        checkWiFiConnection();
    }
    
    // NTP güncelleme - DÜZELTİLDİ: WiFi kontrolü önce yapılıyor
    if (WiFi.status() == WL_CONNECTED) {
        // v2.7: senkron yokken her turda degil 30sn'de bir dene - her deneme
        // cevap gelmezse ~1sn bloklar (NTPClient)
        if (now - lastNTPUpdate > (ntpSynced ? 3600000UL : 30000UL) || lastNTPUpdate == 0) {
            lastNTPUpdate = now;
            if (timeClient.update()) {
                unsigned long ntpTime = timeClient.getEpochTime();
                if (ntpTime > 1000000000UL) {
                    ntpSynced = true;
                    lastKnownEpochTime  = ntpTime;
                    lastEpochUpdateMillis = millis();
                    Serial.println("NTP senkronize edildi: " + String(ntpTime));
                    saveLastKnownTime();
                }
            }
        }
    }
    
    // MQTT bağlantısını kontrol et (WiFi varsa, 5 saniyede bir)
    if (WiFi.status() == WL_CONNECTED) {
        if (!mqttEnabled) {
            // kapali: baglanma, varsa bagli kalma
            if (mqttClient.connected()) mqttClient.disconnect();
        } else if (!mqttClient.connected()) {
            if (now - lastMqttAttempt > mqttRetryInterval) {
                lastMqttAttempt = now;
                reconnectMQTT();
                mqttRetryInterval = mqttClient.connected()
                    ? 5000 : min(mqttRetryInterval * 2, 60000UL);
            }
        } else {
            mqttClient.loop();
        }
    }
    
    // Zamanlama kontrolü (60 saniyede bir) - WiFi olsun olmasın çalışır
    if (now - lastScheduleCheck > 60000) {
        lastScheduleCheck = now;
        checkSchedule();

        // v2.8.1: dakikada bir degil saatte bir - EEPROM aslinda flash, her
        // commit bir sektor silme dongusu harciyor (dakikada 1 = ~525 bin/yil)
        static uint8_t minutesSinceTimeSave = 0;
        if (++minutesSinceTimeSave >= 60 && getReliableEpochTime() > 0) {
            minutesSinceTimeSave = 0;
            saveLastKnownTime();
        }
    }
    
    // Status yayınlama (30 saniyede bir, MQTT bağlıysa)
    if (mqttClient.connected() && now - lastStatusPublish > 30000) {
        lastStatusPublish = now;
        publishStatus();
    }

    // v2.8: HUB'a durum push - degisiklikte hemen, yoksa 30sn'de bir (canlilik)
    if (WiFi.status() == WL_CONNECTED &&
        (pushPending || now - lastStatusPush > STATUS_PUSH_INTERVAL)) {
        pushPending = false;
        lastStatusPush = now;
        sendStatusPush();
    }

    yield();  // WDT besleme ve WiFi stack için
}
