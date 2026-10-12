#include "wifi_nets.h"
#include <WiFi.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

#define WIFI_NETS_FILE "/wifi_nets.json"
static const int WIFI_NETS_MAX = 6;
static const uint32_t WIFI_TRY_MS = 10000;  // ag basina deneme suresi

static JsonDocument loadNets() {
  JsonDocument d;
  File f = LittleFS.open(WIFI_NETS_FILE, "r");
  if (!f || deserializeJson(d, f) || !d.is<JsonArray>()) d.to<JsonArray>();
  if (f) f.close();
  return d;
}

static bool saveNets(JsonDocument& d) {
  File f = LittleFS.open(WIFI_NETS_FILE, "w");
  if (!f) return false;
  serializeJson(d, f);
  f.close();
  return true;
}

// Kayitli ag listesi erisimcisi (arayuz acilista siradan dener)
int wifiNetsCount() {
  JsonDocument d = loadNets();
  return d.as<JsonArray>().size();
}

bool wifiNetsGet(int idx, String& ssid, String& pass) {
  JsonDocument d = loadNets();
  JsonArray a = d.as<JsonArray>();
  if (idx < 0 || idx >= (int)a.size()) return false;
  ssid = (const char*)(a[idx]["ssid"] | "");
  pass = (const char*)(a[idx]["pass"] | "");
  return ssid.length() > 0;
}

String wifiNetsInfoJson() {
  JsonDocument out;
  bool conn = WiFi.status() == WL_CONNECTED;
  out["ssid"] = conn ? WiFi.SSID() : "";
  out["rssi"] = conn ? WiFi.RSSI() : 0;
  out["ip"] = conn ? WiFi.localIP().toString() : "";
  JsonArray saved = out["saved"].to<JsonArray>();
  JsonDocument d = loadNets();
  int i = 0;
  for (JsonObject n : d.as<JsonArray>()) {
    JsonObject o = saved.add<JsonObject>();
    o["idx"] = i++;
    o["ssid"] = n["ssid"] | "";
  }
  String s;
  serializeJson(out, s);
  return s;
}

bool wifiNetsAdd(const String& ssid, const String& pass) {
  if (!ssid.length() || ssid.length() > 32 || pass.length() > 63) return false;
  JsonDocument d = loadNets();
  JsonArray a = d.as<JsonArray>();
  for (JsonObject n : a) {
    if (ssid == (n["ssid"] | "")) {
      n["pass"] = pass;
      return saveNets(d);
    }
  }
  if ((int)a.size() >= WIFI_NETS_MAX) return false;
  JsonObject n = a.add<JsonObject>();
  n["ssid"] = ssid;
  n["pass"] = pass;
  return saveNets(d);
}

bool wifiNetsDelete(int idx) {
  JsonDocument d = loadNets();
  JsonArray a = d.as<JsonArray>();
  if (idx < 0 || idx >= (int)a.size()) return false;
  a.remove(idx);
  return saveNets(d);
}

void wifiNetsClear() {
  LittleFS.remove(WIFI_NETS_FILE);
}

bool wifiNetsMakePreferred(int idx) {
  JsonDocument d = loadNets();
  JsonArray a = d.as<JsonArray>();
  if (idx < 0 || idx >= (int)a.size()) return false;
  JsonDocument out;
  JsonArray b = out.to<JsonArray>();
  b.add(a[idx]);
  for (int i = 0; i < (int)a.size(); i++)
    if (i != idx) b.add(a[i]);
  return saveNets(out);
}
