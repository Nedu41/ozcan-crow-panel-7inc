// Canli gokyuzu sahnesi: ev-lamba display.cpp'deki (ILI9341) animasyonun CrowPanel'e tasinmis hali.
// Gunes/ay (gercek evre), yildizlar, samanyolu, kayan yildiz, bulut, yagmur/kar/simsek/sis, tepeler;
// gokyuzu rengi gunes yuksekligine gore. Kucuk 16 renkli (palet) bir tampona cizilir; ekranda LVGL'de
// buyutulur, webde 8 bit BMP olarak sunulur (/api/sky.bmp).
#include "sky.h"

#include <ArduinoJson.h>
#include <time.h>

#include "app_state.h"

static uint8_t* idx = nullptr;          // SKY_W*SKY_H palet indeksi (PSRAM)
static uint16_t* rgb = nullptr;         // SKY_W*SKY_H RGB565 (LVGL icin)
static SemaphoreHandle_t smtx;

static inline uint16_t c565(uint8_t r, uint8_t g, uint8_t b) { return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3); }

enum : uint8_t {
  P_BG = 0, P_SKY1, P_SKY2, P_SKY3, P_SKY4, P_CLOUD_L, P_CLOUD_D, P_ACCENT, P_PRECIP, P_FLASH,
  P_PANEL, P_WHITE, P_CYAN, P_ORANGE, P_GRAY, P_RED
};

// Kucuk cizim katmani (TFT_eSprite yerine): palet indeksli
struct Canvas {
  uint16_t pal[16] = {};
  void setPaletteColor(uint8_t i, uint16_t c) { pal[i & 15] = c; }
  inline void drawPixel(int x, int y, uint8_t c) { if ((unsigned)x < SKY_W && (unsigned)y < SKY_H) idx[y * SKY_W + x] = c; }
  void drawFastHLine(int x, int y, int w, uint8_t c) {
    if ((unsigned)y >= SKY_H) return;
    int x1 = x + w;
    if (x < 0) x = 0;
    if (x1 > SKY_W) x1 = SKY_W;
    if (x1 > x) memset(idx + y * SKY_W + x, c, x1 - x);
  }
  void drawFastVLine(int x, int y, int h, uint8_t c) {
    if ((unsigned)x >= SKY_W) return;
    int y1 = y + h;
    if (y < 0) y = 0;
    if (y1 > SKY_H) y1 = SKY_H;
    for (; y < y1; y++) idx[y * SKY_W + x] = c;
  }
  void drawLine(int x0, int y0, int x1, int y1, uint8_t c) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1, err = dx + dy;
    for (;;) {
      drawPixel(x0, y0, c);
      if (x0 == x1 && y0 == y1) break;
      int e2 = 2 * err;
      if (e2 >= dy) { err += dy; x0 += sx; }
      if (e2 <= dx) { err += dx; y0 += sy; }
    }
  }
  void fillRect(int x, int y, int w, int h, uint8_t c) { for (int j = 0; j < h; j++) drawFastHLine(x, y + j, w, c); }
  void fillCircle(int cx, int cy, int r, uint8_t c) {
    for (int dy = -r; dy <= r; dy++) {
      int w = (int)sqrtf((float)(r * r - dy * dy));
      drawFastHLine(cx - w, cy + dy, 2 * w + 1, c);
    }
  }
  void drawCircle(int cx, int cy, int r, uint8_t c) {
    int x = r, y = 0, e = 1 - r;
    while (x >= y) {
      drawPixel(cx + x, cy + y, c); drawPixel(cx - x, cy + y, c); drawPixel(cx + x, cy - y, c); drawPixel(cx - x, cy - y, c);
      drawPixel(cx + y, cy + x, c); drawPixel(cx - y, cy + x, c); drawPixel(cx + y, cy - x, c); drawPixel(cx - y, cy - x, c);
      y++;
      if (e < 0) e += 2 * y + 1; else { x--; e += 2 * (y - x) + 1; }
    }
  }
  void fillRoundRect(int x, int y, int w, int h, int r, uint8_t c) {
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    for (int j = 0; j < h; j++) {
      int off = 0;
      if (j < r) off = r - (int)sqrtf((float)(r * r - (r - j) * (r - j)));
      else if (j >= h - r) { int k = j - (h - r - 1); off = r - (int)sqrtf((float)(r * r - k * k)); }
      drawFastHLine(x + off, y + j, w - 2 * off, c);
    }
  }
};
static Canvas S;

constexpr int SKY_BAND_UNUSED = 0;

// Hava bilgisi (CURRENT_WEATHER_DOC'tan)
struct WeatherView { bool ok = false, isDay = true; float wind = 0; int clouds = 0, code = -1; String sunrise, sunset; };
static WeatherView wv;
static void readWeather() {
  if (!weatherMutex || xSemaphoreTake(weatherMutex, pdMS_TO_TICKS(15)) != pdTRUE) return;
  String desc = CURRENT_WEATHER_DOC["description"] | "";
  wv.ok = desc == "OK";
  wv.wind = CURRENT_WEATHER_DOC["wind_speed"] | 0.0f;
  wv.clouds = CURRENT_WEATHER_DOC["clouds"] | 0;
  wv.code = CURRENT_WEATHER_DOC["weathercode"] | -1;
  wv.isDay = (CURRENT_WEATHER_DOC["is_day"] | 1) == 1;
  wv.sunrise = String((const char*)(CURRENT_WEATHER_DOC["sunrise_time"] | ""));
  wv.sunset = String((const char*)(CURRENT_WEATHER_DOC["sunset_time"] | ""));
  xSemaphoreGive(weatherMutex);
}

// Demolar (web: /sky_demo, /weather_demo)
static volatile bool skyDemoReq = false;
static float skyDemoMin = -1, skyDemoLeft = 0;
const float SKY_DEMO_SPEED = 12.0f;  // dakika / saniye
static volatile bool wxDemoReq = false;
static int wxDemoIdx = -1;
static uint32_t wxDemoStart = 0;
void startSkyDemo() { skyDemoReq = true; }
void startWeatherDemo() { wxDemoReq = true; }

enum Scene : uint8_t { SC_NONE, SC_CLEAR, SC_PARTLY, SC_CLOUDY, SC_FOG, SC_DRIZZLE, SC_RAIN, SC_SNOW, SC_THUNDER };

static Scene sceneFromCode(int c) {
  if (c == 0) return SC_CLEAR;
  if (c == 1 || c == 2) return SC_PARTLY;
  if (c == 45 || c == 48) return SC_FOG;
  if (c >= 51 && c <= 57) return SC_DRIZZLE;
  if ((c >= 61 && c <= 67) || (c >= 80 && c <= 82)) return SC_RAIN;
  if ((c >= 71 && c <= 77) || c == 85 || c == 86) return SC_SNOW;
  if (c >= 95) return SC_THUNDER;
  return SC_CLOUDY;  // 3 ve bilinmeyenler
}
struct SkyTime {
  bool valid;
  float elev;      // derece; <0 gunes ufkun altinda
  bool evening;    // ogleden sonra / aksam (kizillik sabahtan daha kirmizi)
  float dayFrac;   // 0 = gundogumu, 1 = gunbatimi (gunes yayi icin)
  float noonH;     // gunes ogleni (saat) - ay konumu icin
};
static float frand(float a, float b) { return a + (b - a) * (esp_random() / 4294967295.0f); }

typedef uint8_t RGB[3];
static uint16_t skyColors[4];  // simsek parlamasindan sonra geri yuklemek icin

static uint16_t shade(const RGB c, float k) {
  return c565((uint8_t)min(255.0f, c[0] * k), (uint8_t)min(255.0f, c[1] * k), (uint8_t)min(255.0f, c[2] * k));
}
static bool sceneMinutes(float& out) {
  if (skyDemoMin >= 0) { out = skyDemoMin; return true; }
  struct tm t;
  if (!getLocalTime(&t, 0)) return false;
  out = t.tm_hour * 60 + t.tm_min + t.tm_sec / 60.0f;
  return true;
}

static SkyTime skyTime() {
  SkyTime s{false, wv.isDay ? 30.0f : -30.0f, false, 0.5f, 12.5f};
  if (wv.sunrise.length() != 5 || wv.sunset.length() != 5) return s;
  float now;
  if (!sceneMinutes(now)) return s;
  float rise = wv.sunrise.substring(0, 2).toInt() * 60 + wv.sunrise.substring(3, 5).toInt();
  float set = wv.sunset.substring(0, 2).toInt() * 60 + wv.sunset.substring(3, 5).toInt();
  if (set <= rise) return s;
  const float PEAK = 50.0f, RATE = 0.2f, EVE_RATE = 0.25f;  // gercekte ~0,18 derece/dk; 0,5 kizilligi 2,5 kat hizli sonduruyordu
  s.valid = true;
  s.noonH = (rise + set) / 120.0f;
  if (now >= rise && now <= set) {
    s.dayFrac = (now - rise) / (set - rise);
    s.elev = PEAK * sinf(PI * s.dayFrac);
    s.evening = s.dayFrac > 0.5f;
  } else if (now < rise) {
    s.elev = -RATE * (rise - now);
    s.dayFrac = 0;
  } else {
    s.elev = -EVE_RATE * (now - set);  // aksam kizilligi sabahtan daha cabuk sonsun
    s.evening = true;
    s.dayFrac = 1;
  }
  if (s.elev < -30) s.elev = -30;
  return s;
}

static void mixRGB(const RGB a, const RGB b, float f, RGB out) {
  f = constrain(f, 0.0f, 1.0f);
  for (int j = 0; j < 3; j++) out[j] = (uint8_t)(a[j] + (b[j] - a[j]) * f);
}

// Acik gokyuzu, gunes yuksekligine gore (ust -> ufuk). Aksam tablosu ufukta
// daha kizil/mor (toz, nem), sabah daha pembe-altin.
static const float SKY_E[10] = {-18, -12, -7, -3, 0, 3, 8, 15, 30, 50};
static const RGB SKY_AM[10][4] = {
  {{0,0,0},{0,0,0},{0,0,2},{1,2,6}},               // gece: siyah
  {{0,0,2},{1,2,8},{4,6,20},{14,14,38}},           // astronomik tan
  {{6,12,44},{22,30,80},{60,55,110},{120,80,120}}, // tan agarmasi (mor)
  {{16,26,84},{80,60,130},{220,80,85},{255,105,50}},    // safak kizilligi
  {{30,50,130},{150,90,140},{255,110,70},{255,125,35}},  // gundogumu
  {{40,80,165},{120,125,190},{250,145,90},{255,165,60}},
  {{35,90,180},{70,130,210},{140,175,220},{220,210,190}},  // altin saat sonu
  {{28,85,185},{55,120,210},{100,160,230},{175,205,235}},
  {{20,75,178},{42,108,202},{78,148,226},{150,195,240}},
  {{14,64,170},{34,98,198},{68,138,224},{140,190,242}},    // ogle
};
static const RGB SKY_PM[10][4] = {
  {{0,0,0},{0,0,0},{0,0,2},{1,2,6}},
  {{0,0,2},{2,2,8},{8,4,18},{22,8,20}},
  {{6,6,34},{40,18,62},{130,30,60},{190,40,30}},   // alacakaranlik (mor-kizil)
  {{20,20,72},{120,40,90},{240,55,45},{255,75,20}},
  {{40,45,110},{170,65,105},{255,80,40},{255,100,20}},    // gunbatimi
  {{40,75,160},{140,100,160},{255,120,70},{255,145,45}},
  {{35,90,180},{70,130,210},{140,175,220},{220,210,190}},
  {{28,85,185},{55,120,210},{100,160,230},{175,205,235}},
  {{20,75,178},{42,108,202},{78,148,226},{150,195,240}},
  {{14,64,170},{34,98,198},{68,138,224},{140,190,242}},
};

