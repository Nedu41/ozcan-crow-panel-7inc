#include "web_server.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <ESPAsyncWebServer.h>
#include "efekt.h"
#include "sky.h"
#include <vector>
#include "app_state.h"
#include "storage.h"
#include "devices.h"
#include "weather.h"
#include "telegram.h"
#include "events.h"
#include "news.h"
#include "wifi_nets.h"
#include "alerts.h"
#include "relay_webpage_multi.h"

static volatile unsigned long restartAt = 0;  // 0 = yok; webLoop() zamani gelince yeniden baslatir

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
void wsSafeText(AsyncWebSocketClient* c, const String& payload);  // bellek yetmezse cokmeden atlar
void wsSafeTextAll(const String& payload);

// ===== ASYNC PROXY WORKER (blocking HTTPClient cagrilarini paylasilan
// AsyncTCP task'indan ayirmak icin) =====
// NOT: String'i FreeRTOS kuyruguna dogrudan koymuyoruz - xQueueSend/Receive
// struct'i ham memcpy ile kopyalar, String'in kendi kopya/yikici mantigini
// atlar; iki kopya ayni heap bufferini free etmeye calisip cift-free/heap
// bozulmasina yol acabilir. Sabit boyutlu char[] guvenli (POD, memcpy-safe).
struct ProxyJob {
  enum Kind : uint8_t { CONTROL, SCAN } kind;
  uint32_t reqId;
  uint32_t wsClientId;
  char ip[16];
  int relay;
  char action[4];
};
QueueHandle_t proxyQueue = nullptr;

// NOT: eskiden her istekte flashLED() GPIO2'yi yakip sonduruyordu. GPIO2 ayni
// zamanda TFT_DC - canli ekran animasyonu surekli SPI aktarimi yaparken DC'yi
// baska gorevden degistirmek goruntuyu/ekran ayarlarini bozar. LED kaldirildi.

// ============== HTTP handlers ==============

void handleRoot(AsyncWebServerRequest *request) {
  // Kullanıcının isteği üzerine LittleFS yerine relay_webpage_multi.h'den yüklendi
  // no-cache: firmware guncellenince tarayici eski (onbellekteki) arayuzu gostermesin
  AsyncWebServerResponse *res = request->beginResponse_P(200, "text/html", HTML_PAGE);
  res->addHeader("Cache-Control", "no-cache");
  request->send(res);
}

// DEGISTI: gövde /fetch_status VE WS "status" push'unun ortak kaynağı olsun
// diye ayrı fonksiyona çıkarıldı - tek doğruluk kaynağı.
String buildStatusPayload() {
  StaticJsonDocument<4096> responseDoc;
  responseDoc["type"] = "status";
  responseDoc["fw"] = FW_VERSION;
  responseDoc["news"] = newsStatus();

  // Cihaz durumlarını olduğu gibi gönder
  if (xSemaphoreTake(statusMutex, MUTEX_TIMEOUT) == pdTRUE) {
    responseDoc["devices"].set(CURRENT_STATUS_DOC);
    xSemaphoreGive(statusMutex);
  }

  // Schedule bilgisini AYRI nesne olarak gönder
  // HTML: d.schedule[dev.name]["r1_start"] şeklinde okur
  // Böylece cihaz OFFLINE olsa bile schedule görünür
  if (xSemaphoreTake(scheduleMutex, MUTEX_TIMEOUT) == pdTRUE) {
    responseDoc["schedule"].set(SCHEDULE_CONFIG_DOC);
    xSemaphoreGive(scheduleMutex);
  }

  if (xSemaphoreTake(weatherMutex, MUTEX_TIMEOUT) == pdTRUE) {
    responseDoc["weather"].set(CURRENT_WEATHER_DOC);
    responseDoc["weather_config"].set(WEATHER_CONFIG_DOC);
    xSemaphoreGive(weatherMutex);
  }

  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 0)) {
    char timeBuffer[10];
    strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", &timeinfo);
    responseDoc["current_time"] = timeBuffer;
  } else {
    responseDoc["current_time"] = "N/A";
  }

  // HUB saglik teshisi: kendiliginden yeniden baslama sebebi ve bellek
  // (PSRAM yok - buyuk bloklar tukenirse NoMemory/cokme)
  JsonObject hub = responseDoc["hub"].to<JsonObject>();
  hub["uptime"] = millis() / 1000;
  hub["reset"] = (int)esp_reset_reason();  // 4=panic, 5=int WDT, 6=task WDT, 9=brownout
  hub["heap"] = ESP.getFreeHeap();
  hub["min_heap"] = ESP.getMinFreeHeap();
  hub["max_block"] = ESP.getMaxAllocHeap();

  String output;
  serializeJson(responseDoc, output);
  return output;
}

void handleStatusFetch(AsyncWebServerRequest *request) {
  request->send(200, "application/json", buildStatusPayload());
}

