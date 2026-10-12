#pragma once
#include <Arduino.h>

void guiInit();   // ekran + dokunmatik + LVGL (boardPowerSeq çağrısı dahil)
void guiLoop();   // loop() içinde sık çağrılır
void uiStart();   // WiFi akışı: kayıtlı ağa bağlan, olmazsa kurulum ekranı
