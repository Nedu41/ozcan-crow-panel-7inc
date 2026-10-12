#include "devices.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <map>
#include "app_state.h"
#include "storage.h"
#include "web_server.h"

// ============== Sure sinirli govde okuma ==============
// BUGFIX (2026-09-29): durum gorevi 2 saattir donmustu - .140 cevap ortasinda
// fisten cekilince getString() icindeki zaman asimisiz dongude takildi, butun
// cihazlar son gorulen halde (ON/cevrimici) kaldi.
String readBodyWithTimeout(HTTPClient& http, uint32_t timeoutMs) {
  String body;
  WiFiClient* stream = http.getStreamPtr();
  if (!stream) return body;
  int remaining = http.getSize();  // -1: Content-Length yok, baglanti kapanana kadar oku
  if (remaining > 0) body.reserve(remaining);
  unsigned long start = millis();
  while (remaining != 0 && millis() - start < timeoutMs) {
    int avail = stream->available();
    if (avail > 0) {
      while (avail-- > 0 && remaining != 0) {
        int c = stream->read();
        if (c < 0) break;
        body += (char)c;
        if (remaining > 0) remaining--;
      }
    } else if (!stream->connected()) {
      break;
    } else {
      delay(1);
    }
  }
  if (remaining > 0) Serial.printf("[HTTP] govde eksik/zaman asimi (%d bayt kaldi)\n", remaining);
  return body;
}

// ============== Device status via HTTP ==============
// DÜZELTİLDİ: HTTP nesneleri scope'da düzgün kapatılıyor, timeout artırıldı
String getDeviceStatusJson(const String& ip) {
  if (WiFi.status() != WL_CONNECTED) {
    return "{\"r1_state\":\"ERR\",\"r2_state\":\"ERR\",\"mac\":\"NO_WIFI\"}";
  }
  if (ip.length() < 7) return "{\"r1_state\":\"ERR\",\"r2_state\":\"ERR\",\"mac\":\"NO_IP\"}";

  String result;
  String macId = "NO_MAC";

  // Status isteği
  {
    HTTPClient http;
    http.setTimeout(800);
    http.setConnectTimeout(800);
    String url = "http://" + ip + "/status?hub=1";  // v2.8+: cihaz push'u bu IP'ye yollar
    if (http.begin(url)) {
      int code = http.GET();
      if (code == 200) {
        result = readBodyWithTimeout(http, 1500);
      } else {
        http.end();
        return "{\"r1_state\":\"OFFLINE\",\"r2_state\":\"OFFLINE\",\"mac\":\"OFFLINE\"}";
      }
      http.end();
    } else {
      return "{\"r1_state\":\"OFFLINE\",\"r2_state\":\"OFFLINE\",\"mac\":\"OFFLINE\"}";
    }
  }

  // Firmware v2.7+ MAC'i /status icinde veriyor - ikinci HTTP istegine gerek yok
  if (result.indexOf("\"mac\":") >= 0) return result;

  // MAC isteği - ayrı scope ile bellek temizlenir
  {
    HTTPClient httpMac;
    httpMac.setTimeout(500);
    httpMac.setConnectTimeout(500);
    String macUrl = "http://" + ip + "/mac";
    if (httpMac.begin(macUrl)) {
      int macCode = httpMac.GET();
      if (macCode == 200) {
        macId = readBodyWithTimeout(httpMac, 1000);
        macId.trim();
        if (macId.length() > 17) macId = macId.substring(0, 17);
      }
      httpMac.end();
    }
  }

  // JSON'a mac ekle
  result.trim();
  if (result.endsWith("}")) {
    result.remove(result.length() - 1);
    result += ",\"mac\":\"" + macId + "\"}";
  }

  return result;
}
// ============== Send control signal ==============
// DEGISTI: WS control_result mesaji icin basari/hata bilgisi donduruluyor
bool sendControlSignal(const String& ip, int relay, const String& action, const String& src) {
  if (ip == "" || relay == 0 || action == "") return false;
  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);
  // BUGFIX: connect timeout verilmezse HTTPClient varsayilani 5sn - cevrimdisi
  // cihaza komut 5sn bekliyor, tarayici 3sn'de "zaman asimi" diyordu; otomasyon
  // ve komut kuyrugu da her cevrimdisi cihaz icin 5sn kilitleniyordu.
  http.setConnectTimeout(800);
  String url = "http://" + ip + "/control?id=" + String(relay) + "&action=" + action + "&src=" + src;
  // on/off idempotent - zayif sinyalde kaybolan istek/cevap icin bir kez daha
  // denemek guvenli (role zaten degistiyse ikinci istek bir sey degistirmez)
  for (int attempt = 0; attempt < 2; attempt++) {
    http.begin(url);
    int code = http.GET();
    http.end();
    if (code == 200) return true;
  }
  return false;
}