static void skyAt(float e, bool evening, RGB out[4]) {
  const RGB (*T)[4] = evening ? SKY_PM : SKY_AM;
  int i = 0;
  while (i < 8 && e >= SKY_E[i + 1]) i++;
  float f = (e - SKY_E[i]) / (SKY_E[i + 1] - SKY_E[i]);
  for (int k = 0; k < 4; k++) mixRGB(T[i][k], T[i + 1][k], f, out[k]);
}

// Gunes rengi: ufukta kizil-turuncu, yukseldikce sari-beyaz
static void sunColorAt(float e, RGB out) {
  static const float E[6] = {-2, 0, 3, 8, 20, 40};
  static const RGB C[6] = {{255,50,25},{255,80,30},{255,130,45},{255,190,80},{255,225,120},{255,242,175}};
  int i = 0;
  while (i < 4 && e >= E[i + 1]) i++;
  mixRGB(C[i], C[i + 1], (e - E[i]) / (E[i + 1] - E[i]), out);
}

// 0 = alacakaranlik yok, 1 = tam kizillik (gunes ufuk civarindayken) -
// kapali/yagisli havalarin paletinde ufka sizan renk icin
static float twilightFactor() {
  SkyTime st = skyTime();
  if (!st.valid) return 0;
  return constrain(1.0f - fabsf(st.elev - 0.5f) / 9.0f, 0.0f, 1.0f);
}

// Acik/parcali bulutlu gokyuzu paleti: tamamen gunes yuksekligine gore
static void setOpenSkyPalette(Scene sc, const SkyTime& st) {
  RGB sky[4];
  skyAt(st.elev, st.evening, sky);
  for (int i = 0; i < 4; i++) {
    skyColors[i] = c565(sky[i][0], sky[i][1], sky[i][2]);
    S.setPaletteColor(P_SKY1 + i, skyColors[i]);
  }
  RGB sun;
  sunColorAt(st.elev, sun);
  static const RGB MOON = {240, 238, 205};
  bool sunUp = st.elev > -2;
  const uint8_t* acc = sunUp ? sun : MOON;
  S.setPaletteColor(P_ACCENT, c565(acc[0], acc[1], acc[2]));

  // Zemin (tepeler): gunduz koyu yesil, alacakaranlikta siyaha yakin siluet
  float light = constrain((st.elev + 4) / 16.0f, 0.0f, 1.0f);
  static const RGB GROUND_D = {14, 32, 20}, GROUND_N = {16, 18, 30};
  RGB g;
  mixRGB(GROUND_N, GROUND_D, light, g);
  S.setPaletteColor(P_BG, c565(g[0], g[1], g[2]));

  // Bulutlar: gunduz beyaz, gece koyu gri; gunes ufuk civarindayken alttan
  // ufuk rengiyle (pembe/turuncu) boyanir
  float tw = constrain(1.0f - fabsf(st.elev - 1) / 8.0f, 0.0f, 1.0f);
  static const RGB CL_D = {240,244,248}, CD_D = {190,200,212}, CL_N = {70,78,98}, CD_N = {45,50,66};
  RGB cl, cd;
  mixRGB(CL_N, CL_D, light, cl);
  mixRGB(CD_N, CD_D, light, cd);
  mixRGB(cl, sky[3], tw * 0.55f, cl);
  mixRGB(cd, sky[2], tw * 0.5f, cd);

  if (sc == SC_PARTLY) {
    S.setPaletteColor(P_CLOUD_L, c565(cl[0], cl[1], cl[2]));
    S.setPaletteColor(P_CLOUD_D, c565(cd[0], cd[1], cd[2]));
  } else if (sunUp) {
    // Acik gunduz: bulut yok - CLOUD_D gunes haresi (gunes ile gok arasi)
    RGB glow;
    mixRGB(sun, sky[2], 0.55f, glow);
    S.setPaletteColor(P_CLOUD_D, c565(glow[0], glow[1], glow[2]));
    S.setPaletteColor(P_CLOUD_L, c565(255, 222, 170));
  } else {
    // Acik gece: sicak renkli yildizlar + Samanyolu/ay haresi
    S.setPaletteColor(P_CLOUD_L, c565(255, 222, 170));
    S.setPaletteColor(P_CLOUD_D, c565(48, 52, 84));
  }
  S.setPaletteColor(P_PRECIP, c565(165, 190, 255));  // yagis yok: mavi yildizlar
  S.setPaletteColor(P_FLASH, c565(165, 163, 148));    // ay denizleri
}

static float chromaK = 1.95f;  // yayinlanan paletin doygunluk carpani (setScenePalette ayarlar)
static void setScenePalette(Scene sc, bool day) {
  SkyTime st = skyTime();
  if ((sc == SC_CLEAR || sc == SC_PARTLY) && st.valid) {
    setOpenSkyPalette(sc, st);
    return;
  }
  // Zemin (tepe siluetleri): karda beyaz, siste soluk, digerlerinde koyu
  {
    RGB g = {24, 36, 30};
    if (sc == SC_SNOW) { g[0] = 200; g[1] = 205; g[2] = 214; }
    else if (sc == SC_FOG) { g[0] = 96; g[1] = 100; g[2] = 104; }
    float gk = day ? 1.0f : 0.3f;
    S.setPaletteColor(P_BG, shade(g, gk));
  }
  // gok (ust->alt), acik bulut, koyu bulut, vurgu (gunes/ay/-), yagis
  static const RGB CLEAR_D[4] = {{20,70,160},{40,100,190},{70,135,215},{120,175,230}};
  static const RGB PARTLY_D[4] = {{30,80,165},{55,110,190},{85,140,210},{130,175,225}};
  static const RGB NIGHT[4] = {{4,8,26},{8,14,38},{12,22,52},{20,32,68}};
  static const RGB CLOUDY[4] = {{104,108,114},{124,128,134},{144,148,154},{164,168,174}};  // notr gri
  static const RGB FOG[4] = {{120,124,130},{135,139,145},{150,154,160},{165,169,175}};
  static const RGB RAIN[4] = {{78,82,88},{94,98,104},{110,114,120},{128,132,138}};  // notr gri
  static const RGB SNOW[4] = {{85,98,118},{105,118,138},{128,142,162},{152,166,186}};
  static const RGB STORM[4] = {{20,20,38},{28,28,50},{36,36,62},{44,44,74}};

  const RGB* sky = CLOUDY;
  RGB cl = {205,210,216}, cd = {150,156,166}, acc = {255,205,50}, pr = {150,190,235};
  auto set3 = [](RGB d, uint8_t r, uint8_t g, uint8_t b) { d[0] = r; d[1] = g; d[2] = b; };
  float k = 1.0f, kc = 1.0f;  // gece karartma (gok, bulut)

  switch (sc) {
    case SC_CLEAR:
    case SC_PARTLY:
      if (day) {
        sky = (sc == SC_CLEAR) ? CLEAR_D : PARTLY_D;
        set3(cl, 240,244,248); set3(cd, 190,200,212);
      } else {
        sky = NIGHT;
        set3(cl, 95,105,125); set3(cd, 65,72,90); set3(acc, 240,238,205);  // ay
      }
      break;
    case SC_FOG:     sky = FOG;  set3(cl, 205,208,212); set3(cd, 185,188,192); break;
    case SC_DRIZZLE:
    case SC_RAIN:    sky = RAIN; set3(cl, 190,194,200); set3(cd, 140,144,152); set3(pr, 175,210,250); break;
    case SC_SNOW:    sky = SNOW; set3(cl, 220,225,232); set3(cd, 175,182,192); set3(pr, 250,252,255); break;
    case SC_THUNDER: sky = STORM; set3(cl, 95,95,115); set3(cd, 62,62,80); set3(pr, 120,160,225); break;
    default: break;
  }
  // Gece: her havada gokyuzu siyah (ufka dogru cok hafif lacivert) - gece
  // oldugu belirgin olsun. Bulutlar siyah uzerinde gri kalir.
  static const RGB BLACK_NIGHT[4] = {{0,0,0},{0,0,3},{2,3,10},{6,8,20}};
  if (!day) {
    sky = BLACK_NIGHT;
    kc = (sc == SC_CLEAR || sc == SC_PARTLY) ? 1.0f : 0.55f;
  }
  // Kapali/sisli/ciselemede gok cismi bulutlarin arkasindan soluk gorunur:
  // gunduz solgun gunes, gece ay - gece/gunduz her havada ayirt edilsin
  if (sc != SC_CLEAR && sc != SC_PARTLY) {
    if (day) set3(acc, 235,225,170);
    else set3(acc, 225,222,195);
  }

  // Gundogumu/gunbatimi kizilligi: ufka dogru turuncu-kizil, tepede mor-lacivert.
  // Kalin bulutta sonumlu ama tamamen kaybolmaz (ufuktan hafif sizinti gercekci).
  RGB dawn[4];
  float glow = twilightFactor() * (1.0f - constrain(wv.clouds, 0, 100) / 100.0f * 0.6f);
  if (glow > 0.02f) {
    static const RGB DUSK[4] = {{45,20,68},{110,42,78},{205,78,58},{255,142,58}};  // ust->ufuk
    for (int i = 0; i < 4; i++)
      for (int j = 0; j < 3; j++) dawn[i][j] = (uint8_t)(sky[i][j] * (1 - glow) + DUSK[i][j] * glow);
    sky = dawn;
    k = max(k, 1.0f - glow * 0.3f);  // kizillik zaten koyulastiriyor, ustune tam gece karartmasi binmesin
  }

  for (int i = 0; i < 4; i++) {
    skyColors[i] = shade(sky[i], k);
    S.setPaletteColor(P_SKY1 + i, skyColors[i]);
  }
  S.setPaletteColor(P_CLOUD_L, shade(cl, kc));
  S.setPaletteColor(P_CLOUD_D, shade(cd, kc));
  S.setPaletteColor(P_ACCENT, shade(acc, 1.0f));
  S.setPaletteColor(P_PRECIP, shade(pr, day ? 1.0f : 0.8f));
  // P_FLASH sadece firtinada simsek icin; diger sahnelerde (ay gorundugunde)
  // ayin gri tonu: denizler (maria), kenar kararmasi, golge siniri. Palet 16 renk dolu.
  S.setPaletteColor(P_FLASH, sc == SC_THUNDER ? c565(255, 255, 215) : c565(165, 163, 148));
  // gri havalarda (kapali, ciseleme, yagmur, sis, firtina) renk doygunlugu artirilmaz: gokyuzu gri kalsin
  chromaK = (sc == SC_CLOUDY || sc == SC_DRIZZLE || sc == SC_RAIN || sc == SC_FOG || sc == SC_THUNDER) ? 0.25f : 1.95f;
}