// "/save?relay_id=1&days=127" gibi bir yoldan tek parametre okur
String queryParam(const String& url, const String& key) {
  int q = url.indexOf('?');
  if (q < 0) return "";
  String pattern = key + "=";
  int pos = q + 1;
  while (pos < (int)url.length()) {
    int amp = url.indexOf('&', pos);
    if (amp < 0) amp = url.length();
    if (url.substring(pos, pos + pattern.length()) == pattern) {
      return url.substring(pos + pattern.length(), amp);
    }
    pos = amp + 1;
  }
  return "";
}

// Zamanlayici ayarini HUB'in kendi kaydina yazar (cihaza gonderme ayri). Proxy ve ekran kullanir.
bool scheduleSave(const String& ip, int relay_id, const String& start_h, const String& start_m, const String& end_h,
                  const String& end_m, int days, const String& sunrise_act, const String& sunset_act) {
  if (relay_id < 1 || relay_id > MAX_RELAYS) return false;
  int devIdx = -1;
  for (int i = 0; i < (int)doc.size(); i++) {
    if (doc[i]["ip"] == ip) { devIdx = i; break; }
  }
  if (devIdx < 0 || devIdx >= MAX_DEVICES) return false;
  String devName = doc[devIdx]["name"];
  if (xSemaphoreTake(scheduleMutex, MUTEX_TIMEOUT) != pdTRUE) return false;
  JsonObject obj = SCHEDULE_CONFIG_DOC[devName].to<JsonObject>();
  String rKey = "r" + String(relay_id);
  obj[rKey + "_start"] = start_h + ":" + start_m;
  obj[rKey + "_end"] = end_h + ":" + end_m;
  obj[rKey + "_days"] = days;
  obj[rKey + "_sunrise_act"] = sunrise_act;
  obj[rKey + "_sunset_act"] = sunset_act;
  sunriseAction[devIdx][relay_id - 1] = sunrise_act;
  sunsetAction[devIdx][relay_id - 1] = sunset_act;
  saveScheduleConfig();
  xSemaphoreGive(scheduleMutex);
  setForceRefresh();
  return true;
}

// Ekrandan: HUB kaydi + ESP01'e /save (engelleyici HTTP; ayri gorevden cagrilir). days=0 -> pasif.
bool scheduleApply(const String& ip, int relay, int sh, int sm, int eh, int em, int days, const String& sunrise, const String& sunset) {
  char hh[3], mm[3], h2[3], m2[3];
  snprintf(hh, 3, "%02d", sh); snprintf(mm, 3, "%02d", sm); snprintf(h2, 3, "%02d", eh); snprintf(m2, 3, "%02d", em);
  scheduleSave(ip, relay, hh, mm, h2, m2, days, sunrise, sunset);
  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.setConnectTimeout(500);
  String url = "http://" + ip + "/save?relay_id=" + relay + "&start_h=" + hh + "&start_m=" + mm + "&end_h=" + h2 + "&end_m=" + m2 +
               "&days=" + days + "&active=" + (days ? 1 : 0) + "&sunrise_act=" + sunrise + "&sunset_act=" + sunset;
  if (!http.begin(url)) return false;
  int code = http.GET();
  http.end();
  return code > 0;
}

// ============== handleProxy (Genişletilmiş ve Hata Ayıklamalı) ==============
void handleProxy(AsyncWebServerRequest *request) {

  Serial.println("=== PROXY İSTEĞİ ===");
  Serial.printf("Method: %s\n", request->method() == AsyncWebRequestMethod::HTTP_GET ? "GET" : "POST");

  // Tüm parametreleri listele
  for (int i = 0; i < request->args(); i++) {
    Serial.printf("Arg[%d]: %s = %s\n", i, request->argName(i).c_str(), request->arg(i).c_str());
  }

  if (!request->hasArg("ip") || !request->hasArg("url")) {
    Serial.println("HATA: ip veya url parametresi eksik");
    request->send(400, "text/plain", "HATA: ip ve url parametreleri gerekli");
    return;
  }

  String ip = request->arg("ip");
  String urlPath = request->arg("url");

  Serial.printf("Hedef IP: %s\n", ip.c_str());
  Serial.printf("URL Path: %s\n", urlPath.c_str());

  // URL decode işlemi
  urlPath.replace("%2F", "/");
  urlPath.replace("%3F", "?");
  urlPath.replace("%3D", "=");
  urlPath.replace("%26", "&");

  // /save isteği kontrolü
  if (urlPath.startsWith("/save")) {
    Serial.println("=== SCHEDULE KAYIT İSTEĞİ ===");

    // BUGFIX: parametreler istegin kendi argumanlari DEGIL, "url" argumaninin
    // icindeki sorgu dizesinde (/proxy?ip=..&url=/save?relay_id=1&...).
    // Eskiden request->arg("relay_id") hep bos -> relay_id=0 -> HUB zamanlayiciyi
    // hic kaydetmiyordu (canli cihazda "schedule":null bu yuzdendi).
    int relay_id = queryParam(urlPath, "relay_id").toInt();
    String sunrise_act = queryParam(urlPath, "sunrise_act");
    String sunset_act = queryParam(urlPath, "sunset_act");
    String start_h = queryParam(urlPath, "start_h");
    String start_m = queryParam(urlPath, "start_m");
    String end_h = queryParam(urlPath, "end_h");
    String end_m = queryParam(urlPath, "end_m");
    int days = queryParam(urlPath, "days").toInt();
    
    Serial.printf("relay_id: %d\n", relay_id);
    Serial.printf("sunrise_act: %s\n", sunrise_act.c_str());
    Serial.printf("sunset_act: %s\n", sunset_act.c_str());
    Serial.printf("start: %s:%s\n", start_h.c_str(), start_m.c_str());
    Serial.printf("end: %s:%s\n", end_h.c_str(), end_m.c_str());
    Serial.printf("days: %d\n", days);

    scheduleSave(ip, relay_id, start_h, start_m, end_h, end_m, days, sunrise_act, sunset_act);
  }

  // Orijinal proxy devam eder
  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.setConnectTimeout(500);
  
  String fullUrl = "http://" + ip + urlPath;
  Serial.printf("Proxy forwarding to: %s\n", fullUrl.c_str());
  
  if (http.begin(fullUrl)) {
    int code = http.GET();
    String resp = (code > 0) ? readBodyWithTimeout(http, 2000) : "ERROR";

    Serial.printf("Proxy response code: %d\n", code);
    Serial.printf("Response length: %d\n", resp.length());

    if (code > 0) {
      request->send(200, "text/plain", resp);
    } else {
      request->send(500, "text/plain", "Hedef cihaza ulaşılamadı");
    }
    http.end();
  } else {
    Serial.println("Proxy: HTTP begin failed");
    request->send(500, "text/plain", "HTTP bağlantı hatası");
  }
  
  Serial.println("=== PROXY İSTEĞİ TAMAMLANDI ===\n");
}