// "BC:FF:4D:40:3E:0F" / "bcff4d403e0f" -> "bcff4d403e0f"; gecersizse ""
String normMac(const String& raw) {
  String m = raw;
  m.trim();
  m.toLowerCase();
  m.replace(":", "");
  if (m.length() != 12) return "";
  for (size_t i = 0; i < m.length(); i++) {
    if (!isxdigit(m[i])) return "";
  }
  return m;
}

// Cihaz listesini (doc) diske yazar - saveDevices tek yazma yolu kalsin
static void persistDevices() {
  String json;
  serializeJson(doc, json);
  saveDevices(json);
}

// WLED gibi: IP'si degisen (DHCP) cihazi mDNS'te MAC'iyle bulup kaydini gunceller.
// ESP01'ler MAC'i mDNS TXT kaydinda yayinliyor - aday basina HTTP gerekmiyor.
static bool followIpChanges() {
  int n = MDNS.queryService("http", "tcp");  // blocking ~1-3sn, bu gorevde sorun degil
  bool changed = false;
  for (int i = 0; i < n; i++) {
    String mac = MDNS.hasTxt(i, "mac") ? normMac(MDNS.txt(i, "mac")) : "";
    if (!mac.length()) continue;
    String newIp = MDNS.IP(i).toString();
    for (JsonObject device : doc.as<JsonArray>()) {
      if ((device["mac"] | String("")) == mac && (device["ip"] | String("")) != newIp) {
        Serial.printf("[Cihaz] %s IP degisti: %s -> %s (MAC %s)\n",
                      device["name"] | "", device["ip"] | "", newIp.c_str(), mac.c_str());
        device["ip"] = newIp;
        changed = true;
      }
    }
  }
  return changed;
}

// ============== Durum push (ESP01 firmware v2.8+) ==============
// Cihaz role/zamanlama degisince ve 30sn'de bir /status JSON'unu yerel aga UDP
// yayini olarak atar. Push'u taze olan cihaz HTTP ile yoklanmaz; susarsa
// (eski firmware, cihaz kapali) yoklama kaldigi yerden devam eder.
// Paketler yalnizca durum gorevinde islenir - cihaz listesine ayni gorevden erisilsin.
static WiFiUDP pushRx;
static std::map<String, unsigned long> lastPushAt;  // cihaz adi -> millis
static unsigned long lastAnyPush = 0;               // herhangi bir cihazdan son push
const unsigned long PUSH_FRESH_MS = 35000;          // 15sn nabiz (v2.9) + 1 kayip paket payi

static bool pushFresh(const String& name) {
  auto it = lastPushAt.find(name);
  return it != lastPushAt.end() && millis() - it->second < PUSH_FRESH_MS;
}

