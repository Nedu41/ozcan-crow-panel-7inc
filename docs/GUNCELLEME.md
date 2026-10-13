# Yazilim guncelleme (kablosuz)

Uc yol var; hepsi cihaz acikken, ayni agdan:

1. **Dosyadan (web)**: `http://crowhub.local` > Sistem > "Dosyadan guncelle". Dosya: `.pio/build/crowpanel/firmware.bin` (PlatformIO `pio run` ciktisi). Cihaz yazar ve yeniden baslar.
2. **GitHub'dan**: GitHub'da depoda bir *Release* olustur, etiketi surum numarasi yap (ornek `v7.3.0`) ve `firmware.bin` dosyasini yayina ekle.
   Cihazda: Ayar > "Guncelleme kaynagi" (veya web > Sistem) ile `kullanici/depo` gir, sonra "Guncellemeyi denetle" / "GitHub'dan guncelle". Yalnizca herkese acik (public) depolar; yeni surum, calisandan buyukse "yeni" diye bildirilir.
3. **PlatformIO ArduinoOTA**: `pio run -t upload --upload-port <ip>`. Ana sayfa cok yavasken takilabilir; o zaman USB (COM3) kullan.

Surum numarasi `include/config.h` `FW_VERSION` (ornek `7.2.0-crow`); yeni yayinda artirilmali.
Yeni uclar: `/update` (POST dosya), `/api/guncelle`, `/api/guncelle/denetle`, `/api/guncelle/github`, `/api/guncelle/kaynak?repo=`.
DIKKAT: `ESPAsyncWebServer` "/yol" kaydini "/yol/..." altini da eslesir; genel yol alt yollardan SONRA kaydedilmeli (bkz. web_extra.cpp, guncelle.cpp).
