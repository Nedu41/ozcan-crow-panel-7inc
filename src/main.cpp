// CrowPanel HUB: ev-lamba "Merkezi HUB" + LVGL dokunmatik arayüz.
// Akış: guiInit -> uiStart (kayıtlı ağlara bağlan / dokunmatik WiFi kurulumu) -> hubOnWifiUp (hizmetler).
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <WiFi.h>

#include "alerts.h"
#include "automation.h"
#include "app_state.h"
#include "data_task.h"
#include "devices.h"
#include "events.h"
#include "hub.h"
#include "mqtt_link.h"
#include "storage.h"
#include "telegram.h"
#include "ui.h"
#include "weather.h"
#include "web_server.h"
#include "ses.h"
#include "reminders.h"
#include "mutfak.h"

static bool hubUp = false;     // hizmetler baslatildi mi (hubStartTask tetiklendi)
static volatile bool hubReady = false;  // hizmetler hazir: loop() onlari calistirabilir

// Hizmetleri ayri gorevde baslat: NTP/mDNS/web kurulumu saniyelerce surebilir; LVGL dongusunde calisirsa ekran donuyordu
static void hubStartTask(void*) {
  Serial.println("WiFi bağlandı. IP: " + WiFi.localIP().toString());
  if (MDNS.begin("crowhub")) Serial.println("mDNS: crowhub.local");
  ArduinoOTA.setHostname("crowhub");
  ArduinoOTA.begin();
  forceNtpSync();

  xTaskCreatePinnedToCore(updateAllDeviceStatuses, "Status", 8192, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(updateWeatherAndControlTask, "Weather", 8192, NULL, 1, NULL, 0);
  startDataTask();
  setupWebServer();
  hubReady = true;

#ifndef DEV_SCREENSHOT  // geliştirme imajında her yüklemede mesaj gitmesin
  vTaskDelay(pdMS_TO_TICKS(3000));
  sendTelegramAlert(String("🚀 *CrowPanel HUB başlatıldı*\n📶 IP: ") + WiFi.localIP().toString() + "\n📡 MAC: " + WiFi.macAddress());
#endif
  vTaskDelete(nullptr);
}

void hubOnWifiUp() {
  if (hubUp) {
    MDNS.end();
    MDNS.begin("crowhub");  // ağ değişti: mDNS'i yeniden duyur
    return;
  }
  hubUp = true;
  xTaskCreatePinnedToCore(hubStartTask, "HubUp", 8192, NULL, 1, NULL, 0);
}

void setup() {
  Serial.begin(115200);
  Serial.printf("PSRAM: %s, %u bayt\n", psramFound() ? "var" : "YOK", ESP.getPsramSize());
  if (!psramFound()) { Serial.println("PSRAM yok: ekran baslatilmiyor"); return; }

  statusMutex = xSemaphoreCreateMutex();
  weatherMutex = xSemaphoreCreateMutex();
  newsMutex = xSemaphoreCreateMutex();
  initEvents();

  if (!LittleFS.begin(true)) {  // true: bozuksa biçimlendir
    Serial.println("LittleFS baslatilamadi, yeniden baslatiliyor");
    delay(3000);
    ESP.restart();
  }
  alertsInit();
  remindersInit();
  sesInit();  // I2S hoparlor (belgeden pinler)
  sesCal(SES_ACILIS);  // varsayilan kapali
  loadDevices();
  loadWeatherConfig();
  loadScheduleConfig();
  mqttSetup();

  guiInit();
  uiStart();
}

// Hatirlatici (5 sn'de bir) + mutfak timer'i. CrowPanel ekraninda yumurta gorunumu yok: hazir olunca uyari bandi.
static void kitchenTick() {
  static uint32_t lastRm = 0;
  if (millis() - lastRm >= 5000) { lastRm = millis(); remindersTick(); }
  mtTick();
  static bool lastDone = false; static String mtId;
  bool d = mtOn() && mtDone();
  if (d && !lastDone) { mtId = "mt:" + String(millis()); alertsRaise(mtId, 2, "YUMURTA HAZIR", String(MT_AD[mtHedef()]) + " - afiyet olsun"); }
  if (!d && lastDone && mtId.length()) { alertsAck(mtId); mtId = ""; }   // sifirlaninca bant kalkar
  lastDone = d;
}

void loop() {
  guiLoop();
  kitchenTick();
  if (hubReady) {
    webLoop();
    ArduinoOTA.handle();
    mqttLoop();
  }
  delay(5);
}
