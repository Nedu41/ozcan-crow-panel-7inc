#pragma once
#include <Arduino.h>

// LedPanel-Pico HTTP istemcisi (kimlik doğrulamasız, aynı WiFi). Adres: mDNS ledpanel.local
// ya da elle girilen IP/ad (NVS "led"/"host"). Engelleyici HTTP ayrı görevde.
struct LedState {
  volatile bool ok = false;     // son /api/state başarılı mı
  volatile int pat = -1, bp = 0, fxs = 0, rot = 0, fps = 0;
  char ip[20] = "";             // çözülen adres
};
extern LedState ledState;

void ledpanelStart();                        // görevi başlat (WiFi bağlanınca)
void ledpanelSend(const String& pathQuery);  // örn "/api/fx?id=36&hiz=60" (kuyruğa atar)
void ledpanelSetHost(const String& host);    // boş = otomatik (mDNS)
String ledpanelHost();                       // elle girilen (boş olabilir)
