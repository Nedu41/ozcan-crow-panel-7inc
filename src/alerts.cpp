#include "alerts.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <LittleFS.h>
#include <time.h>
#include <math.h>
#include "app_state.h"

// ============== Durum (alertsMutex ile korunur) ==============
static const int MAX_ALERTS = 6;
static const int MAX_ACKED = 24;
static Alert alerts[MAX_ALERTS];
static int alertCount = 0;
static String acked[MAX_ACKED];  // halka: en eskiler ezilir
static int ackedHead = 0;
static SemaphoreHandle_t alertsMutex = nullptr;
static volatile uint32_t version = 1;
static String eqState = "henuz denenmedi";  // uzaktan teshis (/alerts)

struct Lock {
  Lock() { xSemaphoreTake(alertsMutex, portMAX_DELAY); }
  ~Lock() { xSemaphoreGive(alertsMutex); }
};

static bool isAcked(const String& id) {
  for (int i = 0; i < MAX_ACKED; i++) if (acked[i] == id) return true;
  return false;
}

static void saveAcks() {
  String out = "[";
  {
    Lock l;
    bool first = true;
    for (int i = 0; i < MAX_ACKED; i++) {
      if (!acked[i].length()) continue;
      if (!first) out += ",";
      out += "\"" + acked[i] + "\"";
      first = false;
    }
  }
  out += "]";
  File f = LittleFS.open(ALERT_ACK_FILE, "w");
  if (f) { f.print(out); f.close(); }
}

void alertsInit() {
  alertsMutex = xSemaphoreCreateMutex();
  File f = LittleFS.open(ALERT_ACK_FILE, "r");
  if (!f) return;
  JsonDocument d;
  if (!deserializeJson(d, f)) {
    Lock l;
    for (JsonVariant v : d.as<JsonArray>()) {
      acked[ackedHead] = v.as<String>();
      ackedHead = (ackedHead + 1) % MAX_ACKED;
    }
  }
  f.close();
}

// "demo:" / "test:" kayitlari kalici onay listesine yazilmaz (tekrar gosterilebilsin)
static bool isTransient(const String& id) { return id.startsWith("demo:") || id.startsWith("test:"); }

static int findAlert(const String& id) {
  for (int i = 0; i < alertCount; i++) if (alerts[i].id == id) return i;
  return -1;
}

static void removeAt(int i) {
  for (int j = i; j < alertCount - 1; j++) alerts[j] = alerts[j + 1];
  alertCount--;
}

void alertsRaise(const String& id, uint8_t kind, const String& title, const String& detail) {
  Lock l;
  if (isAcked(id)) return;
  int i = findAlert(id);
  if (i >= 0) {
    if (alerts[i].detail != detail || alerts[i].title != title) {
      alerts[i].title = title;
      alerts[i].detail = detail;
      version++;
    }
    return;
  }
  if (alertCount >= MAX_ALERTS) removeAt(0);  // en eskiyi at
  Alert& a = alerts[alertCount++];
  a.id = id;
  a.kind = kind;
  a.title = title;
  a.detail = detail;
  a.since = millis();
  version++;
  Serial.printf("[Uyari] YENI: %s | %s | %s\n", id.c_str(), title.c_str(), detail.c_str());
}

bool alertsAck(const String& id) {
  bool changed = false;
  {
    Lock l;
    if (id == "*") {
      for (int i = 0; i < alertCount; i++) {
        if (isTransient(alerts[i].id)) continue;
        acked[ackedHead] = alerts[i].id;
        ackedHead = (ackedHead + 1) % MAX_ACKED;
      }
      changed = alertCount > 0;
      alertCount = 0;
    } else {
      int i = findAlert(id);
      if (i >= 0) {
        if (!isTransient(id)) {
          acked[ackedHead] = id;
          ackedHead = (ackedHead + 1) % MAX_ACKED;
        }
        removeAt(i);
        changed = true;
      }
    }
    if (changed) version++;
  }
  if (changed) saveAcks();
  return changed;
}

int alertsSnapshot(Alert* out, int maxN) {
  Lock l;
  int n = alertCount < maxN ? alertCount : maxN;
  for (int i = 0; i < n; i++) out[i] = alerts[i];
  return n;
}

uint32_t alertsVersion() { return version; }

