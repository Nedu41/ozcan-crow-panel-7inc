#include "mqtt_link.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"
#include "app_state.h"
#include "devices.h"
#include "events.h"

WiFiClient espClient;
PubSubClient mqttClient(espClient);

String mqttClientID = "CrowHub_" + String((uint32_t)ESP.getEfuseMac(), HEX);

String topicRelay1Set = "crowhub/relay/1/set";
String topicRelay2Set = "crowhub/relay/2/set";
String topicRelay1State = "crowhub/relay/1/state";
String topicRelay2State = "crowhub/relay/2/state";

// ESP01 v2.5 olaylari: esp01/<mac>/event {"relay":1,"state":1,"src":"zaman"}.
// Sadece listedeki cihazlar (genel broker'da yabancilarin olaylari da gelir).
// HUB'in kendi gonderdigi komutlar zaten kayitli - onlarin ESP01 yankisi atlanir.
static void handleDeviceEvent(const String& topic, const String& msg) {
  int a = topic.indexOf('/'), b = topic.lastIndexOf('/');
  String mac = normMac(topic.substring(a + 1, b));
  if (!mac.length()) return;
  String name = "";
  for (JsonObject d : doc.as<JsonArray>()) {
    if ((d["mac"] | String("")) == mac) { name = d["name"] | ""; break; }
  }
  if (!name.length()) return;

  JsonDocument ev;
  if (deserializeJson(ev, msg)) return;
  String src = ev["src"] | "";
  if (src == "gunbatimi" || src == "gundogumu" || src == "bulut" || src == "bulut-geri" || src == "web") return;
  addEvent(name, ev["relay"] | 0, (ev["state"] | 0) == 1, src, true);
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg;
  msg.reserve(length);
  for (unsigned int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }

  String t = topic;
  if (t.startsWith("esp01/") && t.endsWith("/event")) {
    handleDeviceEvent(t, msg);
    return;
  }

  // RÖLE 1
  // NOT: Sunucu kartında fiziksel röle pin bağlantısı yoktur;
  //      röle kontrolü ESP01 cihazlara HTTP ile iletilir.
  //      Bu callback lokal LED/test amaçlıdır.
  if (String(topic) == topicRelay1Set) {
    if (msg == "1" || msg == "ON") {
      // Relay 1 -> HTTP üzerinden gönder (lokal pin yok)
      mqttClient.publish(topicRelay1State.c_str(), "1", true);
      Serial.println("[MQTT] Relay1 ON komutu alındı");
    } else if (msg == "0" || msg == "OFF") {
      mqttClient.publish(topicRelay1State.c_str(), "0", true);
      Serial.println("[MQTT] Relay1 OFF komutu alındı");
    }
  }

  // RÖLE 2
  if (String(topic) == topicRelay2Set) {
    if (msg == "1" || msg == "ON") {
      mqttClient.publish(topicRelay2State.c_str(), "1", true);
      Serial.println("[MQTT] Relay2 ON komutu alındı");
    } else if (msg == "0" || msg == "OFF") {
      mqttClient.publish(topicRelay2State.c_str(), "0", true);
      Serial.println("[MQTT] Relay2 OFF komutu alındı");
    }
  }
}

void reconnectMQTT() {
  static unsigned long lastTry = 0;
  if (millis() - lastTry < 3000) return;
  lastTry = millis();

  if (mqttClient.connect(mqttClientID.c_str())) {
    mqttClient.subscribe(topicRelay1Set.c_str());
    mqttClient.subscribe(topicRelay2Set.c_str());
    mqttClient.subscribe("esp01/+/event");
  }
}

void mqttSetup() {
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
}

void mqttLoop() {
  if (!mqttClient.connected()) {
    reconnectMQTT();
  } else {
    mqttClient.loop();  // Sadece bagliysa loop calissin
  }
}
