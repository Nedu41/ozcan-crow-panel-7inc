#pragma once
#include <lvgl.h>

// HUB ana ekranı: sekmeli (Ana / Lambalar / Efekt / Ayar). Ekran nesnesini döndürür.
lv_obj_t* hubUiCreate();
// Ekran yönü değişince sekme çubuğunu yeniden yerleştirir
void hubUiOnRotate();
