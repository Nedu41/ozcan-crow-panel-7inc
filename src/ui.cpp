// LVGL arayüzü: ekran sürücüsü bağlantısı + WiFi kurulum akışı + yardımcı pencereler.
// HUB sayfaları (ana, lambalar, efekt, ayarlar): ui_hub.cpp
#include "ui.h"

#include <Preferences.h>
#include <WiFi.h>
#include <lvgl.h>

#include "board.h"
#include "hub.h"
#include "ui_common.h"
#include "ui_hub.h"
#include "wifi_nets.h"

LGFX lcd;
static uint8_t rot = 0;
static int backlightPct = 100;
static lv_display_t* disp;

// ------------------------------------------------------------------ ekran / dokunmatik bağlantısı
// ---- Ekran renk ayari: kontrast (%50..130) ve renk parlakligi (%40..100). Arka isiktan bagimsiz, yazilim (flush'ta kanal tablosu)
static int contrastPct = 100, gainPct = 100, satPct = 140;  // satPct: canlilik (doygunluk) %80..220
static uint8_t lutR[32], lutG[64], lutB[32];
static bool lutIdentity = true;
static void buildLut() {
  lutIdentity = contrastPct == 100 && gainPct == 100 && satPct == 100;
  float c = contrastPct / 100.0f, g = gainPct / 100.0f;
  auto f = [&](float v) {  // v: 0..1
    float o = ((v - 0.5f) * c + 0.5f) * g;
    return o < 0 ? 0.f : o > 1 ? 1.f : o;
  };
  for (int i = 0; i < 32; i++) { lutR[i] = lutB[i] = (uint8_t)(f(i / 31.0f) * 31.0f + 0.5f); }
  for (int i = 0; i < 64; i++) lutG[i] = (uint8_t)(f(i / 63.0f) * 63.0f + 0.5f);
}
void uiSetColorAdjust(int contrast, int gain, bool save) {
  contrastPct = constrain(contrast, 50, 130);
  gainPct = constrain(gain, 40, 100);
  buildLut();
  if (save) {
    Preferences p;
    p.begin("ui", false);
    p.putUChar("ct", contrastPct);
    p.putUChar("dg", gainPct);
    p.end();
  }
  lv_obj_invalidate(lv_screen_active());
}
void uiSetSaturation(int sat, bool save) {
  satPct = constrain(sat, 80, 220);
  buildLut();
  if (save) {
    Preferences p;
    p.begin("ui", false);
    p.putUChar("sa", (uint8_t)(satPct / 2));  // 40..110 -> uint8
    p.end();
  }
  lv_obj_invalidate(lv_screen_active());
}
int uiGetSaturation() { return satPct; }
int uiGetContrast() { return contrastPct; }
int uiGetGain() { return gainPct; }

static void flushCb(lv_display_t* d, const lv_area_t* a, uint8_t* px) {
  if (!lutIdentity) {
    uint16_t* p = (uint16_t*)px;
    uint32_t n = (uint32_t)(a->x2 - a->x1 + 1) * (a->y2 - a->y1 + 1);
    const int sat = satPct * 128 / 100;  // 128 = %100
    for (uint32_t i = 0; i < n; i++) {
      uint16_t c = p[i];
      int r = (c >> 11) << 3, g = ((c >> 5) & 63) << 2, b = (c & 31) << 3;
      if (sat != 128) {  // doygunluk: parlaklik (luma) etrafinda renk farkini buyut
        int y = (r * 77 + g * 150 + b * 29) >> 8;
        r = y + (((r - y) * sat) >> 7); g = y + (((g - y) * sat) >> 7); b = y + (((b - y) * sat) >> 7);
        r = r < 0 ? 0 : r > 255 ? 255 : r; g = g < 0 ? 0 : g > 255 ? 255 : g; b = b < 0 ? 0 : b > 255 ? 255 : b;
      }
      p[i] = (uint16_t)((lutR[r >> 3] << 11) | (lutG[g >> 2] << 5) | lutB[b >> 3]);
    }
  }
  lcd.pushImage(a->x1, a->y1, a->x2 - a->x1 + 1, a->y2 - a->y1 + 1, (lgfx::rgb565_t*)px);
  lv_display_flush_ready(d);
}

