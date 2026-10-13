// ESP-IDF esp_lcd RGB paneli: iki PSRAM framebuffer (bkz. display.h).
#include "display.h"

#include <esp32s3/rom/cache.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_rgb.h>

#include "board.h"

static const int PW = 800, PH = 480;
static const size_t FB_PX = (size_t)PW * PH;
static esp_lcd_panel_handle_t panel = nullptr;
static uint16_t* fb[2] = {nullptr, nullptr};
static int cur = 0;  // görünen tampon
static SemaphoreHandle_t vsyncSem = nullptr;

static bool IRAM_ATTR onVsync(esp_lcd_panel_handle_t, const esp_lcd_rgb_panel_event_data_t*, void*) {
  BaseType_t woken = pdFALSE;
  xSemaphoreGiveFromISR(vsyncSem, &woken);
  return woken == pdTRUE;
}

void displayInit() {
  esp_lcd_rgb_panel_config_t c = {};
  c.clk_src = LCD_CLK_SRC_DEFAULT;
  c.timings.pclk_hz = 15000000;  // wiki değeri; 21/24 MHz LovyanGFX ile bozuldu
  c.timings.h_res = PW;
  c.timings.v_res = PH;
  c.timings.hsync_pulse_width = 48;
  c.timings.hsync_back_porch = 40;
  c.timings.hsync_front_porch = 40;
  c.timings.vsync_pulse_width = 31;
  c.timings.vsync_back_porch = 13;
  c.timings.vsync_front_porch = 1;
  c.timings.flags.hsync_idle_low = 1;
  c.timings.flags.vsync_idle_low = 1;
  c.timings.flags.de_idle_high = 0;
  c.timings.flags.pclk_active_neg = 0;
  c.timings.flags.pclk_idle_high = 0;
  c.data_width = 16;
  c.bits_per_pixel = 16;
  c.num_fbs = 2;
  c.bounce_buffer_size_px = 0;  // 800*8 bounce buffer denendi: cekirdek 1'i ve PSRAM bant genisligini tuketip arayuz karesini 5-7 kat yavaslatti
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
  vsyncSem = xSemaphoreCreateBinary();
  esp_lcd_rgb_panel_event_callbacks_t cb = {};
  cb.on_vsync = onVsync;
  ESP_ERROR_CHECK(esp_lcd_rgb_panel_register_event_callbacks(panel, &cb, nullptr));
  ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
  ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
  void *f0 = nullptr, *f1 = nullptr;
  ESP_ERROR_CHECK(esp_lcd_rgb_panel_get_frame_buffer(panel, 2, &f0, &f1));
  fb[0] = (uint16_t*)f0;
  fb[1] = (uint16_t*)f1;
  memset(fb[0], 0, FB_PX * 2);
  memset(fb[1], 0, FB_PX * 2);
  Cache_WriteBack_Addr((uint32_t)fb[0] & ~31u, FB_PX * 2);
  Cache_WriteBack_Addr((uint32_t)fb[1] & ~31u, FB_PX * 2);
  cur = 0;
}

void* displayBuffer(int i) { return fb[i & 1]; }

static void writeBack(const void* p, size_t n) {
  uint32_t a = (uint32_t)p & ~31u;
  Cache_WriteBack_Addr(a, (uint32_t)p + n - a);
}  // yalniz efekt yolu

static void showBuffer(int idx) {
  xSemaphoreTake(vsyncSem, 0);                              // eski sinyali at
  esp_lcd_panel_draw_bitmap(panel, 0, 0, PW, PH, fb[idx]);  // framebuffer ise kopyalamaz, vsync'te geçer
  xSemaphoreTake(vsyncSem, pdMS_TO_TICKS(60));              // sayfa çevrildi: LVGL eski tampona yazabilir
  cur = idx;
}

void displayFlush(const lv_area_t* a, uint8_t* px, bool last) {
  (void)a;
  if (!last) return;
  // LVGL direct mod iki tamponu kendi kopyalariyla esler (onbellekten); cevirmeden once TUM kirli satirlari PSRAM'e yaz
  // (veri onbellegi 32 KB, hepsini yazmak hizli). Yoksa eski/yari veri gosterilir ve ekran titreyerek kararir.
  Cache_WriteBack_All();
  showBuffer((uint16_t*)px == fb[0] ? 0 : 1);
}

uint16_t* displayHidden() { return fb[cur ^ 1]; }

void displayFlip(uint16_t* hidden) {
  writeBack(hidden, FB_PX * 2);
  showBuffer(hidden == fb[0] ? 0 : 1);
}

bool displayReadRow(int y, uint16_t* out) {
  if (!fb[cur]) return false;
  memcpy(out, fb[cur] + (size_t)y * PW, PW * 2);
  return true;
}
