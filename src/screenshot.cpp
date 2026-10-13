// Geliştirme aracı: GET /screen.bmp -> ekranın anlık görüntüsü (24 bit BMP).
// Yalnızca -DDEV_SCREENSHOT ile derlenir; teslim imajında kapalı olmalı (bkz. platformio.ini).
#ifdef DEV_SCREENSHOT
#include <ESPAsyncWebServer.h>

#include "display.h"

extern volatile int devTabReq;     // ui_hub.cpp
extern volatile bool devRotReq;
extern volatile int devSkyMode;
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
  server.on("/dev/sky", HTTP_GET, [](AsyncWebServerRequest* r) {
    devSkyMode = r->hasParam("m") ? r->getParam("m")->value().toInt() : 0;
    r->send(200, "text/plain", "ok");
  });
  // /dev/bench: PSRAM (gorunen framebuffer) -> DRAM kopyalama suresi. LCD bounce kesmesi her 495 us'de 12800 bayt boyle kopyalar.
  server.on("/dev/bench", HTTP_GET, [](AsyncWebServerRequest* r) {
    static uint8_t dram[12800];
    const uint8_t* src = (const uint8_t*)displayHidden();
    uint32_t tot = 0, mx = 0, n = 0;
    for (int i = 0; i < 300; i++) {
      const uint8_t* s = src + (size_t)((i * 37) % 380) * 1600;  // her seferinde baska bolge (onbellek soguk)
      uint32_t t0 = micros();
      memcpy(dram, s, sizeof(dram));
      uint32_t dt = micros() - t0;
      tot += dt; if (dt > mx) mx = dt; n++;
      vTaskDelay(1);
    }
    char b[96];
    snprintf(b, sizeof(b), "ort=%u us maks=%u us n=%u (12800 bayt, butce 495 us)", (unsigned)(tot / n), (unsigned)mx, (unsigned)n);
    r->send(200, "text/plain", b);
  });
  server.on("/dev/rot", HTTP_GET, [](AsyncWebServerRequest* r) {
    devRotReq = true;
    r->send(200, "text/plain", "ok");
  });
  server.on("/screen.bmp", HTTP_GET, [](AsyncWebServerRequest* req) {
    const int W = displayW(), H = displayH();
    const size_t rowBytes = (W * 3 + 3) & ~3u;
    const size_t total = 54 + rowBytes * H;
    curRow = -1;
    displaySnapshot(true);
    AsyncWebServerResponse* r = req->beginChunkedResponse("image/bmp", [W, H, rowBytes, total](uint8_t* buf, size_t maxLen, size_t index) -> size_t {
      if (index >= total) { displaySnapshot(false); return 0; }
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
          static uint16_t r565[800];
          displayReadRow(y, r565);
          for (int x = 0; x < W; x++) {
            uint16_t c = r565[x];
            rowBuf[x * 3] = (c & 31) << 3; rowBuf[x * 3 + 1] = ((c >> 5) & 63) << 2; rowBuf[x * 3 + 2] = ((c >> 11) & 31) << 3;
          }
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
