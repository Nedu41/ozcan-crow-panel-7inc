# Donanım (yalnızca teyit edilenler)

Kaynak: `crow-7inc.jpeg` (kartın arka yüzü fotoğrafı, 2026-10-12).

## Kart (etiket + serigrafi)
- Elecrow **CrowPanel ESP32 HMI Display, Basic Series**, 7.0 inç
- SKU: **DIS08070H**, kart revizyonu **V3.0**
- Giriş: 5V 2A
- U2: metal kalkanlı ESP32 modülü

## Ölçülen (`esptool flash_id`, 2026-10-12, COM3 = CH340)
- Çip: ESP32-S3 (QFN56) rev v0.2, kristal 40 MHz
- PSRAM: gömülü 8 MB
- Flash: **4 MB** (üretici c8, aygıt 4016), quad, 3.3V
- MAC: 28:37:2f:84:6a:90

## Konnektörler / pinler (serigrafi)
| Konnektör | Pinler |
|---|---|
| I2C | IO20 = SCL, IO19 = SDA, 3V3, GND |
| GPIO_D | IO38, NC, 3V3, GND |
| UART0 | IO44 = RX0, IO43 = TX0, 3V3, GND |
| SPK (I2S) | IO18 = LRCLK, IO42 = BCLK, IO17 = SDIN |

Diğer: USB-C (UART0 yazılı), BOOT ve RESET düğmeleri, TF (microSD) yuvası, BAT konnektörü.

## Üretici belgesinden (Elecrow GitHub, V3.0 şematik/örnekler) — kartta henüz test edilmedi
Modül: ESP32-S3-WROOM-1-N4R8 (4 MB flash + 8 MB octal PSRAM; flash ölçümüyle uyumlu).
- Ekran: 800x480 RGB565 (16 bit), 50 pin FPC
  - B0..B4 = IO15, 7, 6, 5, 4; G0..G5 = IO9, 46, 3, 8, 16, 1; R0..R4 = IO14, 21, 47, 48, 45
  - DE = IO41, VSYNC = IO40, HSYNC = IO39, PCLK = IO0
  - Zamanlama: PCLK 15 MHz (wiki) / 24 MHz (V3.0 örneği), pclk_active_neg=1, HSYNC 40/48/40, VSYNC 1/31/13, ikisi de aktif-düşük
  - Arka ışık: IO2 (HIGH = açık, PWM olur); en son açılır
- I2C (IO19 SDA, IO20 SCL, ortak hat, kart üstü 4.7k pull-up):
  - PCA9557 GPIO genişletici **0x18**: IO0 = dokunmatik RST, IO1 = dokunmatik INT, IO2 = LCD-RESET, IO3 = LCD güç
  - GT911 dokunmatik **0x5D**, INT/RST doğrudan MCU'ya bağlı değil (poll ile okunur)
  - V3.0 açılış sırası: Wire.begin(19,20) → PCA9557 hepsi çıkış, IO0/IO1 LOW → 20 ms → IO0 HIGH → 100 ms → IO1 giriş → ekran başlat → arka ışık
  - PCA9557 yazmaçları: giriş 0x00, çıkış 0x01, polarite 0x02, yapılandırma 0x03 (1 = giriş)
- TF kart (SPI): CS = IO10, MOSI = IO11, SCK = IO12, MISO = IO13
- I2S hoparlör: IO17 (SDIN), IO18 (LRCLK), IO42 (BCLK)
- BOOT = IO0 (PCLK ile aynı pin, çalışırken buton olarak kullanılamaz)
- Seri: USB-C üzerinden CH340 → UART0 (IO43/IO44). IO19/IO20 S3'ün native-USB pinleri ama burada I2C'ye ayrılı: `ARDUINO_USB_CDC_ON_BOOT=0` olmalı.

## Serbest GPIO
Yalnızca **IO38** (GPIO_D). Diğer tüm pinler ekran, I2C, TF, I2S veya UART'a ayrılı. Kendi işimiz için yeni pin atamadan önce bu listeye bak.

## Kartta test edildi (2026-10-12)
- Ekran: yukarıdaki RGB pinleri + 15 MHz ile görüntü doğru, açılış sırası çalışıyor
- PCA9557 (0x18) ve GT911 (0x5D) I2C'de cevap veriyor; PSRAM 8 MB
- Dokunmatik: GT911 ham koordinatı ekranla aynı yönde (eksen çevirme YOK). Dikey/yatay dönüş dönüşümü `src/board.h` `touchToScreen()` içinde, yatay ve iki dikey yönde doğrulandı (LovyanGFX rot 1 ve 3 formülleri ilk varsayımın tersi çıktı)
- Platform: espressif32 7.0.1 (Arduino core 2.0.17), LovyanGFX 1.2.32; Bus_RGB için `Panel_RGB.hpp`/`Bus_RGB.hpp` elle include edilir

## Teyit bekleyen
- Ekran 24 MHz'de de kararlı mı (şimdilik 15 MHz)
- TF kart, I2S hoparlör
