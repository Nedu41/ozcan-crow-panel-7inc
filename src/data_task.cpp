// Haber / hava / deprem çekme görevi. ev-lamba'da bunlar TFT sprite belleği yüzünden
// displayTask içindeydi; burada 8 MB PSRAM var, ayrı görev yeterli.
#include "data_task.h"

#include <WiFi.h>

#include "alerts.h"
#include "app_state.h"
#include "news.h"
#include "weather.h"

static void dataTask(void*) {
  uint32_t lastNews = 0, lastWx = 0, lastEq = 0;
  bool newsOk = false, eqOk = false, first = true;
  // deprem ilk sorgusu açılıştan ~60 sn sonra (haber+hava ile üst üste binmesin)
  lastEq = millis() - (ALERT_EQ_INTERVAL_MS - 60000UL);
  for (;;) {
    uint32_t now = millis();
    if (WiFi.status() == WL_CONNECTED) {
      if (first || weatherRefreshRequested || now - lastWx >= 600000UL) {
        weatherRefreshRequested = false;
        lastWx = now;
        forceNtpSync();
        updateWeatherFromConfig();
      }
      if (first || now - lastNews >= (newsOk ? NEWS_UPDATE_INTERVAL : 30000UL)) {
        lastNews = now;
        newsOk = fetchNews();
      }
      if (now - lastEq >= (eqOk ? ALERT_EQ_INTERVAL_MS : 120000UL)) {
        lastEq = now;
        eqOk = fetchEarthquakes();
      }
      first = false;
    }
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

void startDataTask() { xTaskCreatePinnedToCore(dataTask, "Data", 16384, nullptr, 1, nullptr, 0); }
