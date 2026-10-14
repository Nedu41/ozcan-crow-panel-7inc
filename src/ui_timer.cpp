// Lamba zamanlayıcı penceresi (web'deki zamanlayıcı bölümüyle aynı alanlar):
// röle seçimi, başlangıç/bitiş saati, günler, gündoğumu/günbatımı eylemi, Kaydet / Sil.
#include <ArduinoJson.h>

#include "app_state.h"
#include "config.h"
#include "ui_common.h"
#include "ui_hub.h"
#include "web_server.h"

struct TimerJob { char ip[20]; int relay, sh, sm, eh, em, days; char rise[4], set[4]; };
static QueueHandle_t jobQ;

static void jobTask(void*) {
  TimerJob j;
  for (;;) {
    if (xQueueReceive(jobQ, &j, portMAX_DELAY) != pdTRUE) continue;
    scheduleApply(j.ip, j.relay, j.sh, j.sm, j.eh, j.em, j.days, j.rise, j.set);
  }
}

static lv_obj_t *dlg, *title, *relBtn[MAX_RELAYS], *rl[4], *dayBtn[7], *riseBtn, *setBtn;
static int cur[2];  // seçili cihaz/röle
static int tDev = -1, tRelay = 1, tDays = 127, tRise = 0, tSet = 0;
static char tIp[20], tName[40];
static const char* ACT_TXT[] = {"Pasif", "AÇ", "KAPAT"};
static const char* ACT_VAL[] = {"0", "ON", "OFF"};
static const char* DAYS[] = {"Pt", "Sa", "Ça", "Pe", "Cu", "Ct", "Pz"};

static int actIdx(const String& s) { return s == "ON" ? 1 : s == "OFF" ? 2 : 0; }

