#include "telegram.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>
#include "config.h"
#if __has_include("secrets.h") && !defined(SECRETS_DISABLED)
#include "secrets.h"  // TELEGRAM_BOT_TOKEN, TELEGRAM_CHAT_ID (gitignore'da)
#endif
#ifndef TELEGRAM_BOT_TOKEN
#define TELEGRAM_BOT_TOKEN ""
#define TELEGRAM_CHAT_ID ""
#endif

// =================== Ayar: NVS "tg" (derleme zamani secrets.h yalniz varsayilan) ===================
static String tgToken, tgChat;
static bool tgLoaded = false;
static void tgLoad() {
  if (tgLoaded) return;
  Preferences p;
  p.begin("tg", false);
  tgToken = p.getString("token", TELEGRAM_BOT_TOKEN);
  tgChat = p.getString("chat", TELEGRAM_CHAT_ID);
  p.end();
  tgLoaded = true;
}
void telegramSetConfig(const String& token, const String& chat) {
  Preferences p;
  p.begin("tg", false);
  p.putString("token", token);
  p.putString("chat", chat);
  p.end();
  tgToken = token;
  tgChat = chat;
  tgLoaded = true;
}
String telegramChatId() { tgLoad(); return tgChat; }
bool telegramConfigured() { tgLoad(); return tgToken.length() > 10 && tgChat.length() > 0; }

// =================== URL encode helper ===================
String urlEncode(const String& str) {
  String encoded;
  encoded.reserve(str.length() * 3);
  char c;
  for (size_t i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += c;
    } else if (c == ' ') {
      encoded += '+';
    } else {
      char buf[5];
      sprintf(buf, "%%%02X", (uint8_t)c);
      encoded += buf;
    }
  }
  return encoded;
}

// =================== sendTelegramAlert function (GÜNCELLENDİ) ===================
void sendTelegramAlert(const String& message) {
  if (!TELEGRAM_ENABLED) {
    Serial.println("[Telegram] Özellik devre dışı.");
    return;
  }

  if (!telegramConfigured()) {
    Serial.println("[Telegram] Token/chat id girilmemis, gonderilmedi.");
    return;
  }

  if (message.length() == 0) {
    Serial.println("[Telegram] Mesaj boş, gönderilmedi.");
    return;
  }

  // Wi-Fi kontrolü
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[Telegram] WiFi bağlı değil!");
    return;
  }

  Serial.println("\n--- TELEGRAM GÖNDERİM BAŞLIYOR ---");
  Serial.printf("Mesaj uzunluğu: %d karakter\n", message.length());
  Serial.println("Mesaj içeriği:");
  Serial.println(message);

  WiFiClientSecure client;
  client.setInsecure();  // SSL sertifika kontrolünü atla

  HTTPClient http;
  http.setReuse(false);
  http.setTimeout(10000);  // 10 saniye timeout

  // URL'yi oluştur - Markdown formatını kullanıyoruz.
  String encodedMsg = urlEncode(message);
  String url = "https://api.telegram.org/bot" + tgToken + "/sendMessage?chat_id=" + tgChat + "&text=" + encodedMsg + "&parse_mode=Markdown&disable_web_page_preview=true";

  Serial.printf("Telegram URL uzunluğu: %d\n", url.length());
  Serial.printf("Chat ID: %s\n", tgChat.c_str());

  // Bağlantıyı başlat
  if (http.begin(client, url)) {
    int httpCode = http.GET();
    String response = http.getString();

    Serial.printf("[Telegram] HTTP Kodu: %d\n", httpCode);
    Serial.printf("[Telegram] Yanıt: %s\n", response.c_str());

    if (httpCode == HTTP_CODE_OK) {
      Serial.println("[Telegram] ✅ Mesaj başarıyla gönderildi.");
    } else if (httpCode == 400) {
      Serial.println("[Telegram] ❌ HATA 400: Token veya Chat ID hatalı olabilir.");
    } else if (httpCode == 404) {
      Serial.println("[Telegram] ❌ HATA 404: Bot bulunamadı.");
    } else if (httpCode == 403) {
      Serial.println("[Telegram] ❌ HATA 403: Bot bu sohbete mesaj gönderemez.");
    } else if (httpCode < 0) {
      Serial.printf("[Telegram] ❌ Bağlantı hatası: %s\n", http.errorToString(httpCode).c_str());
    } else {
      Serial.printf("[Telegram] ❌ Bilinmeyen hata kodu: %d\n", httpCode);
    }

    http.end();
  } else {
    Serial.println("[Telegram] ❌ HTTP begin() başarısız. URL çözümlenemedi.");
  }

  Serial.println("--- TELEGRAM GÖNDERİM TAMAMLANDI ---\n");
}
