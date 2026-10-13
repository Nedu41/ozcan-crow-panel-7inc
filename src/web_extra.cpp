// Web arayüzü için ek uçlar: haber, sistem (ekran parlaklığı/yön, Telegram, bilgi).
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>

#include "app_state.h"
#include "config.h"
#include "efekt.h"
#include "guncelle.h"
#include "sky.h"
#include "telegram.h"
#include "ui.h"
#include "ui_common.h"

extern AsyncWebServer server;
extern volatile bool devRotReq;  // ui_hub.cpp: ekran yönünü LVGL döngüsünde değiştirir

void webExtraRoutes() {
  guncelleWebKur(server);  // /update (dosya), /api/guncelle/*
  // Canli gokyuzu karesi (8 bit paletli BMP, ~38 KB); web Ana sayfa ~1 sn'de bir ceker
  server.on("/api/sky.bmp", AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest* r) {
    skyStepIfStale(400);
    AsyncWebServerResponse* res = r->beginChunkedResponse("image/bmp", [](uint8_t* buf, size_t maxLen, size_t index) -> size_t {
      return skyBmpRead(buf, maxLen, index);
    });
    res->addHeader("Cache-Control", "no-store");
    r->send(res);
  });

  server.on("/api/news", AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest* r) {
    String t;
    if (newsMutex && xSemaphoreTake(newsMutex, pdMS_TO_TICKS(50)) == pdTRUE) { t = NEWS_TICKER_TEXT; xSemaphoreGive(newsMutex); }
    JsonDocument d;
    JsonArray a = d["items"].to<JsonArray>();
    static const char* SEP = "     +++     ";
    int start = 0;
    while (start < (int)t.length()) {
      int e = t.indexOf(SEP, start);
      String one = e < 0 ? t.substring(start) : t.substring(start, e);
      one.trim();
      if (one.length()) a.add(one);
      if (e < 0) break;
      start = e + strlen(SEP);
    }
    String o;
    serializeJson(d, o);
    r->send(200, "application/json", o);
  });

  server.on("/api/sistem/set", AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest* r) {
    if (r->hasParam("bl")) uiSetBacklight(r->getParam("bl")->value().toInt());
    if (r->hasParam("sa")) uiSetSaturation(r->getParam("sa")->value().toInt(), true);
    if (r->hasParam("ct") || r->hasParam("dg"))
      uiSetColorAdjust(r->hasParam("ct") ? r->getParam("ct")->value().toInt() : uiGetContrast(),
                       r->hasParam("dg") ? r->getParam("dg")->value().toInt() : uiGetGain(), true);
    r->send(200, "text/plain", "OK");
  });
  server.on("/api/sistem/rot", AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest* r) {
    devRotReq = true;
    r->send(200, "text/plain", "OK");
  });
  // NOT: genel yol ("/api/sistem") alt yollardan SONRA kaydedilir (aksi halde /api/sistem/set onu gölgelerdi)
  server.on("/api/sistem", AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest* r) {
    JsonDocument d;
    d["bl"] = uiGetBacklight();
    d["ct"] = uiGetContrast();
    d["dg"] = uiGetGain();
    d["sa"] = uiGetSaturation();
    d["land"] = uiIsLandscape();
    d["tg"] = telegramConfigured();
    d["tg_chat"] = telegramChatId();
    d["ai"] = efektAnahtarVar();
    d["fw"] = FW_VERSION;
    d["ssid"] = WiFi.SSID();
    d["rssi"] = WiFi.RSSI();
    d["ip"] = WiFi.localIP().toString();
    d["up"] = millis() / 1000;
    d["heap"] = ESP.getFreeHeap();
    String o;
    serializeJson(d, o);
    r->send(200, "application/json", o);
  });
  // Telegram: token boşsa mevcut korunur (yalnızca sohbet no değiştirilebilir)
  server.on("/api/telegram", AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest* r) {
    String tok = r->hasParam("token") ? r->getParam("token")->value() : "";
    String chat = r->hasParam("chat") ? r->getParam("chat")->value() : telegramChatId();
    tok.trim();
    chat.trim();
    if (tok.length()) telegramSetConfig(tok, chat);
    r->send(200, "text/plain", "OK");
  });
}
