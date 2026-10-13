#include "efekt.h"

#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#include <driver/gpio.h>
#include <soc/io_mux_reg.h>

#include "ui_common.h"

extern AsyncWebServer server;
static volatile uint32_t frameCnt = 0, drawUs = 0, dropCnt = 0, pktCnt = 0;
EfektState efektSt;

static const int KW = 160, KH = 96;  // kare: yatay 160x96, dikey 96x160 (5x büyütülür -> 800x480)
static const uint16_t UDP_PORT = 4048;

// ------------------------------------------------------------------ köprü adresi
static volatile bool devOpenReq = false, devCloseReq = false;  // web/geliştirme: ekranı LVGL zamanlayıcısında aç/kapat
static String bridgeBase, bridgeKey;  // http://ip:8766 , anahtar
static SemaphoreHandle_t mtx;

static void setBridge(const String& u, bool save) {
  int q = u.indexOf("?k=");
  int e = u.indexOf('/', 8);
  if (q < 0 || e < 0) return;
  String base = u.substring(0, e), key = u.substring(q + 3);
  xSemaphoreTake(mtx, portMAX_DELAY);
  bool changed = base != bridgeBase || key != bridgeKey;
  bridgeBase = base;
  bridgeKey = key;
  xSemaphoreGive(mtx);
  if (changed && save) {
    Preferences p;
    p.begin("efekt", false);
    p.putString("u", u);
    p.end();
  }
}

void efektRoutes() {
  server.on("/api/aibr", AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest* r) {
    if (r->hasParam("u")) setBridge(r->getParam("u")->value(), true);
    r->send(200, "text/plain", "OK");
  });
#ifdef DEV_SCREENSHOT
  server.on("/dev/fx", AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest* r) { devOpenReq = true; r->send(200, "text/plain", "ok"); });
#endif
  // Web arayüzü: köprüyü cihaz üzerinden kumanda eder (efekt ekranda açılır)
  server.on("/api/efekt/durum", AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest* r) {
    JsonDocument d;
    d["ok"] = (bool)efektSt.ok;
    d["durum"] = efektSt.durum;
    d["ad"] = efektSt.ad;
    JsonArray l = d["liste"].to<JsonArray>();
    char nm[24][32];
    int n = efektListe(nm, 24);
    for (int i = 0; i < n; i++) l.add(nm[i]);
    String o;
    serializeJson(d, o);
    r->send(200, "application/json", o);
  });
  server.on("/api/efekt/uret", AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest* r) {
    if (r->hasParam("p")) { efektUret(r->getParam("p")->value()); devOpenReq = true; }
    r->send(200, "text/plain", "OK");
  });
  server.on("/api/efekt/oynat", AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest* r) {
    if (r->hasParam("ad")) { efektOynat(r->getParam("ad")->value()); devOpenReq = true; }
    r->send(200, "text/plain", "OK");
  });
  server.on("/api/efekt/kaydet", AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest* r) {
    if (r->hasParam("ad")) efektKaydet(r->getParam("ad")->value());
    r->send(200, "text/plain", "OK");
  });
  server.on("/api/efekt/sil", AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest* r) {
    if (r->hasParam("ad")) efektSil(r->getParam("ad")->value());
    r->send(200, "text/plain", "OK");
  });
  server.on("/api/efekt/ayar", AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest* r) {
    float h = r->hasParam("hiz") ? r->getParam("hiz")->value().toFloat() : 1.0f;
    float i = r->hasParam("isik") ? r->getParam("isik")->value().toFloat() : 1.0f;
    efektAyar(h, i);
    r->send(200, "text/plain", "OK");
  });
  server.on("/api/efekt/durdur", AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest* r) {
    efektDurdur();
    devCloseReq = true;
    r->send(200, "text/plain", "OK");
  });
  server.on("/api/aikey", AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest* r) {
    if (r->hasParam("set")) efektAnahtar(r->getParam("set")->value());
    r->send(200, "application/json", efektAnahtarVar() ? "{\"var\":true}" : "{\"var\":false}");
  });
  server.on("/api/efekt", AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest* r) {
    bool land = uiIsLandscape();
    r->send(200, "application/json", String("{\"w\":") + (land ? KW : KH) + ",\"h\":" + (land ? KH : KW) + ",\"kare\":" + frameCnt + ",\"cizim_us\":" + drawUs + ",\"dusen\":" + dropCnt + ",\"paket\":" + pktCnt + "}");
  });
}

