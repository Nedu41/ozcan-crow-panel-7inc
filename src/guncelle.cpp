// Yazilim guncelleme: web'den dosya + GitHub release. Bkz. docs/GUNCELLEME.md
#include "guncelle.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <Update.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "config.h"

static volatile GuncelleDurum durum = GD_BOSTA;
static volatile int ilerleme = 0;
static String mesaj, sonSurum, assetUrl, kaynak;
static bool kaynakYuklu = false;
static volatile bool mesgul = false;  // denetleme/indirme gorevi calisiyor

GuncelleDurum guncelleDurumu() { return durum; }
int guncelleIlerleme() { return ilerleme; }
String guncelleMesaj() { return mesaj; }
String guncelleSonSurum() { return sonSurum; }

static void kaynakYukle() {
  if (kaynakYuklu) return;
  Preferences p;
  p.begin("upd", false);
  kaynak = p.getString("repo", "");
  p.end();
  kaynakYuklu = true;
}
String guncelleKaynak() { kaynakYukle(); return kaynak; }
void guncelleKaynakAyarla(const String& repo) {
  String r = repo;
  r.trim();
  // yalniz "kullanici/depo" (harf, rakam, - _ . ve tek /) kabul edilir
  bool ok = r.length() == 0;
  if (!ok) {
    int slash = r.indexOf('/');
    ok = slash > 0 && slash < (int)r.length() - 1 && r.indexOf('/', slash + 1) < 0;
    for (size_t i = 0; ok && i < r.length(); i++) {
      char c = r[i];
      ok = isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '/';
    }
  }
  if (!ok) return;
  Preferences p;
  p.begin("upd", false);
  p.putString("repo", r);
  p.end();
  kaynak = r;
  kaynakYuklu = true;
  sonSurum = "";
  assetUrl = "";
}

// "v7.1.2-crow" -> 7001002 (karsilastirma icin); cozulemezse -1
static long surumSayi(const String& v) {
  int a = 0, b = 0, c = 0;
  const char* p = v.c_str();
  if (*p == 'v' || *p == 'V') p++;
  if (sscanf(p, "%d.%d.%d", &a, &b, &c) < 2) return -1;
  return (long)a * 1000000L + b * 1000L + c;
}

bool guncelleYeniVarMi() {
  if (!sonSurum.length()) return false;
  long y = surumSayi(sonSurum), m = surumSayi(FW_VERSION);
  if (y < 0 || m < 0) return sonSurum != String(FW_VERSION);
  return y > m;
}

static bool guncelleMesaj_(GuncelleDurum d, const String& m) { durum = d; mesaj = m; return d != GD_HATA; }