void handleGetDevices(AsyncWebServerRequest *request) {
  request->send(200, "application/json", DEVICE_LIST_JSON);
}

// NOT: handleSaveDevices ve handleSaveWeatherConfig artik burada degil.
// AsyncWebServer'da server.arg("plain") karsiligi yok - POST govdesi
// (JSON) ayri bir onBody callback ile geliyor (bkz. setupWebServer() icindeki
// server.on(..., HTTP_POST, ...) 3-parametreli kayitlar).

void handleGetWeatherConfig(AsyncWebServerRequest *request) {
  String out;
  if (xSemaphoreTake(weatherMutex, MUTEX_TIMEOUT) == pdTRUE) {
    serializeJson(WEATHER_CONFIG_DOC, out);
    xSemaphoreGive(weatherMutex);
  } else {
    out = "{}";
  }
  request->send(200, "application/json", out);
}

// DEGISTI: govde artik AsyncWebServer'in onBody callback'inden geliyor,
// bu yuzden JSON dogrudan parametre olarak aliniyor (server.arg("plain") yok).
void applyWeatherConfig(AsyncWebServerRequest *request, const String& body) {
  DynamicJsonDocument inDoc(1536);  // "targets" dizisi coklu secimle buyuyebiliyor
  DeserializationError err = deserializeJson(inDoc, body);
  if (err) {
    request->send(400, "text/plain", "HATA");
    return;
  }

  // Eğer sadece refresh isteniyorsa
  if (inDoc.containsKey("refresh") && inDoc["refresh"] == true) {
    Serial.println("DEBUG: Manuel hava durumu yenileme isteği");
    weatherRefreshRequested = true;  // blocking HTTPS'i hava gorevi yapar
    request->send(200, "text/plain", "REFRESHED");
    return;
  }

  bool configChanged = false;
  bool addressChanged = false;

  if (xSemaphoreTake(weatherMutex, MUTEX_TIMEOUT) == pdTRUE) {
    // Yeni değerleri al
    String newCity = inDoc["city"] | "";
    String newDistrict = inDoc["district"] | "";
    String newNeighbourhood = inDoc["neighbourhood"] | "";

    // Adres değişimi kontrolü
    if (newCity != (WEATHER_CONFIG_DOC["city"] | "") || newDistrict != (WEATHER_CONFIG_DOC["district"] | "") || newNeighbourhood != (WEATHER_CONFIG_DOC["neighbourhood"] | "")) {
      addressChanged = true;
      WEATHER_CONFIG_DOC["city"] = newCity;
      WEATHER_CONFIG_DOC["district"] = newDistrict;
      WEATHER_CONFIG_DOC["neighbourhood"] = newNeighbourhood;
      // Adres değişince koordinatları sil ki tekrar geocode yapılsın
      WEATHER_CONFIG_DOC["latitude"] = "";
      WEATHER_CONFIG_DOC["longitude"] = "";
      configChanged = true;
    }

    // targets dizisi degisti mi (siralamaya duyarli string karsilastirma -
    // web tarafi hep ayni sirada gonderiyor, yeterli ve basit)
    String newTargets, oldTargets;
    serializeJson(inDoc["targets"].as<JsonArray>(), newTargets);
    serializeJson(WEATHER_CONFIG_DOC["targets"].as<JsonArray>(), oldTargets);

    // Diğer ayar değişim kontrolü
    // NOT: sol taraf da "| varsayilan" ile okunuyor - ArduinoJson v7'de ham
    // MemberProxy'yi cast'siz != ile karsilastirmak private kopya ctor hatasi veriyordu.
    if ((inDoc["active"] | false) != (WEATHER_CONFIG_DOC["active"] | false) || newTargets != oldTargets || (inDoc["threshold"] | 3) != (WEATHER_CONFIG_DOC["threshold"] | 3) || (inDoc["action"] | "") != (WEATHER_CONFIG_DOC["action"] | "") || (inDoc["wind_threshold"] | 15.0f) != (WEATHER_CONFIG_DOC["wind_threshold"] | 15.0f) || (inDoc["light_threshold"] | 60) != (WEATHER_CONFIG_DOC["light_threshold"] | 60) || (inDoc["daylight_window"] | true) != (WEATHER_CONFIG_DOC["daylight_window"] | true)) {
      configChanged = true;
      WEATHER_CONFIG_DOC["daylight_window"] = inDoc["daylight_window"] | true;
      WEATHER_CONFIG_DOC["active"] = inDoc["active"];
      WEATHER_CONFIG_DOC["targets"].set(inDoc["targets"]);
      WEATHER_CONFIG_DOC["threshold"] = inDoc["threshold"];
      WEATHER_CONFIG_DOC["light_threshold"] = inDoc["light_threshold"] | 60;
      WEATHER_CONFIG_DOC["action"] = inDoc["action"];
      WEATHER_CONFIG_DOC["wind_threshold"] = inDoc["wind_threshold"];  // Rüzgar eşiği kaydedildi
    }

    if (configChanged) {
      // Eğer adres değiştiyse ve koordinatlar varsa, bunları güncelle
      String newLat = inDoc["latitude"] | "";
      String newLon = inDoc["longitude"] | "";
      if (newLat.length() > 2 && newLon.length() > 2) {
        WEATHER_CONFIG_DOC["latitude"] = newLat;
        WEATHER_CONFIG_DOC["longitude"] = newLon;
        WEATHER_CONFIG_DOC["city_name"] = inDoc["city_name"] | "";
      }

      saveWeatherConfig();
    }
    xSemaphoreGive(weatherMutex);

    // DEGISTI: eskiden HER ayar degisikliginde burada (AsyncTCP task'inda)
    // blocking HTTPS ile hava yeniden cekiliyordu (1-3sn tum web/WS donuyordu).
    // Otomatik kayitta her alan degisikligi bunu tetikleyecegi icin: sadece
    // konum degisince, ve hava gorevine devredilerek yenileniyor. Esik/role
    // gibi ayarlar hava verisini degistirmiyor, yeniden cekmeye gerek yok.
    if (addressChanged) {
      Serial.println("DEBUG: Konum degisti, hava yenilemesi istendi");
      weatherRefreshRequested = true;
    }
  }

  request->send(200, "text/plain", "OK");
}

