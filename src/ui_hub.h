#pragma once
#include <lvgl.h>

// HUB ana ekranı: sekmeli (Ana / Lambalar / Ayar). Ekran nesnesini döndürür.
lv_obj_t* hubUiCreate();
// Ekran yönü değişince sekme çubuğunu yeniden yerleştirir
void hubUiOnRotate();

// Lamba zamanlayici penceresi (ui_timer.cpp); devIdx: cihaz listesindeki sira
void uiTimerOpen(int devIdx);
