// LVGL arayüzü: ekran sürücüsü bağlantısı + WiFi kurulum akışı + ana ekran (yer tutucu)
#include "ui.h"

#include <Preferences.h>
#include <WiFi.h>
#include <lvgl.h>

#include "board.h"

// ---- Tema: koyu/gotik, siyah çerçeve, pastel vurgu. Koyu zemin -> açık yazı, pastel zemin -> koyu yazı ----
#define C_BG     lv_color_hex(0x14141A)
#define C_CARD   lv_color_hex(0x1F1F28)
#define C_BTN    lv_color_hex(0x2C2C38)
#define C_FRAME  lv_color_hex(0x000000)
#define C_TEXT   lv_color_hex(0xDADAE4)
#define C_DIM    lv_color_hex(0x9A9AAE)
#define C_CYAN   lv_color_hex(0x7FE0DC)
#define C_ORANGE lv_color_hex(0xFFB27A)
#define C_GREEN  lv_color_hex(0x8FE0A0)
#define C_RED    lv_color_hex(0xFF9A9A)
#define C_ONPASTEL lv_color_hex(0x0E0E14)

LGFX lcd;
static uint8_t rot = 0;
static lv_display_t* disp;

// ------------------------------------------------------------------ ekran / dokunmatik bağlantısı
static void flushCb(lv_display_t* d, const lv_area_t* a, uint8_t* px) {
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

static void applyRotation(uint8_t r) {
  rot = r & 3;
  lcd.setRotation(rot);
  lv_display_set_resolution(disp, lcd.width(), lcd.height());
  Preferences p;
  p.begin("ui", false);
  p.putUChar("rot", rot);
  p.end();
}

void guiInit() {
  Preferences p;
  p.begin("ui", false);
  rot = p.getUChar("rot", 0) & 3;
  p.end();

  boardPowerSeq();
  lcd.init();
  lcd.setRotation(rot);
  lcd.fillScreen(TFT_BLACK);
  pinMode(PIN_BL, OUTPUT);
  digitalWrite(PIN_BL, HIGH);

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

// ------------------------------------------------------------------ WiFi kayıt
static String savedSsid, savedPass;

static void loadWifi() {
  Preferences p;
  p.begin("wifi", false);
  savedSsid = p.getString("ssid", "");
  savedPass = p.getString("pass", "");
  p.end();
}

static void saveWifi(const String& s, const String& pw) {
  Preferences p;
  p.begin("wifi", false);
  p.putString("ssid", s);
  p.putString("pass", pw);
  p.end();
  savedSsid = s;
  savedPass = pw;
}

// ------------------------------------------------------------------ ekranlar
extern "C" { extern const lv_font_t noto16, noto20, noto28; }

static lv_obj_t *scrList, *scrPass, *scrBusy, *scrHome;
static lv_obj_t *listBox, *listInfo;
static lv_obj_t *passSsid, *passTa, *passMsg, *passKb, *passEye;
static lv_obj_t *busyLbl, *busyCancel;
static lv_obj_t *homeInfo;

enum State { ST_IDLE, ST_SCANNING, ST_CONNECTING };
static State st = ST_IDLE;
static uint32_t stSince = 0;
static String curSsid, curPass;
static bool connectFromBoot = false;

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

static lv_obj_t* newLabel(lv_obj_t* par, const char* t, const lv_font_t* f, lv_color_t c) {
  lv_obj_t* l = lv_label_create(par);
  lv_label_set_text(l, t);
  lv_obj_set_style_text_font(l, f, 0);
  lv_obj_set_style_text_color(l, c, 0);
  return l;
}

// primary: pastel zemin + koyu yazı; değilse koyu düğme + açık yazı. Çerçeve hep siyah.
static lv_obj_t* newBtn(lv_obj_t* par, const char* t, bool primary, lv_event_cb_t cb, int w = LV_SIZE_CONTENT) {
  lv_obj_t* b = lv_button_create(par);
  lv_obj_set_size(b, w, 56);
  lv_obj_set_style_radius(b, 10, 0);
  lv_obj_set_style_bg_color(b, primary ? C_CYAN : C_BTN, 0);
  lv_obj_set_style_border_color(b, C_FRAME, 0);
  lv_obj_set_style_border_width(b, 3, 0);
  lv_obj_set_style_shadow_width(b, 0, 0);
  lv_obj_set_style_pad_hor(b, 20, 0);
  lv_obj_t* l = lv_label_create(b);
  lv_label_set_text(l, t);
  lv_obj_set_style_text_font(l, &noto20, 0);
  lv_obj_set_style_text_color(l, primary ? C_ONPASTEL : C_TEXT, 0);
  lv_obj_center(l);
  if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, nullptr);
  return b;
}

static lv_obj_t* newRow(lv_obj_t* par) {
  lv_obj_t* r = lv_obj_create(par);
  lv_obj_set_size(r, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(r, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(r, 0, 0);
  lv_obj_set_style_pad_all(r, 0, 0);
  lv_obj_set_style_pad_gap(r, 10, 0);
  lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_remove_flag(r, LV_OBJ_FLAG_SCROLLABLE);
  return r;
}

static void showScan();
static void showPass(const String& ssid);
static void showHome();
static void startConnect(const String& ssid, const String& pass, bool fromBoot);

// ---- Ağ listesi
static void onNetClick(lv_event_t* e) {
  const char* s = (const char*)lv_event_get_user_data(e);
  showPass(String(s));
}

static void fillList(int n) {
  lv_obj_clean(listBox);
  // aynı adlı ağları ele (en güçlü RSSI ilk gelsin diye sıralı gez)
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
    lv_obj_set_style_border_width(b, 3, 0);
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
    lv_label_set_text_fmt(sl, "%s  %d dBm", open ? "açık" : "şifreli", rssi);
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

static void buildList() {
  scrList = newScreen();
  lv_obj_t* top = newRow(scrList);
  lv_obj_t* t = newLabel(top, "WiFi Kurulumu", &noto28, C_CYAN);
  lv_obj_set_flex_grow(t, 1);
  newBtn(top, LV_SYMBOL_REFRESH "  Yenile", false, onRefresh);
  listInfo = newLabel(scrList, "", &noto20, C_DIM);
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
  bool pw = lv_textarea_get_password_mode(passTa);
  lv_textarea_set_password_mode(passTa, !pw);
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

static void buildPass() {
  scrPass = newScreen();
  lv_obj_set_style_pad_all(scrPass, 8, 0);
  lv_obj_t* top = newRow(scrPass);
  newLabel(top, "Şifre", &noto28, C_CYAN);
  passSsid = newLabel(top, "", &noto28, C_ORANGE);
  lv_obj_set_flex_grow(passSsid, 1);
  lv_label_set_long_mode(passSsid, LV_LABEL_LONG_DOT);

  lv_obj_t* r = newRow(scrPass);
  passTa = lv_textarea_create(r);
  lv_obj_set_flex_grow(passTa, 1);
  lv_textarea_set_one_line(passTa, true);
  lv_textarea_set_password_mode(passTa, true);
  lv_textarea_set_placeholder_text(passTa, "WiFi şifresi");
  lv_obj_set_style_bg_color(passTa, C_CARD, 0);
  lv_obj_set_style_text_color(passTa, C_TEXT, 0);
  lv_obj_set_style_border_color(passTa, C_FRAME, 0);
  lv_obj_set_style_border_width(passTa, 3, 0);
  lv_obj_set_style_text_font(passTa, &noto28, 0);
  passEye = newBtn(r, LV_SYMBOL_EYE_OPEN, false, onEye);

  lv_obj_t* r2 = newRow(scrPass);
  newBtn(r2, "Geri", false, onPassBack);
  newBtn(r2, "Bağlan", true, onPassGo);
  passMsg = newLabel(r2, "", &noto20, C_RED);
  lv_obj_set_flex_grow(passMsg, 1);

  passKb = lv_keyboard_create(scrPass);
  lv_obj_set_size(passKb, LV_PCT(100), LV_PCT(48));
  lv_obj_set_style_bg_color(passKb, C_BG, 0);
  lv_obj_set_style_pad_all(passKb, 4, 0);
  lv_obj_set_style_bg_color(passKb, C_BTN, LV_PART_ITEMS);
  lv_obj_set_style_text_color(passKb, C_TEXT, LV_PART_ITEMS);
  lv_obj_set_style_text_font(passKb, &noto20, LV_PART_ITEMS);
  lv_obj_set_style_border_color(passKb, C_FRAME, LV_PART_ITEMS);
  lv_obj_set_style_border_width(passKb, 2, LV_PART_ITEMS);
  lv_obj_set_style_shadow_width(passKb, 0, LV_PART_ITEMS);
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
  busyLbl = newLabel(scrBusy, "", &noto28, C_TEXT);
  lv_obj_set_style_text_align(busyLbl, LV_TEXT_ALIGN_CENTER, 0);
  busyCancel = newBtn(scrBusy, "Vazgeç", false, onBusyCancel);
}

static void startConnect(const String& ssid, const String& pass, bool fromBoot) {
  curSsid = ssid;
  curPass = pass;
  connectFromBoot = fromBoot;
  lv_label_set_text_fmt(busyLbl, "%s\nağına bağlanılıyor…", ssid.c_str());
  lv_screen_load(scrBusy);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.disconnect();
  WiFi.begin(ssid.c_str(), pass.c_str());
  st = ST_CONNECTING;
  stSince = millis();
}

// ---- Ana ekran (yer tutucu: HUB sayfaları buraya gelecek)
static void onHomeWifi(lv_event_t*) { showScan(); }
static void onHomeRot(lv_event_t*) {
  applyRotation(rot + 1);
  lv_obj_invalidate(lv_screen_active());
}

static void buildHome() {
  scrHome = newScreen();
  newLabel(scrHome, "CrowPanel HUB", &noto28, C_CYAN);
  lv_obj_t* card = lv_obj_create(scrHome);
  lv_obj_set_size(card, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_style_bg_color(card, C_CARD, 0);
  lv_obj_set_style_border_color(card, C_FRAME, 0);
  lv_obj_set_style_border_width(card, 3, 0);
  lv_obj_set_style_radius(card, 10, 0);
  lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  homeInfo = newLabel(card, "", &noto20, C_TEXT);
  lv_obj_t* r = newRow(scrHome);
  newBtn(r, "WiFi değiştir", false, onHomeWifi);
  newBtn(r, "Ekranı döndür", false, onHomeRot);
}

static void showHome() {
  lv_label_set_text_fmt(homeInfo, "Ağ: %s\nIP: %s\nSinyal: %d dBm", WiFi.SSID().c_str(),
                        WiFi.localIP().toString().c_str(), WiFi.RSSI());
  lv_screen_load(scrHome);
}

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
      saveWifi(curSsid, curPass);
      showHome();
    } else if (el > 20000 || (s == WL_CONNECT_FAILED && el > 3000)) {
      st = ST_IDLE;
      WiFi.disconnect();
      if (connectFromBoot) {
        showScan();
        lv_label_set_text(listInfo, "Kayıtlı ağa bağlanılamadı. Başka bir ağ seç.");
      } else {
        showPass(curSsid);
        lv_label_set_text(passMsg, s == WL_CONNECT_FAILED ? "Bağlanamadı: şifre yanlış olabilir" : "Bağlanamadı (zaman aşımı)");
      }
    }
  }
}

void uiStart() {
  buildList();
  buildPass();
  buildBusy();
  buildHome();
  lv_timer_create(tick, 250, nullptr);
  loadWifi();
  if (savedSsid.length()) startConnect(savedSsid, savedPass, true);
  else showScan();
}