void handleGetWeatherStatus(AsyncWebServerRequest *request) {
  String out;
  if (xSemaphoreTake(weatherMutex, MUTEX_TIMEOUT) == pdTRUE) {
    serializeJson(CURRENT_WEATHER_DOC, out);
    xSemaphoreGive(weatherMutex);
  } else out = "{}";
  request->send(200, "application/json", out);
}

void handleNotFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

void handleWiFiReset(AsyncWebServerRequest *request) {
  wifiNetsClear();  // yoksa acilista listedeki aga baglanir, kurulum portali hic acilmazdi
  request->send(200, "text/plain", "Reset");
  delay(1000);
  ESP.restart();
}

void handleRestart(AsyncWebServerRequest *request) {
  request->send(200, "text/plain", "Restart");
  delay(1000);
  ESP.restart();
}

// NOT: handleManualControl kaldirildi - eskiden toggle() butonu ayrica
// fetch("/manual_control") atiyordu, simdi WS "control" mesaji ayni isi
// (lastManualControlAction guncelleme + zorla durum yenileme) proxyWorkerTask
// icinde tek round-trip'te yapiyor (bkz. asagida CONTROL job islenmesi).

void handleTestTelegram(AsyncWebServerRequest *request) {
  Serial.println("DEBUG: Test Telegram mesajı gönderiliyor...");

  // Test mesajı oluştur
  struct tm timeinfo;
  String timeStr = "N/A";
  if (getLocalTime(&timeinfo, 0)) {
    char buffer[20];
    strftime(buffer, sizeof(buffer), "%H:%M:%S", &timeinfo);
    timeStr = String(buffer);
  }

  String testMsg = "🔧 *TEST MESAJI* 🔧\n"
                   "Bu mesaj /test_telegram endpoint'inden gönderildi.\n"
                   "📍 IP: "
                   + WiFi.localIP().toString() + "\n"
                                                 "🕒 Saat: "
                   + timeStr + "\n"
                               "📡 MAC: "
                   + WiFi.macAddress() + "\n"
                                         "✅ Telegram bağlantısı çalışıyor!";

  sendTelegramAlert(testMsg);

  request->send(200, "text/plain", "Test mesajı gönderildi. Serial Monitor'ü kontrol edin.");
}

