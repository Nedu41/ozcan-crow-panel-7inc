#pragma once
#include <Arduino.h>
#include <lvgl.h>

// CrowPanel 7" ekran çıkışı: ESP-IDF esp_lcd RGB paneli (Arduino core 3.x / IDF 5.5).
// İki tam ekran PSRAM framebuffer'ı + DRAM "bounce buffer" (PSRAM bant genişliği dalgalanmalarına dayanıklı, Espressif'in
// drift/titreme için önerdiği yapı). Yeni kare gizli tampona yazılır, vsync'te sayfa çevrilir (yırtılma yok).
// Piksel biçimi yerel RGB565 (LVGL ile aynı), yön yazılımla (aşağıdaki dönüşümle) uygulanır.
void displayInit(uint8_t rot);
void displaySetRotation(uint8_t rot);          // 0/2 yatay, 1/3 dikey
int displayW();                                // mantıksal genişlik (yöne göre)
int displayH();
// LVGL kısmi flush: px = alanın yerel RGB565 pikselleri (mantıksal koordinat)
void displayFlush(const lv_area_t* a, const uint16_t* px, bool last);
// Efekt/tam kare yolu: gizli tampon (800x480, satır başına 800 piksel, DONANIM yönü); yazınca displayFlip
uint16_t* displayHidden();
void displayFlip(uint16_t* hidden);            // gizliyi göster; sonraki LVGL karesi tam eşitler
bool displayReadRow(int y, uint16_t* out);     // mantıksal satır (ekran görüntüsü için), görünen tampondan

void displayEffectEnd();                       // efekt kapandi: LVGL yazimi yeniden acilir (cagiran tam yeniden cizim ister)
void displaySnapshot(bool on);                 // true: gorunen tamponun anlik kopyasini al (displayReadRow bundan okur); false: birak