// ------------------------------------------------------------------ köprü istemcisi
struct Cmd { char path[320]; };
static QueueHandle_t cmdQ;
static char names[24][32];
static int nameCount = 0;

static String enc(const String& s) {
  String o;
  char b[4];
  for (unsigned i = 0; i < s.length(); i++) {
    uint8_t c = s[i];
    if (isalnum(c) || c == '-' || c == '_' || c == '.') o += (char)c;
    else { snprintf(b, sizeof(b), "%%%02X", c); o += b; }
  }
  return o;
}

static void send(const String& path) {
  if (!cmdQ || path.length() >= sizeof(Cmd::path)) return;
  Cmd c{};
  strlcpy(c.path, path.c_str(), sizeof(c.path));
  xQueueSend(cmdQ, &c, 0);
}

void efektUret(const String& p) { send("/uret?prompt=" + enc(p)); }
void efektOynat(const String& ad) { send("/oynat?ad=" + enc(ad)); }
void efektKaydet(const String& ad) { send("/kaydet?ad=" + enc(ad)); }
void efektSil(const String& ad) { send("/sil?ad=" + enc(ad)); }
void efektAyar(float hiz, float isik) { send("/ayar?hiz=" + String(hiz, 2) + "&isik=" + String(isik, 2)); }
void efektDurdur() { send("/durdur"); }

static String apiKey;
static volatile bool keyPush = false;  // anahtar köprüye iletilecek
bool efektAnahtarVar() { return apiKey.length() > 0; }
void efektAnahtar(const String& k) {
  apiKey = k;
  apiKey.trim();
  Preferences p;
  p.begin("efekt", false);
  p.putString("gk", apiKey);
  p.end();
  keyPush = true;
}

int efektListe(char out[][32], int max) {
  xSemaphoreTake(mtx, portMAX_DELAY);
  int n = min(nameCount, max);
  for (int i = 0; i < n; i++) strlcpy(out[i], names[i], 32);
  xSemaphoreGive(mtx);
  return n;
}

static bool httpGet(const String& path, String* body) {
  String base, key;
  xSemaphoreTake(mtx, portMAX_DELAY);
  base = bridgeBase;
  key = bridgeKey;
  xSemaphoreGive(mtx);
  if (!base.length()) return false;
  HTTPClient http;
  http.setConnectTimeout(1200);
  http.setTimeout(2500);
  String url = base + path + (path.indexOf('?') >= 0 ? "&" : "?") + "k=" + key + "&ip=" + WiFi.localIP().toString();
  if (!http.begin(url)) return false;
  int code = http.GET();
  if (code == 200 && body) *body = http.getString();
  http.end();
  return code == 200;
}

static void pollDurum() {
  String body;
  if (!httpGet("/durum", &body)) { efektSt.ok = false; return; }
  JsonDocument d;
  if (deserializeJson(d, body)) { efektSt.ok = false; return; }
  static uint32_t lastHash = 0;
  xSemaphoreTake(mtx, portMAX_DELAY);
  int n = 0;
  uint32_t h = 5381;
  for (JsonVariant v : d["liste"].as<JsonArray>()) {
    if (n >= 24) break;
    strlcpy(names[n], v | "", sizeof(names[n]));
    for (const char* c = names[n]; *c; c++) h = h * 33 + (uint8_t)*c;
    n++;
  }
  if (n != nameCount || h != lastHash) { nameCount = n; lastHash = h; efektSt.listeSurum++; }
  xSemaphoreGive(mtx);
  strlcpy(efektSt.durum, d["durum"] | "", sizeof(efektSt.durum));
  strlcpy(efektSt.ad, d["ad"] | "", sizeof(efektSt.ad));
  if (!efektSt.ok && apiKey.length()) keyPush = true;  // köprü yeniden göründü: anahtarı tekrar ilet
  efektSt.ok = true;
}