static void touchCb(lv_indev_t*, lv_indev_data_t* d) {
  static bool down = false;
  static int x = 0, y = 0;
  Touch t = touchRead();
  if (t.fresh) {
    down = t.down;
    if (down) touchToScreen(t.x, t.y, rot, x, y);
  }
  d->state = down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
  d->point.x = x;
  d->point.y = y;
}

// Arka ışık: GPIO2 (MT9201 EN), PWM. Donanımda PWM ile kısma henüz doğrulanmadı (%100 = sürekli HIGH).
static void applyBacklight() {
  int duty = constrain(backlightPct, 10, 100) * 255 / 100;
  ledcWrite(0, duty);
}

void uiSetBacklight(int pct) {
  backlightPct = constrain(pct, 10, 100);
  applyBacklight();
  Preferences p;
  p.begin("ui", false);
  p.putUChar("bl", backlightPct);
  p.end();
}
int uiGetBacklight() { return backlightPct; }
bool uiIsLandscape() { return lcd.width() > lcd.height(); }

void uiRotate() {
  rot = (rot + 1) & 3;
  lcd.setRotation(rot);
  lv_display_set_resolution(disp, lcd.width(), lcd.height());
  Preferences p;
  p.begin("ui", false);
  p.putUChar("rot", rot);
  p.end();
  hubUiOnRotate();
  lv_obj_invalidate(lv_screen_active());
}

