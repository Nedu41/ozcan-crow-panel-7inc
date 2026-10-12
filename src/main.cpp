// Donanım testi: ekran "merhaba" + dokunmatik + yön (yatay/dikey) (docs/DONANIM.md)
#include "board.h"

LGFX lcd;
uint8_t rot = 0;  // 0/2 yatay, 1/3 dikey

constexpr int BTN_W = 150, BTN_H = 60;

void drawUi() {
  lcd.fillScreen(TFT_BLACK);
  lcd.setTextColor(0xB5B6, TFT_BLACK);  // acik gri
  lcd.setTextSize(3);
  lcd.setCursor(20, 20);
  lcd.print("Merhaba CrowPanel");
  lcd.setTextSize(2);
  lcd.setCursor(20, 70);
  lcd.printf("%dx%d  yon=%u (%s)", lcd.width(), lcd.height(), rot, lcd.width() > lcd.height() ? "yatay" : "dikey");
  // Dondur dugmesi: sag alt; koyu zemin + acik yazi
  int bx = lcd.width() - BTN_W - 10, by = lcd.height() - BTN_H - 10;
  lcd.fillRoundRect(bx, by, BTN_W, BTN_H, 8, 0x2104);
  lcd.drawRoundRect(bx, by, BTN_W, BTN_H, 8, 0x07FF);
  lcd.setTextColor(0x07FF, 0x2104);
  lcd.setCursor(bx + 28, by + 22);
  lcd.print("DONDUR");
  // Kose isaretleri (eksen yonu dogrulamasi): sol ust dolu
  lcd.fillRect(0, 0, 14, 14, 0xFD20);
}

void setRot(uint8_t r) {
  rot = r & 3;
  lcd.setRotation(rot);
  drawUi();
  Serial.printf("yon=%u %dx%d\n", rot, lcd.width(), lcd.height());
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.printf("PSRAM: %s, %u bayt\n", psramFound() ? "var" : "YOK", ESP.getPsramSize());
  if (!psramFound()) { Serial.println("PSRAM yok: ekran baslatilmiyor"); return; }

  boardPowerSeq();
  for (uint8_t a : {ADDR_PCA9557, ADDR_GT911}) {
    Wire.beginTransmission(a);
    Serial.printf("I2C 0x%02X: %s\n", a, Wire.endTransmission() == 0 ? "cevap verdi" : "YOK");
  }

  lcd.init();
  setRot(0);
  pinMode(PIN_BL, OUTPUT);
  digitalWrite(PIN_BL, HIGH);  // en son
  Serial.println("ekran acildi (seriden 'r' = dondur)");
}

void loop() {
  static uint32_t last;
  if (Serial.available() && Serial.read() == 'r') setRot(rot + 1);

  Touch t = touchRead();
  if (t.down && millis() - last > 30) {
    last = millis();
    int x, y;
    touchToScreen(t.x, t.y, rot, x, y);
    Serial.printf("ham %u,%u -> ekran %d,%d (yon %u)\n", t.x, t.y, x, y, rot);
    int bx = lcd.width() - BTN_W - 10, by = lcd.height() - BTN_H - 10;
    if (x >= bx && x < bx + BTN_W && y >= by && y < by + BTN_H) {
      setRot(rot + 1);
      delay(300);
      return;
    }
    lcd.fillCircle(x, y, 8, 0x07FF);
  }
  delay(5);
}
