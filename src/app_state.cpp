#include "app_state.h"

volatile bool forceStatusRefresh = false;

String DEVICE_LIST_JSON = "[]";
DynamicJsonDocument doc(2048);

StaticJsonDocument<4096> CURRENT_STATUS_DOC;
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