// Bekleyen push paketlerini isler; durum degistiyse true. listChanged: IP guncellendi.
static bool handleStatusPushes(bool& listChanged) {
  bool updated = false;
  static char buf[1024];
  while (pushRx.parsePacket() > 0) {
    int len = pushRx.read(buf, sizeof(buf) - 1);
    if (len <= 0) continue;
    buf[len] = 0;
    lastAnyPush = millis();
    JsonDocument tempDoc;
    if (deserializeJson(tempDoc, buf) != DeserializationError::Ok) continue;
    String mac = normMac(tempDoc["mac"] | "");
    if (!mac.length() || !doc.is<JsonArray>()) continue;
    String srcIp = pushRx.remoteIP().toString();
    // Once MAC ile ara; bulunamazsa MAC'i henuz ogrenilmemis (hic cevrimici
    // gorulmemis) kaydi IP'den tani - cihaz acilir acilmaz ekrana gelsin.
    // MAC'i kayitli ama farkli olan kayit IP eslesse de alinmaz (IP baska
    // cihaza gecmis olabilir).
    JsonObject match;
    for (JsonObject device : doc.as<JsonArray>()) {
      if ((device["mac"] | String("")) == mac) { match = device; break; }
    }
    if (match.isNull()) {
      for (JsonObject device : doc.as<JsonArray>()) {
        if ((device["mac"] | String("")).length() == 0 && (device["ip"] | String("")) == srcIp) {
          match = device;
          match["mac"] = mac;
          listChanged = true;
          Serial.printf("[Push] %s MAC ogrenildi: %s\n", match["name"] | "", mac.c_str());
          break;
        }
      }
    }
    if (!match.isNull()) {  // listede olmayan cihaz yoksayilir
      JsonObject device = match;
      String name = device["name"] | "";
      if (!name.length()) continue;
      if (xSemaphoreTake(statusMutex, MUTEX_TIMEOUT) == pdTRUE) {
        CURRENT_STATUS_DOC[name].set(tempDoc);
        xSemaphoreGive(statusMutex);
      }
      lastPushAt[name] = millis();
      if ((device["ip"] | String("")) != srcIp) {  // DHCP ile IP degismis
        Serial.printf("[Push] %s IP degisti: %s -> %s\n", name.c_str(), device["ip"] | "", srcIp.c_str());
        device["ip"] = srcIp;
        listChanged = true;
      }
      updated = true;
    }
  }
  return updated;
}

// "Zombi" WiFi: WiFi.status() CONNECTED diyor ama trafik gecmiyor (2026-09-30
// seri logda goruldu - DNS hatalari, ping yok, HUB dakikalarca agdan dusuk;
// loop()'taki status kontrolu bunu yakalamiyor). Cihazlar 15sn'de bir push
// attigi icin 90sn hic paket gelmemesi yerel agin olu oldugunun isareti.
// Butun cihazlar gercekten kapaliysa yeniden baglanma zararsiz; 5dk'da bir sinirli.
static void checkZombieWiFi() {
  static unsigned long lastKick = 0;
  const unsigned long SILENCE_MS = 90000, KICK_EVERY_MS = 300000;
  if (WiFi.status() != WL_CONNECTED || lastAnyPush == 0) return;  // hic push gormediysek (eski firmware) dokunma
  if (millis() - lastAnyPush < SILENCE_MS) return;
  if (lastKick != 0 && millis() - lastKick < KICK_EVERY_MS) return;
  lastKick = millis();
  Serial.printf("[WiFi] %lus'dir hic cihaz push'u yok - baglanti zombi olabilir, yeniden baglaniliyor\n",
                (millis() - lastAnyPush) / 1000);
  WiFi.disconnect(false);
  vTaskDelay(pdMS_TO_TICKS(200));
  WiFi.reconnect();
}

