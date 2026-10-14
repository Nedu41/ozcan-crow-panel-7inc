// LVGL 9.2 yapılandırması — yalnızca varsayılandan farklı olanlar (kalanı lv_conf_internal.h)
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 16
#define LV_USE_STDLIB_MALLOC  LV_STDLIB_CUSTOM
#define LV_USE_STDLIB_STRING  LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_CLIB
#define LV_DEF_REFR_PERIOD 16

// Türkçe karakterli Noto Sans (src/fonts), semboller için Montserrat fallback
#define LV_USE_QRCODE 0
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_48 0
#define LV_FONT_CUSTOM_DECLARE extern const lv_font_t noto16, noto20, noto28;
#define LV_FONT_DEFAULT &noto20

#endif