// ============== WebSocket ==============
// Protokol (bkz. proje plani):
//   istemci->sunucu: {"type":"control","reqId":N,"ip":"...","relay":1,"action":"on"}
//                     {"type":"scan_devices","reqId":N}
//   sunucu->istemci:  {"type":"status", ...}  (baglaninca + arka plan gorevlerinden push)
//                     {"type":"control_result","reqId":N,"ip":"...","relay":1,"ok":true}
//                     {"type":"scan_result","reqId":N,"devices":[{"ip","mac","hostname"}]}
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("[WS] Istemci #%u baglandi\n", client->id());
    wsSafeText(client, buildStatusPayload());  // ilk anlik goruntu
    wsSafeText(client, alertsJson());          // aktif uyarilar
  } else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("[WS] Istemci #%u ayrildi\n", client->id());
  } else if (type == WS_EVT_DATA) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    // Basitlik icin sadece tek-parcali (fragmentsiz) metin mesajlari isleniyor -
    // kontrol/tarama mesajlari zaten kucuk (<256 byte), boluk gerekmiyor.
    if (!info->final || info->index != 0 || info->len != len || info->opcode != WS_TEXT) return;

    StaticJsonDocument<256> msg;
    if (deserializeJson(msg, (const char*)data, len)) return;
    String t = msg["type"] | "";

    if (t == "control") {
      ProxyJob job{};
      job.kind = ProxyJob::CONTROL;
      job.reqId = msg["reqId"] | 0;
      job.wsClientId = client->id();
      String ip = msg["ip"] | "";
      String action = msg["action"] | "";
      strncpy(job.ip, ip.c_str(), sizeof(job.ip) - 1);
      strncpy(job.action, action.c_str(), sizeof(job.action) - 1);
      job.relay = msg["relay"] | 0;
      xQueueSend(proxyQueue, &job, 0);
    } else if (t == "scan_devices") {
      ProxyJob job{};
      job.kind = ProxyJob::SCAN;
      job.reqId = msg["reqId"] | 0;
      job.wsClientId = client->id();
      xQueueSend(proxyQueue, &job, 0);
    }
  }
}

// ============== Async proxy worker (blocking HTTP islerini AsyncTCP
// task'inin disinda yapar) ==============
void proxyWorkerTask(void *pvParameters) {
  ProxyJob job;
  for (;;) {
    if (xQueueReceive(proxyQueue, &job, portMAX_DELAY) != pdTRUE) continue;

    if (job.kind == ProxyJob::CONTROL) {
      lastManualControlAction = millis();  // eskiden /manual_control fetch'i yapiyordu
      bool ok = false;
      String ip = job.ip;
      String action = job.action;
      if (ip.length() >= 7 && job.relay >= 1 && job.relay <= MAX_RELAYS &&
          (action == "on" || action == "off")) {
        ok = sendControlSignal(ip, job.relay, action, "web");
        String devName = ip;
        for (JsonObject d : doc.as<JsonArray>()) {
          if ((d["ip"] | String("")) == ip) { devName = d["name"] | ip; break; }
        }
        addEvent(devName, job.relay, action == "on", "web", ok);
        // Cihazin push'unu beklemeden durumu hemen guncelle - yoksa arada
        // baska cihazlarin push'uyla giden "status" butonu eski haline
        // cizip geri donduruyordu.
        if (ok && xSemaphoreTake(statusMutex, MUTEX_TIMEOUT) == pdTRUE) {
          JsonObject st = CURRENT_STATUS_DOC[devName].as<JsonObject>();
          if (!st.isNull()) {
            st[job.relay == 1 ? "r1_state" : "r2_state"] = action == "on" ? "ON" : "OFF";
            st[job.relay == 1 ? "relay_1" : "relay_2"] = action == "on" ? 1 : 0;
          }
          xSemaphoreGive(statusMutex);
        }
      }

      StaticJsonDocument<192> out;
      out["type"] = "control_result";
      out["reqId"] = job.reqId;
      out["ip"] = ip;
      out["relay"] = job.relay;
      out["ok"] = ok;
      String payload;
      serializeJson(out, payload);
      AsyncWebSocketClient *c = ws.client(job.wsClientId);
      if (c && c->status() == WS_CONNECTED) wsSafeText(c, payload);

      if (ok) wsBroadcastStatus();  // diger acik ekranlar (telefon/PC) da aninda gorsun
      setForceRefresh();  // updateAllDeviceStatuses'i hemen uyandirir -> WS "status" push
    } else if (job.kind == ProxyJob::SCAN) {
      // WiFi'de tek multicast sorgu/cevap kolayca kayboluyor (yeni modul ilk
      // taramada cikmayip ikincide cikiyordu) - 3 tur sorgula, sonuclari birlestir.
      std::vector<std::pair<IPAddress, String>> found;
      for (int round = 0; round < 3; round++) {
        int n = MDNS.queryService("http", "tcp");
        for (int i = 0; i < n; i++) {
          IPAddress ip = MDNS.IP(i);
          bool dup = false;
          for (auto& f : found) if (f.first == ip) { dup = true; break; }
          if (!dup) found.push_back({ip, MDNS.hostname(i)});
        }
      }

      StaticJsonDocument<1536> result;
      result["type"] = "scan_result";
      result["reqId"] = job.reqId;
      JsonArray arr = result.createNestedArray("devices");

      for (auto& f : found) {
        IPAddress ip = f.first;
        const String& hostname = f.second;

        // mDNS'in _http._tcp servisini advertise eden HERKES aday olur
        // (yazici, router vb. olabilir) - gercekten bizim ESP01 dugumumuz mu
        // diye mevcut /mac endpoint'iyle dogruluyoruz. Zayif sinyalde ilk
        // istek gecikebiliyor: 1.5sn sure + bir tekrar.
        String mac = "";
        for (int attempt = 0; attempt < 2 && !mac.length(); attempt++) {
          HTTPClient http;
          http.setTimeout(1500);
          http.setConnectTimeout(1500);
          if (http.begin("http://" + ip.toString() + "/mac")) {
            if (http.GET() == 200) {
              mac = readBodyWithTimeout(http, 1500);
              mac.trim();
            }
            http.end();
          }
          mac = normMac(mac);
        }
        if (!mac.length()) continue;  // ESP01 /mac formatina uymuyor

        // Kayitli cihaz: ayni IP VEYA ayni MAC (IP'si degismis olabilir - durum
        // gorevi onu MAC'iyle bulup kaydini kendisi guncelliyor, "yeni" degil)
        bool known = false;
        for (JsonObject d : doc.as<JsonArray>()) {
          if (String((const char*)(d["ip"] | "")) == ip.toString() ||
              (d["mac"] | String("")) == mac) { known = true; break; }
        }
        if (known) continue;

        JsonObject o = arr.createNestedObject();
        o["ip"] = ip.toString();
        o["mac"] = mac;
        o["hostname"] = hostname;
      }

      String payload;
      serializeJson(result, payload);
      AsyncWebSocketClient *c = ws.client(job.wsClientId);
      if (c && c->status() == WS_CONNECTED) wsSafeText(c, payload);
    }
  }
}