String alertsJson() {
  JsonDocument d;
  d["type"] = "alerts";
  JsonArray arr = d["list"].to<JsonArray>();
  {
    Lock l;
    for (int i = 0; i < alertCount; i++) {
      JsonObject o = arr.add<JsonObject>();
      o["id"] = alerts[i].id;
      o["kind"] = alerts[i].kind;
      o["title"] = alerts[i].title;
      o["detail"] = alerts[i].detail;
    }
  }
  d["eq"] = eqState;
  String s;
  serializeJson(d, s);
  return s;
}

void alertsDemo(bool on) {
  if (on) {
    alertsRaise("demo:wx", 0, "HAVA UYARISI - YARIN (DEMO)", "rüzgar 62 km/s, yağış 30 mm, gök gürültülü fırtına");
    alertsRaise("demo:eq", 1, "YAKIN DEPREM M4.2 (DEMO)", "Örnek İlçe (Örnek İl) - 12 km - 30.09 14:20");
    return;
  }
  Lock l;
  for (int i = alertCount - 1; i >= 0; i--) {
    if (alerts[i].id.startsWith("demo:")) { removeAt(i); version++; }
  }
}

String alertsTickerText() {
  String t;
  Lock l;
  for (int i = 0; i < alertCount; i++) {
    if (millis() - alerts[i].since > ALERT_TICKER_MS) continue;
    t += "!!! " + alerts[i].title + ": " + alerts[i].detail + " !!!     +++     ";
  }
  return t;
}

// ============== Hava uyarilari ==============
// Open-Meteo gunluk tahmininden (ek HTTPS istegi yok - hava cagrisina 2 alan eklendi).
// NOT: resmi MGM uyarisi degil, model tahmininden turetilir.
void alertsEvaluateWeather(JsonArray times, JsonArray codes, JsonArray gusts, JsonArray precip) {
  struct Cand { String id, title, detail; };
  Cand cand[2];
  int n = 0;
  for (int i = 1; i <= 2 && i < (int)times.size(); i++) {
    float gust = gusts[i] | 0.0f;
    float rain = precip[i] | 0.0f;
    int code = codes[i] | -1;
    String why;
    if (gust >= ALERT_WIND_KMH) why += "rüzgar " + String((int)roundf(gust)) + " km/s";
    if (rain >= ALERT_RAIN_MM) { if (why.length()) why += ", "; why += "yağış " + String((int)roundf(rain)) + " mm"; }
    if (code == 95 || code == 96 || code == 99) { if (why.length()) why += ", "; why += code == 95 ? "gök gürültülü fırtına" : "dolulu fırtına"; }
    if (code == 75 || code == 86) { if (why.length()) why += ", "; why += "yoğun kar"; }
    if (!why.length()) continue;
    String date = times[i] | "";
    cand[n].id = "wx:" + date;
    cand[n].title = String("HAVA UYARISI - ") + (i == 1 ? "BUGÜN" : "YARIN");
    cand[n].detail = why;
    n++;
  }
  // Tahmin duzeldiyse/gecmisse onaylanmamis eski hava uyarilarini kaldir
  {
    Lock l;
    for (int i = alertCount - 1; i >= 0; i--) {
      if (alerts[i].kind != 0 || alerts[i].id.startsWith("test:")) continue;
      bool keep = false;
      for (int c = 0; c < n; c++) if (cand[c].id == alerts[i].id) keep = true;
      if (!keep) { removeAt(i); version++; }
    }
  }
  for (int c = 0; c < n; c++) alertsRaise(cand[c].id, 0, cand[c].title, cand[c].detail);
}

// ============== Deprem (AFAD) ==============
static double numOf(JsonVariantConst v) {
  if (v.is<const char*>()) return atof(v.as<const char*>());
  return v.as<double>();
}

static double distKm(double la1, double lo1, double la2, double lo2) {
  const double R = 6371.0, k = M_PI / 180.0;
  double dLa = (la2 - la1) * k, dLo = (lo2 - lo1) * k;
  double a = sin(dLa / 2) * sin(dLa / 2) + cos(la1 * k) * cos(la2 * k) * sin(dLo / 2) * sin(dLo / 2);
  return 2 * R * asin(sqrt(a));
}