void guiInit() {
  Preferences p;
  p.begin("ui", false);
  rot = p.getUChar("rot", 0) & 3;
  backlightPct = p.getUChar("bl", 100);
  contrastPct = p.getUChar("ct", 100);
  gainPct = p.getUChar("dg", 100);
  satPct = p.getUChar("sa", 70) * 2;  // ilk kurulumda %140
  buildLut();
  p.end();

  boardPowerSeq();
  lcd.init();
  lcd.setRotation(rot);
  lcd.fillScreen(TFT_BLACK);
  ledcSetup(0, 5000, 8);
  ledcAttachPin(PIN_BL, 0);
  applyBacklight();

  lv_init();
  lv_tick_set_cb([]() -> uint32_t { return millis(); });
  const uint32_t bufPx = 800 * 40;
  void* b1 = heap_caps_malloc(bufPx * 2, MALLOC_CAP_SPIRAM);
  void* b2 = heap_caps_malloc(bufPx * 2, MALLOC_CAP_SPIRAM);
  disp = lv_display_create(lcd.width(), lcd.height());
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
  lv_display_set_flush_cb(disp, flushCb);
  lv_display_set_buffers(disp, b1, b2, bufPx * 2, LV_DISPLAY_RENDER_MODE_PARTIAL);

  lv_indev_t* in = lv_indev_create();
  lv_indev_set_type(in, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(in, touchCb);
}

void guiLoop() { lv_timer_handler(); }

// ------------------------------------------------------------------ ekranlar
static lv_obj_t *scrList, *scrPass, *scrBusy, *scrHome;
static lv_obj_t *listBox, *listInfo;
static lv_obj_t *passSsid, *passTa, *passMsg, *passKb;
static lv_obj_t *busyLbl;

enum State { ST_IDLE, ST_SCANNING, ST_CONNECTING };
static State st = ST_IDLE;
static uint32_t stSince = 0;
static String curSsid, curPass;
static bool connectFromBoot = false;
static int bootIdx = 0;  // açılışta kayıtlı ağlardan sıradaki

static lv_obj_t* newScreen() {
  lv_obj_t* s = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(s, C_BG, 0);
  lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(s, 0, 0);
  lv_obj_set_style_radius(s, 0, 0);
  lv_obj_set_style_pad_all(s, 12, 0);
  lv_obj_set_style_pad_gap(s, 10, 0);
  lv_obj_set_style_text_color(s, C_TEXT, 0);
  lv_obj_set_style_text_font(s, &noto20, 0);
  lv_obj_set_flex_flow(s, LV_FLEX_FLOW_COLUMN);
  lv_obj_remove_flag(s, LV_OBJ_FLAG_SCROLLABLE);
  return s;
}

static void showScan();
static void showPass(const String& ssid);
static void showHome();
static void startConnect(const String& ssid, const String& pass, bool fromBoot);

// ---- Ağ listesi
// Kayıtlı ağ mı? (şifresi listede varsa tekrar sorulmaz)
static bool savedNet(const String& ssid, String* pass = nullptr) {
  for (int i = 0;; i++) {
    String a, b;
    if (!wifiNetsGet(i, a, b)) return false;
    if (a == ssid) { if (pass) *pass = b; return true; }
  }
}

static void onNetClick(lv_event_t* e) {
  const char* s = (const char*)lv_event_get_user_data(e);
  String ssid(s), pw;
  if (savedNet(ssid, &pw)) startConnect(ssid, pw, false);  // kayıtlı: şifre sorma (yanlışsa bağlanamayınca şifre ekranı açılır)
  else showPass(ssid);
}

static void fillList(int n) {
  lv_obj_clean(listBox);
  int idx[40], cnt = 0;
  for (int i = 0; i < n && cnt < 40; i++) idx[cnt++] = i;
  for (int a = 0; a < cnt; a++)
    for (int b = a + 1; b < cnt; b++)
      if (WiFi.RSSI(idx[b]) > WiFi.RSSI(idx[a])) { int t = idx[a]; idx[a] = idx[b]; idx[b] = t; }
  static char names[40][34];
  int shown = 0;
  for (int k = 0; k < cnt; k++) {
    String ssid = WiFi.SSID(idx[k]);
    if (ssid.length() == 0) continue;
    bool dup = false;
    for (int j = 0; j < shown; j++) if (ssid == names[j]) dup = true;
    if (dup) continue;
    strlcpy(names[shown], ssid.c_str(), sizeof(names[0]));
    int rssi = WiFi.RSSI(idx[k]);
    bool open = WiFi.encryptionType(idx[k]) == WIFI_AUTH_OPEN;

    lv_obj_t* b = lv_button_create(listBox);
    lv_obj_set_size(b, LV_PCT(100), 64);
    lv_obj_set_style_radius(b, 10, 0);
    lv_obj_set_style_bg_color(b, C_CARD, 0);
    lv_obj_set_style_border_color(b, C_FRAME, 0);
    lv_obj_set_style_border_width(b, 2, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_set_flex_flow(b, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(b, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t* wl = lv_label_create(b);
    lv_label_set_text(wl, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_font(wl, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(wl, rssi > -60 ? C_GREEN : (rssi > -75 ? C_ORANGE : C_RED), 0);
    lv_obj_t* nl = lv_label_create(b);
    lv_label_set_text(nl, names[shown]);
    lv_label_set_long_mode(nl, LV_LABEL_LONG_DOT);
    lv_obj_set_flex_grow(nl, 1);
    lv_obj_set_style_text_color(nl, C_TEXT, 0);
    lv_obj_t* sl = lv_label_create(b);
    lv_label_set_text_fmt(sl, "%s  %d dBm", savedNet(ssid) ? "kayıtlı" : (open ? "açık" : "şifreli"), rssi);
    lv_obj_set_style_text_font(sl, &noto16, 0);
    lv_obj_set_style_text_color(sl, C_DIM, 0);
    lv_obj_add_event_cb(b, onNetClick, LV_EVENT_CLICKED, names[shown]);
    shown++;
  }
  lv_label_set_text_fmt(listInfo, shown ? "%d ağ bulundu. Bağlanmak için birine dokun." : "Ağ bulunamadı. Yenile'ye dokun.", shown);
}

static void startScan() {
  WiFi.mode(WIFI_STA);
  WiFi.scanDelete();
  WiFi.scanNetworks(true);
  st = ST_SCANNING;
  stSince = millis();
  lv_label_set_text(listInfo, "Ağlar aranıyor…");
}

static void onRefresh(lv_event_t*) { startScan(); }
static void onListBack(lv_event_t*) {
  if (WiFi.status() == WL_CONNECTED) showHome();
}

static void buildList() {
  scrList = newScreen();
  lv_obj_t* top = uiRow(scrList);
  lv_obj_t* t = uiLabel(top, "WiFi Kurulumu", &noto28, C_CYAN);
  lv_obj_set_flex_grow(t, 1);
  uiBtn(top, "Geri", false, onListBack);
  uiBtn(top, LV_SYMBOL_REFRESH "  Yenile", false, onRefresh);
  listInfo = uiLabel(scrList, "", &noto20, C_DIM);
  listBox = lv_obj_create(scrList);
  lv_obj_set_width(listBox, LV_PCT(100));
  lv_obj_set_flex_grow(listBox, 1);
  lv_obj_set_style_bg_opa(listBox, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(listBox, 0, 0);
  lv_obj_set_style_pad_all(listBox, 0, 0);
  lv_obj_set_style_pad_gap(listBox, 8, 0);
  lv_obj_set_flex_flow(listBox, LV_FLEX_FLOW_COLUMN);
}

static void showScan() {
  lv_screen_load(scrList);
  startScan();
}

// ---- Şifre ekranı
static void onPassBack(lv_event_t*) { showScan(); }
static void onEye(lv_event_t*) {
  lv_textarea_set_password_mode(passTa, !lv_textarea_get_password_mode(passTa));
}
static void doConnectFromPass() {
  startConnect(curSsid, String(lv_textarea_get_text(passTa)), false);
}
static void onPassGo(lv_event_t*) { doConnectFromPass(); }
static void onKb(lv_event_t* e) {
  lv_event_code_t c = lv_event_get_code(e);
  if (c == LV_EVENT_READY) doConnectFromPass();
  else if (c == LV_EVENT_CANCEL) showScan();
}

static void styleKeyboard(lv_obj_t* kb) {
  lv_obj_set_style_bg_color(kb, C_BG, 0);
  lv_obj_set_style_pad_all(kb, 4, 0);
  lv_obj_set_style_bg_color(kb, C_BTN, LV_PART_ITEMS);
  lv_obj_set_style_text_color(kb, C_TEXT, LV_PART_ITEMS);
  lv_obj_set_style_text_font(kb, &noto20, LV_PART_ITEMS);
  lv_obj_set_style_border_color(kb, C_FRAME, LV_PART_ITEMS);
  lv_obj_set_style_border_width(kb, 2, LV_PART_ITEMS);
  lv_obj_set_style_shadow_width(kb, 0, LV_PART_ITEMS);
}

static void styleTextarea(lv_obj_t* ta) {
  lv_obj_set_style_bg_color(ta, C_CARD, 0);
  lv_obj_set_style_text_color(ta, C_TEXT, 0);
  lv_obj_set_style_border_color(ta, C_FRAME, 0);
  lv_obj_set_style_border_width(ta, 2, 0);
  lv_obj_set_style_text_font(ta, &noto28, 0);
}

static void buildPass() {
  scrPass = newScreen();
  lv_obj_set_style_pad_all(scrPass, 8, 0);
  lv_obj_t* top = uiRow(scrPass);
  uiLabel(top, "Şifre", &noto28, C_CYAN);
  passSsid = uiLabel(top, "", &noto28, C_ORANGE);
  lv_obj_set_flex_grow(passSsid, 1);
  lv_label_set_long_mode(passSsid, LV_LABEL_LONG_DOT);

  lv_obj_t* r = uiRow(scrPass);
  passTa = lv_textarea_create(r);
  lv_obj_set_flex_grow(passTa, 1);
  lv_textarea_set_one_line(passTa, true);
  lv_textarea_set_password_mode(passTa, true);
  lv_textarea_set_placeholder_text(passTa, "WiFi şifresi");
  styleTextarea(passTa);
  uiBtn(r, LV_SYMBOL_EYE_OPEN, false, onEye);

  lv_obj_t* r2 = uiRow(scrPass);
  uiBtn(r2, "Geri", false, onPassBack);
  uiBtn(r2, "Bağlan", true, onPassGo);
  passMsg = uiLabel(r2, "", &noto20, C_RED);
  lv_obj_set_flex_grow(passMsg, 1);

  passKb = lv_keyboard_create(scrPass);
  lv_obj_set_size(passKb, LV_PCT(100), LV_PCT(48));
  styleKeyboard(passKb);
  lv_keyboard_set_textarea(passKb, passTa);
  lv_obj_add_event_cb(passKb, onKb, LV_EVENT_ALL, nullptr);
}

static void showPass(const String& ssid) {
  curSsid = ssid;
  lv_label_set_text(passSsid, ssid.c_str());
  lv_textarea_set_text(passTa, "");
  lv_textarea_set_password_mode(passTa, true);
  lv_label_set_text(passMsg, "");
  lv_screen_load(scrPass);
}

// ---- Bağlanıyor ekranı
static void onBusyCancel(lv_event_t*) {
  WiFi.disconnect();
  st = ST_IDLE;
  showScan();
}

static void buildBusy() {
  scrBusy = newScreen();
  lv_obj_set_flex_align(scrBusy, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_t* sp = lv_spinner_create(scrBusy);
  lv_obj_set_size(sp, 90, 90);
  lv_obj_set_style_arc_color(sp, C_CYAN, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(sp, C_CARD, LV_PART_MAIN);
  busyLbl = uiLabel(scrBusy, "", &noto28, C_TEXT);
  lv_obj_set_style_text_align(busyLbl, LV_TEXT_ALIGN_CENTER, 0);
  uiBtn(scrBusy, "Vazgeç", false, onBusyCancel);
}

static void startConnect(const String& ssid, const String& pass, bool fromBoot) {
  curSsid = ssid;
  curPass = pass;
  connectFromBoot = fromBoot;
  lv_label_set_text_fmt(busyLbl, "%s\nağına bağlanılıyor…", ssid.c_str());
  lv_screen_load(scrBusy);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  WiFi.disconnect();
  WiFi.begin(ssid.c_str(), pass.c_str());
  st = ST_CONNECTING;
  stSince = millis();
}

static void showHome() { lv_screen_load(scrHome); }

void uiShowWifiSetup() { showScan(); }

// ---- Durum makinesi (250 ms)
static void tick(lv_timer_t*) {
  if (st == ST_SCANNING) {
    int n = WiFi.scanComplete();
    if (n >= 0) {
      st = ST_IDLE;
      fillList(n);
      WiFi.scanDelete();
    } else if (n == WIFI_SCAN_FAILED || millis() - stSince > 15000) {
      st = ST_IDLE;
      lv_label_set_text(listInfo, "Arama başarısız. Yenile'ye dokun.");
    }
  } else if (st == ST_CONNECTING) {
    wl_status_t s = WiFi.status();
    uint32_t el = millis() - stSince;
    if (s == WL_CONNECTED) {
      st = ST_IDLE;
      // listeye ekle ve tercih edilen yap (varsa şifreyi günceller)
      wifiNetsAdd(curSsid, curPass);
      for (int i = 0;; i++) {
        String a, b;
        if (!wifiNetsGet(i, a, b)) break;
        if (a == curSsid) { if (i) wifiNetsMakePreferred(i); break; }
      }
      hubOnWifiUp();
      showHome();
    } else if (el > 12000 || (s == WL_CONNECT_FAILED && el > 3000)) {
      st = ST_IDLE;
      WiFi.disconnect();
      String ns, np;
      if (connectFromBoot && wifiNetsGet(++bootIdx, ns, np)) {
        startConnect(ns, np, true);
      } else if (connectFromBoot) {
        showScan();
        lv_label_set_text(listInfo, "Kayıtlı ağlara bağlanılamadı. Bir ağ seç.");
      } else {
        showPass(curSsid);
        lv_label_set_text(passMsg, s == WL_CONNECT_FAILED ? "Bağlanamadı: şifre yanlış olabilir" : "Bağlanamadı (zaman aşımı)");
      }
    }
  }
}

// ------------------------------------------------------------------ Metin girişi / onay pencereleri
static lv_obj_t* overlay;
static lv_obj_t* ovTa;
static UiPromptCb ovPromptCb;
static UiConfirmCb ovConfirmCb;

static void closeOverlay() {
  if (overlay) lv_obj_delete_async(overlay);
  overlay = nullptr;
}

static lv_obj_t* newOverlay() {
  closeOverlay();
  overlay = lv_obj_create(lv_layer_top());
  lv_obj_set_size(overlay, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(overlay, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(overlay, LV_OPA_80, 0);
  lv_obj_set_style_border_width(overlay, 0, 0);
  lv_obj_set_style_radius(overlay, 0, 0);
  lv_obj_set_style_pad_all(overlay, 8, 0);
  lv_obj_set_flex_flow(overlay, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_gap(overlay, 8, 0);
  lv_obj_remove_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_text_color(overlay, C_TEXT, 0);
  lv_obj_set_style_text_font(overlay, &noto20, 0);
  return overlay;
}

static void ovKb(lv_event_t* e) {
  lv_event_code_t c = lv_event_get_code(e);
  if (c == LV_EVENT_READY) {
    UiPromptCb cb = ovPromptCb;
    String v = lv_textarea_get_text(ovTa);
    closeOverlay();
    if (cb) cb(v.c_str());
  } else if (c == LV_EVENT_CANCEL) {
    closeOverlay();
  }
}
static void ovOk(lv_event_t*) {
  UiPromptCb cb = ovPromptCb;
  String v = lv_textarea_get_text(ovTa);
  closeOverlay();
  if (cb) cb(v.c_str());
}
static void ovCancel(lv_event_t*) { closeOverlay(); }
static void ovEye(lv_event_t*) { lv_textarea_set_password_mode(ovTa, !lv_textarea_get_password_mode(ovTa)); }

void uiPrompt(const char* title, const char* initial, bool password, UiPromptCb cb) {
  ovPromptCb = cb;
  lv_obj_t* o = newOverlay();
  lv_obj_t* top = uiRow(o);
  lv_obj_t* t = uiLabel(top, title, &noto28, C_CYAN);
  lv_obj_set_flex_grow(t, 1);
  lv_obj_t* r = uiRow(o);
  ovTa = lv_textarea_create(r);
  lv_obj_set_flex_grow(ovTa, 1);
  lv_textarea_set_one_line(ovTa, true);
  lv_textarea_set_password_mode(ovTa, password);
  lv_textarea_set_text(ovTa, initial ? initial : "");
  styleTextarea(ovTa);
  if (password) uiBtn(r, LV_SYMBOL_EYE_OPEN, false, ovEye);
  lv_obj_t* r2 = uiRow(o);
  uiBtn(r2, "Vazgeç", false, ovCancel);
  uiBtn(r2, "Tamam", true, ovOk);
  lv_obj_t* kb = lv_keyboard_create(o);
  lv_obj_set_size(kb, LV_PCT(100), LV_PCT(48));
  styleKeyboard(kb);
  lv_keyboard_set_textarea(kb, ovTa);
  lv_obj_add_event_cb(kb, ovKb, LV_EVENT_ALL, nullptr);
}

static void ovYes(lv_event_t*) {
  UiConfirmCb cb = ovConfirmCb;
  closeOverlay();
  if (cb) cb();
}

void uiConfirm(const char* text, UiConfirmCb cb) {
  ovConfirmCb = cb;
  lv_obj_t* o = newOverlay();
  lv_obj_set_flex_align(o, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_t* card = uiCard(o);
  lv_obj_set_width(card, LV_PCT(80));
  lv_obj_t* l = uiLabel(card, text, &noto28, C_TEXT);
  lv_obj_set_width(l, LV_PCT(100));
  lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
  lv_obj_t* r = uiRow(card);
  uiBtn(r, "Vazgeç", false, ovCancel);
  uiBtn(r, "Evet", true, ovYes);
}

// ------------------------------------------------------------------ başlangıç
void uiStart() {
  buildList();
  buildPass();
  buildBusy();
  scrHome = hubUiCreate();
  lv_timer_create(tick, 250, nullptr);
  String s0, p0;
  bootIdx = 0;
  if (wifiNetsGet(0, s0, p0)) startConnect(s0, p0, true);
  else showScan();
}
