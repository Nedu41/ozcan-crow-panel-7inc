#pragma once
#include <Arduino.h>
#include <lvgl.h>

// Çift tamponlu ekran çıkışı: LovyanGFX Bus_RGB tek framebuffer'ı özel DMA zinciriyle tarar. İkinci bir PSRAM
// tamponu ayırıp yeni kareyi gizli tampona yazar, vsync darbesinde DMA işaretçilerini ona çeviririz (yırtılma/kayma yok).
// Yalnızca yatay yönde (rot 0/2); dikeyde LovyanGFX'in kendi yazımı kullanılır.
void dbufInit();                    // lcd.init() sonrası, LVGL'den önce
bool dbufActive();                  // hazır ve yön yatay mı
void dbufFlush(const lv_area_t* a, const uint16_t* px, bool last);  // LVGL kısmi flush (px: yerel RGB565)
uint8_t* dbufHidden();              // gösterilmeyen tampon (satır adımı 1600 bayt, ham bayt sırası ters)
void dbufFlip(uint8_t* hidden);     // gizliyi göster (tamamı yazıldı varsay, sonraki LVGL karesi tam eşitler)
void dbufRestoreOriginal();         // dikey yöne geçmeden önce asıl tampona dön
bool dbufReadRow(int y, uint16_t* out800);  // görünen tamponun satırı (yerel RGB565) - ekran görüntüsü için
