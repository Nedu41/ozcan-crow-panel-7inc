#pragma once
#include <Arduino.h>

// Kayitli WiFi aglari (SuDepoSis'teki "WiFi" bolumunun karsiligi).
// Liste LittleFS'te; 0. eleman tercih edilen ag. Arayuz acilista sirayla dener,
// hicbiri tutmazsa dokunmatik WiFi kurulum ekrani acilir.

int wifiNetsCount();
bool wifiNetsGet(int idx, String& ssid, String& pass);  // sifre dahil - yalniz ic kullanim

// Web API icin JSON (sifreler asla disari verilmez)
String wifiNetsInfoJson();
bool wifiNetsAdd(const String& ssid, const String& pass);   // varsa sifreyi gunceller
bool wifiNetsDelete(int idx);
bool wifiNetsMakePreferred(int idx);                        // listenin basina alir
void wifiNetsClear();                                       // WiFi sifirlama: liste de silinir