// Animasyon durumu
struct Drop { float x, y, v; uint8_t len; };
struct Cloud { float x; int16_t y; float s, v; };
struct Star { int16_t x, y; uint8_t ph; };
static Drop drops[70];
static int dropCount = 0;
static Cloud cloudsA[6];
static int cloudCount = 0;
static const int STAR_COUNT = 90;
static Star stars[STAR_COUNT];
static uint32_t frameNo = 0;
static uint32_t nextStrikeMs = 0, strikeEndMs = 0;
static int16_t boltX[7], boltY[7];
static const int MW_COUNT = 220;           // Samanyolu noktalari
static int16_t mwX[MW_COUNT], mwY[MW_COUNT];
static uint32_t meteorStart = 0, nextMeteor = 0;
static float mX, mY, mVX, mVY;

// Sahnenin "pencere" bandi: panellerin arasinda serbest kalan orta bolge
const int WIN_TOP = 86, WIN_BOT = 134, WIN_MID = (WIN_TOP + WIN_BOT) / 2;
// Ufuk: tepe siluetlerinin ortalama ust siniri; gunes/ay buradan dogup batar
const int HORIZON_Y = 160;
const int ARC_TOP = 104;  // ogle gunesi (sicaklik/ruzgar yazilarinin altinda kalsin)
static int8_t hillH[SKY_W];   // yakin sira (koyu, dolu)
static int8_t farH[SKY_W];    // uzak daglar (yuksek, soluk - noktali)

static void initScene(Scene sc, bool day) {
  dropCount = 0;
  if (sc == SC_RAIN || sc == SC_THUNDER) dropCount = 70;
  else if (sc == SC_DRIZZLE) dropCount = 40;
  else if (sc == SC_SNOW) dropCount = 50;
  for (int i = 0; i < dropCount; i++) {
    Drop& d = drops[i];
    d.x = frand(0, SKY_W + 20);
    d.y = frand(-SKY_H, SKY_H);
    if (sc == SC_SNOW)        { d.v = frand(22, 45);   d.len = (uint8_t)frand(1, 3); }
    else if (sc == SC_DRIZZLE){ d.v = frand(150, 220); d.len = (uint8_t)frand(4, 7); }
    else                      { d.v = frand(260, 380); d.len = (uint8_t)frand(8, 14); }
  }

  cloudCount = (sc == SC_CLEAR) ? 0 : (sc == SC_PARTLY) ? 2 : (sc == SC_FOG) ? 0 : 5;
  if (!day && sc == SC_CLOUDY) cloudCount = 3;  // gece kapali: ay ve yildizlar sik sik gorunsun
  for (int i = 0; i < cloudCount; i++) {
    Cloud& c = cloudsA[i];
    c.s = (sc == SC_PARTLY) ? frand(0.7f, 0.9f) : frand(0.8f, 1.3f);
    c.x = frand(-60, SKY_W);
    c.y = (int16_t)frand(WIN_TOP - 4, WIN_BOT - 16);
    c.v = frand(5, 14);
  }

  for (int i = 0; i < STAR_COUNT; i++) {
    stars[i].x = (int16_t)frand(2, SKY_W - 2);
    stars[i].y = (int16_t)frand(2, HORIZON_Y - 42);  // daglarin ustundeki gok
    stars[i].ph = (uint8_t)frand(0, 50);
  }
  // Samanyolu: sol alttan sag uste capraz, ortasi yogun bir serit
  for (int i = 0; i < MW_COUNT; i++) {
    float u = frand(0, 1);
    float off = (frand(-1, 1) + frand(-1, 1) + frand(-1, 1)) * 9;  // ~normal dagilim
    mwX[i] = (int16_t)(30 + u * 260 + off * 0.6f);
    mwY[i] = (int16_t)(HORIZON_Y - u * (HORIZON_Y - 4) + off);
  }
  // Daglar: sinus toplamlari - her acilista ayni manzara. Uzak sira sivri
  // tepeli (|sin| ile), yakin sira yuvarlak ve alcak.
  for (int x = 0; x < SKY_W; x++) {
    hillH[x] = (int8_t)(9 + 7 * sinf(x * 0.021f + 1.3f) + 4 * sinf(x * 0.057f + 0.4f) + 1.5f * sinf(x * 0.13f));
    float peaks = 18 * (1 - fabsf(sinf(x * 0.018f + 0.6f))) + 9 * (1 - fabsf(sinf(x * 0.047f + 2.1f)));
    farH[x] = (int8_t)(12 + peaks + 1.5f * sinf(x * 0.21f));
  }
  nextStrikeMs = millis() + 1500;
  strikeEndMs = 0;
  nextMeteor = millis() + (uint32_t)frand(15000, 40000);
  meteorStart = 0;
}

// 4 gok rengi, bant sinirlari Bayer desenli noktalarla (dithering) eritilir:
// 16 renkli palette duz seritler yerine yumusak gradyan gibi gorunur
static const uint8_t BAYER4[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};

static void drawSky() {
  const int band = HORIZON_Y / 4;
  const int BLEND = 12;  // sinirin her iki yaninda gecis satiri
  for (int y = 0; y < SKY_H; y++) {
    int i = min(y / band, 3);
    int base = P_SKY1 + i, next = -1;
    float p = 0;
    int below = (i + 1) * band, above = i * band;
    if (i < 3 && y >= below - BLEND) { next = base + 1; p = (y - (below - BLEND)) / (2.0f * BLEND); }
    else if (i > 0 && y < above + BLEND) { next = base - 1; p = ((above + BLEND) - y) / (2.0f * BLEND); }
    S.drawFastHLine(0, y, SKY_W, base);
    if (next >= 0) {
      int th = (int)(p * 16);
      const uint8_t* row = BAYER4[y & 3];
      for (int x = 0; x < SKY_W; x++)
        if (row[x & 3] < th) S.drawPixel(x, y, next);
    }
  }
}

static void drawHills(bool farVisible) {
  // Uzak daglar: ayni renk ama noktali -> havanin arkasinda soluk kalir
  // (hava perspektifi), yakin sira onun onunde tam koyu. Siste uzak sira yok.
  for (int x = 0; x < SKY_W && farVisible; x++) {
    int top = HORIZON_Y - farH[x];
    for (int y = top; y < HORIZON_Y; y++)
      if (((x + y) & 3) != 0 || y <= top + 1) S.drawPixel(x, y, P_BG);
  }
  for (int x = 0; x < SKY_W; x++) {
    int top = HORIZON_Y - hillH[x];
    S.drawFastVLine(x, top, SKY_H - top, P_BG);
  }
}

// Yumusak hare: iceride yogun, disarida seyrek titresimli noktalar
static void drawHalo(int cx, int cy, int r, int w, uint8_t col) {
  const int H = r + w;
  for (int dy = -H; dy <= H; dy++) {
    for (int dx = -H; dx <= H; dx++) {
      int d2 = dx * dx + dy * dy;
      if (d2 <= (r + 1) * (r + 1) || d2 > H * H) continue;
      bool inner = d2 <= (r + w / 2) * (r + w / 2);
      if (inner ? ((dx + dy) & 1) == 0 : ((dx & 1) && (dy & 1) && ((dx + dy) & 3) == 0))
        S.drawPixel(cx + dx, cy + dy, col);
    }
  }
}

// Gok cismi yayi: f=0 doguda ufukta, 0.5 en yuksekte, 1 batida ufukta
// BUGFIX (2026-10-20): yay ufuk cizgisinde (HORIZON_Y) bitiyordu ama onunde 10-20 px tepe var -> gunes gercek
// batistan ~45 dk once tepenin arkasina giriyordu (gun batimi 18:13, 17:53'te gunes yoktu). Yay artik o x'teki tepe
// ustunde biter: gun dogumu/batiminda gunesin merkezi tepe cizgisinde.
static int hillTop(int x) { return HORIZON_Y - hillH[constrain(x, 0, SKY_W - 1)]; }
static void arcPos(float f, int& x, int& y) {
  x = 24 + (int)(f * (SKY_W - 48));
  int base = hillTop(x);
  y = base - (int)(sinf(PI * f) * (base - ARC_TOP));
}

static void sunPos(const SkyTime& st, int& x, int& y) {
  if (!st.valid) { x = 160; y = WIN_MID; return; }
  arcPos(st.dayFrac, x, y);
  if (st.elev < 0) y = hillTop(x) + (int)(-st.elev * 4);  // batmis: tepelerin arkasina iner
}

// Ay: gunese gore evre kadar geride - yeni ay gunesle birlikte, dolunay gece
// yarisi meridyende. Ufuk ustunde degilse false (o gece ay yok ya da gec dogar).
static bool moonPos(const SkyTime& st, float phase, int& x, int& y) {
  if (!st.valid) { x = 160; y = WIN_MID; return true; }
  float nowMin;
  if (!sceneMinutes(nowMin)) return false;
  float nowH = nowMin / 60.0f;
  float transit = fmodf(st.noonH + phase * 24.84f, 24.0f);
  float ha = nowH - transit;
  if (ha > 12) ha -= 24;
  if (ha < -12) ha += 24;
  const float HALF = 7.0f;  // ufuk ustunde kalma yarisi (saat)
  if (fabsf(ha) > HALF + 0.3f) return false;
  arcPos((ha + HALF) / (2 * HALF), x, y);
  return true;
}

static void drawSun(int cx, int cy, float t, float elev, bool glow) {
  int r = elev < 6 ? 19 : 16;  // ufukta daha iri gorunur
  if (glow) drawHalo(cx, cy, r, 12, P_CLOUD_D);
  if (elev > 6) {
    float rot = t * 0.4f;
    for (int k = 0; k < 10; k++) {
      float a = rot + k * (2 * PI / 10);
      float c = cosf(a), s = sinf(a);
      int r2 = r + 13 + (int)(2 * sinf(t * 2 + k));
      S.drawLine(cx + c * (r + 5), cy + s * (r + 5), cx + c * r2, cy + s * r2, P_ACCENT);
      S.drawLine(cx + c * (r + 5) + 1, cy + s * (r + 5), cx + c * r2 + 1, cy + s * r2, P_ACCENT);
    }
  }
  S.fillCircle(cx, cy, r + (int)sinf(t * 1.5f), P_ACCENT);
}

