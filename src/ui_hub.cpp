// HUB sayfaları: Ana (saat, hava, uyarı, haber), Lambalar, Efekt, Ayar.
// LVGL yalnızca ana döngü görevinden çağrılır; veri/ağ işleri diğer görevlerde (paylaşılan durum mutex'le okunur).
#include "ui_hub.h"

#include <WiFi.h>
#include <time.h>

#include "alerts.h"
#include "app_state.h"
#include "config.h"
#include "devices.h"
#include "events.h"
#include "storage.h"
#include "telegram.h"
#include "efekt.h"
#include "sky.h"
#include "ui.h"
#include "ui_common.h"
#include "web_server.h"
#include "wifi_nets.h"

static lv_obj_t* tv;
static lv_obj_t *tabHome, *tabLamps, *tabFx, *tabSet;

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
static lv_obj_t *skyCard, *skyImg, *boxT, *boxW, *boxF, *lblTempC, *lblWindT, *lblWindDir, *lblWindVal, *lblWindU;
static const int IC_W = 26, IC_H = 22;
// Hava karti paleti (ev-lamba ekranindan farkli): pastel pembe / lavanta / nane + soluk sari "bugun"; koyu yari saydam zemin
#define WX_TEMP  lv_color_hex(0xF4A6C8)
#define WX_WIND  lv_color_hex(0xB9A6F4)
#define WX_FC    lv_color_hex(0xA6F4D0)
#define WX_TODAY lv_color_hex(0xF4E3A6)
static lv_obj_t *fcCol[7], *fcDay[7], *fcTmp[7], *fcIcon[7];
static uint8_t* fcBuf[7];
static int fcN = 0, fcCode[7], fcMax[7], fcMin[7];
static uint32_t fcSig = 0;
static void drawMiniIcon(lv_obj_t* cv, int code, bool past);
static lv_image_dsc_t skyDsc;
static int skyCardW = 0;
static lv_obj_t *cardAlert, *lblAlertTitle, *lblAlertDetail, *lblNews;  // lblNews: alttaki kayan yazi
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

  // Hava karti: ev-lamba ekraninin ayni duzeni - canli gokyuzu arka plan, sicaklik/ruzgar kutulari,
  // 7 gunluk tahmin seridi (kutular seffaf, yalniz cerceve). Olcek kart genisligine gore (layoutSky).
  skyInit();
  skyCard = lv_obj_create(tabHome);
  lv_obj_set_style_bg_color(skyCard, lv_color_black(), 0);
  lv_obj_set_width(skyCard, LV_PCT(100));
  lv_obj_set_height(skyCard, 380);
  lv_obj_set_style_pad_all(skyCard, 0, 0);
  lv_obj_set_style_radius(skyCard, 12, 0);
  lv_obj_set_style_border_color(skyCard, C_FRAME, 0);
  lv_obj_set_style_border_width(skyCard, 3, 0);
  lv_obj_set_style_clip_corner(skyCard, true, 0);
  lv_obj_remove_flag(skyCard, LV_OBJ_FLAG_SCROLLABLE);
  skyDsc.header.magic = LV_IMAGE_HEADER_MAGIC;
  skyDsc.header.cf = LV_COLOR_FORMAT_RGB565;
  skyDsc.header.w = SKY_BW;
  skyDsc.header.h = SKY_BH;
  skyDsc.header.stride = SKY_BW * 2;
  skyDsc.data_size = SKY_BW * SKY_BH * 2;
  skyDsc.data = (const uint8_t*)skyRGB565();
  skyImg = lv_image_create(skyCard);
  lv_image_set_src(skyImg, &skyDsc);
  lv_image_set_pivot(skyImg, 0, 0);
  lv_obj_set_pos(skyImg, 0, 0);

  auto box = [&](lv_color_t col) {
    lv_obj_t* o = lv_obj_create(skyCard);
    lv_obj_set_style_bg_color(o, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_30, 0);
    lv_obj_set_style_border_color(o, col, 0);
    lv_obj_set_style_border_width(o, 2, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
  };
  boxT = box(WX_TEMP);
  lblWxDesc = uiLabel(boxT, "Hava durumu yükleniyor…", &noto28, C_TEXT);
  lblTemp = uiLabel(boxT, "--", &lv_font_montserrat_48, WX_TEMP);
  lblTempC = uiLabel(boxT, "°C", &noto28, WX_TEMP);
  boxW = box(WX_WIND);
  lblWindT = uiLabel(boxW, "RÜZGAR", &noto20, C_DIM);
  lblWindDir = uiLabel(boxW, "", &noto28, C_TEXT);
  lblWindVal = uiLabel(boxW, "--", &lv_font_montserrat_48, WX_WIND);
  lblWindU = uiLabel(boxW, "km/s", &noto28, WX_WIND);
  boxF = box(WX_FC);
  for (int i = 0; i < 7; i++) {
    fcCol[i] = box(WX_TODAY);
    lv_obj_set_style_bg_opa(fcCol[i], LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(fcCol[i], LV_OPA_TRANSP, 0);  // yalniz bugun cerceveli
    fcDay[i] = uiLabel(fcCol[i], "", &noto16, C_TEXT);
    fcTmp[i] = uiLabel(fcCol[i], "", &noto16, WX_FC);
    fcBuf[i] = (uint8_t*)heap_caps_calloc(IC_W * IC_H, 4, MALLOC_CAP_SPIRAM);
    fcIcon[i] = lv_canvas_create(fcCol[i]);
    lv_canvas_set_buffer(fcIcon[i], fcBuf[i], IC_W, IC_H, LV_COLOR_FORMAT_ARGB8888);
    lv_image_set_pivot(fcIcon[i], 0, 0);
  }
  // Gizli etiketler (tickHome ortak kodu icin)
  lblWxCity = uiLabel(skyCard, "", &noto16, C_DIM);
  lblWxMore = uiLabel(skyCard, "", &noto16, C_DIM);
  lblSun = uiLabel(skyCard, "", &noto16, C_DIM);
  lv_obj_add_flag(lblWxCity, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(lblWxMore, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(lblSun, LV_OBJ_FLAG_HIDDEN);

  cardAlert = uiCard(tabHome);
  lv_obj_set_style_border_color(cardAlert, C_ORANGE, 0);
  lblAlertTitle = uiLabel(cardAlert, "", &noto28, C_ORANGE);
  lblAlertDetail = uiLabel(cardAlert, "", &noto20, C_TEXT);
  lv_obj_set_width(lblAlertDetail, LV_PCT(100));
  lv_label_set_long_mode(lblAlertDetail, LV_LABEL_LONG_WRAP);
  uiBtn(cardAlert, "Anladım", false, onAlertAck);
  lv_obj_add_flag(cardAlert, LV_OBJ_FLAG_HIDDEN);

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
    float temp = CURRENT_WEATHER_DOC["temp"] | 0.0f;
    float wind = CURRENT_WEATHER_DOC["wind_speed"] | 0.0f;
    String wdir = CURRENT_WEATHER_DOC["wind_direction"] | "";
    int code = CURRENT_WEATHER_DOC["weathercode"] | -1;
    int n = 0, c[7], mx[7], mn[7];
    for (JsonArray d : CURRENT_WEATHER_DOC["fc"].as<JsonArray>()) {
      if (n >= 7) break;
      c[n] = d[0] | -1; mx[n] = d[1] | 0; mn[n] = d[2] | 0; n++;
    }
    xSemaphoreGive(weatherMutex);
    if (desc == "OK") {
      snprintf(buf, sizeof(buf), "%.0f", temp);
      uiSetText(lblTemp, buf);
      uiSetText(lblWxDesc, wmoText(code)[0] ? wmoText(code) : "Hava durumu");
      uiSetText(lblWindDir, wdir.c_str());
      snprintf(buf, sizeof(buf), "%.1f", wind);
      uiSetText(lblWindVal, buf);
      lv_obj_align_to(lblTempC, lblTemp, LV_ALIGN_OUT_RIGHT_BOTTOM, 4, -6);
      lv_obj_align_to(lblWindU, lblWindVal, LV_ALIGN_OUT_RIGHT_BOTTOM, 4, -6);
    } else if (desc == "NO_LOCATION") {
      uiSetText(lblWxDesc, "Konum ayarlanmamış (Ayar sayfası)");
    } else if (desc == "FETCH_ERROR") {
      uiSetText(lblWxDesc, "Hava verisi alınamadı");
    }
    // tahmin seridi: degisince ikonlari yeniden ciz
    uint32_t sig = 5381 + n;
    for (int i = 0; i < n; i++) sig = sig * 33 + c[i] * 7 + mx[i] * 3 + mn[i];
    struct tm tt;
    int todayW = getLocalTime(&tt, 0) ? tt.tm_wday : -1;
    sig = sig * 33 + todayW + 1;
    if (sig != fcSig && desc == "OK") {
      fcSig = sig;
      static const char* DAYS[7] = {"Pz", "Pt", "Sa", "Ça", "Pe", "Cu", "Ct"};
      for (int i = 0; i < 7; i++) {
        bool has = i < n;
        lv_obj_set_style_opa(fcCol[i], has ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        if (!has) continue;
        bool today = i == 1, past = i == 0;
        uiSetText(fcDay[i], todayW < 0 ? "--" : (today ? "BUGÜN" : DAYS[(todayW + i + 6) % 7]));
        lv_obj_set_style_text_color(fcDay[i], today ? WX_TODAY : past ? C_DIM : C_TEXT, 0);
        snprintf(buf, sizeof(buf), "%d/%d", mx[i], mn[i]);
        uiSetText(fcTmp[i], buf);
        lv_obj_set_style_text_color(fcTmp[i], past ? C_DIM : WX_FC, 0);
        lv_obj_set_style_border_opa(fcCol[i], today ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        drawMiniIcon(fcIcon[i], c[i], past);
      }
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

  // haber: tam genislik kayan yazi (tüm başlıklar, kesintisiz)
  String news;
  if (takeMutex(newsMutex)) { news = NEWS_TICKER_TEXT; xSemaphoreGive(newsMutex); }
  if (news.length() && news != lastNews && lblNews) {
    lastNews = news;
    lv_label_set_text(lblNews, news.c_str());
    lv_obj_update_layout(lblNews);
    int wpx = lv_text_get_width(news.c_str(), news.length(), &noto20, 0) + 200;  // dongu boslugu
    lv_obj_set_style_anim_duration(lblNews, (uint32_t)wpx * 1000UL / 110, 0);   // ~110 px/sn
  }
}

// ------------------------------------------------------------------ LAMBALAR
struct CardRef { int dev; int relay; };
static CardRef refs[MAX_DEVICES][2];
static lv_obj_t* lampBox;
static lv_obj_t* lampInfo;
static lv_obj_t* lampDot[MAX_DEVICES];
static lv_obj_t* lampBtn[MAX_DEVICES][2];
static lv_obj_t* lampWifi[MAX_DEVICES];
static lv_obj_t* lampSched[MAX_DEVICES][2];
static int tmrIdx[MAX_DEVICES];
static void onTimer(lv_event_t* e) { uiTimerOpen(*(int*)lv_event_get_user_data(e)); }
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
    memset(lampWifi, 0, sizeof(lampWifi));
    memset(lampSched, 0, sizeof(lampSched));
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
      tmrIdx[i] = i;
      lv_obj_t* tb = uiBtn(hr, "Zamanlayıcı", false, nullptr, LV_SIZE_CONTENT, 40);
      lv_obj_set_style_pad_hor(tb, 10, 0);
      lv_obj_set_style_text_font(uiBtnLabel(tb), &noto16, 0);
      lv_obj_add_event_cb(tb, onTimer, LV_EVENT_CLICKED, &tmrIdx[i]);
      lampWifi[i] = uiLabel(card, "", &noto16, C_DIM);
      for (int r = 0; r < MAX_RELAYS; r++) {
        refs[i][r] = {i, r + 1};
        lv_obj_t* rr = uiRow(card);
        char rn[4];
        snprintf(rn, sizeof(rn), "R%d", r + 1);
        uiLabel(rr, rn, &noto20, C_DIM);
        lampBtn[i][r] = uiBtn(rr, "", false, nullptr, LV_SIZE_CONTENT, 52);
        lv_obj_set_flex_grow(lampBtn[i][r], 1);
        lv_obj_add_event_cb(lampBtn[i][r], onRelay, LV_EVENT_CLICKED, &refs[i][r]);
        lampSched[i][r] = uiLabel(card, "", &noto16, C_DIM);
        lv_obj_set_width(lampSched[i][r], LV_PCT(100));
        lv_label_set_long_mode(lampSched[i][r], LV_LABEL_LONG_DOT);
      }
      i++;
    }
    lampSig = sig;
  }
  uiSetText(lampInfo, n ? "" : "Henüz cihaz yok. Web arayüzünden (cihazlar sayfası) ekleyin.");

  // durumları güncelle (web kartıyla aynı: dBm, röle durumu, zamanlayıcı özeti)
  if (!takeMutex(statusMutex)) return;
  int i = 0;
  for (JsonObject d : arr) {
    if (i >= MAX_DEVICES) break;
    String name = d["name"] | "";
    JsonObject st = CURRENT_STATUS_DOC[name].as<JsonObject>();
    String s1 = st.isNull() ? "OFFLINE" : String((const char*)(st["r1_state"] | "OFFLINE"));
    bool online = s1 != "OFFLINE" && s1 != "ERR";
    if (lampDot[i]) setDot(lampDot[i], online);
    if (lampWifi[i]) {
      char w[40];
      int rssi = st.isNull() ? 0 : (int)(st["wifi_rssi"] | 0);
      if (online && rssi) {
        snprintf(w, sizeof(w), "WiFi %d dBm", rssi);
        lv_color_t c = rssi >= -50 ? lv_color_hex(0x8FE0A0) : rssi >= -60 ? lv_color_hex(0xC5E0A0) : rssi >= -70 ? C_ORANGE : rssi >= -80 ? lv_color_hex(0xFFE08A) : C_RED;
        lv_obj_set_style_text_color(lampWifi[i], c, 0);
      } else {
        snprintf(w, sizeof(w), "WiFi çevrimdışı");
        lv_obj_set_style_text_color(lampWifi[i], C_RED, 0);
      }
      uiSetText(lampWifi[i], w);
    }
    for (int r = 0; r < MAX_RELAYS; r++) {
      if (!lampBtn[i][r]) continue;
      String s = st.isNull() ? "OFFLINE" : String((const char*)(st[r == 0 ? "r1_state" : "r2_state"] | "OFFLINE"));
      bool on = s == "ON";
      lv_obj_t* l = uiBtnLabel(lampBtn[i][r]);
      uiSetText(l, !online ? "HATA" : (on ? "AÇIK" : "KAPALI"));
      uiBtnOn(lampBtn[i][r], online && on, C_CYAN);
      if (!online) lv_obj_set_style_text_color(l, C_RED, 0);
      // zamanlayıcı özeti
      char info[96] = "";
      if (takeMutex(scheduleMutex)) {
        JsonObject o = SCHEDULE_CONFIG_DOC[name].as<JsonObject>();
        if (!o.isNull()) {
          String k = "r" + String(r + 1);
          String a = o[k + "_start"] | "", b = o[k + "_end"] | "", su = o[k + "_sunrise_act"] | "", sd = o[k + "_sunset_act"] | "";
          int days = o[k + "_days"] | 0;
          if (days > 0 && a.length() == 5 && b.length() == 5) snprintf(info, sizeof(info), "%s - %s", a.c_str(), b.c_str());
          if (su == "ON" || su == "OFF") { strlcat(info, su == "ON" ? "  Gündoğumu: AÇ" : "  Gündoğumu: KAPAT", sizeof(info)); }
          if (sd == "ON" || sd == "OFF") { strlcat(info, sd == "ON" ? "  Günbatımı: AÇ" : "  Günbatımı: KAPAT", sizeof(info)); }
        }
        xSemaphoreGive(scheduleMutex);
      }
      uiSetText(lampSched[i][r], info);
    }
    i++;
  }
  xSemaphoreGive(statusMutex);
}

// ------------------------------------------------------------------ EFEKT (yapay zeka efektleri, PC köprüsü)
static lv_obj_t *fxDot, *fxStat, *fxHizLbl, *fxIsikLbl, *fxGrid, *fxSaveBtn, *fxHizSlider, *fxIsikSlider;
static uint32_t fxListShown = 0;
static char fxPending[32];

static void onFxNewOk(const char* t) {
  if (!*t) return;
  efektUret(String(t));
  efektEkranAc();
}
static void onFxNew(lv_event_t*) { uiPrompt("Nasıl bir efekt istersin?", "", false, onFxNewOk); }
static void onFxPlay(lv_event_t* e) {
  efektOynat(String((const char*)lv_event_get_user_data(e)));
  efektEkranAc();
}
static void onFxDelOk() { efektSil(String(fxPending)); }
static void onFxDel(lv_event_t* e) {
  char b[80];
  strlcpy(fxPending, (const char*)lv_event_get_user_data(e), sizeof(fxPending));
  snprintf(b, sizeof(b), "\"%s\" silinsin mi?", fxPending);
  uiConfirm(b, onFxDelOk);
}
static void onFxSaveOk(const char* t) { if (*t) efektKaydet(String(t)); }
static void onFxSave(lv_event_t*) { uiPrompt("Son efektin adı", "", false, onFxSaveOk); }
static void onFxTune(lv_event_t* e) {
  float hiz = lv_slider_get_value(fxHizSlider) / 100.0f, isik = lv_slider_get_value(fxIsikSlider) / 100.0f;
  char b[40];
  snprintf(b, sizeof(b), "Hız (%.1fx)", hiz);
  lv_label_set_text(fxHizLbl, b);
  snprintf(b, sizeof(b), "Işık (%%%d)", (int)(isik * 100));
  lv_label_set_text(fxIsikLbl, b);
  if (lv_event_get_code(e) == LV_EVENT_RELEASED) efektAyar(hiz, isik);
}
static void onFxStop(lv_event_t*) { efektDurdur(); }

static void buildFx() {
  lv_obj_t* c = uiCard(tabFx);
  lv_obj_t* r = uiRow(c);
  fxDot = dot(r);
  fxStat = uiLabel(r, "PC köprüsü aranıyor…", &noto20, C_TEXT);
  lv_obj_set_flex_grow(fxStat, 1);
  uiBtn(r, "Yeni efekt", true, onFxNew, LV_SIZE_CONTENT, 48);

  lv_obj_t* fc = uiCard(tabFx);
  uiLabel(fc, "Kayıtlı efektler (uzun bas: sil)", &noto16, C_DIM);
  fxGrid = lv_obj_create(fc);
  lv_obj_set_width(fxGrid, LV_PCT(100));
  lv_obj_set_height(fxGrid, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(fxGrid, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(fxGrid, 0, 0);
  lv_obj_set_style_pad_all(fxGrid, 0, 0);
  lv_obj_set_style_pad_gap(fxGrid, 8, 0);
  lv_obj_set_flex_flow(fxGrid, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_remove_flag(fxGrid, LV_OBJ_FLAG_SCROLLABLE);
  fxSaveBtn = uiBtn(fc, "Son efekti kaydet", false, onFxSave, LV_PCT(100), 48);

  lv_obj_t* sc = uiCard(tabFx);
  fxHizLbl = uiLabel(sc, "Hız (1.0x)", &noto20, C_TEXT);
  fxHizSlider = uiSlider(sc, 10, 400, 100, C_CYAN);
  fxIsikLbl = uiLabel(sc, "Işık (%100)", &noto20, C_TEXT);
  fxIsikSlider = uiSlider(sc, 10, 200, 100, C_ORANGE);
  for (lv_obj_t* s : {fxHizSlider, fxIsikSlider}) {
    lv_obj_add_event_cb(s, onFxTune, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_obj_add_event_cb(s, onFxTune, LV_EVENT_RELEASED, nullptr);
  }
  uiBtn(sc, "Durdur", false, onFxStop, LV_PCT(100), 48);
}

static void tickFx() {
  bool ok = efektSt.ok;
  setDot(fxDot, ok);
  char b[96];
  if (!ok) snprintf(b, sizeof(b), "PC köprüsü yok (efekt_koprusu.py açık olmalı)");
  else if (!strcmp(efektSt.durum, "uretiyor")) snprintf(b, sizeof(b), "Köprü bağlı - efekt üretiliyor…");
  else if (!strcmp(efektSt.durum, "hata")) snprintf(b, sizeof(b), "Köprü bağlı - son üretim başarısız");
  else snprintf(b, sizeof(b), "Köprü bağlı");
  uiSetText(fxStat, b);
  if (fxListShown != efektSt.listeSurum) {
    fxListShown = efektSt.listeSurum;
    lv_obj_clean(fxGrid);
    static char names[24][32];
    int n = efektListe(names, 24);
    for (int i = 0; i < n; i++) {
      lv_obj_t* bt = uiBtn(fxGrid, names[i], false, nullptr, LV_SIZE_CONTENT, 52);
      lv_obj_add_event_cb(bt, onFxPlay, LV_EVENT_SHORT_CLICKED, names[i]);
      lv_obj_add_event_cb(bt, onFxDel, LV_EVENT_LONG_PRESSED, names[i]);
    }
    if (n == 0) uiLabel(fxGrid, "Henüz kayıtlı efekt yok", &noto16, C_DIM);
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
// ---- Telegram (token + chat id) ve hava konumu: ekrandan girilir, NVS/LittleFS'te saklanır
static String tmpTok;
static void tgTestTask(void*) {
  sendTelegramAlert("Telegram bağlantısı kuruldu (CrowPanel HUB).");
  vTaskDelete(nullptr);
}
static void onTgChat(const char* t) {
  telegramSetConfig(tmpTok, String(t));
  tmpTok = "";
  xTaskCreatePinnedToCore(tgTestTask, "TgTest", 8192, nullptr, 1, nullptr, 0);
}
static void onTgTok(const char* t) {
  tmpTok = t;
  uiPrompt("Telegram sohbet (chat) no", telegramChatId().c_str(), false, onTgChat);
}
static void onSetTg(lv_event_t*) { uiPrompt("Telegram bot token", "", false, onTgTok); }

static void onAiKey(const char* t) { efektAnahtar(String(t)); }
static void onSetAi(lv_event_t*) {
  uiPrompt("Gemini API anahtarı (alma: aistudio.google.com/apikey)", "", false, onAiKey);
}

static String tmpCity;
static void onDistrict(const char* t) {
  if (takeMutex(weatherMutex)) {
    WEATHER_CONFIG_DOC["city"] = tmpCity;
    WEATHER_CONFIG_DOC["district"] = String(t);
    WEATHER_CONFIG_DOC["neighbourhood"] = "";
    WEATHER_CONFIG_DOC["latitude"] = "";   // boş: sonraki hava turunda adresten koordinat bulunur
    WEATHER_CONFIG_DOC["longitude"] = "";
    WEATHER_CONFIG_DOC["city_name"] = "";
    saveWeatherConfig();
    xSemaphoreGive(weatherMutex);
  }
  weatherRefreshRequested = true;
}
static void onCity(const char* t) {
  tmpCity = t;
  uiPrompt("İlçe (isteğe bağlı)", "", false, onDistrict);
}
static void onSetLoc(lv_event_t*) { uiPrompt("Şehir (hava durumu konumu)", "", false, onCity); }

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
  uiBtn(r, "Hava konumu", false, onSetLoc);
  uiBtn(r, "Telegram", false, onSetTg);
  uiBtn(r, "Yapay zeka anahtarı", false, onSetAi);
  uiBtn(r, "Yeniden başlat", false, onSetRestart);
  uiBtn(r, "WiFi'leri sil", false, onSetWipe);
}

static void tickSet() {
  static uint32_t last = 0;
  if (millis() - last < 2000) return;
  last = millis();
  uint32_t up = millis() / 1000;
  char b[360];
  snprintf(b, sizeof(b),
           "Ağ: %s   Sinyal: %d dBm\nIP: %s   Adres: %s.local\nSürüm: %s   Telegram: %s\nÇalışma: %lu sa %lu dk   Bellek: %u KB boş",
           WiFi.SSID().c_str(), WiFi.RSSI(), WiFi.localIP().toString().c_str(), "crowhub", FW_VERSION,
           telegramConfigured() ? "açık" : "kapalı", efektAnahtarVar() ? "anahtar var" : "anahtar yok", (unsigned long)(up / 3600), (unsigned long)(up / 60 % 60), (unsigned)(ESP.getFreeHeap() / 1024));
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
volatile int devTimerReq = -1;

// ---- hava karti: yerlesim (sahne koordinatlari x olcek) ve mini ikonlar
static void layoutSky(int w) {
  float sc = (w - 6) / (float)SKY_W;
  if (w - 6 >= 640) { sc = 2.0f; lv_obj_set_width(skyCard, 646); }  // yatay: tam 1:1 (hizli, net)
  auto P = [&](float v) { return (int)(v * sc + 0.5f); };
  lv_image_set_scale(skyImg, (int)(128 * sc));
  lv_obj_set_height(skyCard, P(SKY_H) + 6);
  auto place = [&](lv_obj_t* o, float x, float y, float ww, float hh) {
    lv_obj_set_pos(o, P(x), P(y));
    lv_obj_set_size(o, P(ww), P(hh));
    lv_obj_set_style_radius(o, P(7), 0);
  };
  place(boxT, 6, 6, 150, 80);
  place(boxW, 164, 6, 150, 80);
  place(boxF, 6, 134, 308, 46);
  lv_obj_set_pos(lblWxDesc, P(6), P(2));
  lv_obj_set_pos(lblTemp, P(4), P(26));
  lv_obj_align_to(lblTempC, lblTemp, LV_ALIGN_OUT_RIGHT_BOTTOM, 4, -6);
  lv_obj_set_pos(lblWindT, P(6), P(2));
  lv_obj_align(lblWindDir, LV_ALIGN_TOP_RIGHT, -P(6), P(2));
  lv_obj_set_pos(lblWindVal, P(4), P(26));
  lv_obj_align_to(lblWindU, lblWindVal, LV_ALIGN_OUT_RIGHT_BOTTOM, 4, -6);
  for (int i = 0; i < 7; i++) {
    place(fcCol[i], 10 + i * 43 + 1, 136, 41, 42);
    lv_obj_set_pos(fcDay[i], 0, 0);
    lv_obj_set_pos(fcTmp[i], 0, P(11));
    lv_obj_set_width(fcDay[i], P(41));
    lv_obj_set_width(fcTmp[i], P(41));
    lv_obj_set_style_text_align(fcDay[i], LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_align(fcTmp[i], LV_TEXT_ALIGN_CENTER, 0);
    lv_image_set_scale(fcIcon[i], (int)(256 * sc * 0.75f));
    lv_obj_set_pos(fcIcon[i], P(41 / 2.0f - IC_W * 0.75f / 2), P(23));
  }
}

struct Ic {  // ARGB8888 tuvale cizim (seffaf arka plan)
  lv_obj_t* cv;
  void px(int x, int y, lv_color_t c) { if ((unsigned)x < IC_W && (unsigned)y < IC_H) lv_canvas_set_px(cv, x, y, c, LV_OPA_COVER); }
  void hline(int x, int y, int w, lv_color_t c) { for (int i = 0; i < w; i++) px(x + i, y, c); }
  void rect(int x, int y, int w, int h, lv_color_t c) { for (int j = 0; j < h; j++) hline(x, y + j, w, c); }
  void disc(int cx, int cy, int r, lv_color_t c) {
    for (int dy = -r; dy <= r; dy++) { int hw = (int)sqrtf((float)(r * r - dy * dy)); hline(cx - hw, cy + dy, 2 * hw + 1, c); }
  }
  void line(int x0, int y0, int x1, int y1, lv_color_t c) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1, e = dx + dy;
    for (;;) {
      px(x0, y0, c);
      if (x0 == x1 && y0 == y1) break;
      int e2 = 2 * e;
      if (e2 >= dy) { e += dy; x0 += sx; }
      if (e2 <= dx) { e += dx; y0 += sy; }
    }
  }
  void cloud(int cx, int cy, lv_color_t c) { disc(cx - 5, cy + 1, 4, c); disc(cx + 1, cy - 2, 5, c); disc(cx + 6, cy + 1, 4, c); rect(cx - 5, cy + 1, 11, 4, c); }
};

static void drawMiniIcon(lv_obj_t* cv, int code, bool past) {
  lv_canvas_fill_bg(cv, lv_color_black(), LV_OPA_TRANSP);
  Ic g{cv};
  lv_color_t gray = lv_color_hex(0x969696);
  lv_color_t sun = past ? gray : WX_TODAY, cloud = past ? gray : lv_color_hex(0xE6E0F8), drop = past ? gray : WX_WIND;
  int cx = IC_W / 2, cy = IC_H / 2;
  int sc = code < 0 ? 0 : code == 0 ? 1 : (code == 1 || code == 2) ? 2 : (code == 45 || code == 48) ? 4 : (code >= 51 && code <= 57) ? 5
         : ((code >= 61 && code <= 67) || (code >= 80 && code <= 82)) ? 5 : ((code >= 71 && code <= 77) || code == 85 || code == 86) ? 6 : code >= 95 ? 7 : 3;
  switch (sc) {
    case 1:
      g.disc(cx, cy, 5, sun);
      for (int a = 0; a < 8; a++) { float r = a * PI / 4; g.line(cx + cosf(r) * 7, cy + sinf(r) * 7, cx + cosf(r) * 9, cy + sinf(r) * 9, sun); }
      break;
    case 2: g.disc(cx - 4, cy - 3, 5, sun); g.cloud(cx + 2, cy + 2, cloud); break;
    case 4: for (int k = -1; k <= 1; k++) g.hline(cx - 9, cy + k * 5, 18, gray); break;
    case 5:
      g.cloud(cx, cy - 3, cloud);
      for (int k = -1; k <= 1; k++) g.line(cx + k * 5, cy + 5, cx + k * 5 - 2, cy + 9, drop);
      break;
    case 6: g.cloud(cx, cy - 3, cloud); for (int k = -1; k <= 1; k++) g.disc(cx + k * 5, cy + 7, 1, lv_color_white()); break;
    case 7:
      g.cloud(cx, cy - 3, cloud);
      g.line(cx + 1, cy + 3, cx - 2, cy + 7, sun); g.line(cx - 2, cy + 7, cx + 2, cy + 7, sun); g.line(cx + 2, cy + 7, cx - 1, cy + 10, sun);
      break;
    case 0: break;
    default: g.cloud(cx, cy, cloud); break;
  }
  lv_obj_invalidate(cv);
}

// Gokyuzu sahnesi: Ana sekmesi acikken ~10 kare/sn; kart genisligine gore olcek
static void skyTick(lv_timer_t*) {
  if (!tv || lv_tabview_get_tab_active(tv) != 0 || !skyImg) return;
  int w = lv_obj_get_width(skyCard);
  if (w != skyCardW && w > 40) {
    skyCardW = w;
    layoutSky(w);
  }
  skyStep();
  lv_obj_invalidate(skyImg);
}

static void tick(lv_timer_t*) {
  if (devTabReq >= 0) { lv_tabview_set_active(tv, devTabReq, LV_ANIM_OFF); devTabReq = -1; }
  if (devRotReq) { devRotReq = false; uiRotate(); }
  if (devTimerReq >= 0) { int i = devTimerReq; devTimerReq = -1; uiTimerOpen(i); }
  uint32_t act = lv_tabview_get_tab_active(tv);
  tickHome();  // saat her zaman; ucuz
  if (act == 1) tickLamps();
  if (act == 2) tickFx();
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

  lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_gap(scr, 0, 0);
  tv = lv_tabview_create(scr);
  lv_obj_set_width(tv, LV_PCT(100));
  lv_obj_set_flex_grow(tv, 1);
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
  tabFx = newTab("Efekt");
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
  // Alt kayan yazi seridi (tum sekmelerde): dünya haberleri
  lv_obj_t* tk = lv_obj_create(scr);
  lv_obj_set_size(tk, LV_PCT(100), 40);
  lv_obj_set_style_bg_color(tk, C_CARD, 0);
  lv_obj_set_style_bg_opa(tk, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(tk, C_FRAME, 0);
  lv_obj_set_style_border_width(tk, 3, 0);
  lv_obj_set_style_radius(tk, 0, 0);
  lv_obj_set_style_pad_hor(tk, 8, 0);
  lv_obj_set_style_pad_ver(tk, 2, 0);
  lv_obj_remove_flag(tk, LV_OBJ_FLAG_SCROLLABLE);
  lblNews = uiLabel(tk, "Haberler yükleniyor…", &noto20, C_CYAN);
  lv_obj_set_width(lblNews, LV_PCT(100));
  lv_label_set_long_mode(lblNews, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_obj_align(lblNews, LV_ALIGN_LEFT_MID, 0, 0);
  buildHome();
  buildLamps();
  buildFx();
  buildSet();
  placeTabs();
  lv_timer_create(tick, 500, nullptr);
  lv_timer_create(skyTick, 100, nullptr);
  efektStart();
  return scr;
}
