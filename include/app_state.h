#pragma once
// Gorevler (Status/Weather/ProxyWorker/Display) ve web handler'lari arasinda
// paylasilan durum. Tanimlari app_state.cpp'de.
#include <Arduino.h>
#include <ArduinoJson.h>
#include "config.h"


// setForceRefresh(): durum gorevini 10sn beklemeden hemen uyandirir
extern volatile bool forceStatusRefresh;
inline void setForceRefresh()   { forceStatusRefresh = true; }
inline void clearForceRefresh() { forceStatusRefresh = false; }

// Cihaz listesi [{name, ip}] - devices.json
extern String DEVICE_LIST_JSON;
extern DynamicJsonDocument doc;

// Cihazlardan okunan anlik durum (isim -> /status JSON)
extern StaticJsonDocument<4096> CURRENT_STATUS_DOC;
extern SemaphoreHandle_t statusMutex;
// Cihazin role sayisi (status "relays"; eski/yanitsiz cihaz = 2), 1..MAX_RELAYS
int deviceRelayCount(const String& name);

// Zamanlayici + gundogumu/batimi ayarlari
extern StaticJsonDocument<2048> SCHEDULE_CONFIG_DOC;
extern SemaphoreHandle_t scheduleMutex;
extern String sunriseAction[MAX_DEVICES][MAX_RELAYS];  // "", "ON", "OFF"
extern String sunsetAction[MAX_DEVICES][MAX_RELAYS];

// Hava durumu verisi + ayarlari
extern StaticJsonDocument<2048> CURRENT_WEATHER_DOC;
// 512 -> 1024: "targets" (coklu cihaz+role secimi) dizisi ekstra yer istiyor
extern StaticJsonDocument<1024> WEATHER_CONFIG_DOC;
extern SemaphoreHandle_t weatherMutex;
// Web handler'i blocking HTTPS yapmasin diye yenilemeyi hava gorevine devreder
extern volatile bool weatherRefreshRequested;

// Otomasyon durumu
extern unsigned long lastManualControlAction;
extern unsigned long lastWeatherControlAction;
// [cihaz index][role-1] -> son gonderilen "ON"/"OFF"/"" (bkz. sunriseAction)
extern String lastWeatherDesired[MAX_DEVICES][MAX_RELAYS];

// Ekran/haber
extern volatile bool weatherDisplayDirty;
extern String NEWS_TICKER_TEXT;
extern SemaphoreHandle_t newsMutex;