// ============== Tasks ==============
void updateAllDeviceStatuses(void* pvParameters) {
  pushRx.begin(STATUS_PUSH_PORT);
  unsigned long lastFollow = 0;
  const unsigned long FOLLOW_INTERVAL = 60000;  // cevrimdisi cihaz varken en fazla dakikada 1 tarama
  // Ust uste 2 tur cevap vermeyen cihaz sadece 3 turda bir yoklanir: her
  // cevrimdisi cihaz turu ~1.6sn uzatiyor, digerlerinin guncellenmesini geciktiriyordu.
  // IP ile anahtarlanir - IP degisirse (followIpChanges) yeni adres hemen yoklanir.
  std::map<String, uint8_t> offlineRounds;
  uint32_t round = 0;

  while (1) {
    bool anyOffline = false;
    bool listChanged = false;
    round++;
    if (doc.is<JsonArray>()) {
      for (JsonObject device : doc.as<JsonArray>()) {
        String name = device["name"] | "";
        String ip   = device["ip"]   | "";
        if (name != "" && ip != "") {
          if (pushFresh(name)) continue;  // durum zaten push ile guncel
          uint8_t& misses = offlineRounds[ip];
          if (misses >= 2 && round % 3 != 0) {
            anyOffline = true;  // son bilinen OFFLINE durumu yerinde kalir
            continue;
          }
          String statusJson = getDeviceStatusJson(ip);
          bool offline = statusJson.indexOf("\"OFFLINE\"") >= 0;
          misses = offline ? (misses < 255 ? misses + 1 : misses) : 0;
          // Tek cevapsiz yoklama cihazi kopuk gostermesin (zayif sinyalde ara sira
          // zaman asimi oluyor, cihaz aslinda bagli) - son bilinen durum kalir.
          // Push'u kesilmis cihazda bu zaten ikinci isaret (nabiz da gelmedi) -
          // hemen kopuk goster; yoksa fisi cekilen cihaz ~1dk bagli gorunuyordu.
          if (offline && misses == 1 && !lastPushAt.count(name)) continue;
          StaticJsonDocument<512> tempDoc;  // DÜZELTİLDİ: 1500 yerine 512 (ESP01 yanıtı küçük)
          if (deserializeJson(tempDoc, statusJson) == DeserializationError::Ok) {
            if (xSemaphoreTake(statusMutex, MUTEX_TIMEOUT) == pdTRUE) {
              CURRENT_STATUS_DOC[name].set(tempDoc);
              xSemaphoreGive(statusMutex);
            }
            if (strcmp(tempDoc["r1_state"] | "", "OFFLINE") == 0) {
              anyOffline = true;
            } else {
              // MAC ogrenme: IP degisirse cihazi bununla bulacagiz
              String mac = normMac(tempDoc["mac"] | "");
              if (mac.length() && (device["mac"] | String("")) != mac) {
                device["mac"] = mac;
                listChanged = true;
              }
            }
          }
        }
        vTaskDelay(pdMS_TO_TICKS(20));  // 10ms yerine 20ms - WiFi stack için daha iyi
      }

      if (anyOffline && (lastFollow == 0 || millis() - lastFollow > FOLLOW_INTERVAL)) {
        lastFollow = millis();
        if (followIpChanges()) {
          listChanged = true;
          setForceRefresh();  // yeni IP'yi hemen yokla
        }
      }
      if (listChanged) persistDevices();
    }
    // YENİ: durum güncellendi - bağlı WS istemcilerine anında push et
    // (10s'lik normal tur VE setForceRefresh() ile zorlanan anlık yenileme
    // ikisi de buraya düşer, ayrı bir "push now" mekanizması gerekmiyor)
    wsBroadcastStatus();
    for (int i = 0; i < STATUS_CHECK_INTERVAL / 100; i++) {
      if (forceStatusRefresh) {
        clearForceRefresh();  // DÜZELTİLDİ: inline fonksiyon ile güvenli temizleme
        break;
      }
      bool ipChanged = false;
      if (handleStatusPushes(ipChanged)) wsBroadcastStatus();  // push aninda ekrana
      checkZombieWiFi();
      wsFlushPending();  // birlestirilip ertelenmis yayin varsa gonder
      if (ipChanged) persistDevices();
      vTaskDelay(pdMS_TO_TICKS(100));
    }
  }
}
