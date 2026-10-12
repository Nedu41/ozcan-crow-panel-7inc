#pragma once
#include <Arduino.h>

// Role olay kaydi (son 50): HUB'in kendi komutlari (gunbatimi, bulut, web...)
// ve ESP01'lerin MQTT'den bildirdikleri (zaman, mqtt). "Neden kapanmadi"
// sorusu tahminle degil kayitla cevaplansin diye.
void initEvents();
void addEvent(const String& device, int relay, bool on, const String& src, bool ok);
String eventsJson();  // en yeni basta
