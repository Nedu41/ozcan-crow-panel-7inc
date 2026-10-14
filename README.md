# CrowPanel 7" HUB

Elecrow **CrowPanel ESP32 7.0" (DIS08070H, V3.0)** dokunmatik ekranı ile ev otomasyon merkezi: lamba/priz (röle) düğümlerini yönetir, hava durumu ve deprem/hava uyarılarını gösterir. Dokunmatik arayüz (LVGL) ve aynı özelliklerin web arayüzü vardır.

## Özellikler
- **Ana sayfa:** saat, canlı gökyüzü animasyonu (hava durumuna göre), sıcaklık / nem / rüzgâr, 7 günlük tahmin, hava ve deprem uyarıları, kayan dünya haberleri.
- **Lambalar:** ESP-01 / ESP8266 / ESP32 röle düğümleri (mDNS ile bulunur), açma/kapama, zamanlayıcı (saat, gün, gün doğumu/batımı), bulut ve karanlık otomasyonu.
- **Ayar:** WiFi kurulumu (ekran klavyesi, kayıtlı ağlar), parlaklık / kontrast / canlılık, Telegram uyarıları, hava konumu, yazılım güncelleme.
- **Kablosuz güncelleme:** web'den `.bin` dosyası yükleme, GitHub release'inden indirme, PlatformIO OTA.

## Donanım
ESP32-S3-WROOM-1-N4R8 (4 MB flash, 8 MB PSRAM), 800x480 RGB ekran, GT911 dokunmatik. Pinler ve ölçümler: [docs/DONANIM.md](docs/DONANIM.md).

## Kurulum
- **Hazır imaj:** [Releases](../../releases) sayfasından `crowpanel-hub.factory.bin` (0x0'a yazılır) ya da tarayıcıdan yükleyici sitesi.
- **Derleme:** [PlatformIO](https://platformio.org/) ile `pio run -e release` (USB: `pio run -e release -t upload --upload-port COMx`). Ana sayfa yavaşken kablosuz yükleme takılabilir, USB önerilir.
- İlk açılışta ekrandan WiFi kurulumu yapılır; kimlik bilgisi koda gömülmez. İsteğe bağlı `include/secrets.h` (Telegram) yalnızca geliştirme içindir, `release` imajına girmez.

## Güncelleme
Ayrıntı: [docs/GUNCELLEME.md](docs/GUNCELLEME.md). Cihazdan: Ayar > Güncelleme kaynağı > `kullanıcı/depo`, sonra "Güncellemeyi denetle".

## Röle düğümü firmware'i
8 kart için ([araclar/firmware/esp01-role](araclar/firmware/esp01-role)): ESP-01/07, NodeMCU, D1 mini, Sonoff Basic, ESP32 DevKit, ESP32-C3, ESP32-S3. Hazır imajlar `bin/` altında.

## Notlar
- Ekran saati 12 MHz'dir; yükseltmek görüntüyü bozar (PSRAM bant genişliği). Ayrıntılar `docs/` ve commit mesajlarında.
- Gökyüzü animasyonu bilerek eski, LVGL görüntü ölçekli sürümdür (kare hızı düşük ama kararlı).
