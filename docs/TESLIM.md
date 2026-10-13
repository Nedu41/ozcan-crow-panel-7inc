# Teslim kontrol listesi (Özcan abiye vermeden önce)

1. `platformio.ini` içinden `-DDEV_SCREENSHOT` satırını sil (`/screen.bmp` ve `/dev/*` kapanır).
2. `include/secrets.h` dosyasını sil (Telegram token/chat id). Telegram bu durumda kapalı kalır, hata vermez.
3. `include/relay_webpage_multi.h` ve kodda kişisel bilgi (IP, ad, konum) kalmadığını `grep` ile doğrula (son tarama 2026-10-12: yok).
4. Kartı tamamen sil ve temiz imajı yaz: `python -m esptool --port COMx erase_flash` sonra `pio run -t upload`. (NVS: WiFi dışı ayarlar, LittleFS: `/wifi_nets.json`, cihazlar, hava ayarı silinir. 2026-10-13: kayıtlı WiFi ağları kalabilir, kullanıcı karar verdi; flash silinmezse WiFi ve diğer ayarlar korunur.) Gemini API anahtarı da NVS'te durur (Ayar > Yapay zeka anahtarı); silinecekse flash silinir.
5. İlk açılış denemesi: WiFi kurulum ekranı gelmeli, ağ seç → şifre → IP görünmeli; yeniden başlatınca kendiliğinden bağlanmalı.
6. Ayar sayfasında "WiFi'leri sil" ve web arayüzünde `/reset_wifi` ile fabrika WiFi sıfırlaması çalışıyor.
7. Özcan abiye söylenecekler: cihaz ekleme web arayüzünden (`http://crowhub.local` ya da Ayar sayfasındaki IP), hava konumu web > Hava ayarları, Telegram isterse Ayar > Telegram (bot token + chat no), hava konumu için Ayar > Hava konumu.
