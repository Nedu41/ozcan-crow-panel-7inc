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

// Dogrudan mod (2026-10-13): LVGL kareleri GORUNEN tampona yazilir. Eski yontem (gizli tampona yaz + her karede gorunenden
// kopyala + vsync'te cevir) tam ekran memcpy yapiyordu; bounce kesmesinin PSRAM'den okumasi ile cakisip veri yetismiyor,
// ekranda parazit/titreme yapiyordu (olcum: 12800 bayt PSRAM okuma 574 us > 495 us butce). Kismi guncellemelerde yirtilma
// gozle gorulmez. Yalniz yapay zeka efekti (tam kare) cift tamponu kullanir: efekt acikken LVGL yazimi bastirilir.
static bool effectActive = false;

int displayW() { return (rotation & 1) ? PH : PW; }
int displayH() { return (rotation & 1) ? PW : PH; }
void displaySetRotation(uint8_t r) { rotation = r & 3; }

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
  c.bounce_buffer_size_px = PW * 8;  // iki DRAM tamponu (~26 KB): DMA PSRAM yerine buradan okur
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
  effectActive = true;
}

void displayEffectEnd() { effectActive = false; }

// mantıksal -> fiziksel (touchToScreen'in tersi)
static inline void toPhys(int lx, int ly, int& px, int& py) {
  switch (rotation) {
    case 0: px = lx; py = ly; break;
    case 1: px = PW - 1 - ly; py = lx; break;
    case 2: px = PW - 1 - lx; py = PH - 1 - ly; break;
    default: px = ly; py = PH - 1 - lx; break;
  }
}

void displayFlush(const lv_area_t* a, const uint16_t* px, bool) {
  if (effectActive) return;  // efekt karesi gorunuyor: LVGL ustune yazmasin
  uint16_t* dst = fb[cur];
  int w = a->x2 - a->x1 + 1, h = a->y2 - a->y1 + 1;
  if (rotation == 0) {
    for (int j = 0; j < h; j++) memcpy(dst + (size_t)(a->y1 + j) * PW + a->x1, px + (size_t)j * w, w * 2);
  } else {
    for (int j = 0; j < h; j++) {
      const uint16_t* s = px + (size_t)j * w;
      for (int i = 0; i < w; i++) {
        int ox, oy;
        toPhys(a->x1 + i, a->y1 + j, ox, oy);
        dst[(size_t)oy * PW + ox] = s[i];
      }
    }
  }
}

// Ekran goruntusu atomik olsun: dogrudan modda tampon surekli degisir, satir satir okumak yirtik goruntu verir
static uint16_t* snapFb = nullptr;
void displaySnapshot(bool on) {
  if (!on) { snapFb = nullptr; return; }
  static uint16_t* buf = nullptr;
  if (!buf) buf = (uint16_t*)heap_caps_malloc(FB_PX * 2, MALLOC_CAP_SPIRAM);
  if (!buf) return;
  memcpy(buf, fb[cur], FB_PX * 2);
  snapFb = buf;
}

bool displayReadRow(int y, uint16_t* out) {
  const uint16_t* src = snapFb ? snapFb : fb[cur];
  if (!src) return false;
  for (int x = 0; x < displayW(); x++) {
    int px, py;
    toPhys(x, y, px, py);
    out[x] = src[(size_t)py * PW + px];
  }
  return true;
}
