#include "news.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "app_state.h"

// ============== Metin yardimcilari ==============
// GLCD/Font2/4/7 sadece ASCII cizebiliyor - Turkce karakterler bozuk kutucuk
// olarak cikiyordu. UTF-8 Turkce harfleri ve tipografik isaretleri ASCII'ye cevirir.
String trAscii(const String& in) {
  String out;
  out.reserve(in.length());
  const uint8_t* s = (const uint8_t*)in.c_str();
  size_t n = in.length();
  for (size_t i = 0; i < n; i++) {
    uint8_t c = s[i];
    if (c < 0x80) { out += (c >= 0x20) ? (char)c : ' '; continue; }
    if (c == 0xC3 && i + 1 < n) {
      switch (s[++i]) {
        case 0xA7: out += 'c'; break; case 0x87: out += 'C'; break;
        case 0xB6: out += 'o'; break; case 0x96: out += 'O'; break;
        case 0xBC: out += 'u'; break; case 0x9C: out += 'U'; break;
        case 0xA2: case 0xA1: case 0xA0: out += 'a'; break;
        case 0x82: out += 'A'; break;
        case 0xAE: case 0xAD: out += 'i'; break;
        case 0xBB: out += 'u'; break;
        case 0xA9: case 0xA8: out += 'e'; break;
      }
    } else if (c == 0xC4 && i + 1 < n) {
      switch (s[++i]) {
        case 0x9F: out += 'g'; break; case 0x9E: out += 'G'; break;
        case 0xB1: out += 'i'; break; case 0xB0: out += 'I'; break;
      }
    } else if (c == 0xC5 && i + 1 < n) {
      switch (s[++i]) {
        case 0x9F: out += 's'; break; case 0x9E: out += 'S'; break;
      }
    } else if (c == 0xE2 && i + 2 < n && s[i + 1] == 0x80) {
      uint8_t e = s[i + 2];
      i += 2;
      if (e == 0x98 || e == 0x99) out += '\'';
      else if (e == 0x9C || e == 0x9D) out += '"';
      else if (e == 0x93 || e == 0x94) out += '-';
      else if (e == 0xA6) out += "...";
    } else {
      if (c >= 0xF0) i += 3; else if (c >= 0xE0) i += 2; else if (c >= 0xC0) i += 1;
    }
  }
  return out;
}

String decodeEntities(String s) {
  s.replace("&quot;", "\"");
  s.replace("&#39;", "'");
  s.replace("&apos;", "'");
  s.replace("&lt;", "<");
  s.replace("&gt;", ">");
  s.replace("&nbsp;", " ");
  s.replace("&amp;", "&");  // en sonda - "&amp;quot;" gibi cift kodlamayi bozmasin
  return s;
}

// ============== Haber (AA dunya RSS) ==============
// USB takmadan uzaktan teshis: /fetch_status "news" alaninda gorunur
// LVGL fontunda (src/fonts: Latin-1 + Türkçe) olmayan karakterleri düzeltir/atar:
// tipografik tırnak/tire -> ASCII, kalan desteklenmeyenler atılır. Türkçe harfler korunur.
static String fontSafe(const String& in) {
  String out;
  out.reserve(in.length());
  const uint8_t* s = (const uint8_t*)in.c_str();
  size_t n = in.length();
  for (size_t i = 0; i < n;) {
    uint8_t c = s[i];
    uint32_t cp;
    int len;
    if (c < 0x80) { cp = c; len = 1; }
    else if ((c & 0xE0) == 0xC0 && i + 1 < n) { cp = ((c & 0x1F) << 6) | (s[i + 1] & 0x3F); len = 2; }
    else if ((c & 0xF0) == 0xE0 && i + 2 < n) { cp = ((c & 0x0F) << 12) | ((s[i + 1] & 0x3F) << 6) | (s[i + 2] & 0x3F); len = 3; }
    else { i += (c >= 0xF0) ? 4 : 1; continue; }
    const char* seq = (const char*)(s + i);
    i += len;
    if (cp == 0x2018 || cp == 0x2019) out += '\'';
    else if (cp == 0x201C || cp == 0x201D) out += '"';
    else if (cp == 0x2013 || cp == 0x2014) out += '-';
    else if (cp >= 0x20 && cp != 0x7F && (cp < 0x80 || (cp >= 0xA0 && cp <= 0xFF) || cp == 0x11E || cp == 0x11F ||
                                          cp == 0x130 || cp == 0x131 || cp == 0x15E || cp == 0x15F || cp == 0x2022 || cp == 0x2026))
      out.concat(seq, len);
  }
  return out;
}