// Gercek ay evresi: 0 = yeni ay, 0.5 = dolunay. Bilinen yeni ay 2000-01-06
// 18:14 UTC + sinodik ay 29.530588853 gun. Dogrulandi: 2024-04-08 gunes
// tutulmasi -> 0.994, 2025-03-14 ay tutulmasi -> 0.492.
static float moonPhase() {
  time_t now = time(nullptr);
  if (now < 1600000000) return 0.5f;  // saat henuz senkron degil: dolunay goster
  double p = fmod((now - 947182440.0) / 86400.0 / 29.530588853, 1.0);
  return (float)(p < 0 ? p + 1 : p);
}

static void drawMoon(int cx, int cy, float phase) {
  const int R = 22;
  float c = cosf(2 * PI * phase);
  float litFrac = (1 - c) / 2;
  bool waxing = phase < 0.5f;  // buyuyen: sagdan aydinlik (kuzey yarimkure)

  // Yumusak hare: aydinlik orana gore, iceride yogun, disarida seyrek
  // titresimli (dithered) noktalar - duz halka yerine isik dagilmasi gibi
  if (litFrac > 0.15f) {
    const int H = R + 7;
    for (int dy = -H; dy <= H; dy++) {
      for (int dx = -H; dx <= H; dx++) {
        int d2 = dx * dx + dy * dy;
        if (d2 <= (R + 1) * (R + 1) || d2 > H * H) continue;
        bool inner = d2 <= (R + 4) * (R + 4);
        if (inner ? ((dx + dy) & 1) == 0 : ((dx & 1) && (dy & 1) && ((dx + dy) & 3) == 0))
          S.drawPixel(cx + dx, cy + dy, P_CLOUD_D);
      }
    }
  }

  // Ay denizleri (maria), dolunay fotografindaki yerlerine gore (kuzey yukari).
  // (dx, dy, r) R=22 icin: Imbrium, Serenitatis, Tranquillitatis, Crisium,
  // Procellarum (2 parca), Nubium, Fecunditatis, Nectaris, Frigoris
  static const int8_t MARIA[][3] = {
    {-7, -8, 6}, {3, -8, 4}, {7, -2, 5}, {15, -6, 3},
    {-14, 0, 6}, {-11, 7, 5}, {-3, 8, 4}, {12, 4, 3}, {7, 8, 2}, {-2, -16, 2}
  };
  for (int y = -R; y <= R; y++) {
    float w = sqrtf((float)(R * R - y * y));
    float term = w * c;  // golge siniri (terminator)
    int x0 = waxing ? (int)roundf(term) : (int)roundf(-w);
    int x1 = waxing ? (int)roundf(w) : (int)roundf(-term);
    if (x1 <= x0) continue;  // bu satir tamamen karanlik (gorunmez, gokyuzu kalir)
    S.drawFastHLine(cx + x0, cy + y, x1 - x0, P_ACCENT);
    for (auto& m : MARIA) {
      int dy = y - m[1];
      if (abs(dy) > m[2]) continue;
      int mw = (int)sqrtf((float)(m[2] * m[2] - dy * dy));
      int a = max(x0, m[0] - mw), b = min(x1, m[0] + mw);
      if (b > a) S.drawFastHLine(cx + a, cy + y, b - a, P_FLASH);
    }
    // Kenar kararmasi + yumusak golge siniri
    S.drawPixel(cx + x0, cy + y, P_FLASH);
    S.drawPixel(cx + x1 - 1, cy + y, P_FLASH);
    if (litFrac < 0.97f && x1 - x0 > 3) {
      S.drawPixel(waxing ? cx + x0 + 1 : cx + x1 - 2, cy + y, P_FLASH);
    }
  }
  // Parlak kraterler: Tycho (guney), Copernicus - aydinlik taraftaysa
  auto crater = [&](int dx, int dy) {
    float w = sqrtf((float)(R * R - dy * dy));
    float term = w * c;
    bool lit = waxing ? (dx >= term && dx < w) : (dx >= -w && dx < -term);
    if (lit) { S.drawPixel(cx + dx, cy + dy, P_WHITE); S.drawPixel(cx + dx + 1, cy + dy, P_WHITE); }
  };
  crater(-4, 16);
  crater(-9, -2);
  // Soluk kucuk kraterler (sadece aydinlik tarafta): halka ya da tek nokta
  static const int8_t CRATERS[][3] = {
    {6, -15, 1}, {-13, -11, 1}, {11, 13, 2}, {-17, 9, 1}, {16, -3, 1}, {3, 15, 1}, {-5, 3, 1}, {-15, -4, 2}
  };
  for (auto& k : CRATERS) {
    int dx = k[0], dy = k[1];
    float w = sqrtf((float)(R * R - dy * dy));
    float term = w * c;
    bool lit = waxing ? (dx >= term + 1 && dx < w - 1) : (dx > -w + 1 && dx < -term - 1);
    if (!lit) continue;
    if (k[2] >= 2) S.drawCircle(cx + dx, cy + dy, 2, P_FLASH);
    else S.drawPixel(cx + dx, cy + dy, P_FLASH);
  }
}

// Bulutlarin arkasindaki soluk gunes (isinsiz disk)
static void drawDimSun(int cx, int cy) {
  S.drawCircle(cx, cy, 20, P_SKY4);
  S.fillCircle(cx, cy, 16, P_ACCENT);
}

// Yildizlar gok karardikca kademeli cikar: once en parlaklar (gunes -4
// derecenin altinda), sonra orta, tam karanlikta (-12) hepsi. Orta parlaktakilerin
// bir kismi mavimsi / sicak renkli (renkler sadece acik gecede palette var).
static void drawStars(bool colored, float elev) {
  int tier = elev < -12 ? 3 : elev < -8 ? 2 : elev < -4 ? 1 : 0;
  if (!tier) return;
  for (int i = 0; i < STAR_COUNT; i++) {
    int mag = (i % 5 == 0) ? 1 : (i % 2 == 0) ? 2 : 3;  // 1 = en parlak
    if (mag > tier) continue;
    bool on = ((frameNo + stars[i].ph) % 50) < 44;       // titresim
    uint8_t col;
    if (mag == 1) col = on ? P_WHITE : P_GRAY;
    else if (mag == 2) {
      int k = i % 4;
      col = !on ? P_GRAY : (colored && k == 0) ? P_PRECIP : (colored && k == 1) ? P_CLOUD_L : P_WHITE;
    } else col = on ? P_GRAY : P_SKY3;
    S.drawPixel(stars[i].x, stars[i].y, col);
    if (mag == 1 && on) {  // en parlaklar: kucuk arti
      S.drawPixel(stars[i].x - 1, stars[i].y, P_GRAY);
      S.drawPixel(stars[i].x + 1, stars[i].y, P_GRAY);
      S.drawPixel(stars[i].x, stars[i].y - 1, P_GRAY);
      S.drawPixel(stars[i].x, stars[i].y + 1, P_GRAY);
    }
  }
}

static void drawMilkyWay() {
  for (int i = 0; i < MW_COUNT; i++) S.drawPixel(mwX[i], mwY[i], P_CLOUD_D);
}

// Kayan yildiz: 15-60 sn'de bir, ~0.6 sn suren parlak cizgi + sonen kuyruk
static void updateMeteor(uint32_t now) {
  if (now >= nextMeteor) {
    meteorStart = now;
    nextMeteor = now + (uint32_t)frand(15000, 60000);
    mX = frand(40, SKY_W - 40);
    mY = frand(6, 60);
    float a = frand(0.35f, 0.7f), dir = frand(0, 1) < 0.5f ? -1 : 1;
    mVX = dir * cosf(a) * 260;
    mVY = sinf(a) * 260;
  }
  uint32_t age = now - meteorStart;
  if (!meteorStart || age > 600) return;
  float s = age / 1000.0f;
  for (int k = 0; k < 16; k++) {
    float b = s - k * 0.006f;
    if (b < 0) break;
    S.drawPixel((int)(mX + mVX * b), (int)(mY + mVY * b), k < 4 ? P_WHITE : P_GRAY);
  }
}

static void drawCloud(int x, int y, float s) {
  S.fillRoundRect(x - 12 * s, y + 3 * s, 50 * s, 12 * s, 6 * s, P_CLOUD_D);  // alt golge
  S.fillCircle(x, y, 11 * s, P_CLOUD_L);
  S.fillCircle(x + 14 * s, y - 6 * s, 15 * s, P_CLOUD_L);
  S.fillCircle(x + 28 * s, y, 10 * s, P_CLOUD_L);
  S.fillRoundRect(x - 11 * s, y, 49 * s, 10 * s, 5 * s, P_CLOUD_L);
}

static void updateClouds(float dt) {
  float windBoost = 1.0f + min(wv.wind, 40.0f) / 20.0f;  // ruzgar arttikca bulut hizlanir
  for (int i = 0; i < cloudCount; i++) {
    Cloud& c = cloudsA[i];
    c.x += c.v * windBoost * dt;
    if (c.x > SKY_W + 20) {
      c.x = -60 * c.s;
      c.y = (int16_t)frand(WIN_TOP - 4, WIN_BOT - 16);
    }
    drawCloud((int)c.x, c.y, c.s);
  }
}

static void updatePrecip(Scene sc, float dt, float t) {
  float slant = (sc == SC_SNOW) ? 0 : constrain(wv.wind / 12.0f, 0.0f, 4.0f);  // ruzgara gore egim
  for (int i = 0; i < dropCount; i++) {
    Drop& d = drops[i];
    d.y += d.v * dt;
    if (sc == SC_SNOW) d.x += sinf(t * 1.5f + i) * 0.5f;
    else d.x -= slant * d.v * dt / d.len;
    if (d.y > SKY_H || d.x < -10) {
      d.y = frand(-20, -4);
      d.x = frand(0, SKY_W + 20);
    }
    if (sc == SC_SNOW) S.fillCircle((int)d.x, (int)d.y, d.len, P_PRECIP);
    else S.drawLine((int)d.x, (int)d.y, (int)(d.x - slant), (int)d.y + d.len, P_PRECIP);
  }
}

static void updateLightning(uint32_t now) {
  if (now >= nextStrikeMs) {
    int x = (int)frand(40, SKY_W - 40);
    int y = 8;
    for (int i = 0; i < 7; i++) {
      boltX[i] = x;
      boltY[i] = y;
      x += (int)frand(-16, 16);
      y += (int)frand(12, 20);
    }
    strikeEndMs = now + 260;
    nextStrikeMs = now + (uint32_t)frand(2500, 7500);
  }
  bool striking = now < strikeEndMs;
  bool flash = striking && (strikeEndMs - now) > 170;  // ilk ~90ms gokyuzu parlar
  for (int i = 0; i < 4; i++) {
    S.setPaletteColor(P_SKY1 + i, flash ? c565(185, 185, 215) : skyColors[i]);
  }
  if (striking) {
    for (int i = 0; i < 6; i++) {
      S.drawLine(boltX[i], boltY[i], boltX[i + 1], boltY[i + 1], P_FLASH);
      S.drawLine(boltX[i] + 1, boltY[i], boltX[i + 1] + 1, boltY[i + 1], P_FLASH);
    }
  }
}