// BUGFIX (2026-09-30): push'la birlikte yayin sikligi saniyede birkaca cikti;
// yavas okuyan istemci kuyrugunda ~8KB'lik mesajlar birikince textAll()'in
// make_shared'i bellek bulamayip istisna atti -> abort(), HUB dakikada bir
// yeniden basliyordu (seri log backtrace ile dogrulandi). Uc onlem:
//  - yayinlar birlestirilir: en fazla WS_MIN_INTERVAL_MS'de bir (arada gelen
//    istekler wsFlushPending() ile sonradan tek yayin olarak gider)
//  - yeterli bitisik bellek yoksa yayin atlanir (sonraki tur zaten gelecek)
//  - istemci basina kuyruk 4 mesajla sinirli (platformio.ini
//    WS_MAX_QUEUED_MESSAGES) - dolarsa baglanti kapanir, tarayici 2sn'de
//    yeniden baglanip guncel goruntuyu alir
static const unsigned long WS_MIN_INTERVAL_MS = 500;
static volatile bool wsPending = false;
static volatile unsigned long alertDemoOff = 0;  // /alert_demo: 0 = demo yok, degilse kapanma zamani

// Kontrol ile ayirma arasinda baska gorev (HTTPS hava/haber istegi TLS icin
// ~40KB) bellegi alabiliyor - kutuphanenin make_shared'i o an std::bad_alloc
// atinca yakalanmazsa abort(). Yakala, bu yayini atla; sonraki zaten gelecek.
void wsSafeTextAll(const String& payload) {
  try {
    ws.textAll(payload);
  } catch (const std::exception&) {
    Serial.printf("[WS] bellek yetmedi (en buyuk blok %u), yayin atlandi\n", ESP.getMaxAllocHeap());
  }
}

void wsSafeText(AsyncWebSocketClient* c, const String& payload) {
  try {
    c->text(payload);
  } catch (const std::exception&) {
    Serial.printf("[WS] bellek yetmedi (en buyuk blok %u), mesaj atlandi\n", ESP.getMaxAllocHeap());
  }
}
static volatile unsigned long wsLastSent = 0;

void wsBroadcastStatus() {
  if (millis() - wsLastSent < WS_MIN_INTERVAL_MS) {
    wsPending = true;
    return;
  }
  wsLastSent = millis();
  wsPending = false;
  if (ws.count() == 0) return;
  String payload = buildStatusPayload();
  if (ESP.getMaxAllocHeap() < payload.length() + 8192) {
    Serial.printf("[WS] bellek dar (en buyuk blok %u), yayin atlandi\n", ESP.getMaxAllocHeap());
    return;
  }
  wsSafeTextAll(payload);
}

void wsFlushPending() {
  if (wsPending && millis() - wsLastSent >= WS_MIN_INTERVAL_MS) wsBroadcastStatus();
}