static String newsState = "henuz denenmedi";
String newsStatus() { return newsState; }

static void setNewsState(const String& s) {
  newsState = s;
  Serial.println("[Haber] " + s);
}

// BUGFIX: eskiden tum RSS (dun 15KB, bugun 21.7KB) getString() ile tek parca
// bellege aliniyordu. Ekran sprite'lari + TLS'den sonra o buyuklukte blok
// kalmayinca yanit bos geliyor, kod da LOG YAZMADAN vazgeciyordu ("Haberler
// yukleniyor" takili kaliyordu). Simdi akis halinde okunup ilk NEWS_MAX_ITEMS
// baslik bulununca baglanti kesiliyor (~1KB bellek).
bool fetchNews() {
  if (WiFi.status() != WL_CONNECTED) { setNewsState("WiFi yok"); return false; }
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(5000);
  http.useHTTP10(true);  // chunked kodlama olmasin - ham akista parca boyutlari araya girmesin
  http.setUserAgent("MerkeziHUB/1.0");  // addHeader("User-Agent") ESP32'de yok sayiliyor
  if (!http.begin(client, NEWS_RSS_URL)) { setNewsState("begin basarisiz"); return false; }
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    setNewsState("HTTP " + String(code) + (code < 0 ? " " + http.errorToString(code) : ""));
    http.end();
    return false;
  }

  WiFiClient* s = http.getStreamPtr();
  String text, title, win;
  bool inItem = false, inTitle = false;
  int count = 0;
  unsigned long deadline = millis() + 8000;
  while (count < NEWS_MAX_ITEMS && millis() < deadline && (s->connected() || s->available())) {
    if (!s->available()) { delay(5); continue; }
    char ch = (char)s->read();
    if (inTitle) {
      title += ch;
      if (title.endsWith("</title>")) {
        title.remove(title.length() - 8);
        title.replace("<![CDATA[", "");
        title.replace("]]>", "");
        title = fontSafe(decodeEntities(title));
        title.trim();
        if (title.length()) {
          if (text.length()) text += "     +++     ";
          text += title;
          count++;
        }
        inTitle = false;
        inItem = false;  // haber basina sadece ilk <title>
        title = "";
      } else if (title.length() > 400) {  // bozuk/beklenmedik icerik
        inTitle = false;
        title = "";
      }
      continue;
    }
    win += ch;
    if (win.length() > 8) win.remove(0, win.length() - 8);
    if (!inItem && win.endsWith("<item>")) inItem = true;
    else if (inItem && win.endsWith("<title>")) { inTitle = true; title = ""; }
  }
  http.end();

  if (!count) { setNewsState("baslik bulunamadi (bos heap " + String(ESP.getFreeHeap()) + ")"); return false; }
  if (xSemaphoreTake(newsMutex, MUTEX_TIMEOUT) == pdTRUE) {
    NEWS_TICKER_TEXT = text;
    xSemaphoreGive(newsMutex);
  }
  struct tm t;
  char ts[6] = "--:--";
  if (getLocalTime(&t, 0)) strftime(ts, sizeof(ts), "%H:%M", &t);
  setNewsState("OK " + String(count) + " baslik " + ts);
  return true;
}