// Sis: yere yakin yogun, yukari seyrelen, yavasca suruklenen dalgali perdeler.
// Yogunluk 4px'lik hucrelerde iki sinus dalgasinin toplami; hucre ici Bayer
// desenli noktalarla (dithering) cizilir - duz cizgiler yerine yumusak pus.
static void drawFog(float t) {
  const int TOP = 30, CELL = 4;
  for (int y = TOP; y < SKY_H; y++) {
    float base = (y - TOP) / (float)(SKY_H - TOP);  // 0 ust, 1 yer
    const uint8_t* row = BAYER4[y & 3];
    for (int cx = 0; cx < SKY_W; cx += CELL) {
      float w = 0.5f * sinf(cx * 0.015f + t * 0.22f + y * 0.045f)
              + 0.3f * sinf(cx * 0.037f - t * 0.15f + y * 0.021f);
      float d = base * 0.8f + w * 0.3f;
      int th = (int)(constrain(d, 0.0f, 1.0f) * 16);
      if (th <= 0) continue;
      uint8_t col = d > 0.55f ? P_CLOUD_L : P_CLOUD_D;  // yogun yerde acik, seyrekte ince doku
      for (int x = cx; x < cx + CELL; x++)
        if (row[x & 3] < th) S.drawPixel(x, y, col);
    }
  }
}

#include "ses.h"
// Martilar: uzaktakiler kucuk/gri/yavas (bulutlarin arkasinda), yakindakiler buyuk/beyaz/hizli (onunde).
// Kanat cirpar, ara sira suzulur, hafifce iner-cikar; ekrandan cikinca rastgele yeniden girer. Mantiksal koordinat.
struct Gull { float x, y, v, s, ph, fq, bob, glide; bool near; };
static Gull gulls[6];
static bool gullsInit = false;
static void gullSpawn(Gull& g, bool anywhere) {
  g.near = (&g - gulls) < 2;                                   // ilk ikisi yakin
  g.s = g.near ? frand(6.5f, 9.5f) : frand(2.2f, 3.6f);        // yarim kanat acikligi
  float spd = g.near ? frand(16, 28) : frand(5, 11);
  g.v = (frand(0, 1) < 0.5f) ? spd : -spd;
  g.x = anywhere ? frand(0, SKY_W) : (g.v > 0 ? -frand(15, 120) : SKY_W + frand(15, 120));
  g.y = g.near ? frand(28, HORIZON_Y - 60) : frand(12, HORIZON_Y - 45);
  g.ph = frand(0, 6.28f);
  g.fq = g.near ? frand(3.2f, 4.2f) : frand(2.0f, 2.8f);     // kanat cirpma (Hz)
  g.bob = frand(0, 6.28f);
  g.glide = 0;
}
static void drawGull(const Gull& g, uint8_t col) {
  // Suzulurken kanat hafif yukarida sabit; cirparken dirsek ve uc birlikte iner-kalkar ("M" bicimi)
  float f = g.glide > 0 ? 0.35f : sinf(g.ph);
  float s = g.s, x = g.x, y = g.y;
  float ey = y - s * (0.18f + 0.32f * f);                      // dirsek
  float ty = y - s * (0.05f + 0.55f * f) + s * 0.25f;          // kanat ucu
  S.drawLine(x - s, ty, x - s * 0.45f, ey, col);
  S.drawLine(x - s * 0.45f, ey, x, y, col);
  S.drawLine(x, y, x + s * 0.45f, ey, col);
  S.drawLine(x + s * 0.45f, ey, x + s, ty, col);
  if (g.near) {                                                // yakindakiler kalin: bir alt satir daha
    S.drawLine(x - s * 0.45f, ey + 1, x, y + 1, col);
    S.drawLine(x, y + 1, x + s * 0.45f, ey + 1, col);
  }
}
// near: false = uzaktakiler (bulutlardan once), true = yakindakiler (sonra). dt = 0 ise ilerlemez (ikinci serit).
static void updateGulls(bool nearPass, float dt, float t) {
  if (!gullsInit) { gullsInit = true; for (auto& g : gulls) gullSpawn(g, true); }
  for (auto& g : gulls) {
    if (g.near != nearPass) continue;
    if (dt > 0) {
      g.x += g.v * dt;
      if (g.glide > 0) g.glide -= dt;
      else { g.ph += g.fq * 6.2832f * dt; if (frand(0, 1) < dt * 0.15f) g.glide = frand(1.0f, 2.5f); }
      g.bob += dt * 0.9f;
      g.y += sinf(g.bob) * dt * (g.near ? 3.0f : 1.2f);
      if (g.x < -140 || g.x > SKY_W + 140) gullSpawn(g, false);
    }
    drawGull(g, g.near ? P_WHITE : P_GRAY);
  }
}

// Ziyaretci marti: arada bir uzaktan iyice yaklasir, onden gorunur (gulumser, goz kirpar, gagasiyla ekrana
// uc kez "tik" vurur - ses + carpma isiltisi), sonra uzaklasir. 4-9 dk'da bir (gunduz, yagissiz) ya da web /marti.
// Sure dt ile ilerler (HUB35'te ikinci seritte durur). Gumus marti gibi: beyaz bas, gri kanat, siyah uc,
// sari-turuncu gaga ve alt gagada kirmizi benek.
static volatile bool martiReq = false;
void martiCagir() { martiReq = true; }
static float mvT = -1, mvNext = 0;                 // mvT < 0: ziyaret yok
static float mvSx, mvSy, mvTx, mvTy, mvEx, mvEy;   // baslangic, yakin nokta, cikis
static float mvPh = 0;
static int mvPeck = 0;
static uint16_t mvSavNavy = 0, mvSavMouth = 0;   // ziyaret oncesi palet renkleri                             // calinan tik sayisi (ses bir kez)
static const float MV_IN = 4.5f, MV_HOLD = 5.5f, MV_OUT = 4.0f, MV_K = 2.0f;
static inline float mvEase(float a) { a = constrain(a, 0.0f, 1.0f); return a * a * (3 - 2 * a); }
// Cokgen doldurma (yatay tarama) ve kalin yay: maskot cizimi icin
static void mvPoly(const float* px, const float* py, int n, uint8_t c) {
  float y0 = py[0], y1 = py[0];
  for (int i = 1; i < n; i++) { y0 = min(y0, py[i]); y1 = max(y1, py[i]); }
  for (float y = y0; y <= y1; y += 0.75f) {
    float xs[16]; int m = 0;
    for (int i = 0; i < n && m < 16; i++) {
      float ax = px[i], ay = py[i], bx = px[(i + 1) % n], by = py[(i + 1) % n];
      if ((ay <= y && y < by) || (by <= y && y < ay)) xs[m++] = ax + (y - ay) * (bx - ax) / (by - ay);
    }
    for (int i = 1; i < m; i++) for (int j = i; j > 0 && xs[j - 1] > xs[j]; j--) { float t = xs[j]; xs[j] = xs[j - 1]; xs[j - 1] = t; }
    for (int i = 0; i + 1 < m; i += 2) S.drawFastHLine(xs[i], y, xs[i + 1] - xs[i] + 0.75f, c);
  }
}
static float mvM = 1;   // ayna (drawVisitor ayarlar)
static void mvArc(float cx, float cy, float r, float a0, float a1, uint8_t c, int th) {
  for (int t = 0; t < th; t++) {
    float rr = r + t * 0.6f;
    for (float a = a0; a < a1; a += 6) {
      float b2 = min(a1, a + 6), ra = a * 0.017453f, rb = b2 * 0.017453f;
      S.drawLine(cx + mvM * rr * cosf(ra), cy + rr * sinf(ra), cx + mvM * rr * cosf(rb), cy + rr * sinf(rb), c);
    }
  }
}
#define MV_POLY(c, ...) do { const float _p[] = {__VA_ARGS__}; float _x[12], _y[12]; int _n = sizeof(_p) / sizeof(float) / 2; \
  for (int _i = 0; _i < _n; _i++) { _x[_i] = _p[2 * _i]; _y[_i] = _p[2 * _i + 1]; } mvPoly(_x, _y, _n, c); } while (0)
static const uint8_t MV_NAVY = P_PRECIP, MV_MOUTH = P_FLASH;   // ziyaret suresince bu iki palet yuvasi (bkz updateVisitor)

