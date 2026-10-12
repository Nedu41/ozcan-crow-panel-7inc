#pragma once
#include <Arduino.h>

String urlEncode(const String& str);
void sendTelegramAlert(const String& message);
