// Hoparlor: CrowPanel 7" V3.0 kart ustu I2S amfi (SPK konnektoru). Pinler URETICI BELGESINDEN, kartta denenmedi:
// SDIN IO17, LRCLK IO18, BCLK IO42 (docs/DONANIM.md). HUB35'teki ses.cpp ile ayni arayuz; nota LEDC yerine
// I2S'e 16 kHz kare dalga ornekleri olarak yazilir. Seviye ornek genligiyle ayarlanir.
#include "ses.h"

#include <Preferences.h>
#include <driver/i2s.h>

constexpr int PIN_SDIN = 17, PIN_LRCLK = 18, PIN_BCLK = 42;
constexpr int SR = 16000;
constexpr i2s_port_t PORT = I2S_NUM_0;

struct Nota { uint16_t hz, ms; };  // hz = 0: sus
static const Nota TIK[] = {{3200, 12}};
static const Nota ACILIS[] = {{523, 90}, {659, 90}, {784, 90}, {1047, 160}};
static const Nota UYARI[] = {{880, 180}, {0, 80}, {660, 180}, {0, 80}, {880, 180}, {0, 80}, {660, 260}};
static const Nota DEPREM[] = {{1200, 120}, {0, 60}, {1200, 120}, {0, 60}, {1200, 120}, {0, 300},
                              {1200, 120}, {0, 60}, {1200, 120}, {0, 60}, {1200, 120}};
static const Nota BILDIRIM[] = {{988, 70}, {0, 40}, {1319, 120}};
static const Nota MARTI[] = {{2300, 70}, {2150, 70}, {1950, 80}, {1700, 130}, {0, 90},
                             {2250, 60}, {2050, 70}, {1750, 120}, {0, 120},
                             {1550, 55}, {0, 45}, {1600, 55}, {0, 45}, {1550, 55}, {0, 45}, {1500, 70}};
static const Nota DENEME[] = {{440, 250}, {0, 80}, {880, 250}, {0, 80}, {1760, 250}};

static QueueHandle_t q;
static bool acik = true, hazir = false;
static bool acilis = false;  // her yeniden baslamada calmasin diye varsayilan kapali
static int seviye = 6;

static void notaCal(uint16_t hz, uint16_t ms) {
  static int16_t buf[256 * 2];                      // 256 stereo ornek (L = R)
  const int16_t amp = (int16_t)(seviye * 1200);     // seviye 10 ~ %37 tam olcek (amfi zaten guclu)
  uint32_t n = (uint32_t)SR * ms / 1000, per = hz ? SR / hz : 0, ph = 0;
  while (n > 0) {
    int m = n > 256 ? 256 : n;
    for (int i = 0; i < m; i++) {
      int16_t v = 0;
      if (per) { v = (ph < per / 2) ? amp : -amp; if (++ph >= per) ph = 0; }
      buf[2 * i] = buf[2 * i + 1] = v;
    }
    size_t w;
    i2s_write(PORT, buf, m * 4, &w, portMAX_DELAY);
    n -= m;
  }
}

static void gorev(void*) {
  SesTur t;
  for (;;) {
    if (xQueueReceive(q, &t, portMAX_DELAY) != pdTRUE) continue;
    const Nota* n; size_t k;
    switch (t) {
      case SES_TIK:      n = TIK;      k = sizeof(TIK) / sizeof(Nota); break;
      case SES_ACILIS:   n = ACILIS;   k = sizeof(ACILIS) / sizeof(Nota); break;
      case SES_UYARI:    n = UYARI;    k = sizeof(UYARI) / sizeof(Nota); break;
      case SES_DEPREM:   n = DEPREM;   k = sizeof(DEPREM) / sizeof(Nota); break;
      case SES_BILDIRIM: n = BILDIRIM; k = sizeof(BILDIRIM) / sizeof(Nota); break;
      case SES_MARTI:    n = MARTI;    k = sizeof(MARTI) / sizeof(Nota); break;
      default:           n = DENEME;   k = sizeof(DENEME) / sizeof(Nota); break;
    }
    for (size_t i = 0; i < k; i++) notaCal(n[i].hz, n[i].ms);
    notaCal(0, 30);                                 // susunca sifir ornek: amfi bosta cizirdamasin
    i2s_zero_dma_buffer(PORT);
  }
}

void sesInit() {
  Preferences p;
  p.begin("ses", true);
  acik = p.getBool("on", true);
  acilis = p.getBool("acl", false);
  seviye = constrain((int)p.getUChar("lv", 6), 1, 10);
  p.end();
  i2s_config_t c = {};
  c.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  c.sample_rate = SR;
  c.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  c.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  c.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  c.dma_buf_count = 4;
  c.dma_buf_len = 256;
  c.tx_desc_auto_clear = true;
  i2s_pin_config_t pins = {};
  pins.mck_io_num = I2S_PIN_NO_CHANGE;
  pins.bck_io_num = PIN_BCLK;
  pins.ws_io_num = PIN_LRCLK;
  pins.data_out_num = PIN_SDIN;
  pins.data_in_num = I2S_PIN_NO_CHANGE;
  if (i2s_driver_install(PORT, &c, 0, nullptr) != ESP_OK || i2s_set_pin(PORT, &pins) != ESP_OK) {
    Serial.println("[Ses] I2S baslatilamadi - ses kapali");
    return;
  }
  i2s_zero_dma_buffer(PORT);
  q = xQueueCreate(4, sizeof(SesTur));
  xTaskCreatePinnedToCore(gorev, "Ses", 3072, nullptr, 2, nullptr, 1);
  hazir = true;
  Serial.println("[Ses] I2S hazir (SDIN 17, LRCLK 18, BCLK 42)");
}

void sesCal(SesTur t) {
  if (!hazir || !q || (!acik && t != SES_DENEME) || (t == SES_ACILIS && !acilis)) return;
  xQueueSend(q, &t, 0);
}

bool sesAcik() { return acik; }
bool sesAcilisAcik() { return acilis; }
void sesAcilisAyarla(bool on) {
  acilis = on;
  Preferences p;
  p.begin("ses", false);
  p.putBool("acl", on);
  p.end();
}
int sesSeviye() { return seviye; }

void sesAcikAyarla(bool on) {
  acik = on;
  Preferences p;
  p.begin("ses", false);
  p.putBool("on", on);
  p.end();
}

void sesSeviyeAyarla(int v) {
  seviye = constrain(v, 1, 10);
  Preferences p;
  p.begin("ses", false);
  p.putUChar("lv", seviye);
  p.end();
}
