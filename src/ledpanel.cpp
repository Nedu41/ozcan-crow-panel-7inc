#include "ledpanel.h"

#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>

LedState ledState;

struct LedCmd { char path[96]; };
static QueueHandle_t cmdQ;
static String manualHost;
static String resolved;  // kullanılan adres (IP)
static volatile bool resolveNow = false;

String ledpanelHost() { return manualHost; }

void ledpanelSetHost(const String& host) {
  manualHost = host;
  manualHost.trim();
  Preferences p;
  p.begin("led", false);
  p.putString("host", manualHost);
  p.end();
  resolved = "";
  resolveNow = true;
}

void ledpanelSend(const String& pathQuery) {
  if (!cmdQ || pathQuery.length() >= sizeof(LedCmd::path)) return;
  LedCmd c{};
  strlcpy(c.path, pathQuery.c_str(), sizeof(c.path));
  xQueueSend(cmdQ, &c, 0);
}

static bool resolveHost() {
  if (manualHost.length()) {
    IPAddress ip;
    if (ip.fromString(manualHost)) { resolved = manualHost; return true; }
    // ad girildiyse (ör. ledpanel.local) mDNS ile çöz
    String n = manualHost;
    n.replace(".local", "");
    IPAddress r = MDNS.queryHost(n, 2000);
    if (r != IPAddress((uint32_t)0)) { resolved = r.toString(); return true; }
    return false;
  }
  IPAddress r = MDNS.queryHost("ledpanel", 2000);
  if (r == IPAddress((uint32_t)0)) return false;
  resolved = r.toString();
  return true;
}

static bool httpGet(const String& path, String* body) {
  HTTPClient http;
  http.setConnectTimeout(1200);
  http.setTimeout(2000);
  if (!http.begin("http://" + resolved + path)) return false;
  int code = http.GET();
  if (code == 200 && body) *body = http.getString();
  http.end();
  return code == 200;
}

static void pollState() {
  String body;
  if (!httpGet("/api/state", &body)) { ledState.ok = false; return; }
  JsonDocument filter;
  filter["pat"] = true; filter["bp"] = true; filter["fxs"] = true; filter["rot"] = true; filter["fps"] = true;
  JsonDocument d;
  if (deserializeJson(d, body, DeserializationOption::Filter(filter))) { ledState.ok = false; return; }
  ledState.pat = d["pat"] | -1;
  ledState.bp = d["bp"] | 0;
  ledState.fxs = d["fxs"] | 0;
  ledState.rot = d["rot"] | 0;
  ledState.fps = d["fps"] | 0;
  strlcpy(ledState.ip, resolved.c_str(), sizeof(ledState.ip));
  ledState.ok = true;
}

static void ledTask(void*) {
  uint32_t lastPoll = 0;
  uint8_t fails = 0;
  for (;;) {
    if (WiFi.status() != WL_CONNECTED) { ledState.ok = false; vTaskDelay(pdMS_TO_TICKS(1000)); continue; }
    if (resolved.length() == 0 || resolveNow || fails >= 3) {
      resolveNow = false;
      if (!resolveHost()) { ledState.ok = false; vTaskDelay(pdMS_TO_TICKS(5000)); continue; }
      fails = 0;
    }
    LedCmd c;
    while (xQueueReceive(cmdQ, &c, 0) == pdTRUE) {
      String body;
      if (httpGet(String(c.path), &body)) fails = 0;
      lastPoll = 0;  // sonucu hemen oku
    }
    if (millis() - lastPoll >= 3000) {
      lastPoll = millis();
      pollState();
      fails = ledState.ok ? 0 : fails + 1;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void ledpanelStart() {
  static bool started = false;
  if (started) return;
  started = true;
  Preferences p;
  p.begin("led", false);
  manualHost = p.getString("host", "");
  p.end();
  cmdQ = xQueueCreate(8, sizeof(LedCmd));
  xTaskCreatePinnedToCore(ledTask, "LedPanel", 8192, nullptr, 1, nullptr, 0);
}