static bool githubSorgula() {
  kaynakYukle();
  if (!kaynak.length()) return guncelleMesaj_(GD_HATA, "Güncelleme kaynağı ayarlanmamış (kullanıcı/depo)");
  if (WiFi.status() != WL_CONNECTED) return guncelleMesaj_(GD_HATA, "WiFi bağlı değil");
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setUserAgent("CrowPanelHUB");
  http.setConnectTimeout(8000);
  http.setTimeout(10000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if (!http.begin(client, "https://api.github.com/repos/" + kaynak + "/releases/latest")) return guncelleMesaj_(GD_HATA, "Bağlantı açılamadı");
  int code = http.GET();
  if (code != 200) {
    http.end();
    return guncelleMesaj_(GD_HATA, code == 404 ? "Yayın (release) bulunamadı: " + kaynak : "GitHub cevabı: HTTP " + String(code));
  }
  JsonDocument filter;
  filter["tag_name"] = true;
  filter["assets"][0]["name"] = true;
  filter["assets"][0]["browser_download_url"] = true;
  JsonDocument d;
  DeserializationError e = deserializeJson(d, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (e) return guncelleMesaj_(GD_HATA, String("Yanıt okunamadı: ") + e.c_str());
  sonSurum = (const char*)(d["tag_name"] | "");
  assetUrl = "";
  for (JsonObject a : d["assets"].as<JsonArray>()) {
    String n = a["name"] | "";
    if (n == "firmware.bin") { assetUrl = (const char*)(a["browser_download_url"] | ""); break; }
    if (!assetUrl.length() && n.endsWith(".bin")) assetUrl = (const char*)(a["browser_download_url"] | "");
  }
  if (!sonSurum.length()) return guncelleMesaj_(GD_HATA, "Sürüm etiketi yok");
  if (!assetUrl.length()) return guncelleMesaj_(GD_HATA, sonSurum + ": yayında firmware.bin dosyası yok");
  return guncelleMesaj_(GD_BOSTA, guncelleYeniVarMi() ? "Yeni sürüm: " + sonSurum : "Güncel (" + String(FW_VERSION) + ")");
}

static void denetleGorevi(void*) {
  durum = GD_DENETLENIYOR;
  mesaj = "Denetleniyor…";
  githubSorgula();
  mesgul = false;
  vTaskDelete(nullptr);
}

void guncelleDenetle() {
  if (mesgul) return;
  mesgul = true;
  xTaskCreatePinnedToCore(denetleGorevi, "UpdChk", 10240, nullptr, 1, nullptr, 0);
}

static void indirGorevi(void*) {
  if (!assetUrl.length() && !githubSorgula()) { mesgul = false; vTaskDelete(nullptr); }
  if (!assetUrl.length()) { mesgul = false; vTaskDelete(nullptr); }
  durum = GD_INDIRILIYOR;
  ilerleme = 0;
  mesaj = "İndiriliyor: " + sonSurum;
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setUserAgent("CrowPanelHUB");
  http.setConnectTimeout(10000);
  http.setTimeout(15000);
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);  // GitHub release dosyasi baska sunucuya yonlendirir
  bool ok = false;
  if (!http.begin(client, assetUrl)) {
    guncelleMesaj_(GD_HATA, "İndirme bağlantısı açılamadı");
  } else {
    int code = http.GET();
    int len = http.getSize();
    if (code != 200 || len <= 0) {
      guncelleMesaj_(GD_HATA, "İndirme başarısız: HTTP " + String(code));
    } else if (!Update.begin(len)) {
      guncelleMesaj_(GD_HATA, String("Yer yok / bölüm hatası: ") + Update.errorString());
    } else {
      WiFiClient* st = http.getStreamPtr();
      static uint8_t buf[1460];
      int toplam = 0;
      uint32_t son = millis();
      while (toplam < len && (http.connected() || st->available())) {
        int n = st->readBytes(buf, min((int)sizeof(buf), len - toplam));
        if (n <= 0) { if (millis() - son > 15000) break; delay(5); continue; }
        son = millis();
        if (Update.write(buf, n) != (size_t)n) break;
        toplam += n;
        ilerleme = (int)((int64_t)toplam * 100 / len);
      }
      if (toplam == len && Update.end(true)) {
        ok = true;
      } else {
        Update.abort();
        guncelleMesaj_(GD_HATA, String("Yazma başarısız: ") + (toplam == len ? Update.errorString() : "bağlantı koptu"));
      }
    }
    http.end();
  }
  if (ok) {
    guncelleMesaj_(GD_TAMAM, "Güncellendi, yeniden başlıyor…");
    ilerleme = 100;
    vTaskDelay(pdMS_TO_TICKS(1500));
    ESP.restart();
  }
  mesgul = false;
  vTaskDelete(nullptr);
}

void guncelleGithubBaslat() {
  if (mesgul) return;
  mesgul = true;
  xTaskCreatePinnedToCore(indirGorevi, "UpdDl", 12288, nullptr, 1, nullptr, 0);
}

String guncelleJson() {
  JsonDocument d;
  d["durum"] = (int)durum;
  d["ilerleme"] = ilerleme;
  d["mesaj"] = mesaj;
  d["surum"] = FW_VERSION;
  d["son"] = sonSurum;
  d["yeni"] = guncelleYeniVarMi();
  d["kaynak"] = guncelleKaynak();
  String o;
  serializeJson(d, o);
  return o;
}

static volatile bool dosyaHata = false;

void guncelleWebKur(AsyncWebServer& server) {
  // Dosyadan: tarayici .bin yukler (POST /update, multipart)
  server.on(
      "/update", HTTP_POST,
      [](AsyncWebServerRequest* r) {
        bool ok = !Update.hasError() && !dosyaHata;
        AsyncWebServerResponse* res = r->beginResponse(ok ? 200 : 500, "text/plain", ok ? "OK" : String("HATA: ") + Update.errorString());
        res->addHeader("Connection", "close");
        r->send(res);
        if (ok) { durum = GD_TAMAM; mesaj = "Güncellendi, yeniden başlıyor…"; delay(800); ESP.restart(); }
      },
      [](AsyncWebServerRequest* r, String filename, size_t index, uint8_t* data, size_t len, bool final) {
        if (!index) {
          dosyaHata = !filename.endsWith(".bin");
          if (dosyaHata) return;
          Serial.printf("[Güncelle] dosya: %s\n", filename.c_str());
          durum = GD_INDIRILIYOR;
          ilerleme = 0;
          mesaj = "Dosya yükleniyor";
          if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { dosyaHata = true; return; }
        }
        if (dosyaHata) return;
        if (Update.write(data, len) != len) { dosyaHata = true; Update.abort(); return; }
        if (final && !Update.end(true)) dosyaHata = true;
      });
  server.on("/api/guncelle/denetle", HTTP_GET, [](AsyncWebServerRequest* r) { guncelleDenetle(); r->send(200, "text/plain", "OK"); });
  server.on("/api/guncelle/github", HTTP_GET, [](AsyncWebServerRequest* r) { guncelleGithubBaslat(); r->send(200, "text/plain", "OK"); });
  server.on("/api/guncelle/kaynak", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (r->hasParam("repo")) guncelleKaynakAyarla(r->getParam("repo")->value());
    r->send(200, "text/plain", "OK");
  });
  // genel yol alt yollardan sonra (ESPAsyncWebServer "/yol" kaydi "/yol/..." altini da eşler)
  server.on("/api/guncelle", HTTP_GET, [](AsyncWebServerRequest* r) { r->send(200, "application/json", guncelleJson()); });
}
