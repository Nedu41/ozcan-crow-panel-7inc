#include "mutfak.h"

#include "ses.h"

const char* const MT_AD[6] = {"CIG", "RAFADAN", "KAYISI", "AZ PISMIS", "TAM PISMIS", "COK PISMIS"};
const uint16_t MT_T[5] = {180, 300, 420, 540, 720};

// Web (AsyncTCP gorevi) yazar, displayTask okur: alanlar kucuk, kisa kritik bolge yeterli
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
static volatile bool on = false, run = false, done = false;
static uint32_t acc = 0, t0 = 0, dur = 300, doneMs = 0;
static int hedef = 2, lastSt = 0;

static uint32_t elMs() { return acc + (run ? millis() - t0 : 0); }

bool mtOn() { return on; }
bool mtRunning() { return run; }
bool mtDone() { return done; }
float mtElapsed() { return elMs() / 1000.0f; }
uint32_t mtDur() { return dur; }
int mtHedef() { return hedef; }
int mtStage(float e) { int s = 0; for (int i = 0; i < 5; i++) if (e >= MT_T[i]) s = i + 1; return s; }

void mtReset() {
  portENTER_CRITICAL(&mux);
  on = run = done = false;
  acc = 0;
  lastSt = 0;
  portEXIT_CRITICAL(&mux);
}

void mtStart(int h, uint32_t sec) {
  mtReset();
  portENTER_CRITICAL(&mux);
  hedef = constrain(h, 1, 5);
  dur = constrain(sec, 10UL, 3600UL);
  t0 = millis();
  run = on = true;
  portEXIT_CRITICAL(&mux);
  sesCal(SES_BILDIRIM);
}

void mtSet(int h, uint32_t sec) {
  portENTER_CRITICAL(&mux);
  hedef = constrain(h, 1, 5);
  dur = constrain(sec, 10UL, 3600UL);
  if (done && elMs() / 1000 < dur) done = false;  // sure uzatildi: alarm biter, sayim surer
  portEXIT_CRITICAL(&mux);
}

void mtPause(bool p) {
  portENTER_CRITICAL(&mux);
  bool changed = false;
  if (p && run) { acc += millis() - t0; run = false; changed = true; }
  else if (!p && !run && on && !done) { t0 = millis(); run = true; changed = true; }
  portEXIT_CRITICAL(&mux);
  if (changed) sesCal(SES_TIK);
}

void mtTick() {
  if (!on || !run) return;
  float e = mtElapsed();
  int st = mtStage(e);
  if (!done && e >= dur) {
    done = true;
    doneMs = millis();
    lastSt = st;
    sesCal(SES_UYARI);
    return;
  }
  if (st > lastSt) { lastSt = st; if (!done) sesCal(SES_BILDIRIM); }
  if (done) {  // sifirlanana kadar 10 dk boyunca dakikada bir hatirlat, sonra kapan
    static uint32_t lastMin = 0;
    uint32_t mn = (millis() - doneMs) / 60000UL;
    if (mn == 0) lastMin = 0;
    else if (mn != lastMin && mn < 10) { lastMin = mn; sesCal(SES_UYARI); }
    if (mn >= 10) mtReset();
  }
}

String mtJson() {
  String s = "{\"on\":";
  s += on ? "true" : "false";
  s += ",\"run\":"; s += run ? "true" : "false";
  s += ",\"done\":"; s += done ? "true" : "false";
  s += ",\"el\":" + String(elMs()) + ",\"dur\":" + String(dur) + ",\"hedef\":" + String(hedef);
  s += ",\"st\":" + String(mtStage(mtElapsed())) + ",\"t\":[";
  for (int i = 0; i < 5; i++) s += String(MT_T[i]) + (i < 4 ? "," : "]}");
  return s;
}
