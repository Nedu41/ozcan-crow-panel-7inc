// HUB sayfaları: Ana (saat, hava, uyarı, haber), Lambalar, LedPanel, Ayar.
// LVGL yalnızca ana döngü görevinden çağrılır; veri/ağ işleri diğer görevlerde (paylaşılan durum mutex'le okunur).
#include "ui_hub.h"

#include <WiFi.h>
#include <time.h>

#include "alerts.h"
#include "app_state.h"
#include "config.h"
#include "devices.h"
#include "events.h"
#include "ledpanel.h"
#include "ui.h"
#include "ui_common.h"
#include "web_server.h"
#include "wifi_nets.h"

static lv_obj_t* tv;
static lv_obj_t *tabHome, *tabLamps, *tabLed, *tabSet;

// ------------------------------------------------------------------ yardımcılar
static bool takeMutex(SemaphoreHandle_t m) { return m && xSemaphoreTake(m, pdMS_TO_TICKS(15)) == pdTRUE; }

static lv_obj_t* newTab(const char* name) {
  lv_obj_t* t = lv_tabview_add_tab(tv, name);
  lv_obj_set_style_pad_all(t, 10, 0);
  lv_obj_set_style_pad_gap(t, 10, 0);
  lv_obj_set_style_bg_opa(t, LV_OPA_TRANSP, 0);
  lv_obj_set_flex_flow(t, LV_FLEX_FLOW_COLUMN);
  return t;
}

static lv_obj_t* dot(lv_obj_t* par) {
  lv_obj_t* d = lv_obj_create(par);
  lv_obj_set_size(d, 16, 16);
  lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(d, 0, 0);
  lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
  lv_obj_remove_flag(d, LV_OBJ_FLAG_SCROLLABLE);
  return d;
}

// Bağlantı/sağlık göstergesi: yeşil = bağlı, kırmızı = yok
static void setDot(lv_obj_t* d, bool ok) {
  lv_color_t c = ok ? C_GREEN : C_RED;
  lv_obj_set_style_bg_color(d, c, 0);
  lv_obj_set_style_shadow_width(d, 10, 0);
  lv_obj_set_style_shadow_color(d, c, 0);
  lv_obj_set_style_shadow_opa(d, LV_OPA_60, 0);
}

// ------------------------------------------------------------------ ANA
static lv_obj_t *lblTime, *lblDate, *lblTemp, *lblWxDesc, *lblWxCity, *lblWxMore, *lblSun;
static lv_obj_t *cardAlert, *lblAlertTitle, *lblAlertDetail, *lblNews;
static String alertId;
static String lastNews;

static const char* wmoText(int c) {
  if (c == 0) return "Açık";
  if (c <= 2) return "Parçalı bulutlu";
  if (c == 3) return "Bulutlu";
  if (c == 45 || c == 48) return "Sisli";
  if (c >= 51 && c <= 57) return "Çisenti";
  if (c >= 61 && c <= 67) return "Yağmurlu";
  if (c >= 71 && c <= 77) return "Karlı";
  if (c >= 80 && c <= 82) return "Sağanak";
  if (c == 85 || c == 86) return "Kar sağanağı";
  if (c >= 95) return "Fırtına";
  return "";
}

static void onAlertAck(lv_event_t*) {
  if (alertId.length()) alertsAck(alertId);
}