void webExtraRoutes();  // web_extra.cpp
#ifdef DEV_SCREENSHOT
void devScreenshotRoute(AsyncWebServer& server);
#endif

void setupWebServer() {
  // YENİ: ESP01'lere giden blocking HTTP çağrılarını (röle kontrolü, mDNS
  // tarama doğrulaması) paylaşılan AsyncTCP task'ından ayıran worker
  proxyQueue = xQueueCreate(10, sizeof(ProxyJob));
  xTaskCreatePinnedToCore(proxyWorkerTask, "ProxyWorker", 8192, NULL, 1, NULL, 1);

  // WebSocket - buton tıklaması artık HTTP proxy yerine WS "control" mesajı
  // gönderiyor, durum güncellemeleri de 2sn'lik polling yerine push ediliyor
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  efektRoutes();
  webExtraRoutes();
  server.on("/lamps", AsyncWebRequestMethod::HTTP_GET, handleRoot);
  server.on("/efekt", AsyncWebRequestMethod::HTTP_GET, handleRoot);
  server.on("/sistem", AsyncWebRequestMethod::HTTP_GET, handleRoot);

  // Uyarilar (hava/deprem): onay + teshis + deneme
  server.on("/alert_ack", AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("id")) { request->send(400, "text/plain", "id yok"); return; }
    bool ok = alertsAck(request->getParam("id")->value());
    request->send(200, "text/plain", ok ? "OK" : "YOK");
  });
  server.on("/alerts", AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", alertsJson());
  });
  // Uyari demosu: ornek uyarilar 20 sn gorunur, sonra webLoop kaldirir
  server.on("/alert_demo", AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request) {
    alertsDemo(true);
    alertDemoOff = millis() + 20000UL;
    request->send(200, "text/plain", "OK");
  });
  // Deneme: /alert_test?kind=eq|wx (onaylayinca gider)
  server.on("/alert_test", AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request) {
    bool eq = request->hasParam("kind") && request->getParam("kind")->value() == "eq";
    String id = "test:" + String(millis());
    if (eq) alertsRaise(id, 1, "YAKIN DEPREM M4.2 (TEST)", "Örnek İlçe (Örnek İl) - 12 km - 30.09 14:20");
    else alertsRaise(id, 0, "HAVA UYARISI - YARIN (TEST)", "rüzgar 62 km/s, yağış 30 mm");
    request->send(200, "text/plain", "OK");
  });

  // HTTP rotaları
  server.on("/", AsyncWebRequestMethod::HTTP_GET, handleRoot);
  server.on("/settings", AsyncWebRequestMethod::HTTP_GET, handleRoot);
  server.on("/weather", AsyncWebRequestMethod::HTTP_GET, handleRoot);
  server.on("/log", AsyncWebRequestMethod::HTTP_GET, handleRoot);
  server.on("/device", AsyncWebRequestMethod::HTTP_GET, handleRoot);
  server.on("/notes", AsyncWebRequestMethod::HTTP_GET, handleRoot);
  server.on("/get_notes", AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", readNotes());
  });
  server.on("/save_notes", AsyncWebRequestMethod::HTTP_POST,
    [](AsyncWebServerRequest *request) {},
    nullptr,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      if (index == 0) request->_tempObject = new String();
      String *body = (String *)request->_tempObject;
      body->concat((const char *)data, len);
      if (index + len == total) {
        bool ok = saveNotes(*body);
        delete body;
        request->_tempObject = nullptr;
        request->send(ok ? 200 : 400, "text/plain", ok ? "OK" : "HATA");
      }
    });
  server.on("/events", AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", eventsJson());
  });
  server.on("/fetch_status", AsyncWebRequestMethod::HTTP_GET, handleStatusFetch);
  // HUB ekraninda 24 saatlik gokyuzu dongusunu ~2 dakikada oynatir
  server.on("/sky_demo", AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request) {
    startSkyDemo();
    request->send(200, "text/plain", "Gokyuzu demosu basladi (~2 dk)");
  });
  server.on("/weather_demo", AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request) {
    startWeatherDemo();
    request->send(200, "text/plain", "Hava demosu basladi (~1 dk)");
  });
  server.on("/proxy", AsyncWebRequestMethod::HTTP_GET, handleProxy);
  server.on("/get_devices", AsyncWebRequestMethod::HTTP_GET, handleGetDevices);
  server.on("/save_devices", AsyncWebRequestMethod::HTTP_POST,
    [](AsyncWebServerRequest *request) {},  // govde tamamlaninca onBody yanitliyor
    nullptr,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      if (index == 0) request->_tempObject = new String();
      String *body = (String *)request->_tempObject;
      body->concat((const char *)data, len);
      if (index + len == total) {
        saveDevices(*body);
        delete body;
        request->_tempObject = nullptr;
        request->send(200, "text/plain", "OK");
      }
    });
  server.on("/get_weather_config", AsyncWebRequestMethod::HTTP_GET, handleGetWeatherConfig);
  server.on("/save_weather_config", AsyncWebRequestMethod::HTTP_POST,
    [](AsyncWebServerRequest *request) {},
    nullptr,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      if (index == 0) request->_tempObject = new String();
      String *body = (String *)request->_tempObject;
      body->concat((const char *)data, len);
      if (index + len == total) {
        applyWeatherConfig(request, *body);
        delete body;
        request->_tempObject = nullptr;
      }
    });
  server.on("/get_weather_status", AsyncWebRequestMethod::HTTP_GET, handleGetWeatherStatus);
  server.on("/reset_wifi", AsyncWebRequestMethod::HTTP_GET, handleWiFiReset);

  // ---- WiFi aglari (SuDepoSis'teki bolum gibi): tara / kaydet / baglan / sil ----
  server.on("/wifi", AsyncWebRequestMethod::HTTP_GET, handleRoot);
  server.on("/wifi_info", AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", wifiNetsInfoJson());
  });
  // Tarama arka planda (async): ilk cagri baslatir, sayfa sonuc gelene kadar yoklar -
  // bloklayan scanNetworks() AsyncTCP gorevini birkac saniye kilitlerdi
  server.on("/wifi_scan", AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request) {
    int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_FAILED) {
      WiFi.scanNetworks(true);
      request->send(200, "application/json", "{\"scanning\":true}");
      return;
    }
    if (n == WIFI_SCAN_RUNNING) {
      request->send(200, "application/json", "{\"scanning\":true}");
      return;
    }
    JsonDocument out;
    JsonArray list = out["networks"].to<JsonArray>();
    for (int i = 0; i < n; i++) {
      if (WiFi.SSID(i).length() == 0) continue;
      bool dup = false;  // ayni SSID'li birden cok erisim noktasi: en gucluyu tut
      for (JsonObject o : list) if (o["ssid"] == WiFi.SSID(i)) { dup = true; break; }
      if (dup) continue;
      JsonObject o = list.add<JsonObject>();
      o["ssid"] = WiFi.SSID(i);
      o["rssi"] = WiFi.RSSI(i);
      o["secured"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
    }
    WiFi.scanDelete();
    String s;
    serializeJson(out, s);
    request->send(200, "application/json", s);
  });
  // Sifre URL'de gitmesin diye POST form
  server.on("/wifi_add", AsyncWebRequestMethod::HTTP_POST, [](AsyncWebServerRequest *request) {
    String ssid = request->hasParam("ssid", true) ? request->getParam("ssid", true)->value() : "";
    String pass = request->hasParam("pass", true) ? request->getParam("pass", true)->value() : "";
    bool ok = wifiNetsAdd(ssid, pass);
    request->send(ok ? 200 : 400, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false,\"msg\":\"SSID gecersiz ya da liste dolu (en fazla 6)\"}");
  });
  server.on("/wifi_delete", AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request) {
    int idx = request->hasParam("idx") ? request->getParam("idx")->value().toInt() : -1;
    bool ok = wifiNetsDelete(idx);
    request->send(ok ? 200 : 400, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
  });
  // Canli ag degistirmek guvenilir degil (SuDepoSis'te de ayni sonuc) - tercih
  // edilen yapilip yeniden baslatilir; tutmazsa acilista siradaki aglar denenir
  server.on("/wifi_connect", AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request) {
    int idx = request->hasParam("idx") ? request->getParam("idx")->value().toInt() : -1;
    bool ok = wifiNetsMakePreferred(idx);
    request->send(ok ? 200 : 400, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
    if (ok) restartAt = millis() + 1500;
  });
  server.on("/restart_hub", AsyncWebRequestMethod::HTTP_GET, handleRestart);
  server.on("/test_telegram", AsyncWebRequestMethod::HTTP_GET, handleTestTelegram);  // YENİ: Test endpoint'i

