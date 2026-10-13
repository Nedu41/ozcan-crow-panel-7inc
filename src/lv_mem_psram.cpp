// LVGL bellek ayirici: PSRAM'den (iç RAM'i TLS/WiFi için boş bırakır).
// Arduino malloc'u kucuk istekleri iç RAM'e koyuyordu; arayuz nesneleri iç RAM'i bitirince HTTPS
// ("SSL - Memory allocation failed") calismaz oldu. lv_conf.h: LV_USE_STDLIB_MALLOC = LV_STDLIB_CUSTOM.
#include <esp_heap_caps.h>
#include <lvgl.h>

extern "C" {
void lv_mem_init(void) {}
void lv_mem_deinit(void) {}
lv_mem_pool_t lv_mem_add_pool(void*, size_t) { return NULL; }
void lv_mem_remove_pool(lv_mem_pool_t) {}
void* lv_malloc_core(size_t size) { return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }
void* lv_realloc_core(void* p, size_t size) { return heap_caps_realloc(p, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }
void lv_free_core(void* p) { heap_caps_free(p); }
void lv_mem_monitor_core(lv_mem_monitor_t*) {}
lv_result_t lv_mem_test_core(void) { return LV_RESULT_OK; }
}
