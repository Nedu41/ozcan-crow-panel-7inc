#pragma once
#include <Arduino.h>
#include <lvgl.h>

// CrowPanel 7" ekran çıkışı: ESP-IDF esp_lcd RGB paneli (Arduino core 3.x / IDF 5.5), yalnızca YATAY (800x480).
// Elecrow'un resmî yöntemi gibi LVGL "direct" modda doğrudan sürücünün iki tam ekran PSRAM tamponuna çizer (kopyalama yok);
// çizim bitince vsync'te tampon çevrilir. DRAM "bounce buffer" PSRAM bant genişliği dalgalanmalarına dayanıklıdır (Espressif önerisi).
void displayInit();
void* displayBuffer(int i);                    // sürücü tamponu 0/1 (800x480 RGB565) - LVGL'e verilir
// LVGL flush (direct mod): alanı önbellekten belleğe yaz; son alanda vsync'te tamponu göster
void displayFlush(const lv_area_t* a, uint8_t* px, bool last);
// Efekt/tam kare yolu: gösterilmeyen tampon; yazınca displayFlip
uint16_t* displayHidden();
void displayFlip(uint16_t* hidden);
bool displayReadRow(int y, uint16_t* out);     // ekran görüntüsü için, görünen tampondan
