#pragma once
// Mutfak timer (yumurta): LedPanel-Pico mutfak.h'nin HUB'a uyarlamasi. Web Mutfak sekmesinden kurulur,
// ekranda sahnenin yerine kaynayan suda yumurta kesiti (display.cpp). Sure yumurta kaynayan suya
// kondugu andan itibarendir. Her asamada kisa ses, bitince alarm; sifirlanana kadar 10 dk dakikada bir.
#include <Arduino.h>

extern const char* const MT_AD[6];   // CIG, RAFADAN, KAYISI, AZ PISMIS, TAM PISMIS, COK PISMIS
extern const uint16_t MT_T[5];       // asama baslangiclari (sn): 180, 300, 420, 540, 720

bool mtOn();                          // timer kurulu (sayiyor ya da duraklatilmis ya da bitti)
bool mtRunning();
bool mtDone();
float mtElapsed();                    // sn
uint32_t mtDur();                     // hedef sure (sn)
int mtHedef();                        // 1..5 (MT_AD indeksi)
int mtStage(float e);                 // 0..5

void mtStart(int hedef, uint32_t sec);
void mtSet(int hedef, uint32_t sec);  // calisirken hedef/sure degistir
void mtPause(bool p);
void mtReset();
void mtTick();                        // ses + otomatik kapanma; displayTask her karede cagirir
String mtJson();                      // web: {"on":..,"run":..,"done":..,"el":ms,"dur":..,"hedef":..,"st":..,"t":[..]}
