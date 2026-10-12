#include "events.h"
#include <ArduinoJson.h>
#include <time.h>
#include "config.h"

struct Ev {
  time_t t;
  char dev[24];
  uint8_t relay;
  bool on;
  char src[14];
  bool ok;
};

static const int MAX_EV = 50;
static Ev ring[MAX_EV];
static int head = 0, count = 0;
static SemaphoreHandle_t evMutex = nullptr;

void initEvents() {
  evMutex = xSemaphoreCreateMutex();
}

void addEvent(const String& device, int relay, bool on, const String& src, bool ok) {
  if (!evMutex || xSemaphoreTake(evMutex, MUTEX_TIMEOUT) != pdTRUE) return;
  Ev& e = ring[head];
  e.t = time(nullptr);
  strlcpy(e.dev, device.c_str(), sizeof(e.dev));
  e.relay = relay;
  e.on = on;
  strlcpy(e.src, src.c_str(), sizeof(e.src));
  e.ok = ok;
  head = (head + 1) % MAX_EV;
  if (count < MAX_EV) count++;
  xSemaphoreGive(evMutex);
  Serial.printf("[Olay] %s R%d %s (%s)%s\n", e.dev, relay, on ? "AC" : "KAPAT", e.src, ok ? "" : " BASARISIZ");
}

String eventsJson() {
  JsonDocument out;
  JsonArray arr = out.to<JsonArray>();
  if (evMutex && xSemaphoreTake(evMutex, MUTEX_TIMEOUT) == pdTRUE) {
    for (int i = 0; i < count; i++) {
      const Ev& e = ring[(head - 1 - i + MAX_EV) % MAX_EV];
      struct tm tmv;
      localtime_r(&e.t, &tmv);
      char ts[20];
      strftime(ts, sizeof(ts), "%d.%m %H:%M:%S", &tmv);
      JsonObject o = arr.add<JsonObject>();
      o["t"] = ts;
      o["dev"] = e.dev;
      o["relay"] = e.relay;
      o["on"] = e.on;
      o["src"] = e.src;
      o["ok"] = e.ok;
    }
    xSemaphoreGive(evMutex);
  }
  String s;
  serializeJson(out, s);
  return s;
}
