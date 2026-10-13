#pragma once
#include <Arduino.h>

void guiInit();   // ekran + dokunmatik + LVGL (boardPowerSeq çağrısı dahil)
void guiLoop();   // loop() içinde sık çağrılır
void uiStart();   // WiFi akışı: kayıtlı ağa bağlan, olmazsa kurulum ekranı

uint8_t uiRotation();  // ekran yonu 0..3