static void buildHome() {
  lv_obj_t* r = uiRow(tabHome);
  lv_obj_set_flex_align(r, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lblTime = uiLabel(r, "--:--", &lv_font_montserrat_48, C_CYAN);
  lblDate = uiLabel(r, "", &noto20, C_TEXT);
  lv_obj_set_style_text_align(lblDate, LV_TEXT_ALIGN_RIGHT, 0);

  lv_obj_t* wx = uiCard(tabHome);
  lv_obj_t* wr = uiRow(wx);
  lblTemp = uiLabel(wr, "--°", &lv_font_montserrat_48, C_ORANGE);
  lv_obj_t* col = lv_obj_create(wr);
  lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(col, 0, 0);
  lv_obj_set_style_pad_all(col, 0, 0);
  lv_obj_set_flex_grow(col, 1);
  lv_obj_set_height(col, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  lv_obj_remove_flag(col, LV_OBJ_FLAG_SCROLLABLE);
  lblWxDesc = uiLabel(col, "Hava durumu yükleniyor…", &noto28, C_TEXT);
  lblWxCity = uiLabel(col, "", &noto16, C_DIM);
  lv_obj_set_width(lblWxCity, LV_PCT(100));
  lv_label_set_long_mode(lblWxCity, LV_LABEL_LONG_DOT);
  lblWxMore = uiLabel(wx, "", &noto20, C_TEXT);
  lblSun = uiLabel(wx, "", &noto16, C_DIM);

  cardAlert = uiCard(tabHome);
  lv_obj_set_style_border_color(cardAlert, C_ORANGE, 0);
  lblAlertTitle = uiLabel(cardAlert, "", &noto28, C_ORANGE);
  lblAlertDetail = uiLabel(cardAlert, "", &noto20, C_TEXT);
  lv_obj_set_width(lblAlertDetail, LV_PCT(100));
  lv_label_set_long_mode(lblAlertDetail, LV_LABEL_LONG_WRAP);
  uiBtn(cardAlert, "Anladım", false, onAlertAck);
  lv_obj_add_flag(cardAlert, LV_OBJ_FLAG_HIDDEN);

  lv_obj_t* nc = uiCard(tabHome);
  uiLabel(nc, "Dünya haberleri", &noto16, C_DIM);
  lblNews = uiLabel(nc, "Haberler yükleniyor…", &noto20, C_TEXT);
  lv_obj_set_width(lblNews, LV_PCT(100));
  lv_label_set_long_mode(lblNews, LV_LABEL_LONG_WRAP);
}

static void tickHome() {
  static const char* gun[] = {"Pazar", "Pazartesi", "Salı", "Çarşamba", "Perşembe", "Cuma", "Cumartesi"};
  static const char* ay[] = {"Ocak", "Şubat", "Mart", "Nisan", "Mayıs", "Haziran", "Temmuz", "Ağustos", "Eylül", "Ekim", "Kasım", "Aralık"};
  struct tm t;
  char buf[96];
  if (getLocalTime(&t, 0)) {
    snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
    uiSetText(lblTime, buf);
    snprintf(buf, sizeof(buf), "%d %s %d\n%s", t.tm_mday, ay[t.tm_mon], t.tm_year + 1900, gun[t.tm_wday]);
    uiSetText(lblDate, buf);
  }

  // hava
  if (takeMutex(weatherMutex)) {
    String desc = CURRENT_WEATHER_DOC["description"] | "";
    String city = CURRENT_WEATHER_DOC["city_name"] | "";
    float temp = CURRENT_WEATHER_DOC["temp"] | 0.0f;
    float wind = CURRENT_WEATHER_DOC["wind_speed"] | 0.0f;
    String wdir = CURRENT_WEATHER_DOC["wind_direction"] | "";
    int hum = CURRENT_WEATHER_DOC["humidity"] | 0;
    int clouds = CURRENT_WEATHER_DOC["clouds"] | 0;
    int code = CURRENT_WEATHER_DOC["weathercode"] | -1;
    String sr = CURRENT_WEATHER_DOC["sunrise_time"] | "";
    String ss = CURRENT_WEATHER_DOC["sunset_time"] | "";
    xSemaphoreGive(weatherMutex);
    if (desc == "OK") {
      snprintf(buf, sizeof(buf), "%.0f°", temp);
      uiSetText(lblTemp, buf);
      uiSetText(lblWxDesc, wmoText(code)[0] ? wmoText(code) : "Hava durumu");
      uiSetText(lblWxCity, city.c_str());
      snprintf(buf, sizeof(buf), "Rüzgâr %.0f km/s %s   Nem %%%d   Bulut %%%d", wind, wdir.c_str(), hum, clouds);
      uiSetText(lblWxMore, buf);
      snprintf(buf, sizeof(buf), "Gün doğumu %s   Gün batımı %s", sr.c_str(), ss.c_str());
      uiSetText(lblSun, buf);
    } else if (desc == "NO_LOCATION") {
      uiSetText(lblTemp, "");
      uiSetText(lblWxDesc, "Konum ayarlanmamış");
      uiSetText(lblWxCity, "Web arayüzü > Hava ayarlarından şehir girin");
      uiSetText(lblWxMore, "");
      uiSetText(lblSun, "");
    } else if (desc == "FETCH_ERROR") {
      uiSetText(lblWxDesc, "Hava verisi alınamadı");
      uiSetText(lblWxCity, city.c_str());
    }
  }

  // uyarı bandı
  static uint32_t aver = 0xFFFFFFFF;
  if (alertsVersion() != aver) {
    aver = alertsVersion();
    Alert a[2];
    int n = alertsSnapshot(a, 2);
    if (n > 0) {
      alertId = a[0].id;
      uiSetText(lblAlertTitle, a[0].title.c_str());
      uiSetText(lblAlertDetail, a[0].detail.c_str());
      lv_obj_remove_flag(cardAlert, LV_OBJ_FLAG_HIDDEN);
    } else {
      alertId = "";
      lv_obj_add_flag(cardAlert, LV_OBJ_FLAG_HIDDEN);
    }
  }

  // haber
  String news;
  if (takeMutex(newsMutex)) { news = NEWS_TICKER_TEXT; xSemaphoreGive(newsMutex); }
  if (news.length()) {
    // başlıklar "     +++     " ile ayrılı; 6 sn'de bir sıradaki gösterilir
    static const char* SEP = "     +++     ";
    int count = 1;
    for (int p = news.indexOf(SEP); p >= 0; p = news.indexOf(SEP, p + 1)) count++;
    int idx = (millis() / 6000) % count, start = 0;
    for (int k = 0; k < idx; k++) start = news.indexOf(SEP, start) + strlen(SEP);
    int end = news.indexOf(SEP, start);
    String one = end < 0 ? news.substring(start) : news.substring(start, end);
    if (one != lastNews) {
      lastNews = one;
      lv_label_set_text(lblNews, one.c_str());
    }
  }
}

// ------------------------------------------------------------------ LAMBALAR
struct CardRef { int dev; int relay; };
static CardRef refs[MAX_DEVICES][2];
static lv_obj_t* lampBox;
static lv_obj_t* lampInfo;
static lv_obj_t* lampDot[MAX_DEVICES];
static lv_obj_t* lampBtn[MAX_DEVICES][2];
static String lampSig;

struct Ctl { char ip[20]; char name[40]; int relay; bool on; };
static QueueHandle_t ctlQ;

static void ctlTask(void*) {
  Ctl c;
  for (;;) {
    if (xQueueReceive(ctlQ, &c, portMAX_DELAY) != pdTRUE) continue;
    lastManualControlAction = millis();
    bool ok = sendControlSignal(c.ip, c.relay, c.on ? "on" : "off", "dokunmatik");
    addEvent(c.name, c.relay, c.on, "dokunmatik", ok);
    if (ok && xSemaphoreTake(statusMutex, MUTEX_TIMEOUT) == pdTRUE) {
      JsonObject st = CURRENT_STATUS_DOC[c.name].as<JsonObject>();
      if (!st.isNull()) {
        st[c.relay == 1 ? "r1_state" : "r2_state"] = c.on ? "ON" : "OFF";
        st[c.relay == 1 ? "relay_1" : "relay_2"] = c.on ? 1 : 0;
      }
      xSemaphoreGive(statusMutex);
    }
    if (ok) wsBroadcastStatus();
    setForceRefresh();
  }
}

static void onRelay(lv_event_t* e) {
  CardRef* r = (CardRef*)lv_event_get_user_data(e);
  JsonArray arr = doc.as<JsonArray>();
  if (r->dev >= (int)arr.size()) return;
  JsonObject d = arr[r->dev];
  Ctl c{};
  strlcpy(c.ip, (const char*)(d["ip"] | ""), sizeof(c.ip));
  String name = d["name"] | "";
  strlcpy(c.name, name.c_str(), sizeof(c.name));
  c.relay = r->relay;
  bool cur = false;
  if (takeMutex(statusMutex)) {
    cur = strcmp(CURRENT_STATUS_DOC[name]["r" + String(r->relay) + "_state"] | "", "ON") == 0;
    xSemaphoreGive(statusMutex);
  }
  c.on = !cur;
  xQueueSend(ctlQ, &c, 0);
  // hemen geri bildirim: durum gelene kadar yeni hâli göster
  uiBtnOn(lampBtn[r->dev][r->relay - 1], c.on, C_CYAN);
}

static void buildLamps() {
  lampInfo = uiLabel(tabLamps, "", &noto20, C_DIM);
  lampBox = lv_obj_create(tabLamps);
  lv_obj_set_width(lampBox, LV_PCT(100));
  lv_obj_set_height(lampBox, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(lampBox, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(lampBox, 0, 0);
  lv_obj_set_style_pad_all(lampBox, 0, 0);
  lv_obj_set_style_pad_gap(lampBox, 10, 0);
  lv_obj_set_flex_flow(lampBox, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_remove_flag(lampBox, LV_OBJ_FLAG_SCROLLABLE);
  ctlQ = xQueueCreate(8, sizeof(Ctl));
  xTaskCreatePinnedToCore(ctlTask, "UiCtl", 6144, nullptr, 1, nullptr, 0);
}

static void tickLamps() {
  JsonArray arr = doc.as<JsonArray>();
  String sig;
  int n = 0;
  for (JsonObject d : arr) {
    if (n >= MAX_DEVICES) break;
    sig += String((const char*)(d["name"] | "")) + "|";
    n++;
  }
  if (sig != lampSig) {  // cihaz listesi değişti: kartları yeniden kur
    lampSig = sig;
    lv_obj_clean(lampBox);
    memset(lampDot, 0, sizeof(lampDot));
    memset(lampBtn, 0, sizeof(lampBtn));
    int i = 0;
    for (JsonObject d : arr) {
      if (i >= MAX_DEVICES) break;
      lv_obj_t* card = uiCard(lampBox);
      lv_obj_set_width(card, LV_PCT(48));
      lv_obj_t* hr = uiRow(card);
      lampDot[i] = dot(hr);
      lv_obj_t* nm = uiLabel(hr, String((const char*)(d["name"] | "?")).c_str(), &noto20, C_TEXT);
      lv_obj_set_flex_grow(nm, 1);
      lv_label_set_long_mode(nm, LV_LABEL_LONG_DOT);
      for (int r = 0; r < MAX_RELAYS; r++) {
        refs[i][r] = {i, r + 1};
        lampBtn[i][r] = uiBtn(card, "", false, nullptr, LV_PCT(100), 52);
        lv_obj_add_event_cb(lampBtn[i][r], onRelay, LV_EVENT_CLICKED, &refs[i][r]);
      }
      i++;
    }
    lampSig = sig;
  }
  uiSetText(lampInfo, n ? "" : "Henüz cihaz yok. Web arayüzünden (cihazlar sayfası) ekleyin.");

  // durumları güncelle
  if (!takeMutex(statusMutex)) return;
  int i = 0;
  for (JsonObject d : arr) {
    if (i >= MAX_DEVICES) break;
    String name = d["name"] | "";
    JsonObject st = CURRENT_STATUS_DOC[name].as<JsonObject>();
    String s1 = st.isNull() ? "OFFLINE" : String((const char*)(st["r1_state"] | "OFFLINE"));
    bool online = s1 != "OFFLINE" && s1 != "ERR";
    if (lampDot[i]) setDot(lampDot[i], online);
    for (int r = 0; r < MAX_RELAYS; r++) {
      if (!lampBtn[i][r]) continue;
      String s = st.isNull() ? "OFFLINE" : String((const char*)(st[r == 0 ? "r1_state" : "r2_state"] | "OFFLINE"));
      bool on = s == "ON";
      char b[48];
      snprintf(b, sizeof(b), "Röle %d   %s", r + 1, !online ? "çevrimdışı" : (on ? "AÇIK" : "kapalı"));
      lv_obj_t* l = uiBtnLabel(lampBtn[i][r]);
      uiSetText(l, b);
      uiBtnOn(lampBtn[i][r], online && on, C_CYAN);
      if (!online) lv_obj_set_style_text_color(l, C_DIM, 0);
    }
    i++;
  }
  xSemaphoreGive(statusMutex);
}

// ------------------------------------------------------------------ LEDPANEL
static lv_obj_t *ledDot, *ledStat, *ledSpeedLbl, *ledBrLbl, *ledSpeed, *ledBright, *ledRotBtn;
static lv_obj_t* fxBtn[16];
struct Fx { int id; const char* name; };
static const Fx FX[] = {{31, "Alev"}, {35, "Plazma"}, {36, "Gökkuşağı"}, {34, "Matrix"}, {37, "Yıldızlı gece"},
                        {41, "Kar"}, {42, "Yağmur"}, {43, "Fırtına"}, {44, "Doğa manzarası"}, {45, "Okyanus"},
                        {46, "Havai fişek"}, {47, "Radar"}, {48, "Kalp atışı"}};
static const int NFX = sizeof(FX) / sizeof(FX[0]);
static int fxHiz = 60;  // son seçilen hız (ms)

static void onFx(lv_event_t* e) {
  int idx = (int)(intptr_t)lv_event_get_user_data(e);
  ledpanelSend("/api/fx?id=" + String(FX[idx].id) + "&hiz=" + String(fxHiz));
}
static void onSpeed(lv_event_t* e) {
  int v = lv_slider_get_value(lv_event_get_target_obj(e));  // sağ = hızlı
  fxHiz = 310 - v;
  char b[40];
  snprintf(b, sizeof(b), "Hız (%d ms)", fxHiz);
  lv_label_set_text(ledSpeedLbl, b);
  if (lv_event_get_code(e) == LV_EVENT_RELEASED)
    ledpanelSend("/api/set?fxs=" + String(fxHiz));
}
static void onBright(lv_event_t* e) {
  int v = lv_slider_get_value(lv_event_get_target_obj(e));
  char b[40];
  snprintf(b, sizeof(b), "Işık (%d)", v);
  lv_label_set_text(ledBrLbl, b);
  if (lv_event_get_code(e) == LV_EVENT_RELEASED) ledpanelSend("/api/set?bp=" + String(v));
}
static void onLedRot(lv_event_t*) { ledpanelSend("/api/set?rot=" + String((ledState.rot + 1) & 3)); }
static void onLedHostOk(const char* t) { ledpanelSetHost(String(t)); }
static void onLedHost(lv_event_t*) { uiPrompt("LedPanel adresi (boş = otomatik)", ledpanelHost().c_str(), false, onLedHostOk); }

static void buildLed() {
  lv_obj_t* c = uiCard(tabLed);
  lv_obj_t* r = uiRow(c);
  ledDot = dot(r);
  ledStat = uiLabel(r, "LedPanel aranıyor…", &noto20, C_TEXT);
  lv_obj_set_flex_grow(ledStat, 1);
  uiBtn(r, "Adres", false, onLedHost, LV_SIZE_CONTENT, 44);

  lv_obj_t* fc = uiCard(tabLed);
  uiLabel(fc, "Efektler", &noto16, C_DIM);
  lv_obj_t* grid = lv_obj_create(fc);
  lv_obj_set_width(grid, LV_PCT(100));
  lv_obj_set_height(grid, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(grid, 0, 0);
  lv_obj_set_style_pad_all(grid, 0, 0);
  lv_obj_set_style_pad_gap(grid, 8, 0);
  lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_remove_flag(grid, LV_OBJ_FLAG_SCROLLABLE);
  for (int i = 0; i < NFX; i++) {
    fxBtn[i] = uiBtn(grid, FX[i].name, false, nullptr, LV_SIZE_CONTENT, 52);
    lv_obj_add_event_cb(fxBtn[i], onFx, LV_EVENT_CLICKED, (void*)(intptr_t)i);
  }

  lv_obj_t* sc = uiCard(tabLed);
  ledSpeedLbl = uiLabel(sc, "Hız (60 ms)", &noto20, C_TEXT);
  ledSpeed = uiSlider(sc, 10, 300, 250, C_CYAN);
  lv_obj_add_event_cb(ledSpeed, onSpeed, LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_add_event_cb(ledSpeed, onSpeed, LV_EVENT_RELEASED, nullptr);
  ledBrLbl = uiLabel(sc, "Işık", &noto20, C_TEXT);
  ledBright = uiSlider(sc, 4, 30, 10, C_ORANGE);
  lv_obj_add_event_cb(ledBright, onBright, LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_add_event_cb(ledBright, onBright, LV_EVENT_RELEASED, nullptr);
  ledRotBtn = uiBtn(sc, "Yön: döndür", false, onLedRot, LV_PCT(100), 52);
}

static void tickLed() {
  bool ok = ledState.ok;
  setDot(ledDot, ok);
  char b[96];
  if (ok) snprintf(b, sizeof(b), "Bağlı  %s   %d fps", ledState.ip, ledState.fps);
  else snprintf(b, sizeof(b), "LedPanel bulunamadı (ledpanel.local)");
  uiSetText(ledStat, b);
  for (int i = 0; i < NFX; i++) uiBtnOn(fxBtn[i], ok && ledState.pat == FX[i].id, C_CYAN);
  if (ok) {
    if (!lv_obj_has_state(ledSpeed, LV_STATE_PRESSED) && ledState.fxs >= 10) {
      fxHiz = ledState.fxs;
      lv_slider_set_value(ledSpeed, 310 - fxHiz, LV_ANIM_OFF);
      snprintf(b, sizeof(b), "Hız (%d ms)", fxHiz);
      uiSetText(ledSpeedLbl, b);
    }
    if (!lv_obj_has_state(ledBright, LV_STATE_PRESSED) && ledState.bp >= 4) {
      lv_slider_set_value(ledBright, constrain(ledState.bp, 4, 30), LV_ANIM_OFF);
      snprintf(b, sizeof(b), "Işık (%d)", ledState.bp);
      uiSetText(ledBrLbl, b);
    }
    snprintf(b, sizeof(b), "Yön: %d  (dokun: döndür)", ledState.rot);
    uiSetText(uiBtnLabel(ledRotBtn), b);
  }
}

// ------------------------------------------------------------------ AYAR
static lv_obj_t *setInfo, *blLbl;

static void onSetWifi(lv_event_t*) { uiShowWifiSetup(); }
static void onSetRot(lv_event_t*) { uiRotate(); }
static void onSetRestart(lv_event_t*) { ESP.restart(); }
static void doWipeWifi() {
  wifiNetsClear();
  ESP.restart();
}
static void onSetWipe(lv_event_t*) { uiConfirm("Kayıtlı tüm WiFi ağları silinsin mi? Cihaz yeniden başlar.", doWipeWifi); }
static void onBl(lv_event_t* e) {
  int v = lv_slider_get_value(lv_event_get_target_obj(e));
  char b[32];
  snprintf(b, sizeof(b), "Ekran parlaklığı (%%%d)", v);
  lv_label_set_text(blLbl, b);
  if (lv_event_get_code(e) == LV_EVENT_RELEASED) uiSetBacklight(v);
}

static void buildSet() {
  lv_obj_t* c = uiCard(tabSet);
  setInfo = uiLabel(c, "", &noto20, C_TEXT);
  lv_obj_t* bc = uiCard(tabSet);
  blLbl = uiLabel(bc, "", &noto20, C_TEXT);
  lv_obj_t* s = uiSlider(bc, 10, 100, uiGetBacklight(), C_CYAN);
  lv_obj_add_event_cb(s, onBl, LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_add_event_cb(s, onBl, LV_EVENT_RELEASED, nullptr);
  char b[32];
  snprintf(b, sizeof(b), "Ekran parlaklığı (%%%d)", uiGetBacklight());
  lv_label_set_text(blLbl, b);

  lv_obj_t* r = lv_obj_create(tabSet);
  lv_obj_set_width(r, LV_PCT(100));
  lv_obj_set_height(r, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(r, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(r, 0, 0);
  lv_obj_set_style_pad_all(r, 0, 0);
  lv_obj_set_style_pad_gap(r, 10, 0);
  lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_remove_flag(r, LV_OBJ_FLAG_SCROLLABLE);
  uiBtn(r, "WiFi ayarı", false, onSetWifi);
  uiBtn(r, "Ekranı döndür", false, onSetRot);
  uiBtn(r, "Yeniden başlat", false, onSetRestart);
  uiBtn(r, "WiFi'leri sil", false, onSetWipe);
}

static void tickSet() {
  static uint32_t last = 0;
  if (millis() - last < 2000) return;
  last = millis();
  uint32_t up = millis() / 1000;
  char b[256];
  snprintf(b, sizeof(b),
           "Ağ: %s   Sinyal: %d dBm\nIP: %s   Adres: %s.local\nSürüm: %s\nÇalışma: %lu sa %lu dk   Bellek: %u KB boş",
           WiFi.SSID().c_str(), WiFi.RSSI(), WiFi.localIP().toString().c_str(), "crowhub", FW_VERSION,
           (unsigned long)(up / 3600), (unsigned long)(up / 60 % 60), (unsigned)(ESP.getFreeHeap() / 1024));
  uiSetText(setInfo, b);
}

// ------------------------------------------------------------------ sekmeler
static void placeTabs() {
  bool land = uiIsLandscape();
  lv_tabview_set_tab_bar_position(tv, land ? LV_DIR_LEFT : LV_DIR_BOTTOM);
  lv_tabview_set_tab_bar_size(tv, land ? 130 : 64);
}

void hubUiOnRotate() {
  if (tv) placeTabs();
}

volatile int devTabReq = -1;  // geliştirme: /dev/tab, /dev/rot (screenshot.cpp)
volatile bool devRotReq = false;

static void tick(lv_timer_t*) {
  if (devTabReq >= 0) { lv_tabview_set_active(tv, devTabReq, LV_ANIM_OFF); devTabReq = -1; }
  if (devRotReq) { devRotReq = false; uiRotate(); }
  uint32_t act = lv_tabview_get_tab_active(tv);
  tickHome();  // saat her zaman; ucuz
  if (act == 1) tickLamps();
  if (act == 2) tickLed();
  if (act == 3) tickSet();
}

lv_obj_t* hubUiCreate() {
  lv_obj_t* scr = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(scr, C_BG, 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(scr, 0, 0);
  lv_obj_set_style_pad_all(scr, 0, 0);
  lv_obj_set_style_radius(scr, 0, 0);
  lv_obj_set_style_text_color(scr, C_TEXT, 0);
  lv_obj_set_style_text_font(scr, &noto20, 0);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  tv = lv_tabview_create(scr);
  lv_obj_set_size(tv, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(tv, C_BG, 0);
  lv_obj_set_style_bg_opa(tv, LV_OPA_COVER, 0);
  lv_obj_t* bar = lv_tabview_get_tab_bar(tv);
  lv_obj_set_style_bg_color(bar, C_CARD, 0);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(bar, 0, 0);
  lv_obj_set_style_pad_all(bar, 4, 0);
  lv_obj_set_style_pad_gap(bar, 4, 0);
  lv_obj_remove_flag(lv_tabview_get_content(tv), LV_OBJ_FLAG_SCROLLABLE);  // yatay kaydırma yok (kaydırıcılarla karışmasın)

  tabHome = newTab("Ana");
  tabLamps = newTab("Lambalar");
  tabLed = newTab("LedPanel");
  tabSet = newTab("Ayar");
  // Sekme düğmeleri: siyah çerçeveli koyu düğme; seçili = pastel zemin + koyu yazı
  for (uint32_t i = 0; i < lv_obj_get_child_count(bar); i++) {
    lv_obj_t* b = lv_obj_get_child(bar, i);
    lv_obj_set_style_radius(b, 10, 0);
    lv_obj_set_style_bg_color(b, C_BTN, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(b, C_TEXT, 0);
    lv_obj_set_style_text_font(b, &noto20, 0);
    lv_obj_set_style_border_color(b, C_FRAME, 0);
    lv_obj_set_style_border_width(b, 3, 0);
    lv_obj_set_style_border_side(b, LV_BORDER_SIDE_FULL, 0);
    lv_obj_set_style_bg_color(b, C_CYAN, LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_STATE_CHECKED);
    lv_obj_set_style_bg_grad_dir(b, LV_GRAD_DIR_NONE, LV_STATE_CHECKED);
    lv_obj_set_style_text_color(b, C_ONPASTEL, LV_STATE_CHECKED);
    lv_obj_set_style_border_color(b, C_FRAME, LV_STATE_CHECKED);
    lv_obj_set_style_border_width(b, 3, LV_STATE_CHECKED);
    lv_obj_set_style_border_side(b, LV_BORDER_SIDE_FULL, LV_STATE_CHECKED);
  }
  buildHome();
  buildLamps();
  buildLed();
  buildSet();
  placeTabs();
  lv_timer_create(tick, 500, nullptr);
  ledpanelStart();
  return scr;
}
