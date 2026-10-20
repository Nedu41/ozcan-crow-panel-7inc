#include "reminders.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <time.h>

#include "alerts.h"

static const char* FILE_PATH = "/reminders.json";
static JsonDocument doc;  // bellekteki kopya (kucuk: en cok ~30 kayit)
static SemaphoreHandle_t mtx = nullptr;

struct Lock {
  Lock() { xSemaphoreTake(mtx, portMAX_DELAY); }
  ~Lock() { xSemaphoreGive(mtx); }
};

void remindersInit() {
  mtx = xSemaphoreCreateMutex();
  File f = LittleFS.open(FILE_PATH, "r");
  if (f) {
    if (deserializeJson(doc, f) || !doc.is<JsonArray>()) doc.to<JsonArray>();
    f.close();
  } else {
    doc.to<JsonArray>();
  }
}

String remindersJson() {
  Lock l;
  String s;
  serializeJson(doc, s);
  return s;
}

bool remindersSave(const String& json) {
  if (json.length() > 6144) return false;
  JsonDocument d;
  if (deserializeJson(d, json) || !d.is<JsonArray>()) return false;
  File f = LittleFS.open(FILE_PATH, "w");
  if (!f) return false;
  f.print(json);
  f.close();
  Lock l;
  doc = d;
  return true;
}

void remindersTick() {
  struct tm t;
  if (!getLocalTime(&t, 0) || t.tm_year < 120) return;  // saat henuz senkron degil
  const int nowMin = t.tm_hour * 60 + t.tm_min;
  char today[11], ymd[9];
  strftime(today, sizeof(today), "%Y-%m-%d", &t);
  strftime(ymd, sizeof(ymd), "%Y%m%d", &t);

  // Gunde birden cok saat: "s": [[9,0],[14,0],[21,0]]; yoksa tek saat "h"/"m" (eski kayitlar).
  // Her saat ayri uyari (kimlik sonunda HHMM) - sabah onaylanan ilac aksam yine hatirlatilir.
  struct Due { String id, title, detail; };
  Due due[16];
  int n = 0;
  {
    Lock l;
    for (JsonObject r : doc.as<JsonArray>()) {
      if (!(r["on"] | true)) continue;
      String rep = r["r"] | "g";
      bool today_ok = rep == "g" ? true
                    : rep == "h" ? (((int)(r["w"] | 0) >> t.tm_wday) & 1)
                    : rep == "t" ? (String((const char*)(r["d"] | "")) == today) : false;
      if (!today_ok) continue;
      JsonArray slots = r["s"].as<JsonArray>();
      int ns = slots.isNull() ? 1 : slots.size();
      for (int k = 0; k < ns && n < 16; k++) {
        int h = slots.isNull() ? (r["h"] | 0) : (slots[k][0] | 0);
        int m = slots.isNull() ? (r["m"] | 0) : (slots[k][1] | 0);
        if (nowMin < h * 60 + m) continue;  // bugun bu saatin zamani henuz gelmedi
        char hm[6], hhmm[5];
        snprintf(hm, sizeof(hm), "%02d:%02d", h, m);
        snprintf(hhmm, sizeof(hhmm), "%02d%02d", h, m);
        String note = r["n"] | "";
        due[n++] = {"rm:" + String((int)(r["i"] | 0)) + ":" + ymd + ":" + hhmm,
                    String("HATIRLATMA: ") + (const char*)(r["t"] | ""), (note.length() ? note + " - " : String("")) + hm};
      }
    }
  }
  // alertsRaise ayni kimlikle yeni kayit acmaz, onaylanmissa hic acmaz - her turda cagirmak guvenli
  for (int k = 0; k < n; k++) alertsRaise(due[k].id, 2, due[k].title, due[k].detail);
}
