// Çift tamponlu ekran çıkışı (bkz. dbuf.h). Araştırma: Espressif "double frame buffer" yaklaşımı; LovyanGFX
// Bus_RGB'de yok, bu yüzden DMA tanımlayıcılarının tampon işaretçileri vsync darbesinde değiştirilir.
#include "dbuf.h"

#include <driver/gpio.h>
#include <esp32s3/rom/cache.h>
#include <soc/io_mux_reg.h>

#include "board.h"

extern LGFX lcd;

static const size_t FB_BYTES = 800 * 480 * 2, ROW_BYTES = 1600, DMA_CHUNK = 4096 - 64;
static uint8_t *fbA = nullptr, *fbB = nullptr, *fbCur = nullptr;
static int fbSkip = 0, vsyncIdle = 0, vsyncPin = -1;
static bool ready = false;

struct R { int16_t x1, y1, x2, y2; };
static const int MAXR = 48;
static R prevR[MAXR], curR[MAXR];
static int nPrev = 0, nCur = 0;
static bool prevFull = true, curFull = false, inFrame = false;

static lgfx::Bus_RGB* rgbBus() { return (lgfx::Bus_RGB*)((lgfx::Panel_Device*)lcd.getPanel())->getBus(); }

void dbufInit() {
  if (ready) return;
  auto* b = rgbBus();
  fbB = (uint8_t*)heap_caps_malloc(FB_BYTES, MALLOC_CAP_SPIRAM);
  if (!fbB) return;
  fbA = fbCur = b->_frame_buffer;
  memcpy(fbB, fbA, FB_BYTES);
  Cache_WriteBack_Addr((uint32_t)fbB, FB_BYTES);
  fbSkip = (uint8_t*)b->_dmadesc_restart.buffer - (uint8_t*)b->_dmadesc[0].buffer;
  vsyncPin = b->config().pin_vsync;
  PIN_INPUT_ENABLE(GPIO_PIN_MUX_REG[vsyncPin]);
  int hi = 0, n = 0;  // boşta seviyesi: çoğunlukta olan (vsync darbesi kısa)
  for (uint32_t t = micros(); micros() - t < 70000; n++) hi += gpio_get_level((gpio_num_t)vsyncPin);
  vsyncIdle = hi * 2 > n ? 1 : 0;
  prevFull = true;
  ready = true;
}

bool dbufActive() { return ready && (lcd.getRotation() & 1) == 0; }

static void flipTo(uint8_t* nb) {
  auto* b = rgbBus();
  uint32_t t = micros();
  while (gpio_get_level((gpio_num_t)vsyncPin) == vsyncIdle && micros() - t < 50000) {}
  size_t n = (FB_BYTES - 1) / DMA_CHUNK + 1;
  portDISABLE_INTERRUPTS();
  for (size_t i = 0; i < n; i++) b->_dmadesc[i].buffer = nb + i * DMA_CHUNK;
  b->_dmadesc_restart.buffer = nb + fbSkip;
  portENABLE_INTERRUPTS();
  fbCur = nb;
}

uint8_t* dbufHidden() { return fbCur == fbA ? fbB : fbA; }

void dbufFlip(uint8_t* hidden) {
  Cache_WriteBack_Addr((uint32_t)hidden, FB_BYTES);
  flipTo(hidden);
  prevFull = true;  // başka yoldan tam kare yazıldı: sonraki LVGL karesi tamamını eşitlesin
  nPrev = 0;
  inFrame = false;
}

void dbufRestoreOriginal() {
  if (ready && fbCur != fbA) { flipTo(fbA); prevFull = true; inFrame = false; }
}

// Görünen tampondan gizliye, önceki karenin değişen bölgelerini kopyala
static void syncHidden(uint8_t* hid) {
  uint8_t* vis = fbCur;
  if (prevFull) { memcpy(hid, vis, FB_BYTES); return; }
  for (int i = 0; i < nPrev; i++) {
    const R& r = prevR[i];
    for (int y = r.y1; y <= r.y2; y++) memcpy(hid + y * ROW_BYTES + r.x1 * 2, vis + y * ROW_BYTES + r.x1 * 2, (r.x2 - r.x1 + 1) * 2);
  }
}

void dbufFlush(const lv_area_t* a, const uint16_t* px, bool last) {
  uint8_t* hid = dbufHidden();
  if (!inFrame) {
    inFrame = true;
    syncHidden(hid);
    nCur = 0;
    curFull = false;
  }
  bool flip = lcd.getRotation() == 2;
  int w = a->x2 - a->x1 + 1, h = a->y2 - a->y1 + 1;
  for (int j = 0; j < h; j++) {
    const uint16_t* s = px + (size_t)j * w;
    int dy = flip ? 479 - (a->y1 + j) : a->y1 + j;
    uint16_t* d = (uint16_t*)(hid + dy * ROW_BYTES);
    if (!flip) {
      d += a->x1;
      for (int i = 0; i < w; i++) d[i] = __builtin_bswap16(s[i]);
    } else {
      d += 799 - a->x1;
      for (int i = 0; i < w; i++) d[-i] = __builtin_bswap16(s[i]);
    }
  }
  R r;
  if (flip) r = {(int16_t)(799 - a->x2), (int16_t)(479 - a->y2), (int16_t)(799 - a->x1), (int16_t)(479 - a->y1)};
  else r = {(int16_t)a->x1, (int16_t)a->y1, (int16_t)a->x2, (int16_t)a->y2};
  Cache_WriteBack_Addr((uint32_t)(hid + r.y1 * ROW_BYTES), (r.y2 - r.y1 + 1) * ROW_BYTES);
  if (nCur < MAXR) curR[nCur++] = r; else curFull = true;
  if (last) {
    flipTo(hid);
    memcpy(prevR, curR, sizeof(R) * nCur);
    nPrev = nCur;
    prevFull = curFull;
    inFrame = false;
  }
}

bool dbufReadRow(int y, uint16_t* out) {
  if (!ready || (lcd.getRotation() & 1)) return false;
  bool flip = lcd.getRotation() == 2;
  int sy = flip ? 479 - y : y;
  const uint16_t* s = (const uint16_t*)(fbCur + sy * ROW_BYTES);
  for (int x = 0; x < 800; x++) out[x] = __builtin_bswap16(flip ? s[799 - x] : s[x]);
  return true;
}
