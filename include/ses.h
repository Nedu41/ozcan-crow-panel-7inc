#pragma once
// Kart hoparlörü: CrowPanel I2S amfi (SDIN 17, LRCLK 18, BCLK 42 - belgeden). Kare dalga; çalma ayrı görevde, çağıran beklemez.
#include <Arduino.h>

enum SesTur : uint8_t { SES_TIK, SES_ACILIS, SES_UYARI, SES_DEPREM, SES_BILDIRIM, SES_DENEME, SES_MARTI };

void sesInit();              // NVS'ten açık/kapalı + seviye
void sesCal(SesTur t);       // kapalıysa sessiz; kuyruk doluysa atlanır
bool sesAcik();
bool sesAcilisAcik();         // acilis melodisi (varsayilan KAPALI)
void sesAcilisAyarla(bool on);
void sesAcikAyarla(bool on);
int sesSeviye();             // 1..10
void sesSeviyeAyarla(int v);
