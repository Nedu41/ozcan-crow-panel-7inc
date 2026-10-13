// Geliştirme aracı: GET /screen.bmp -> ekranın anlık görüntüsü (24 bit BMP).
// Yalnızca -DDEV_SCREENSHOT ile derlenir; teslim imajında kapalı olmalı (bkz. platformio.ini).
#ifdef DEV_SCREENSHOT
#include <ESPAsyncWebServer.h>

#include "board.h"

extern LGFX lcd;
extern volatile int devTabReq;     // ui_hub.cpp
extern volatile bool devRotReq;
extern volatile int devTimerReq;

static uint8_t rowBuf[800 * 3 + 4];
static int curRow = -1;

static void le32(uint8_t* p, uint32_t v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }

void devScreenshotRoute(AsyncWebServer& server) {
  // /dev/tab?i=0..3  /dev/rot : ekran goruntusu testleri icin (LVGL tick'inde uygulanir)
  server.on("/dev/tab", HTTP_GET, [](AsyncWebServerRequest* r) {
    devTabReq = r->hasParam("i") ? r->getParam("i")->value().toInt() : 0;
    r->send(200, "text/plain", "ok");
  });
  server.on("/dev/timer", HTTP_GET, [](AsyncWebServerRequest* r) {
    devTimerReq = r->hasParam("i") ? r->getParam("i")->value().toInt() : 0;
    r->send(200, "text/plain", "ok");
  });
  server.on("/dev/rot", HTTP_GET, [](AsyncWebServerRequest* r) {
    devRotReq = true;
    r->send(200, "text/plain", "ok");
  });
  server.on("/screen.bmp", HTTP_GET, [](AsyncWebServerRequest* req) {
    const int W = lcd.width(), H = lcd.height();
    const size_t rowBytes = (W * 3 + 3) & ~3u;
    const size_t total = 54 + rowBytes * H;
    curRow = -1;
    AsyncWebServerResponse* r = req->beginChunkedResponse("image/bmp", [W, H, rowBytes, total](uint8_t* buf, size_t maxLen, size_t index) -> size_t {
      if (index >= total) return 0;
      uint8_t hdr[54] = {'B', 'M'};
      le32(hdr + 2, total);
      le32(hdr + 10, 54);
      le32(hdr + 14, 40);
      le32(hdr + 18, W);
      le32(hdr + 22, H);
      hdr[26] = 1;
      hdr[28] = 24;
      le32(hdr + 34, rowBytes * H);
      size_t n = 0;
      while (n < maxLen && index + n < total) {
        size_t pos = index + n;
        if (pos < 54) { buf[n++] = hdr[pos]; continue; }
        size_t off = pos - 54;
        int y = H - 1 - (int)(off / rowBytes);
        size_t inRow = off % rowBytes;
        if (curRow != y) {
          static lgfx::rgb888_t px[800];
          lcd.readRect(0, y, W, 1, px);
          for (int x = 0; x < W; x++) { rowBuf[x * 3] = px[x].b; rowBuf[x * 3 + 1] = px[x].g; rowBuf[x * 3 + 2] = px[x].r; }
          curRow = y;
        }
        size_t take = min(maxLen - n, rowBytes - inRow);
        memcpy(buf + n, rowBuf + inRow, take);
        n += take;
      }
      return n;
    });
    req->send(r);
  });
}
#endif