// Maskot marti (onden, gaga sola): iri bitisik gozler, kalin kaslar, kocaman kivrik turuncu gaga (agiz acik,
// kirmizi ic), tepede tuy percemi, uzun beyaz govde, lacivert serit kanat/kuyruk, turuncu ayak.
static bool mvDizzy = false;   // carpma sonrasi: gozler carpi
static void drawVisitor(float x, float y, float k, float flap, bool smile, bool wink, float hit, bool mirror) {
  const float m = mirror ? -1.0f : 1.0f;
  mvM = m;
  const float f = flap * 8 * k;
  for (int s = -1; s <= 1; s += 2) {                              // kanatlar
    auto X = [&](float dx) { return x + m * 1 * k + s * dx; };
    const float Dw = 14 * k;
    MV_POLY(MV_NAVY, X(5 * k), y + 4 * k + Dw, X(17 * k), y - 4 * k - f + Dw, X(31 * k), y - 1 * k - f * 0.6f + Dw,
            X(41 * k), y + 5 * k - f * 0.3f + Dw, X(33 * k), y + 7 * k - f * 0.5f + Dw, X(18 * k), y + 7 * k - f * 0.6f + Dw,
            X(6 * k), y + 13 * k + Dw);
    if (k > 0.5f) for (int j = 0; j < 2; j++) {                   // acik seritler
      float dd = j ? 11.5f * k : 9 * k;
      S.drawLine(X(dd), y + 1 * k - f * 0.5f + Dw + j * 0.4f * k, X(dd + 1.5f * k), y + 9.5f * k - f * 0.55f + Dw, P_GRAY);
      S.drawLine(X(dd + 0.6f * k), y + 1 * k - f * 0.5f + Dw + j * 0.4f * k, X(dd + 2.1f * k), y + 9.5f * k - f * 0.55f + Dw, P_GRAY);
    }
  }
  MV_POLY(MV_NAVY, x - m * 2.5f * k, y + 33 * k, x + m * 5.5f * k, y + 33 * k, x + m * 6.5f * k, y + 42 * k, x + m * 1.5f * k, y + 44 * k, x - m * 3.5f * k, y + 42 * k);  // kuyruk
  S.drawFastHLine(x - m * 2.8f * k, y + 37.5f * k, 9 * k, P_GRAY);
  S.drawFastHLine(x - m * 3.2f * k, y + 40 * k, 9.5f * k, P_GRAY);
  for (int i = 0; i < 2; i++) {                                    // ayaklar
    float dx = i ? 7.5f * k : -5.5f * k;
    S.drawLine(x + m * dx, y + 31 * k, x + m * dx, y + 34 * k, P_ORANGE);
    S.drawLine(x + m * dx + 0.5f * k, y + 31 * k, x + m * dx + 0.5f * k, y + 34 * k, P_ORANGE);
    MV_POLY(P_ORANGE, x + m * dx - 2 * k, y + 34 * k, x + m * dx + 2.5f * k, y + 34 * k, x + m * dx + 0.3f * k, y + 36 * k);
  }
  {  // govde + boyun (gri kenar golgesi)
    const float bx[] = {-7, 8, 9, 9.5f, 8, 3, -3, -7, -7.5f, -6}, by[] = {-4, -6, 8, 22, 31, 36, 36, 31, 20, 8};
    float px[10], py[10];
    for (int i = 0; i < 10; i++) { px[i] = x + m * bx[i] * k; py[i] = y + by[i] * k; }
    mvPoly(px, py, 10, P_GRAY);
    for (int i = 0; i < 10; i++) px[i] += m * (bx[i] > 0 ? -0.8f : 0.3f) * k;
    mvPoly(px, py, 10, P_WHITE);
  }
  S.fillCircle(x + m * 0.5f * k, y - 6 * k, 10.5f * k, P_GRAY);        // bas
  S.fillCircle(x + m * 0.2f * k, y - 6.2f * k, 9.9f * k, P_WHITE);
  if (k > 0.4f) {
    const float ang[3] = {-60, -40, -80}, len[3] = {9, 7.5f, 6.5f};   // tuy percemi
    for (int i = 0; i < 3; i++) {
      float bx = x + m * 5 * k, by = y - 15 * k, ex = bx + m * len[i] * k * cosf(ang[i] * 0.017453f), ey = by + len[i] * k * sinf(ang[i] * 0.017453f);
      MV_POLY(P_WHITE, bx - m * 1.3f * k, by + 0.5f * k, bx + m * 1.3f * k, by + 0.8f * k, ex, ey);
      S.drawLine(bx + m * 1.3f * k, by + 0.8f * k, ex, ey, P_GRAY);
    }
  }
  const float exs[2] = {x - m * 5.2f * k, x + m * 5.0f * k}, eys[2] = {y - 7 * k, y - 7.6f * k}, ers[2] = {5.6f * k, 5.4f * k};
  for (int i = 0; i < 2; i++) {                                    // gozler
    S.fillCircle(exs[i], eys[i], ers[i] + 0.45f * k, P_ORANGE);
    S.fillCircle(exs[i], eys[i], ers[i], P_WHITE);
    if (k > 0.6f) S.drawCircle(exs[i], eys[i], ers[i] * 0.93f, P_GRAY);
    if (mvDizzy) {                                                 // sersem: carpi goz
      float r = ers[i] * 0.55f;
      for (int d = 0; d < 2; d++) {
        S.drawLine(exs[i] - r + d, eys[i] - r, exs[i] + r + d, eys[i] + r, P_PANEL);
        S.drawLine(exs[i] + r + d, eys[i] - r, exs[i] - r + d, eys[i] + r, P_PANEL);
      }
    } else if (wink && i == 1) mvArc(exs[i], eys[i] - 0.5f * k, ers[i] * 0.7f, 20, 160, P_PANEL, 3);
    else {
      float px = exs[i] - m * 1.4f * k, py = eys[i] + 1.0f * k;
      S.fillCircle(px, py, 1.9f * k, P_PANEL);
      if (k > 0.6f) S.fillCircle(px + m * 0.6f * k, py - 0.7f * k, 0.55f * k, P_WHITE);
    }
  }
  if (k > 0.5f) {                                                  // kaslar
    mvArc(exs[0] + 0.3f * k, eys[0] - 0.3f * k, ers[0] + 2.2f * k, 215, 300, P_PANEL, 3);
    mvArc(exs[1], eys[1] - 0.3f * k, ers[1] + 2.2f * k, 240, 325, P_PANEL, 3);
  }
  const float o = smile ? 1.0f : 0.35f;                            // gaga: agiz acikligi
  MV_POLY(MV_MOUTH, x + m * 5 * k, y + 2 * k, x - m * 10 * k, y + 3 * k, x - m * 14 * k, y + 6.5f * k + 2.5f * k * o, x - m * 2 * k, y + 6.5f * k + 2.5f * k * o);
  MV_POLY(P_ORANGE, x + m * 5 * k, y + 2 * k, x - m * 1 * k, y + 4 * k + 2 * k * o, x - m * 8 * k, y + 5.5f * k + 4 * k * o, x - m * 14.5f * k, y + 6.5f * k + 3 * k * o,
          x - m * 15 * k, y + 8 * k + 3.5f * k * o, x - m * 8 * k, y + 8.5f * k + 4.5f * k * o, x, y + 7.5f * k + 3 * k * o, x + m * 5.5f * k, y + 4 * k);
  MV_POLY(P_ORANGE, x + m * 4 * k, y - 1.5f * k, x - m * 4 * k, y - 2.8f * k, x - m * 13 * k, y - 1.5f * k, x - m * 19 * k, y + 1.5f * k,
          x - m * 21.5f * k, y + 5 * k, x - m * 21 * k, y + 8.5f * k, x - m * 19.5f * k, y + 7.5f * k, x - m * 17 * k, y + 4.5f * k,
          x - m * 10 * k, y + 2.8f * k, x - m * 1 * k, y + 3 * k, x + m * 5 * k, y + 1.5f * k);
  if (k > 0.6f) {
    S.drawLine(x - m * 15 * k, y + 1 * k, x - m * 14 * k, y + 1.4f * k, P_PANEL);       // burun deligi
    S.drawLine(x - m * 20.5f * k, y + 5 * k, x - m * 19.5f * k, y + 7.3f * k, MV_MOUTH);   // kanca ucu golgesi
  }
  if (hit > 0) {                                                   // ekrana carpma: gaga ucunda isinsal cizgiler
    float tx = x - m * 21 * k, ty = y + 9.5f * k, r0 = 2 * k, r1 = (3 + 5 * hit) * k;
    for (int i = 0; i < 6; i++) { float an = i * 1.047f + 0.3f; S.drawLine(tx + cosf(an) * r0, ty + sinf(an) * r0, tx + cosf(an) * r1, ty + sinf(an) * r1, P_WHITE); }
  }
}
// Turler: 0 gecis (yandan yana sessizce, ortada bakip gulumser + goz kirpar), 1 selam (yaklasir, gulumser,
// goz kirpar, gider), 2 tik tik (yaklasir, gagasiyla cama 3 kez vurur), 3 karisik (yaklasir, onumuzde bir tur
// atar, cama 2 kez vurur, goz kirpar, gider). Kendiliginden: agirlikli rastgele; web /marti: sirayla.
static int mvMode = 0, mvDemo = 0;
// Cam catlagi (carpma turu): carpma noktasindan isinsal kirik cizgiler + iki halka (orumcek agi). Mantiksal koordinat.
static float ckX = 0, ckY = 0, ckPx[10][4], ckPy[10][4];
static bool ckOn = false;
static void crackMake(float cx, float cy) {
  ckX = cx; ckY = cy; ckOn = true;
  for (int i = 0; i < 10; i++) {
    float a = i * 0.6283f + frand(-0.18f, 0.18f), r = 0;
    for (int j = 0; j < 4; j++) {
      r += frand(9, 20) + j * 6;
      float aj = a + frand(-0.22f, 0.22f);
      ckPx[i][j] = cx + cosf(aj) * r; ckPy[i][j] = cy + sinf(aj) * r * 0.85f;
    }
  }
}
static void crackDraw() {
  if (!ckOn) return;
  for (int i = 0; i < 10; i++) {                     // isinsal kiriklar
    float px = ckX, py = ckY;
    for (int j = 0; j < 4; j++) { S.drawLine(px, py, ckPx[i][j], ckPy[i][j], j < 2 ? P_WHITE : P_GRAY); px = ckPx[i][j]; py = ckPy[i][j]; }
  }
  for (int ring = 0; ring < 2; ring++)               // orumcek agi halkalari
    for (int i = 0; i < 10; i++) { int n = (i + 1) % 10; S.drawLine(ckPx[i][ring], ckPy[i][ring], ckPx[n][ring], ckPy[n][ring], ring ? P_GRAY : P_WHITE); }
  S.fillCircle(ckX, ckY, 2.5f, P_WHITE);            // carpma noktasi
}
static bool mvMir = false;
static float mvDur = 0;
static float mvPeckK(float h, float p0, int n, float& yOff, float& hit, int base) {   // tik: one atil, vur, geri cekil
  float push = 0; hit = 0; yOff = 0;
  for (int i = 0; i < n; i++) {
    float u = (h - p0 - i * 0.55f) / 0.4f;
    if (u >= 0 && u < 1) {
      push = u < 0.45f ? u / 0.45f : 1 - (u - 0.45f) / 0.55f;
      yOff = 6 * push;
      if (u >= 0.4f && u < 0.75f) hit = 1 - (u - 0.4f) / 0.35f;
      if (u >= 0.4f && mvPeck == base + i) { mvPeck = base + i + 1; sesCal(SES_TIK); }  // gaga camda: tik
    }
  }
  return 1 + 0.32f * push;
}
static void updateVisitor(bool allowed, float dt) {
  if (mvNext == 0) mvNext = frand(240, 540);
  if (mvT < 0) {
    if (dt > 0 && allowed) mvNext -= dt;
    bool req = martiReq;
    if (req || (allowed && mvNext <= 0)) {
      martiReq = false; mvT = 0; mvPeck = 0; mvNext = frand(240, 540);
      if (req) mvMode = (mvDemo++) % 5;
      else { float r = frand(0, 1); mvMode = r < 0.42f ? 0 : r < 0.70f ? 1 : r < 0.86f ? 2 : r < 0.95f ? 3 : 4; }
      ckOn = false; mvDizzy = false;
      mvSavNavy = S.pal[MV_NAVY]; mvSavMouth = S.pal[MV_MOUTH];
      S.setPaletteColor(MV_NAVY, c565(44, 56, 80)); S.setPaletteColor(MV_MOUTH, c565(200, 40, 40));
      bool fromLeft = frand(0, 1) < 0.5f;
      if (mvMode == 0) {                                  // gecis: ekranin bir yanindan obur yanina, orta derinlikte
        mvSx = fromLeft ? -50 : SKY_W + 50; mvEx = fromLeft ? SKY_W + 50 : -50;
        mvSy = frand(SKY_H * 0.2f, SKY_H * 0.32f); mvEy = mvSy + frand(-12, 12);
        mvDur = 7.5f; mvMir = fromLeft;                   // saga ucarken gaga saga
      } else {
        mvSx = fromLeft ? frand(20, SKY_W * 0.35f) : frand(SKY_W * 0.65f, SKY_W - 20); mvSy = frand(18, 40);
        mvTx = SKY_W / 2 + frand(-15, 15); mvTy = SKY_H * 0.30f;
        mvEx = fromLeft ? SKY_W + 90 : -90; mvEy = frand(5, 25);
        mvDur = mvMode == 4 ? 9.5f : MV_IN + (mvMode == 1 ? 3.0f : mvMode == 2 ? MV_HOLD : 7.0f) + MV_OUT;
        mvMir = mvTx > mvSx;
      }
    }
    return;
  }
  if (dt > 0) mvT += dt;
  const float t = mvT;
  if (t >= mvDur) { mvT = -1; ckOn = false; mvDizzy = false; S.setPaletteColor(MV_NAVY, mvSavNavy); S.setPaletteColor(MV_MOUTH, mvSavMouth); return; }
  float x, y, k, hit = 0, yo = 0, flapRate = 5.5f;
  bool smile = false, wink = false, mir = mvMir;
  if (mvMode == 4) {                                      // carpma
    const float TH = 2.4f, TS = TH + 0.9f;                // TH: carpma ani, TS: yapisik kalma sonu
    if (t < TH) {                                         // giderek hizlanarak gelir
      float a = t / TH; a = a * a * a;
      x = mvSx + (mvTx - mvSx) * a; y = mvSy + (mvTy - mvSy) * a; k = 0.12f + (MV_K * 1.25f - 0.12f) * a;
      flapRate = 9;
    } else {
      if (!ckOn) { crackMake(mvTx - (mvMir ? -1 : 1) * 21 * MV_K * 1.25f, mvTy + 9.5f * MV_K * 1.25f); sesCal(SES_TIK); sesCal(SES_TIK); }
      mvDizzy = true; k = MV_K * 1.25f; x = mvTx;
      if (t < TS) { y = mvTy; hit = 1 - (t - TH) / (TS - TH); flapRate = 0; }      // cama yapisik
      else { float u = t - TS; y = mvTy + 30 * u * u; flapRate = 2; }             // asagi duser (yercekimi)
    }
  } else if (mvMode == 0) {                               // gecis
    float a = t / mvDur;
    x = mvSx + (mvEx - mvSx) * a; y = mvSy + (mvEy - mvSy) * a + sinf(t * 2.2f) * 3;
    k = 0.85f + 0.25f * sinf(a * 3.1416f);                // ortada biraz daha yakin
    smile = a > 0.35f && a < 0.7f; wink = a > 0.48f && a < 0.54f;
    flapRate = 4.5f;
  } else {
    const float hold = mvDur - MV_IN - MV_OUT;
    if (t < MV_IN) {                                      // yaklasir: kucukten iyice buyuge
      float a = mvEase(t / MV_IN);
      x = mvSx + (mvTx - mvSx) * a; y = mvSy + (mvTy - mvSy) * a; k = 0.12f + (MV_K - 0.12f) * a * a;
      smile = a > 0.75f;
    } else if (t < MV_IN + hold) {                        // yakinda
      float h = t - MV_IN;
      x = mvTx + sinf(h * 1.3f) * 2; y = mvTy + sinf(h * 2.0f) * 1.5f; k = MV_K; smile = true; flapRate = 1.8f;
      if (mvMode == 1) wink = h > 1.2f && h < 1.7f;
      else if (mvMode == 2) { wink = h > 1.0f && h < 1.5f; k = MV_K * mvPeckK(h, 2.0f, 3, yo, hit, 0); }
      else {                                              // karisik: once onumuzde bir tur, sonra 2 tik, sonda goz kirpma
        if (h < 3.0f) {
          float ph = h / 3.0f * 6.2832f;
          x = mvTx + sinf(ph) * SKY_W * 0.22f; y = mvTy + (1 - cosf(ph)) * 10;
          k = MV_K * (0.75f + 0.25f * cosf(ph)); mir = cosf(ph) < 0 ? !mvMir : mvMir; flapRate = 5;
        } else {
          k = MV_K * mvPeckK(h, 3.4f, 2, yo, hit, 0);
          wink = h > 5.2f && h < 5.7f;
        }
      }
    } else {                                              // uzaklasir: kuculerek cikar, gidis yonune bakar
      float a = mvEase((t - MV_IN - hold) / MV_OUT);
      x = mvTx + (mvEx - mvTx) * a; y = mvTy + (mvEy - mvTy) * a; k = MV_K - (MV_K - 0.12f) * a;
      smile = a < 0.25f; mir = mvEx > mvTx; flapRate = 6;
    }
  }
  if (dt > 0) mvPh += dt * flapRate;
  if (y < SKY_H + 80) drawVisitor(x, y + yo, k, sinf(mvPh), smile, wink, hit, mir);
  if (mvDizzy && y < SKY_H + 60) for (int i = 0; i < 3; i++) {   // basinda donen yildizlar (martinin onunde)
    float an = t * 4 + i * 2.094f, sx = x + cosf(an) * 13 * k * 0.5f, sy = y - 17 * k + sinf(an) * 3 * k;
    S.drawLine(sx - 3, sy, sx + 3, sy, P_ORANGE); S.drawLine(sx, sy - 3, sx, sy + 3, P_ORANGE);
    S.drawLine(sx - 2, sy - 2, sx + 2, sy + 2, P_WHITE); S.drawLine(sx + 2, sy - 2, sx - 2, sy + 2, P_WHITE);
  }
  crackDraw();
}