static void loadValues() {
  int sh = 8, sm = 0, eh = 18, em = 0;
  tDays = 127; tRise = tSet = 0;
  if (xSemaphoreTake(scheduleMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    JsonObject o = SCHEDULE_CONFIG_DOC[tName].as<JsonObject>();
    if (!o.isNull()) {
      String k = "r" + String(tRelay);
      String s = o[k + "_start"] | "", e = o[k + "_end"] | "";
      if (s.length() == 5) { sh = s.substring(0, 2).toInt(); sm = s.substring(3).toInt(); }
      if (e.length() == 5) { eh = e.substring(0, 2).toInt(); em = e.substring(3).toInt(); }
      if (o[k + "_days"].is<int>()) tDays = o[k + "_days"].as<int>();
      tRise = actIdx(o[k + "_sunrise_act"] | "");
      tSet = actIdx(o[k + "_sunset_act"] | "");
    }
    xSemaphoreGive(scheduleMutex);
  }
  lv_roller_set_selected(rl[0], constrain(sh, 0, 23), LV_ANIM_OFF);
  lv_roller_set_selected(rl[1], constrain(sm, 0, 59), LV_ANIM_OFF);
  lv_roller_set_selected(rl[2], constrain(eh, 0, 23), LV_ANIM_OFF);
  lv_roller_set_selected(rl[3], constrain(em, 0, 59), LV_ANIM_OFF);
  for (int i = 0; i < 7; i++) uiBtnOn(dayBtn[i], tDays & (1 << i), C_CYAN);
  for (int r = 0; r < MAX_RELAYS; r++) uiBtnOn(relBtn[r], tRelay == r + 1, C_ORANGE);
  char b[40];
  snprintf(b, sizeof(b), "Gündoğumu: %s", ACT_TXT[tRise]);
  lv_label_set_text(uiBtnLabel(riseBtn), b);
  snprintf(b, sizeof(b), "Günbatımı: %s", ACT_TXT[tSet]);
  lv_label_set_text(uiBtnLabel(setBtn), b);
}

static void onRel(lv_event_t* e) { tRelay = (int)(intptr_t)lv_event_get_user_data(e); loadValues(); }
static void onDay(lv_event_t* e) {
  int i = (int)(intptr_t)lv_event_get_user_data(e);
  tDays ^= 1 << i;
  uiBtnOn(dayBtn[i], tDays & (1 << i), C_CYAN);
}
static void onRise(lv_event_t*) {
  tRise = (tRise + 1) % 3;
  char b[40];
  snprintf(b, sizeof(b), "Gündoğumu: %s", ACT_TXT[tRise]);
  lv_label_set_text(uiBtnLabel(riseBtn), b);
}
static void onSet(lv_event_t*) {
  tSet = (tSet + 1) % 3;
  char b[40];
  snprintf(b, sizeof(b), "Günbatımı: %s", ACT_TXT[tSet]);
  lv_label_set_text(uiBtnLabel(setBtn), b);
}
static void onClose(lv_event_t*) { lv_obj_add_flag(dlg, LV_OBJ_FLAG_HIDDEN); }
static void send(bool del) {
  TimerJob j{};
  strlcpy(j.ip, tIp, sizeof(j.ip));
  j.relay = tRelay;
  j.sh = lv_roller_get_selected(rl[0]); j.sm = lv_roller_get_selected(rl[1]);
  j.eh = lv_roller_get_selected(rl[2]); j.em = lv_roller_get_selected(rl[3]);
  j.days = del ? 0 : tDays;
  strlcpy(j.rise, ACT_VAL[del ? 0 : tRise], sizeof(j.rise));
  strlcpy(j.set, ACT_VAL[del ? 0 : tSet], sizeof(j.set));
  xQueueSend(jobQ, &j, 0);
  lv_obj_add_flag(dlg, LV_OBJ_FLAG_HIDDEN);
}
static void onSave(lv_event_t*) { send(false); }
static void onDel(lv_event_t*) { send(true); }

static lv_obj_t* flexRow(lv_obj_t* par) {
  lv_obj_t* r = lv_obj_create(par);
  lv_obj_set_size(r, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(r, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(r, 0, 0);
  lv_obj_set_style_pad_all(r, 0, 0);
  lv_obj_set_style_pad_gap(r, 8, 0);
  lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_remove_flag(r, LV_OBJ_FLAG_SCROLLABLE);
  return r;
}

static lv_obj_t* mkRoller(lv_obj_t* par, int n) {
  static char opts[2][200];
  char* o = opts[n == 24 ? 0 : 1];
  o[0] = 0;
  for (int i = 0; i < n; i++) {
    char b[6];
    snprintf(b, sizeof(b), i ? "\n%02d" : "%02d", i);
    strcat(o, b);
  }
  lv_obj_t* r = lv_roller_create(par);
  lv_roller_set_options(r, o, LV_ROLLER_MODE_INFINITE);
  lv_roller_set_visible_row_count(r, 3);
  lv_obj_set_width(r, 78);
  lv_obj_set_style_text_font(r, &noto28, 0);
  lv_obj_set_style_bg_color(r, C_BTN, 0);
  lv_obj_set_style_text_color(r, C_TEXT, 0);
  lv_obj_set_style_border_color(r, C_FRAME, 0);
  lv_obj_set_style_border_width(r, 2, 0);
  lv_obj_set_style_bg_color(r, C_CYAN, LV_PART_SELECTED);
  lv_obj_set_style_text_color(r, C_ONPASTEL, LV_PART_SELECTED);
  return r;
}

static void build() {
  jobQ = xQueueCreate(4, sizeof(TimerJob));
  xTaskCreatePinnedToCore(jobTask, "UiTimer", 6144, nullptr, 1, nullptr, 0);
  dlg = lv_obj_create(lv_layer_top());
  lv_obj_set_size(dlg, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(dlg, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(dlg, LV_OPA_80, 0);
  lv_obj_set_style_border_width(dlg, 0, 0);
  lv_obj_set_style_radius(dlg, 0, 0);
  lv_obj_set_style_pad_all(dlg, 8, 0);
  lv_obj_remove_flag(dlg, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(dlg, LV_OBJ_FLAG_HIDDEN);

  lv_obj_t* c = uiCard(dlg);
  lv_obj_set_size(c, LV_PCT(100), LV_PCT(100));
  lv_obj_add_flag(c, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(c, LV_DIR_VER);
  title = uiLabel(c, "", &noto28, C_CYAN);

  lv_obj_t* r = flexRow(c);
  uiLabel(r, "Röle", &noto20, C_DIM);
  for (int i = 0; i < MAX_RELAYS; i++) {
    char b[8];
    snprintf(b, sizeof(b), "R%d", i + 1);
    relBtn[i] = uiBtn(r, b, false, nullptr, 80, 48);
    lv_obj_add_event_cb(relBtn[i], onRel, LV_EVENT_CLICKED, (void*)(intptr_t)(i + 1));
  }

  r = flexRow(c);
  uiLabel(r, "Başla", &noto20, C_DIM);
  rl[0] = mkRoller(r, 24);
  uiLabel(r, ":", &noto28, C_TEXT);
  rl[1] = mkRoller(r, 60);
  uiLabel(r, "   Bitiş", &noto20, C_DIM);
  rl[2] = mkRoller(r, 24);
  uiLabel(r, ":", &noto28, C_TEXT);
  rl[3] = mkRoller(r, 60);

  r = flexRow(c);
  for (int i = 0; i < 7; i++) {
    dayBtn[i] = uiBtn(r, DAYS[i], false, nullptr, 54, 48);
    lv_obj_set_style_pad_hor(dayBtn[i], 4, 0);
    lv_obj_add_event_cb(dayBtn[i], onDay, LV_EVENT_CLICKED, (void*)(intptr_t)i);
  }

  r = flexRow(c);
  riseBtn = uiBtn(r, "", false, onRise, LV_SIZE_CONTENT, 48);
  setBtn = uiBtn(r, "", false, onSet, LV_SIZE_CONTENT, 48);

  r = flexRow(c);
  uiBtn(r, "Kaydet", true, onSave, LV_SIZE_CONTENT, 52);
  uiBtn(r, "Sil", false, onDel, LV_SIZE_CONTENT, 52);
  uiBtn(r, "Kapat", false, onClose, LV_SIZE_CONTENT, 52);
}

void uiTimerOpen(int devIdx) {
  if (!dlg) build();
  JsonArray arr = doc.as<JsonArray>();
  if (devIdx < 0 || devIdx >= (int)arr.size()) return;
  tDev = devIdx;
  strlcpy(tIp, (const char*)(arr[devIdx]["ip"] | ""), sizeof(tIp));
  strlcpy(tName, (const char*)(arr[devIdx]["name"] | ""), sizeof(tName));
  tRelay = 1;
  int rc = deviceRelayCount(String(tName));
  for (int i = 0; i < MAX_RELAYS; i++) {  // cihazda olmayan roleleri gizle
    if (i < rc) lv_obj_remove_flag(relBtn[i], LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(relBtn[i], LV_OBJ_FLAG_HIDDEN);
  }
  char b[80];
  snprintf(b, sizeof(b), "%s - Zamanlayıcı", tName);
  lv_label_set_text(title, b);
  loadValues();
  lv_obj_remove_flag(dlg, LV_OBJ_FLAG_HIDDEN);
}
