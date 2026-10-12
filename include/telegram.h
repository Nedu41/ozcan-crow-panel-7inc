#pragma once
#include <Arduino.h>

String urlEncode(const String& str);
void sendTelegramAlert(const String& message);

// Bot token + chat id: NVS'de saklanir (ekrandan girilir). Bos = Telegram kapali.
void telegramSetConfig(const String& token, const String& chat);
bool telegramConfigured();
String telegramChatId();