// Sol kenarda yakin plan agac (tepelerle ayni koyu siluet): dibi sahnenin altindan cikar, taci sol uste tasar.
// Ruzgar arttikca ruzgar altina (saga) egilir, daha hizli/genis sallanir, tac tutamlari ayri titrer; ~27 km/s ustunde
// yapraklar kopup saga savrulur. Govde egilmesi h^2 ile artar (dip sabit). Gunes dogarken (dogu = sol) agacin
// arkasindan cikar: gunes tarafindaki kenara gunes renginde ince isik (arka isik) cizilir. Onizleme: ev-lamba tools/agac_sim.py.
struct TreeLeaf { float x, y, vx, vy, ph; };
static TreeLeaf trLeaf[30];
static int trLeafN = 0;
static float trLeafAcc = 0;
static float trK = 0, trF = 0, trSw = 0, trT = 0;   // bu karenin ruzgar durumu (iki gecis ayni sekli cizsin)
static float trCx[5], trCy[5];
static const int8_t TR_CL[5][4] = {{80, 6, -6, 24}, {74, 26, 4, 18}, {76, -18, 2, 19}, {94, 14, -20, 19}, {92, -12, -22, 18}};
// Agac sekli (ox, oy kaydirmasiyla, tek renk). Ayna: x'ler sol kenara gore (sola dogru eksi = saga dogru arti).
static void treeShape(float ox, float oy, uint8_t c) {
  const float k = trK, f = trF, sw = trSw, t = trT;
  const float bx = 22 + ox, by = SKY_H + 4 + oy, H = 112;
  auto X = [&](float h) { return bx + sw * H * h * h; };
  auto Y = [&](float h) { return by - H * h; };
  float px[12], py[12];
  for (int i = 0; i < 6; i++) {
    float h = i * 0.2f, hw = 9.0f - 6.5f * h;
    px[i] = X(h) - hw; py[i] = Y(h);
    px[11 - i] = X(h) + hw; py[11 - i] = Y(h);
  }
  mvPoly(px, py, 12, c);
  for (int i = 0; i < 5; i++) {
    float h = TR_CL[i][0] / 100.0f, fl = (0.8f + 3.5f * k) * sinf(t * f * 1.7f * 6.283f + i * 1.9f);
    float cx = X(h) + TR_CL[i][1] + sw * 14 * h + fl, cy = Y(h) + TR_CL[i][2] + 0.4f * fl;
    if (abs(TR_CL[i][1]) > 8)
      for (int o = 0; o < 3; o++) S.drawLine(X(h * 0.7f) + o, Y(h * 0.7f), cx + o, cy, c);
    S.fillCircle(cx, cy, TR_CL[i][3], c);
    trCx[i] = cx - ox; trCy[i] = cy - oy;
  }
  const float qx = X(0.84f) + 4 + sw * 12, qy = Y(0.84f) - 8;
  for (int i = 0; i < 16; i++) {                       // kenar tutamlari: tac puruzlu gorunsun
    float a = (i * 22.5f + 8 * sinf(i * 2.3f)) * 0.017453f, rr = 30 + 6 * sinf(i * 1.7f), r = 7 + 2 * sinf(i * 3.1f);
    float fl = (0.8f + 3.5f * k) * sinf(t * f * 2.1f * 6.283f + i * 1.3f);
    S.fillCircle(qx - rr * 1.15f * cosf(a) + fl, qy + rr * 0.9f * sinf(a) + 0.5f * fl, r, c);
  }
}
// rim: gunes agacin arkasinda/yakininda (sx, sy); false ise arka isik yok
static void drawTree(float dt, float t, bool rim, int sx, int sy) {
  static float tw = -1;
  float w = wv.ok ? wv.wind : 0;
  if (tw < 0) tw = w;
  tw += (w - tw) * min(1.0f, dt * 0.5f);              // ruzgar degisince birden degil yavasca
  trK = constrain(tw / 60.0f, 0.0f, 1.0f);           // 60 km/s ve ustu en siddetli
  const float amp = 0.02f + 0.13f * trK;
  trF = 0.25f + 0.60f * trK; trT = t;
  trSw = 0.22f * trK - amp * (sinf(t * trF * 6.283f) + 0.35f * sinf(t * trF * 2.7f * 6.283f + 1.3f)) / 1.35f;
  if (rim) {                                          // gunes tarafina 1-2 px kaydirilmis isik rengi kopya, ustune koyu agac
    float dx = sx - 35.0f, dy = sy - 95.0f, d = sqrtf(dx * dx + dy * dy) + 0.01f;
    treeShape(2 * dx / d, 2 * dy / d, P_ACCENT);
  }
  treeShape(0, 0, P_BG);
  // savrulan yapraklar (dt yalniz ilk seritte > 0: hareket bir kez, cizim her seritte)
  const float k = trK;
  if (dt > 0) {
    if (k > 0.45f) {
      trLeafAcc += (k - 0.45f) * 40 * dt;
      while (trLeafAcc >= 1 && trLeafN < 30) {
        trLeafAcc -= 1;
        int c = (int)frand(0, 4.99f); float r = TR_CL[c][3];
        trLeaf[trLeafN++] = {trCx[c] + r * 0.6f, trCy[c] + frand(-r, r) * 0.6f, (50 + 90 * k) * frand(0.7f, 1.2f), frand(-15, 25), frand(0, 6.28f)};
      }
    } else trLeafAcc = 0;
    for (int i = 0; i < trLeafN;) {
      TreeLeaf& q = trLeaf[i];
      q.x += q.vx * dt; q.y += (q.vy + 18 * sinf(t * 5 + q.ph)) * dt; q.ph += dt * 8;
      if (q.x > SKY_W + 4 || q.y >= SKY_H) trLeaf[i] = trLeaf[--trLeafN]; else i++;
    }
  }
  for (int i = 0; i < trLeafN; i++) {                  // donerek: bir yatay bir dikey
    const TreeLeaf& q = trLeaf[i];
    if (sinf(q.ph) > 0) { S.drawFastHLine(q.x, q.y, 3, P_BG); S.drawFastHLine(q.x, q.y + 1, 3, P_BG); }
    else for (int j = 0; j < 3; j++) S.drawFastHLine(q.x, q.y + j, 2, P_BG);
  }
}

