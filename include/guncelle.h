#pragma once
#include <Arduino.h>
#include <ESPAsyncWebServer.h>

// Yazilim guncelleme (kablosuz): 1) web'den .bin dosyasi yukleme, 2) GitHub son surum (release) indirme.
// PlatformIO'dan ArduinoOTA ayrica calismaya devam eder.
enum GuncelleDurum { GD_BOSTA = 0, GD_DENETLENIYOR, GD_INDIRILIYOR, GD_TAMAM, GD_HATA };

void guncelleWebKur(AsyncWebServer& server);  // /update (dosya), /api/guncelle/*
String guncelleJson();                         // durum + surum + kaynak
GuncelleDurum guncelleDurumu();
int guncelleIlerleme();                        // 0..100
String guncelleMesaj();
String guncelleSonSurum();                     // son denetlenen surum etiketi ("" = bilinmiyor)
bool guncelleYeniVarMi();                      // son surum, calisandan yeni mi
void guncelleDenetle();                        // arka planda GitHub'a sor
void guncelleGithubBaslat();                   // arka planda indir + yaz + yeniden baslat
String guncelleKaynak();                       // "kullanici/depo"
void guncelleKaynakAyarla(const String& repo);
