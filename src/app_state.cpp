#include <map>
#include "app_state.h"

volatile bool forceStatusRefresh = false;

String DEVICE_LIST_JSON = "[]";
DynamicJsonDocument doc(2048);

StaticJsonDocument<4096> CURRENT_STATUS_DOC;

// Cevrimdisi cihazin status JSON'unda "relays" yok: son bilinen sayi hatirlanir (kart yeniden kurulmasin)
static std::map<String, int> relayCountCache;

int deviceRelayCount(const String& name) {
  int n = 0;
  if (statusMutex && xSemaphoreTake(statusMutex, pdMS_TO_TICKS(15)) == pdTRUE) {
    n = CURRENT_STATUS_DOC[name]["relays"] | 0;
    xSemaphoreGive(statusMutex);
  }
  if (n >= 1 && n <= MAX_RELAYS) {
    relayCountCache[name] = n;
    return n;
  }
  auto it = relayCountCache.find(name);
  return it != relayCountCache.end() ? it->second : 2;
}
SemaphoreHandle_t statusMutex = nullptr;

StaticJsonDocument<2048> SCHEDULE_CONFIG_DOC;
SemaphoreHandle_t scheduleMutex = nullptr;
String sunriseAction[MAX_DEVICES][MAX_RELAYS];
String sunsetAction[MAX_DEVICES][MAX_RELAYS];

StaticJsonDocument<2048> CURRENT_WEATHER_DOC;
StaticJsonDocument<1024> WEATHER_CONFIG_DOC;
SemaphoreHandle_t weatherMutex = nullptr;
volatile bool weatherRefreshRequested = false;

unsigned long lastManualControlAction = 0;
unsigned long lastWeatherControlAction = 0;
String lastWeatherDesired[MAX_DEVICES][MAX_RELAYS];

volatile bool weatherDisplayDirty = true;
String NEWS_TICKER_TEXT = "";
SemaphoreHandle_t newsMutex = nullptr;