static void setEqState(const String& s) {
  eqState = s;
  Serial.println("[Deprem] " + s);
}

// Tum Turkiye kutusu, en dusuk esikle (yakin); "yakin mi / bolge mi" burada suzulur.
// AFAD `date` alani saat dilimi belirtmiyor (muhtemelen UTC) - sorgu penceresi
// genis tutuldu, tekrar onlemi eventID ile yapiliyor.
bool fetchEarthquakes() {
  if (WiFi.status() != WL_CONNECTED) { setEqState("WiFi yok"); return false; }
  double myLat = 0, myLon = 0;
  if (xSemaphoreTake(weatherMutex, MUTEX_TIMEOUT) == pdTRUE) {
    myLat = atof((const char*)(WEATHER_CONFIG_DOC["latitude"] | "0"));
    myLon = atof((const char*)(WEATHER_CONFIG_DOC["longitude"] | "0"));
    xSemaphoreGive(weatherMutex);
  }
  if (myLat == 0 && myLon == 0) { setEqState("konum yok"); return false; }
  time_t now = time(nullptr);
  if (now < 1700000000) { setEqState("saat yok"); return false; }

  char s[24], e[24];
  time_t st = now - (time_t)ALERT_EQ_WINDOW_H * 3600, en = now + 7200;
  struct tm tm;
  gmtime_r(&st, &tm); strftime(s, sizeof(s), "%Y-%m-%dT%H:%M:%S", &tm);
  gmtime_r(&en, &tm); strftime(e, sizeof(e), "%Y-%m-%dT%H:%M:%S", &tm);
  String url = String("https://servisnet.afad.gov.tr/apigateway/deprem/apiv2/event/filter?start=") + s + "&end=" + e +
               "&minlat=35.5&maxlat=42.5&minlon=25.5&maxlon=45&minmag=" + String(ALERT_EQ_NEAR_MAG, 1) +
               "&orderby=timedesc&limit=20&format=json";

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(6000);
  http.useHTTP10(true);  // chunked olmasin - akistan dogrudan ayristirilir
  http.setUserAgent("MerkeziHUB/1.0");
  // AFAD eski adresten (deprem.afad.gov.tr/apiv2) buraya 302 veriyordu (2026-09-30)
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if (!http.begin(client, url)) { setEqState("begin basarisiz"); return false; }
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    setEqState("HTTP " + String(code) + (code < 0 ? " " + http.errorToString(code) : ""));
    http.end();
    return false;
  }

  JsonDocument filter;
  JsonObject f = filter.add<JsonObject>();
  f["eventID"] = true; f["location"] = true; f["latitude"] = true; f["longitude"] = true;
  f["magnitude"] = true; f["date"] = true;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (err) { setEqState(String("JSON hata: ") + err.c_str()); return false; }

  int raised = 0, seen = 0;
  JsonArray arr = doc.as<JsonArray>();
  for (int i = (int)arr.size() - 1; i >= 0; i--) {  // eskiden yeniye: yeni olan listede son kalir
    JsonObject ev = arr[i];
    seen++;
    double mag = numOf(ev["magnitude"]);
    double d = distKm(myLat, myLon, numOf(ev["latitude"]), numOf(ev["longitude"]));
    bool near = d <= ALERT_EQ_NEAR_KM && mag >= ALERT_EQ_NEAR_MAG;
    bool far = mag >= ALERT_EQ_FAR_MAG;
    if (!near && !far) continue;
    String date = ev["date"] | "";  // 2026-09-29T07:10:10
    String when = date.length() >= 16 ? date.substring(8, 10) + "." + date.substring(5, 7) + " " + date.substring(11, 16) : date;
    String loc = ev["location"] | "";
    String title = String(near ? "YAKIN DEPREM M" : "DEPREM M") + String(mag, 1);
    String detail = loc + " - " + String((int)roundf(d)) + " km - " + when;
    alertsRaise(String("eq:") + (const char*)(ev["eventID"] | ""), 1, title, detail);
    raised++;
  }

  struct tm t;
  char ts[6] = "--:--";
  if (getLocalTime(&t, 0)) strftime(ts, sizeof(ts), "%H:%M", &t);
  setEqState("OK " + String(seen) + " kayit, esik ustu " + String(raised) + " " + ts);
  return true;
}
