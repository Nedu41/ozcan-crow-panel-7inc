#include <Arduino.h>

#include "ui.h"

void setup() {
  Serial.begin(115200);
  Serial.printf("PSRAM: %s, %u bayt\n", psramFound() ? "var" : "YOK", ESP.getPsramSize());
  if (!psramFound()) { Serial.println("PSRAM yok: ekran baslatilmiyor"); return; }
  guiInit();
  uiStart();
}

void loop() {
  guiLoop();
  delay(5);
}
