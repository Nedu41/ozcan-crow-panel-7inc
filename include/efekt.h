#pragma once
#include <Arduino.h>

// Yapay zeka efektleri: PC'deki köprü (araclar/efekt_koprusu.py) efekti üretir/oynatır, kareleri UDP 4048
// ile bu cihaza yollar; cihaz tam ekranda gösterir. Köprü adresini kendisi bildirir (/api/aibr).
struct EfektState {
  volatile bool ok = false;     // köprü yanıt veriyor
  char durum[16] = "";          // "bos" | "uretiyor" | "oynuyor" | "hata"
  char ad[40] = "";             // oynayan kayıtlı efekt (varsa)
  volatile uint32_t listeSurum = 0;
};
extern EfektState efektSt;

void efektStart();                          // UDP alıcı + köprü görevi + tam ekran katmanı (bir kez, LVGL hazırken)
void efektRoutes();                         // web: /api/aibr, /api/efekt (setupWebServer içinden)
int efektListe(char out[][32], int max);    // kayıtlı efekt adları
void efektUret(const String& prompt);       // yeni efekt üret + oynat
void efektOynat(const String& ad);
void efektKaydet(const String& ad);         // son üretileni kaydet
void efektSil(const String& ad);
void efektAyar(float hiz, float isik);      // hiz 0.1..4, isik 0.05..2
void efektDurdur();
void efektAnahtar(const String& gemini);     // Gemini API anahtarı: NVS'e yazar, köprüye iletir
bool efektAnahtarVar();

// Tam ekran gösterim (dokununca kapanır ve efekti durdurur)
void efektEkranAc();
void efektEkranKapat();
