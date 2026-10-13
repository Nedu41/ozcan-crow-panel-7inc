# Röle düğümü firmware'i (lamba / priz)

HUB'a (crowhub.local) eklenen lamba/priz düğümleri bu firmware'i çalıştırır. Kaynak ev-lamba projesinden alınmıştır. Aynı kod tüm kartlarda çalışır; HUB'a aynı HTTP arayüzünü sunar (`/status /control /mac /save ...`).

## Desteklenen kartlar

| Kart (ortam adı) | Çip | Röle 1 / Röle 2 | Hazır imaj |
|---|---|---|---|
| ESP-01 / ESP-01S (`esp01_1m`) | ESP8266, 1 MB | GPIO0 / GPIO2 | `bin/esp01_1m.bin` |
| ESP-07 (`esp07_gpio45`) | ESP8266 | GPIO4 / GPIO5 | `bin/esp07_gpio45.bin` |
| NodeMCU, ESP-12E/F (`nodemcu`) | ESP8266, 4 MB | D1 (GPIO5) / D2 (GPIO4) | `bin/nodemcu.bin` |
| Wemos / LOLIN D1 mini (`d1_mini`) | ESP8266, 4 MB | D1 (GPIO5) / D2 (GPIO4) | `bin/d1_mini.bin` |
| Sonoff Basic (`sonoff_basic`) | ESP8285, 1 MB | GPIO12 (tek röle) | `bin/sonoff_basic.bin` |
| ESP32 DevKit (`esp32_devkit`) | ESP32 | GPIO26 / GPIO27 | `bin/esp32_devkit.factory.bin` |
| ESP32-C3 SuperMini (`c3_supermini`) | ESP32-C3 | GPIO3 / GPIO4 | `bin/c3_supermini.factory.bin` |
| ESP32-S3 DevKitC-1 (`esp32_s3`) | ESP32-S3 | GPIO4 / GPIO5 | `bin/esp32_s3.factory.bin` |

`*.bin` = uygulama imajı (OTA / ESP8266'da 0x0'dan yazılır). `*.factory.bin` = ESP32 ailesi için bootloader + bölüm tablosu + uygulama (0x0'dan yazılır). Tarayıcıdan yazmak için: **esp-firmware-yukleyici** sitesi (GitHub Pages).

Röle modülünüz **LOW ile çalışıyorsa** (çoğu 2 kanallı optokuplörlü modül): ilgili ortamın `build_flags` satırına `-DRELAY_ACTIVE_LOW` ekleyip yeniden derleyin.

## Yeni cihaz kurulumu
1. Firmware'i yükle (aşağıda). Kod içinde WiFi şifresi yoktur.
2. Cihaz açılınca kurulum ağını açar: **ESP01_SETUP** (ESP8266), **C3_SETUP**, **ESP32_SETUP**, **S3_SETUP**. Telefondan bağlan, açılan sayfada evin WiFi'sini seç, şifreyi gir.
3. Cihaz ağa bağlanır ve `esp01-xxxxxx.local` (ESP32 ailesi: `c3-`, `esp32-`, `s3-`) adıyla duyurulur (xxxxxx = MAC'in son 6 hanesi).
4. HUB'da (web arayüzü > Cihazlar) **Cihaz Tara** ile bulunur, isim verilip eklenir.

## Yükleme
- **Tarayıcıdan (önerilen):** esp-firmware-yukleyici sitesi, Chrome/Edge, USB.
- **PlatformIO:** `pio run -e <kart> -t upload --upload-port COMx` (ESP-01'de USB-seri programlayıcı, GPIO0 → GND ile flaş modu). Sonraki güncellemeler kablosuz (ArduinoOTA) yapılabilir.
- **esptool ile hazır imaj:** ESP8266: `python -m esptool --port COMx write_flash 0x0 bin/<kart>.bin`; ESP32 ailesi: `python -m esptool --port COMx write_flash 0x0 bin/<kart>.factory.bin`.
- Hepsini yeniden derlemek: `python derle_hepsi.py` (imajlar `bin/` altına yazılır).

## Pin notları
ESP-01: GPIO0/2 açılışta HIGH olmalı (röleyi NC üzerinden bağlayın). ESP-07: 4/5 serbest. NodeMCU/D1 mini: D1/D2 güvenli. ESP32 DevKit: 26/27 serbest. C3 SuperMini: GPIO2/8/9 strapping, 18/19 USB, 20/21 seri kullanılmaz, 3/4 serbest. S3: 4/5 serbest (0/3/45/46 strapping, 19/20 USB, 26-37 flaş/PSRAM kullanılmaz). Sonoff Basic'te ikinci röle yoktur (GPIO14 yer tutucu).
