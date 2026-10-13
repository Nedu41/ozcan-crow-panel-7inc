# ozcan-crow-panel-7inc — devir notu

Elecrow CrowPanel 7" (ESP32-S3 dokunmatik ekran) ile **LedPanel-Pico kumandası**. Kullanıcı Türkçe konuşur, kısa cevap ister, kanıtsız deneme önerme. LedPanel-Pico (`D:\Kodlar\LedPanel-Pico`) artık bu cihazdan kumanda edilmiyor; ona dokunma.

## Çalışma kuralları
- Önce kanıt/ölçüm, sonra kod. Pinler/ekran sürücüsü **doğrulanmadan** yazılmaz (global CLAUDE.md pin kontrol listesi: strapping, touch, ADC2, JTAG/USB, flash/PSRAM).
- Doğrulanmamış bir şeyi kesinmiş gibi sunma. `docs/DONANIM.md`'de yalnızca ölçülen/belgeden teyit edilen bilgi durur.
- Arayüz: koyu/gotik tema, siyah çerçeve, pastel renk; açık üstüne açık renk yok. Bkz. global CLAUDE.md tercihleri.
- Commit sonuna: `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. `src/secrets.h` commit'e girmez.

## Yapay zeka efektleri (2026-10-13: LedPanel kumandası kaldırıldı)
LedPanel istemcisi söküldü (flash için). Yerine **Efekt** sekmesi: PC'deki `araclar/efekt_koprusu.py` (HTTP :8767, anahtar `araclar/.kopru_anahtar`) efekt cümlesini Gemini'ye ürettirir, `frame(t,W,H)` numpy kodunu çalıştırır, kareleri **UDP 4048** ile cihaza yollar (10 bayt DDP başlığı + RGB565, 160x96 / dikeyde 96x160, cihazda 5x büyütülüp LVGL'siz doğrudan ekrana basılır). Köprü adresini cihaza kendisi bildirir (`/api/aibr`, NVS). Cihaz uçları: `/api/efekt` (boyut + alınan kare sayısı), `/api/aikey` (Gemini anahtarı, web Ayarlar'da da var). Gemini anahtarı cihazdan girilir (Ayar > Yapay zeka anahtarı, link: aistudio.google.com/apikey) ve köprüye iletilir.
Ölçüm: UDP'de paketler arası <1 ms ise ESP32 paket kaybediyor (0,3 ms'de %78 kare kaybı); köprü 1 ms bekler (~17 fps). Eski `D:\Kodlar\LedPanel-Picoraclari_koprusu.py` :8766'da çalışabilir; çakışmasın diye bu köprü 8767. **Dezavantaj: PC açık ve aynı ağda olmalı** (Özcan abinin evinde PC olmayabilir).

## Mimari karar (2026-10-12)
CrowPanel, `D:\Kodlar\ev-lamba` (Merkezi HUB: ESP01 röle yönetimi, zamanlayıcı, hava, web, OTA, MQTT, Telegram) kodunu devralan **yeni HUB** olur; LedPanel kumandası da aynı cihazda. Eski ILI9341'li ESP32 emekli. ev-lamba'ya dokunma, yalnızca kaynak olarak oku/taşı. İlk sürüm ekranı: hava/saat ana ekranı; lamba ve LedPanel sayfaları sonraki adım.

## Teslim gereksinimi (2026-10-12)
Cihaz Özcan abiye verilecek; evine götürünce kendi WiFi'sine **kodsuz, bilgisayarsız** bağlanabilmeli. Kurallar: koda gömülü SSID/şifre yok, `secrets.h` içeriği (Telegram vb.) cihaza girmez; WiFi kurulumu dokunmatik ekranda (ağ tara → seç → ekran klavyesiyle şifre); bağlanamazsa otomatik kurulum ekranına dön; kayıtlı ağ kalıcı (NVS/LittleFS); LedPanel/ESP01 adresleri IP ile değil mDNS ile bulunur (IP her evde değişir); hava konumu ayarlanabilir.

Lamba düğümleri: Özcan abinin evinde olup olmadığı bilinmiyor, ama mutlaka cihaz ekleyecek → cihaz ekleme (mDNS tarama + isim verme) ekrandan yapılabilmeli. Test için şimdilik kullanıcının kendi lambaları eklenir. ESP01'lere yeni WiFi dağıtma fikri askıda (bkz. ev-lamba hafızası `project-wifi-provision-idea`).

**Gizli bilgiler (Telegram token, API anahtarları, WiFi):** test aşamasında kullanıcının kendi değerleri `src/secrets.h`'ta (gitignore'lu) durabilir. **Teslimden önce hepsi silinir** (secrets.h + cihazdaki kayıtlı WiFi/NVS/LittleFS ayarları temizlenir, flash silinip temiz imaj yazılır); Özcan abi kendi bilgilerini ekrandan girer. Teslim öncesi bunu kullanıcıya hatırlat.

## Durum (2026-10-12, gece)
Çalışan (kartta doğrulandı): ekran + dokunmatik + yatay/dikey, dokunmatik WiFi kurulumu (kayıtlı ağ listesi `/wifi_nets.json`, açılışta sırayla denenir), ev-lamba HUB arka ucu (web arayüzü `http://<ip>/`, cihaz yönetimi, hava, haber, deprem, zamanlayıcı, Telegram, MQTT, OTA), LVGL sayfaları Ana/Lambalar/Efekt/Ayar, **OTA** (`pio run -t upload --upload-port <ip>`). mDNS adı `crowhub.local` (ev-lamba'nın `merkezihub.local`'ı ile çakışmasın diye). Seri port USB girişine göre değişir (COM3/COM14…).
Doğrulanmamış: lamba düğmeleriyle röle kontrolü (cihaz eklenmedi), arka ışık PWM kısma, 24 MHz ekran saati.
Geliştirme araçları: `-DDEV_SCREENSHOT` (platformio.ini) → `GET /screen.bmp`, `/dev/tab?i=`, `/dev/rot`. **Teslimde kapatılır** (bkz. `docs/TESLIM.md`).
Flash: uygulama ~1,82 MB / 1,97 MB OTA yuvası (%92) — yeni özellik eklerken boyutu izle.
Mimari: `src/ui.cpp` (sürücü+WiFi akışı+metin/onay pencereleri), `src/ui_hub.cpp` (sayfalar), `src/ledpanel.cpp` (istemci görevi), `src/data_task.cpp` (haber/hava/deprem), kalanı ev-lamba'dan taşındı (aynı API, aynı web sayfası `include/relay_webpage_multi.h`).
Telegram bot/chat ve hava konumu Ayar sayfasından girilir (NVS / hava ayarı dosyası).
Sıradaki: kullanıcı cevapları (aşağıdaki sorular), LedPanel gerçek test, lamba ekleme testi, Telegram/hava konumunu ekrandan girme, yapay zeka efekti sayfası, web arayüzüne LedPanel bölümü. Ardından Nextion projesi (`D:\Kodlar\ozcan-NX4832k035_011`).
