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
// yukleniyor" takili kaliyordu). Simdi akis halinde okunup ilk maxItems
// baslik bulununca baglanti kesiliyor (~1KB bellek).

// Ayni olay birden cok kaynaktan gelir (Google Haberler): 5+ harfli kelimelerden 3'u daha once
// alinmis bir baslikta varsa tekrar sayilir.
static bool similarTitle(const String& t, const String* kept, int n) {
  for (int k = 0; k < n; k++) {
    int common = 0, i = 0;
    while (i < (int)t.length()) {
      int j = t.indexOf(' ', i);
      if (j < 0) j = t.length();
      String w = t.substring(i, j);
      if (w.length() >= 5 && kept[k].indexOf(w) >= 0 && ++common >= 3) return true;
      i = j + 1;
    }
  }
  return false;
}

// RSS'ten ilk maxItems basligi " +++ " ile birlestirip text'e ekler. stripSource: Google Haberler
// basliginin sonundaki " - Kaynak" atilir. Donus: alinan baslik sayisi, hata -> -1 (err doldurulur).
static int readRss(const char* url, int maxItems, bool stripSource, String& text, String& err) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(5000);
  http.useHTTP10(true);  // chunked kodlama olmasin - ham akista parca boyutlari araya girmesin
  http.setUserAgent("MerkeziHUB/1.0");  // addHeader("User-Agent") ESP32'de yok sayiliyor
  if (!http.begin(client, url)) { err = "begin basarisiz"; return -1; }
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    err = "HTTP " + String(code) + (code < 0 ? " " + http.errorToString(code) : "");
    http.end();
    return -1;
  }

  WiFiClient* s = http.getStreamPtr();
  String title, win, kept[8];
  bool inItem = false, inTitle = false;
  int count = 0, seen = 0;
  unsigned long deadline = millis() + 8000;
  while (count < maxItems && count < 8 && seen < 30 && millis() < deadline && (s->connected() || s->available())) {
    if (!s->available()) { delay(5); continue; }
    char ch = (char)s->read();
    if (inTitle) {
      title += ch;
      if (title.endsWith("</title>")) {
        title.remove(title.length() - 8);
        title.replace("<![CDATA[", "");
        title.replace("]]>", "");
        title = fontSafe(decodeEntities(title));
        if (stripSource) { int d = title.lastIndexOf(" - "); if (d > 10) title.remove(d); }
        title.trim();
        seen++;
        if (title.length() && !similarTitle(title, kept, count)) {
          if (text.length()) text += "     +++     ";
          text += title;
          kept[count++] = title;
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
  if (!count) err = "baslik bulunamadi (bos heap " + String(ESP.getFreeHeap()) + ")";
  return count;
}

// Alt yazi: AA dunya basliklari + "KOCAELI:" etiketiyle bolgenin one cikan haberleri (Google Haberler
// "Kocaeli" aramasi - bircok kaynaktan, onemli olanlar ustte). Biri basarisizsa digeri yine gosterilir.
bool fetchNews() {
  if (WiFi.status() != WL_CONNECTED) { setNewsState("WiFi yok"); return false; }
  String world, local, errW, errL;
  int nW = readRss(NEWS_RSS_URL, NEWS_MAX_ITEMS, false, world, errW);
  int nL = readRss(NEWS_LOCAL_RSS_URL, NEWS_LOCAL_ITEMS, true, local, errL);
  if (nW <= 0 && nL <= 0) { setNewsState("dunya: " + errW + " | kocaeli: " + errL); return false; }
  String text = world;
  if (nL > 0) {                                    // 4'lu haber sayfasi " +++ " ile boler: her yerel basliga onek
    String loc = "Kocaeli: " + local;
    loc.replace("     +++     ", "     +++     Kocaeli: ");
    text += String(text.length() ? "     +++     " : "") + loc;
  }
  if (xSemaphoreTake(newsMutex, MUTEX_TIMEOUT) == pdTRUE) {
    NEWS_TICKER_TEXT = text;
    xSemaphoreGive(newsMutex);
  }
  struct tm t;
  char ts[6] = "--:--";
  if (getLocalTime(&t, 0)) strftime(ts, sizeof(ts), "%H:%M", &t);
  setNewsState("OK dunya " + String(max(nW, 0)) + ", kocaeli " + String(max(nL, 0)) + " baslik " + ts +
               (nW <= 0 ? " (dunya: " + errW + ")" : "") + (nL <= 0 ? " (kocaeli: " + errL + ")" : ""));
  return nW > 0;  // dunya basarisizsa 30 sn'de bir tekrar denensin
}

