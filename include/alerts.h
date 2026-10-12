#pragma once
// Hava (firtina/ruzgar/yagis) ve deprem uyarilari. Onaylanana kadar TFT'de banner
// + web sayfasinda cubuk olarak kalir; onaylananlar kalici hatirlanir (yeniden cikmaz).
#include <Arduino.h>
#include <ArduinoJson.h>

struct Alert {
  String id;        // "wx:2026-10-01" / "eq:729915" - onay bununla tutulur
  uint8_t kind = 0; // 0 = hava, 1 = deprem
  String title;     // UTF-8 Turkce (TFT'de trAscii'den gecirilir)
  String detail;
  uint32_t since = 0;  // millis() - alt yazida ne kadar gorunecegi icin
};

void alertsInit();  // LittleFS acildiktan sonra

// Ayni id zaten onaylanmissa/varsa yeni kayit acmaz (varsa metni gunceller)
void alertsRaise(const String& id, uint8_t kind, const String& title, const String& detail);
bool alertsAck(const String& id);  // "*" = hepsi
int alertsSnapshot(Alert* out, int maxN);  // banner icin kopya
uint32_t alertsVersion();                   // her degisimde artar (web yayini icin)
String alertsJson();                        // {"type":"alerts","list":[...],"eq":"..."}
String alertsTickerText();                  // alt yazi on eki (son 60 dk), Turkce

// Hava demosunda ornek uyarilari gosterir/kaldirir (onay listesine yazilmaz)
void alertsDemo(bool on);

// Open-Meteo gunluk dizilerinden: indeks 1 = bugun, 2 = yarin (past_days=1)
void alertsEvaluateWeather(JsonArray times, JsonArray codes, JsonArray gusts, JsonArray precip);

// AFAD deprem sorgusu (blocking HTTPS/TLS - sadece displayTask'in
// withTlsSpritesFreed'i icinden cagrilir, bkz. display.cpp)
bool fetchEarthquakes();
