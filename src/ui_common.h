// Ortak tema + küçük LVGL yardımcıları (ui.cpp, ui_hub.cpp)
// Tema: koyu/gotik, siyah çerçeve, pastel vurgu. Koyu zemin -> açık yazı, pastel zemin -> koyu yazı.
#pragma once
#include <lvgl.h>

extern "C" { extern const lv_font_t noto16, noto20, noto28, dseg52, dseg64; }

#define C_BG       lv_color_hex(0x14141A)
#define C_CARD     lv_color_hex(0x1F1F28)
#define C_BTN      lv_color_hex(0x2C2C38)
#define C_FRAME    lv_color_hex(0x000000)
#define C_TEXT     lv_color_hex(0xDADAE4)
#define C_DIM      lv_color_hex(0x9A9AAE)
#define C_CYAN     lv_color_hex(0xA070FF)  // canli mor (saat, sekmeler, birincil dugme, kayan yazi)
#define C_ORANGE   lv_color_hex(0xFF4F9A)  // canli pembe
#define C_GREEN    lv_color_hex(0x2BFF8A)
#define C_RED      lv_color_hex(0xFF4040)
#define C_ONPASTEL lv_color_hex(0x0E0E14)

inline lv_obj_t* uiLabel(lv_obj_t* par, const char* t, const lv_font_t* f, lv_color_t c) {
  lv_obj_t* l = lv_label_create(par);
  lv_label_set_text(l, t);
  lv_obj_set_style_text_font(l, f, 0);
  lv_obj_set_style_text_color(l, c, 0);
  return l;
}

// Değişmediyse yeniden çizdirme
inline void uiSetText(lv_obj_t* l, const char* t) {
  if (strcmp(lv_label_get_text(l), t) != 0) lv_label_set_text(l, t);
}

// primary: pastel zemin + koyu yazı; değilse koyu düğme + açık yazı. Çerçeve hep siyah.
inline lv_obj_t* uiBtn(lv_obj_t* par, const char* t, bool primary, lv_event_cb_t cb, int w = LV_SIZE_CONTENT, int h = 56) {
  lv_obj_t* b = lv_button_create(par);
  lv_obj_set_size(b, w, h);
  lv_obj_set_style_radius(b, 10, 0);
  lv_obj_set_style_bg_color(b, primary ? C_CYAN : C_BTN, 0);
  lv_obj_set_style_border_color(b, C_FRAME, 0);
  lv_obj_set_style_border_width(b, 2, 0);
  lv_obj_set_style_shadow_width(b, 0, 0);
  lv_obj_set_style_pad_hor(b, 18, 0);
  lv_obj_t* l = lv_label_create(b);
  lv_label_set_text(l, t);
  lv_obj_set_style_text_font(l, &noto20, 0);
  lv_obj_set_style_text_color(l, primary ? C_ONPASTEL : C_TEXT, 0);
  lv_obj_center(l);
  if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, nullptr);
  return b;
}

// Düğme etiketi (ilk çocuk)
inline lv_obj_t* uiBtnLabel(lv_obj_t* b) { return lv_obj_get_child(b, 0); }

// Düğmeyi "açık" (pastel + parıltı, koyu yazı) ya da "kapalı" (koyu + açık yazı) göster
inline void uiBtnOn(lv_obj_t* b, bool on, lv_color_t glow) {
  lv_obj_set_style_bg_color(b, on ? glow : C_BTN, 0);
  lv_obj_set_style_text_color(uiBtnLabel(b), on ? C_ONPASTEL : C_TEXT, 0);
  lv_obj_set_style_shadow_width(b, on ? 18 : 0, 0);
  lv_obj_set_style_shadow_color(b, glow, 0);
  lv_obj_set_style_shadow_opa(b, on ? LV_OPA_70 : LV_OPA_TRANSP, 0);
}

inline lv_obj_t* uiRow(lv_obj_t* par) {
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

// Çerçeveli kart (siyah çerçeve, koyu gri zemin)
inline lv_obj_t* uiCard(lv_obj_t* par) {
  lv_obj_t* c = lv_obj_create(par);
  lv_obj_set_width(c, LV_PCT(100));
  lv_obj_set_height(c, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_color(c, C_CARD, 0);
  lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(c, C_FRAME, 0);
  lv_obj_set_style_border_width(c, 2, 0);
  lv_obj_set_style_radius(c, 12, 0);
  lv_obj_set_style_pad_all(c, 12, 0);
  lv_obj_set_style_pad_gap(c, 8, 0);
  lv_obj_set_style_text_color(c, C_TEXT, 0);
  lv_obj_set_style_text_font(c, &noto20, 0);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
  lv_obj_remove_flag(c, LV_OBJ_FLAG_SCROLLABLE);
  return c;
}

// Kaydırıcı: koyu iz, pastel gösterge/tutamaç
inline lv_obj_t* uiSlider(lv_obj_t* par, int lo, int hi, int val, lv_color_t col) {
  lv_obj_t* s = lv_slider_create(par);
  lv_obj_set_width(s, LV_PCT(94));
  lv_obj_set_height(s, 18);
  lv_obj_set_style_pad_all(s, 6, LV_PART_KNOB);
  lv_slider_set_range(s, lo, hi);
  lv_slider_set_value(s, val, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(s, C_BTN, LV_PART_MAIN);
  lv_obj_set_style_border_color(s, C_FRAME, LV_PART_MAIN);
  lv_obj_set_style_border_width(s, 2, LV_PART_MAIN);
  lv_obj_set_style_bg_color(s, col, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(s, col, LV_PART_KNOB);
  lv_obj_set_style_border_color(s, C_FRAME, LV_PART_KNOB);
  lv_obj_set_style_border_width(s, 2, LV_PART_KNOB);
  return s;
}

// ---- ui.cpp'nin sağladıkları ----
void uiShowWifiSetup();                 // WiFi kurulum ekranı (tarama + şifre)
void uiRotate();                        // ekranı bir sonraki yöne döndür (kalıcı)
bool uiIsLandscape();
void uiSetBacklight(int pct);           // 10..100
int uiGetBacklight();
void uiSetColorAdjust(int contrast, int gain, bool save);  // kontrast %50..130, renk parlakligi %40..100 (100/100 = degisiklik yok)
int uiGetContrast();
void uiSetSaturation(int sat, bool save);  // canlilik %80..220 (100 = degisiklik yok)
int uiGetSaturation();
int uiGetGain();
// Metin girişi penceresi (ekran klavyesi). OK'ta cb(metin) çağrılır.
typedef void (*UiPromptCb)(const char* text);
void uiPrompt(const char* title, const char* initial, bool password, UiPromptCb cb);
typedef void (*UiConfirmCb)();
void uiConfirm(const char* text, UiConfirmCb cb);
