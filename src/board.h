// CrowPanel 7" V3.0 ekran/dokunmatik sürücüsü. Pin kaynağı: docs/DONANIM.md
#pragma once
#include <Arduino.h>
#include <Wire.h>
constexpr int PIN_SDA = 19, PIN_SCL = 20, PIN_BL = 2;
constexpr uint8_t ADDR_PCA9557 = 0x18, ADDR_GT911 = 0x5D;

inline void pcaWrite(uint8_t reg, uint8_t v) {
  Wire.beginTransmission(ADDR_PCA9557);
  Wire.write(reg);
  Wire.write(v);
  Wire.endTransmission();
}

// V3.0 açılış sırası (dokunmatik RST/INT zamanlaması)
inline void boardPowerSeq() {
  Wire.begin(PIN_SDA, PIN_SCL);
  pcaWrite(0x03, 0xFF);  // reset(): hepsi giriş
  pcaWrite(0x01, 0xFF);  //          çıkış mandalları HIGH
  pcaWrite(0x02, 0x00);  //          polarite normal
  pcaWrite(0x03, 0x00);  // hepsi çıkış
  pcaWrite(0x01, 0xFC);  // IO0, IO1 LOW
  delay(20);
  pcaWrite(0x01, 0xFD);  // IO0 HIGH
  delay(100);
  pcaWrite(0x03, 0x02);  // IO1 giriş
}

// fresh: GT911'de yeni veri var (basıldı ya da bırakıldı). fresh=false iken önceki durum geçerli.
struct Touch { bool fresh, down; uint16_t x, y; };

// GT911 ham koordinatı -> ekran yönüne göre koordinat.

constexpr bool TOUCH_FLIP_X = false, TOUCH_FLIP_Y = false;  // kartta ölçüldü: GT911 ham değeri ekranla aynı yönde
constexpr int NATIVE_W = 800, NATIVE_H = 480;

// rot: ekran yönü (0/2 yatay, 1/3 dikey)
inline void touchToScreen(uint16_t rx, uint16_t ry, uint8_t rot, int& x, int& y) {
  int nx = TOUCH_FLIP_X ? NATIVE_W - 1 - rx : rx;
  int ny = TOUCH_FLIP_Y ? NATIVE_H - 1 - ry : ry;
  switch (rot & 3) {
    case 0: x = nx;                y = ny;                break;
    case 1: x = ny;                y = NATIVE_W - 1 - nx; break;
    case 2: x = NATIVE_W - 1 - nx; y = NATIVE_H - 1 - ny; break;
    default: x = NATIVE_H - 1 - ny; y = nx;               break;
  }
}

// GT911 poll (INT/RST kullanılmaz). Ham koordinat döner.
inline Touch touchRead() {
  Touch t{false, false, 0, 0};
  auto rd = [](uint16_t reg, uint8_t* b, uint8_t n) {
    Wire.beginTransmission(ADDR_GT911);
    Wire.write(reg >> 8);
    Wire.write(reg & 0xFF);
    if (Wire.endTransmission() != 0) return false;
    if (Wire.requestFrom((int)ADDR_GT911, (int)n) != n) return false;
    for (uint8_t i = 0; i < n; i++) b[i] = Wire.read();
    return true;
  };
  uint8_t st;
  if (!rd(0x814E, &st, 1) || !(st & 0x80)) return t;
  t.fresh = true;
  if ((st & 0x0F) > 0) {
    uint8_t p[8];
    if (rd(0x814F, p, 8)) { t.down = true; t.x = p[1] | (p[2] << 8); t.y = p[3] | (p[4] << 8); }
  }
  Wire.beginTransmission(ADDR_GT911);  // durum yazmacını temizle
  Wire.write(0x81); Wire.write(0x4E); Wire.write(0);
  Wire.endTransmission();
  return t;
}