static void bridgeTask(void*) {
  uint32_t last = 0;
  for (;;) {
    if (WiFi.status() != WL_CONNECTED) { efektSt.ok = false; vTaskDelay(pdMS_TO_TICKS(1000)); continue; }
    Cmd c;
    bool did = false;
    while (xQueueReceive(cmdQ, &c, 0) == pdTRUE) { httpGet(String(c.path), nullptr); did = true; }
    if (keyPush && apiKey.length() && httpGet("/anahtar?g=" + enc(apiKey), nullptr)) keyPush = false;
    if (did || millis() - last >= 3000) {
      last = millis();
      pollDurum();
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// ------------------------------------------------------------------ kare alıcı (DDP benzeri UDP: 10 bayt başlık + RGB565)
static uint16_t *fbuf, *fback;  // PSRAM, KW*KW piksel: ekranda gösterilen / UDP'nin doldurduğu

static void udpTask(void*) {
  while (WiFi.status() != WL_CONNECTED) vTaskDelay(pdMS_TO_TICKS(500));
  WiFiUDP udp;
  udp.begin(UDP_PORT);
  static uint8_t pk[1500];
  uint32_t got = 0;  // bu karede alinan bayt
  const uint32_t expect = KW * KH * 2;
  for (;;) {
    int n = udp.parsePacket();
    if (n <= 10) { vTaskDelay(1); continue; }  // paket yoksa 1 tik; varsa bekleme YOK (kuyruk dolmasin)
    n = udp.read(pk, sizeof(pk));
    pktCnt++;
    uint32_t off = (uint32_t)pk[4] << 24 | (uint32_t)pk[5] << 16 | (uint32_t)pk[6] << 8 | pk[7];
    uint32_t len = (uint32_t)pk[8] << 8 | pk[9];
    if (len > (uint32_t)n - 10) len = n - 10;
    if (off + len <= (uint32_t)KW * KW * 2) { memcpy((uint8_t*)fback + off, pk + 10, len); got += len; }
    if (pk[0] & 1) {  // push bayragi: kare sonu
      if (got >= expect) {            // TAM kare: gosterilecek tampona al (yarim kare asla gosterilmez)
        memcpy(fbuf, fback, KW * KW * 2);
        frameCnt++;
      } else dropCnt++;               // eksik kare: at (onceki kare ekranda kalir)
      got = 0;
    }
  }
}

// ------------------------------------------------------------------ tam ekran gösterim
// LVGL yalnızca siyah katmanı + mesajı çizer; kareler LVGL'siz, doğrudan ekrana 5x büyütülerek basılır
// (LVGL'nin ölçekli resim çizimi yavaş ve şerit şerit akıyordu).
static lv_obj_t *ov, *msg;
static bool shown = false;
static uint32_t shownCnt = 0, openedAt = 0, drawAfter = 0;
static uint16_t* blk;  // 5 satırlık blok (internal RAM)

#include "display.h"

static void drawFrame() {
  if (!uiIsLandscape()) return;  // efekt yalnizca yatay yonde (dikeyde mesaj gosterilir)
  const int w = KW, h = KH;
  uint16_t* hid = displayHidden();
  bool flip = uiRotation() == 2;
  for (int y = 0; y < h; y++) {
    const uint16_t* src = fbuf + y * w;
    uint16_t* row = blk;
    if (!flip) {
      for (int x = 0; x < w; x++) {
        uint16_t c = src[x];
        row[0] = row[1] = row[2] = row[3] = row[4] = c;
        row += 5;
      }
    } else {
      for (int x = w - 1; x >= 0; x--) {
        uint16_t c = src[x];
        row[0] = row[1] = row[2] = row[3] = row[4] = c;
        row += 5;
      }
    }
    for (int k = 1; k < 5; k++) memcpy(blk + k * 800, blk, 800 * 2);
    int py = flip ? 480 - (y + 1) * 5 : y * 5;
    memcpy(hid + (size_t)py * 800, blk, 800 * 2 * 5);  // 5 satir art arda
  }
  displayFlip(hid);
}

static void onOvClick(lv_event_t*) { efektEkranKapat(); }

static void ovTick(lv_timer_t*) {
  if (devOpenReq) { devOpenReq = false; efektEkranAc(); }
  if (devCloseReq) { devCloseReq = false; if (shown) { efektEkranKapat(); } }
  if (!shown) return;
  if (!uiIsLandscape()) {  // dikeyde efekt gosterilmez
    lv_label_set_text(msg, "Efekt için ekranı yatay çevirin\n(dokun: çık)");
    lv_obj_remove_flag(msg, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  if (frameCnt != shownCnt) {
    if (shownCnt == 0) {  // ilk kare: mesajı kaldır, LVGL'nin son yeniden çizimini bekle
      lv_obj_add_flag(msg, LV_OBJ_FLAG_HIDDEN);
      drawAfter = millis() + 120;
    }
    shownCnt = frameCnt;
    if (millis() >= drawAfter) { uint32_t t0 = micros(); drawFrame(); drawUs = micros() - t0; }
  } else if (frameCnt == 0 && millis() - openedAt > 6000) {
    lv_label_set_text(msg, efektSt.ok ? "Kare gelmiyor\n(dokun: çık)" : "PC köprüsü bulunamadı\n(dokun: çık)");
    lv_obj_remove_flag(msg, LV_OBJ_FLAG_HIDDEN);
  }
}

void efektEkranAc() {
  if (shown) return;
  shown = true;
  openedAt = millis();
  frameCnt = 0;
  shownCnt = 0;
  memset(fbuf, 0, KW * KW * 2);
  lv_label_set_text(msg, "Yapay zeka efekti hazırlanıyor…");
  lv_obj_remove_flag(msg, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(ov, LV_OBJ_FLAG_HIDDEN);
}

void efektEkranKapat() {
  if (!shown) return;
  shown = false;
  lv_obj_add_flag(ov, LV_OBJ_FLAG_HIDDEN);
  lv_obj_invalidate(lv_screen_active());
  efektDurdur();
}

void efektStart() {
  static bool started = false;
  if (started) return;
  started = true;
  mtx = xSemaphoreCreateMutex();
  fbuf = (uint16_t*)heap_caps_calloc(KW * KW, 2, MALLOC_CAP_SPIRAM);
  fback = (uint16_t*)heap_caps_calloc(KW * KW, 2, MALLOC_CAP_SPIRAM);
  Preferences p;
  p.begin("efekt", true);
  String u = p.getString("u", "");
  p.end();
  if (u.length()) setBridge(u, false);
  p.begin("efekt", true);
  apiKey = p.getString("gk", "");
  p.end();
  keyPush = apiKey.length() > 0;

  blk = (uint16_t*)heap_caps_malloc(800 * 5 * 2, MALLOC_CAP_INTERNAL);
  ov = lv_obj_create(lv_layer_top());
  lv_obj_set_size(ov, LV_PCT(100), LV_PCT(100));
  lv_obj_set_pos(ov, 0, 0);
  lv_obj_set_style_bg_color(ov, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(ov, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(ov, 0, 0);
  lv_obj_set_style_radius(ov, 0, 0);
  lv_obj_set_style_pad_all(ov, 0, 0);
  lv_obj_remove_flag(ov, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(ov, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(ov, onOvClick, LV_EVENT_CLICKED, nullptr);
  msg = uiLabel(ov, "", &noto28, C_TEXT);
  lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(msg);
  lv_obj_remove_flag(msg, LV_OBJ_FLAG_CLICKABLE);
  lv_timer_create(ovTick, 10, nullptr);

  cmdQ = xQueueCreate(8, sizeof(Cmd));
  xTaskCreatePinnedToCore(bridgeTask, "EfektKopru", 8192, nullptr, 1, nullptr, 0);
  xTaskCreatePinnedToCore(udpTask, "EfektUdp", 4096, nullptr, 2, nullptr, 0);
}
