#pragma once
#include <Arduino.h>

// Canli gokyuzu sahnesi (ev-lamba ILI9341 ekranindan tasindi). Bkz. sky.cpp.
constexpr int SKY_W = 320, SKY_H = 184;

void skyInit();                         // tamponlar (LVGL hazirken, bir kez)
void skyStep();                         // hava+saate gore bir kare ilerlet ve ciz
void skyStepIfStale(uint32_t maxAgeMs); // web icin: kare eskiyse yenile
constexpr int SKY_BW = SKY_W * 2, SKY_BH = SKY_H * 2;
const uint16_t* skyRGB565();            // SKY_BW*SKY_BH RGB565 (2x bilinear buyutulmus, LVGL yerel bayt sirasi)
struct SkyRect { int x1, y1, x2, y2; };
int skyApplyDirty(SkyRect* out, int maxOut);  // degisen karolari big'e isle; dikdortgen sayisi (2x resim koordinati)
uint32_t skyFrameSeq();                 // her yeni kare icin artar
void skyWant(bool on);                  // Ana sekmesi acikken true: uretim gorevi calisir
String skyDemoLabel();                  // demo acikken "DEMO 13:20" gibi, degilse bos
void startSkyDemo();                    // 24 saat ~2 dk
void startWeatherDemo();                // tum hava tipleri 8'er sn
size_t skyBmpSize();                    // /api/sky.bmp (8 bit paletli BMP) govdesi
size_t skyBmpRead(uint8_t* buf, size_t maxLen, size_t index);
