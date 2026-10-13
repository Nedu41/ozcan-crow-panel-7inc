#pragma once
#include <Arduino.h>

// AsyncWebServer rotalari + WebSocket (/ws) + ESP01 proxy worker gorevi
void setupWebServer();

// Periyodik isler (WS istemci temizligi, non-blocking LED) - loop()'tan cagrilir
void webLoop();

// Guncel durumu bagli tum WS istemcilerine push eder (en fazla 500ms'de bir;
// daha sik cagrilar ertelenir, wsFlushPending() ile gonderilir)
void wsBroadcastStatus();
void wsFlushPending();

// Zamanlayici (ekran): HUB kaydi + ESP01'e gonderim. days=0: saatli program pasif.
bool scheduleSave(const String& ip, int relay_id, const String& start_h, const String& start_m, const String& end_h,
                  const String& end_m, int days, const String& sunrise_act, const String& sunset_act);
bool scheduleApply(const String& ip, int relay, int sh, int sm, int eh, int em, int days, const String& sunrise, const String& sunset);