#ifdef DEV_SCREENSHOT
  devScreenshotRoute(server);
#endif
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP sunucu başlatıldı.");
}

void webLoop() {
  // AsyncWebServer kendi task'larinda calisir - server.handleClient() yok.
  // Terk edilmis WS baglantilarini (telefon arka plana atildi, laptop
  // uyudu vb.) periyodik temizle.
  static unsigned long lastWsCleanup = 0;
  if (millis() - lastWsCleanup > 5000) {
    lastWsCleanup = millis();
    ws.cleanupClients();
  }
  if (alertDemoOff && (long)(millis() - alertDemoOff) >= 0) {
    alertDemoOff = 0;
    alertsDemo(false);
  }
  // Uyari listesi degisti -> acik ekranlara (telefon/PC) anlik yayin
  static uint32_t lastAlertVer = 0;
  uint32_t av = alertsVersion();
  if (av != lastAlertVer) {
    lastAlertVer = av;
    if (ws.count()) wsSafeTextAll(alertsJson());
  }
  // Ertelenmis yeniden baslatma: HTTP cevabi istemciye ulassin diye
  if (restartAt && (long)(millis() - restartAt) >= 0) {
    Serial.println("[WiFi] Secilen aga baglanmak icin yeniden baslatiliyor");
    WiFi.disconnect(true);
    delay(150);
    ESP.restart();
  }

}
