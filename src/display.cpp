// ESP-IDF esp_lcd RGB paneli: çift PSRAM framebuffer + bounce buffer (bkz. display.h).
#include "display.h"

#include <esp_cache.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_rgb.h>

#include "board.h"

static const int PW = 800, PH = 480;
static const size_t FB_PX = (size_t)PW * PH;
static esp_lcd_panel_handle_t panel = nullptr;
static uint16_t* fb[2] = {nullptr, nullptr};
static int cur = 0;  // görünen
static uint8_t rotation = 0;

struct R { int16_t x1, y1, x2, y2; };  // fiziksel dikdörtgen
static const int MAXR = 48;
static R prevR[MAXR], curR[MAXR];
static int nPrev = 0, nCur = 0;
static bool prevFull = true, curFull = false, inFrame = false;

int displayW() { return (rotation & 1) ? PH : PW; }
int displayH() { return (rotation & 1) ? PW : PH; }
void displaySetRotation(uint8_t r) { rotation = r & 3; prevFull = true; }

void displayInit(uint8_t rot) {
  rotation = rot & 3;
  esp_lcd_rgb_panel_config_t c = {};
  c.clk_src = LCD_CLK_SRC_DEFAULT;
  c.timings.pclk_hz = 15000000;  // wiki değeri; 21/24 MHz bozuldu (PSRAM bant genişliği)
  c.timings.h_res = PW;
  c.timings.v_res = PH;
  c.timings.hsync_pulse_width = 48;
  c.timings.hsync_back_porch = 40;
  c.timings.hsync_front_porch = 40;
  c.timings.vsync_pulse_width = 31;
  c.timings.vsync_back_porch = 13;
  c.timings.vsync_front_porch = 1;
  c.timings.flags.hsync_idle_low = 1;   // LovyanGFX hsync_polarity=0 ile aynı etki
  c.timings.flags.vsync_idle_low = 1;
  c.timings.flags.de_idle_high = 0;
  c.timings.flags.pclk_active_neg = 0;  // LovyanGFX'te lcd_ck_out_edge = pclk_idle_high(0) idi
  c.timings.flags.pclk_idle_high = 0;
  c.data_width = 16;
  c.bits_per_pixel = 16;
  c.num_fbs = 2;
  c.bounce_buffer_size_px = PW * 6;  // iki DRAM tamponu (~19 KB): DMA PSRAM yerine buradan okur
  c.dma_burst_size = 64;
  c.hsync_gpio_num = 39;
  c.vsync_gpio_num = 40;
  c.de_gpio_num = 41;
  c.pclk_gpio_num = 0;
  c.disp_gpio_num = -1;
  // B0..B4, G0..G5, R0..R4
  static const int d[16] = {15, 7, 6, 5, 4, 9, 46, 3, 8, 16, 1, 14, 21, 47, 48, 45};
  for (int i = 0; i < 16; i++) c.data_gpio_nums[i] = d[i];
  c.flags.fb_in_psram = 1;
  ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&c, &panel));
  ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
  ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
  void *f0 = nullptr, *f1 = nullptr;
  ESP_ERROR_CHECK(esp_lcd_rgb_panel_get_frame_buffer(panel, 2, &f0, &f1));
  fb[0] = (uint16_t*)f0;
  fb[1] = (uint16_t*)f1;
  memset(fb[0], 0, FB_PX * 2);
  memset(fb[1], 0, FB_PX * 2);
  esp_cache_msync(fb[0], FB_PX * 2, ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);
  esp_cache_msync(fb[1], FB_PX * 2, ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);
  cur = 0;
  prevFull = true;
}

static void msync(const void* p, size_t n) {
  esp_cache_msync((void*)p, n, ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);
}

uint16_t* displayHidden() { return fb[cur ^ 1]; }

static void flipTo(int idx) {
  esp_lcd_panel_draw_bitmap(panel, 0, 0, PW, PH, fb[idx]);  // framebuffer ise kopyalamaz, vsync'te geçer
  cur = idx;
}

void displayFlip(uint16_t* hidden) {
  msync(hidden, FB_PX * 2);
  flipTo(hidden == fb[0] ? 0 : 1);
  prevFull = true;
  nPrev = 0;
  inFrame = false;
}

// mantıksal -> fiziksel (touchToScreen'in tersi)
static inline void toPhys(int lx, int ly, int& px, int& py) {
  switch (rotation) {
    case 0: px = lx; py = ly; break;
    case 1: px = PW - 1 - ly; py = lx; break;
    case 2: px = PW - 1 - lx; py = PH - 1 - ly; break;
    default: px = ly; py = PH - 1 - lx; break;
  }
}

static void syncHidden(uint16_t* hid) {
  uint16_t* vis = fb[cur];
  if (prevFull) { memcpy(hid, vis, FB_PX * 2); return; }
  for (int i = 0; i < nPrev; i++) {
    const R& r = prevR[i];
    for (int y = r.y1; y <= r.y2; y++) memcpy(hid + (size_t)y * PW + r.x1, vis + (size_t)y * PW + r.x1, (r.x2 - r.x1 + 1) * 2);
  }
}

void displayFlush(const lv_area_t* a, const uint16_t* px, bool last) {
  uint16_t* hid = fb[cur ^ 1];
  if (!inFrame) {
    inFrame = true;
    syncHidden(hid);
    nCur = 0;
    curFull = false;
  }
  int w = a->x2 - a->x1 + 1, h = a->y2 - a->y1 + 1;
  if (rotation == 0) {
    for (int j = 0; j < h; j++) memcpy(hid + (size_t)(a->y1 + j) * PW + a->x1, px + (size_t)j * w, w * 2);
  } else {
    for (int j = 0; j < h; j++) {
      const uint16_t* s = px + (size_t)j * w;
      for (int i = 0; i < w; i++) {
        int ox, oy;
        toPhys(a->x1 + i, a->y1 + j, ox, oy);
        hid[(size_t)oy * PW + ox] = s[i];
      }
    }
  }
  int x1, y1, x2, y2;
  toPhys(a->x1, a->y1, x1, y1);
  toPhys(a->x2, a->y2, x2, y2);
  R r = {(int16_t)min(x1, x2), (int16_t)min(y1, y2), (int16_t)max(x1, x2), (int16_t)max(y1, y2)};
  msync(hid + (size_t)r.y1 * PW, (size_t)(r.y2 - r.y1 + 1) * PW * 2);
  if (nCur < MAXR) curR[nCur++] = r; else curFull = true;
  if (last) {
    flipTo(cur ^ 1);
    memcpy(prevR, curR, sizeof(R) * nCur);
    nPrev = nCur;
    prevFull = curFull;
    inFrame = false;
  }
}

bool displayReadRow(int y, uint16_t* out) {
  if (!fb[cur]) return false;
  for (int x = 0; x < displayW(); x++) {
    int px, py;
    toPhys(x, y, px, py);
    out[x] = fb[cur][(size_t)py * PW + px];
  }
  return true;
}
