#pragma once
#include <Arduino.h>

// Canli gokyuzu sahnesi (ev-lamba ILI9341 ekranindan tasindi). Bkz. sky.cpp.
constexpr int SKY_W = 320, SKY_H = 184;

void skyInit();                         // tamponlar (LVGL hazirken, bir kez)
void skyStep();                         // hava+saate gore bir kare ilerlet ve ciz
void skyStepIfStale(uint32_t maxAgeMs); // web icin: kare eskiyse yenile
const uint16_t* skyRGB565();            // SKY_W*SKY_H RGB565 (LVGL yerel bayt sirasi)
String skyDemoLabel();                  // demo acikken "DEMO 13:20" gibi, degilse bos
void startSkyDemo();                    // 24 saat ~2 dk
void startWeatherDemo();                // tum hava tipleri 8'er sn
size_t skyBmpSize();                    // /api/sky.bmp (8 bit paletli BMP) govdesi
size_t skyBmpRead(uint8_t* buf, size_t maxLen, size_t index);
void martiCagir();  // ziyaretci marti hemen gelsin (web /marti)
