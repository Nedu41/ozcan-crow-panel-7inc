#pragma once
#include <Arduino.h>

// Bulut/karanlik otomasyonu, gundogumu/batimi + sabit saat zamanlayicisi, ruzgar uyarisi
void weatherControlLogic(int currentMinute);
void handleSunSchedule(int currentMinute);
void windAlertLogic();

// FreeRTOS gorevi: hava/haber guncelleme + yukaridakileri dakikada bir calistirir
void updateWeatherAndControlTask(void* pvParameters);