static void renderScene(Scene sc, bool day, float dt, float t, uint32_t now) {
  drawSky();
  bool openSky = (sc == SC_CLEAR || sc == SC_PARTLY);
  SkyTime st = skyTime();
  float elev = st.valid ? st.elev : (day ? 30.0f : -30.0f);
  bool sunUp = elev > -2;
  // Yildizlar: gece yagissiz her havada (kapalida bulutlar gectikce ortulur)
  bool precip = (sc == SC_RAIN || sc == SC_SNOW || sc == SC_THUNDER || sc == SC_DRIZZLE || sc == SC_FOG);
  if (!precip) {
    if (sc == SC_CLEAR && elev < -15) drawMilkyWay();
    drawStars(openSky, elev);
    if (openSky && elev < -10) updateMeteor(now);
  }
  // Gok cismi bulutlardan ONCE cizilir: bulutlar gecerken onu ortup acar.
  // BUGFIX: sadece yagmur/kar/firtina haric tutuluyordu - ciseleme (drizzle,
  // kod 51-57) ve sis de "precip" sayildigi halde disarida kaliyordu, %100
  // bulutlu ciselemede bile soluk gunes gorunuyordu (2026-09-28 gozlemlendi,
  // canli kod 55). Simdi TUM yagisli/sisli sahnelerde gok cismi gizli.
  // Gunes/ay gun icinde yay cizerek hareket eder, tepelerin arkasina batar.
  // Siste gok cismi pusun arkasindan soluk disk olarak secilir (sis ustten cizilir)
  if (!precip || sc == SC_FOG) {
    int x, y;
    if (sunUp) {
      sunPos(st, x, y);
      if (openSky) drawSun(x, y, t, elev, sc == SC_CLEAR);
      else drawDimSun(x, y);
    }
    if (elev < 0 && moonPos(st, moonPhase(), x, y)) drawMoon(x, y, moonPhase());
  }
  drawHills(sc != SC_FOG);
  // Martilar yalniz gunduz ve yagissiz (acik/parcali/kapali/sis) havada
  const bool gullsOn = sunUp && (sc == SC_CLEAR || sc == SC_PARTLY || sc == SC_CLOUDY || sc == SC_FOG);
  if (gullsOn) updateGulls(false, dt, t);
  if (sc == SC_FOG) drawFog(t);
  updateClouds(dt);
  {  // agac bulutlarin onunde (yakin plan); gunes alcakta ve agacin arkasindaysa arka isik
    int sx = 0, sy = 0; bool rim = false;
    if (openSky && sunUp && elev < 12) { sunPos(st, sx, sy); rim = sx < 140; }
    drawTree(dt, t, rim, sx, sy);
  }
  if (gullsOn) updateGulls(true, dt, t);
  updateVisitor(gullsOn, dt);  // ziyaretci marti en onde
  {  // marti sesleri: martilar gorunurken 1-3 dk'da bir (dt yalniz ilk seritte > 0)
    static float callIn = 0;
    if (callIn <= 0) callIn = frand(60, 180);
    if (gullsOn && dt > 0 && (callIn -= dt) <= 0) { callIn = frand(60, 180); }  // kullanici istegi: marti sesi kapali (SES_MARTI duruyor)
  }
  if (sc == SC_THUNDER) updateLightning(now);
  if (dropCount) updatePrecip(sc, dt, t);
  frameNo++;
}


// ============== Sahne dongusu ==============
static const char* WX_NAME[] = {"Acik", "Parcali", "Kapali", "Sis", "Ciseleme", "Yagmur", "Kar", "Firtina"};
static String demoLabel;
static uint32_t lastStep = 0;

String skyDemoLabel() { return demoLabel; }

void skyInit() {
  if (idx) return;
  idx = (uint8_t*)heap_caps_calloc(SKY_W * SKY_H, 1, MALLOC_CAP_SPIRAM);
  rgb = (uint16_t*)heap_caps_calloc(SKY_W * SKY_H, 2, MALLOC_CAP_SPIRAM);
  smtx = xSemaphoreCreateMutex();
}

static void stepLocked() {
  static Scene cur = SC_NONE;
  static bool curDay = true;
  static uint32_t last = 0, lastPalette = 0;
  uint32_t now = millis();
  float dt = last ? min(0.25f, (now - last) / 1000.0f) : 0.05f;
  last = now;
  float t = now / 1000.0f;
  readWeather();

  static const Scene WX_SEQ[] = {SC_CLEAR, SC_PARTLY, SC_CLOUDY, SC_FOG, SC_DRIZZLE, SC_RAIN, SC_SNOW, SC_THUNDER};
  const int WX_N = sizeof(WX_SEQ) / sizeof(WX_SEQ[0]);
  const uint32_t WX_STEP_MS = 8000;
  if (wxDemoReq) { wxDemoReq = false; wxDemoStart = now; wxDemoIdx = 0; }
  if (wxDemoIdx >= 0) {
    wxDemoIdx = (now - wxDemoStart) / WX_STEP_MS;
    if (wxDemoIdx >= WX_N) wxDemoIdx = -1;
  }
  Scene sc = wxDemoIdx >= 0 ? WX_SEQ[wxDemoIdx] : skyDemoMin >= 0 ? SC_CLEAR : wv.ok ? sceneFromCode(wv.code) : SC_CLOUDY;
  if (skyDemoReq) {
    skyDemoReq = false;
    float m;
    skyDemoMin = sceneMinutes(m) ? m : 0;
    skyDemoLeft = 24 * 60;
  }
  if (skyDemoMin >= 0) {
    skyDemoMin = fmodf(skyDemoMin + dt * SKY_DEMO_SPEED, 24 * 60);
    skyDemoLeft -= dt * SKY_DEMO_SPEED;
    if (skyDemoLeft <= 0) skyDemoMin = -1;
    lastPalette = 0;
  }
  if (sc != cur || wv.isDay != curDay) {
    cur = sc;
    curDay = wv.isDay;
    setScenePalette(cur, curDay);
    initScene(cur, curDay);
    lastPalette = now;
  } else if (lastPalette == 0 || now - lastPalette > 20000) {
    setScenePalette(cur, curDay);
    lastPalette = now;
  }
  renderScene(cur, curDay, dt, t, now);

  if (skyDemoMin >= 0) { char b[16]; snprintf(b, sizeof(b), "DEMO %02d:%02d", (int)skyDemoMin / 60, (int)skyDemoMin % 60); demoLabel = b; }
  else if (wxDemoIdx >= 0) demoLabel = String("DEMO ") + WX_NAME[wxDemoIdx];
  else demoLabel = "";

  // canli renkler: palet girislerinin doygunlugu ve parlakligi artirilir (gri tonlar bozulmaz)
  uint16_t bp[16];
  for (int i = 0; i < 16; i++) {
    uint16_t c = S.pal[i];
    float r = ((c >> 11) & 31) * 8.2f, g = ((c >> 5) & 63) * 4.05f, b = (c & 31) * 8.2f;
    float m = (r + g + b) / 3.0f;
    r = m + (r - m) * chromaK; g = m + (g - m) * chromaK; b = m + (b - m) * chromaK;
    r *= 1.16f; g *= 1.16f; b *= 1.16f;
    bp[i] = c565((uint8_t)constrain(r, 0.f, 255.f), (uint8_t)constrain(g, 0.f, 255.f), (uint8_t)constrain(b, 0.f, 255.f));
  }
  for (int i = 0; i < SKY_W * SKY_H; i++) rgb[i] = bp[idx[i] & 15];
}

void skyStep() {
  if (!idx) return;
  if (xSemaphoreTake(smtx, pdMS_TO_TICKS(100)) != pdTRUE) return;
  stepLocked();
  lastStep = millis();
  xSemaphoreGive(smtx);
}

void skyStepIfStale(uint32_t maxAgeMs) {
  if (millis() - lastStep > maxAgeMs) skyStep();
}

const uint16_t* skyRGB565() { return rgb; }

// 8 bit paletli BMP (web): 54 + 1024 palet + SKY_W*SKY_H; satirlar alttan uste, 320 baytlik satir (4'un kati)
size_t skyBmpSize() { return 54 + 1024 + (size_t)SKY_W * SKY_H; }
size_t skyBmpRead(uint8_t* buf, size_t maxLen, size_t index) {
  size_t total = skyBmpSize(), n = 0;
  if (!idx || xSemaphoreTake(smtx, pdMS_TO_TICKS(100)) != pdTRUE) return 0;
  while (n < maxLen && index + n < total) {
    size_t p = index + n;
    uint8_t v;
    if (p < 54) {
      uint8_t h[54] = {'B', 'M'};
      uint32_t sz = total, off = 54 + 1024, w = SKY_W, hh = SKY_H;
      memcpy(h + 2, &sz, 4); memcpy(h + 10, &off, 4);
      uint32_t hs = 40; memcpy(h + 14, &hs, 4); memcpy(h + 18, &w, 4); memcpy(h + 22, &hh, 4);
      h[26] = 1; h[28] = 8;
      uint32_t colors = 16; memcpy(h + 46, &colors, 4);
      v = h[p];
    } else if (p < 54 + 1024) {
      int q = (p - 54) / 4, ch = (p - 54) % 4;
      uint16_t c = q < 16 ? S.pal[q] : 0;
      uint8_t r = ((c >> 11) & 31) << 3, g = ((c >> 5) & 63) << 2, b = (c & 31) << 3;
      v = ch == 0 ? b : ch == 1 ? g : ch == 2 ? r : 0;
    } else {
      size_t q = p - 54 - 1024;
      int row = SKY_H - 1 - q / SKY_W, col = q % SKY_W;
      v = idx[row * SKY_W + col];
    }
    buf[n++] = v;
  }
  xSemaphoreGive(smtx);
  return n;
}
