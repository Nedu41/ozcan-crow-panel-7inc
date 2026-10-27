# ozcan-crow-panel-7inc — devir notu

Elecrow CrowPanel 7" (ESP32-S3 dokunmatik ekran) ile **LedPanel-Pico kumandası**. Kullanıcı Türkçe konuşur, kısa cevap ister, kanıtsız deneme önerme. LedPanel-Pico (`C:\Kodlar\LedPanel-Pico`) artık bu cihazdan kumanda edilmiyor; ona dokunma.

## Çalışma kuralları
- Önce kanıt/ölçüm, sonra kod. Pinler/ekran sürücüsü **doğrulanmadan** yazılmaz (global CLAUDE.md pin kontrol listesi: strapping, touch, ADC2, JTAG/USB, flash/PSRAM).
- Doğrulanmamış bir şeyi kesinmiş gibi sunma. `docs/DONANIM.md`'de yalnızca ölçülen/belgeden teyit edilen bilgi durur.
- Arayüz: koyu/gotik tema, siyah çerçeve, pastel renk; açık üstüne açık renk yok. Bkz. global CLAUDE.md tercihleri.
- Commit sonuna: `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. `src/secrets.h` commit'e girmez.

## Yapay zeka efektleri KALDIRILDI (2026-10-14)
Efekt sekmesi (ekran+web), UDP alıcı (`efekt.cpp`), Gemini anahtarı ayarı, PC köprüsü (`efekt_koprusu.py`, `efektler/`) ve QR kodu (LV_USE_QRCODE=0) silindi; `-fno-access-control` bayrağı kalktı. Sekmeler: Ana / Lambalar / Ayar (Ayar artık indeks 2, `/dev/tab?i=2`). Eski kod git geçmişinde (`core3-esp-lcd` dalı ve 2026-10-13 öncesi commit'ler). Flash: ~1,81 MB / 1,97 MB (%92).

## Mimari karar (2026-10-12)
CrowPanel, `C:\Kodlar\ev-lamba` (Merkezi HUB: ESP01 röle yönetimi, zamanlayıcı, hava, web, OTA, MQTT, Telegram) kodunu devralan **yeni HUB** olur; LedPanel kumandası da aynı cihazda. Eski ILI9341'li ESP32 emekli. ev-lamba'ya dokunma, yalnızca kaynak olarak oku/taşı. İlk sürüm ekranı: hava/saat ana ekranı; lamba ve LedPanel sayfaları sonraki adım.

## Teslim gereksinimi (2026-10-12)
Cihaz Özcan abiye verilecek; evine götürünce kendi WiFi'sine **kodsuz, bilgisayarsız** bağlanabilmeli. Kurallar: koda gömülü SSID/şifre yok, `secrets.h` içeriği (Telegram vb.) cihaza girmez; WiFi kurulumu dokunmatik ekranda (ağ tara → seç → ekran klavyesiyle şifre); bağlanamazsa otomatik kurulum ekranına dön; kayıtlı ağ kalıcı (NVS/LittleFS); LedPanel/ESP01 adresleri IP ile değil mDNS ile bulunur (IP her evde değişir); hava konumu ayarlanabilir.

Lamba düğümleri: Özcan abinin evinde olup olmadığı bilinmiyor, ama mutlaka cihaz ekleyecek → cihaz ekleme (mDNS tarama + isim verme) ekrandan yapılabilmeli. Test için şimdilik kullanıcının kendi lambaları eklenir. ESP01'lere yeni WiFi dağıtma fikri askıda (bkz. ev-lamba hafızası `project-wifi-provision-idea`).

**Gizli bilgiler (Telegram token, API anahtarları, WiFi):** test aşamasında kullanıcının kendi değerleri `src/secrets.h`'ta (gitignore'lu) durabilir. **Teslimden önce hepsi silinir** (secrets.h + cihazdaki kayıtlı WiFi/NVS/LittleFS ayarları temizlenir, flash silinip temiz imaj yazılır); Özcan abi kendi bilgilerini ekrandan girer. Teslim öncesi bunu kullanıcıya hatırlat.

## Durum (2026-10-12, gece)
Çalışan (kartta doğrulandı): ekran + dokunmatik + yatay/dikey, dokunmatik WiFi kurulumu (kayıtlı ağ listesi `/wifi_nets.json`, açılışta sırayla denenir), ev-lamba HUB arka ucu (web arayüzü `http://<ip>/`, cihaz yönetimi, hava, haber, deprem, zamanlayıcı, Telegram, MQTT, OTA), LVGL sayfaları Ana/Lambalar/Efekt/Ayar, **OTA** (`pio run -t upload --upload-port <ip>`). mDNS adı `crowhub.local` (ev-lamba'nın `merkezihub.local`'ı ile çakışmasın diye). Seri port USB girişine göre değişir (COM3/COM14…).
Doğrulanmamış: lamba düğmeleriyle röle kontrolü (cihaz eklenmedi), arka ışık PWM kısma, 24 MHz ekran saati.
Geliştirme araçları: `-DDEV_SCREENSHOT` (platformio.ini) → `GET /screen.bmp`, `/dev/tab?i=`, `/dev/rot`. **Teslimde kapatılır** (bkz. `docs/TESLIM.md`).
Flash: uygulama ~1,85 MB / 1,97 MB OTA yuvası (%94, 2026-10-13) — yeni özellik eklerken boyutu izle.
Mimari: `src/ui.cpp` (sürücü+WiFi akışı+metin/onay pencereleri), `src/ui_hub.cpp` (sayfalar), `src/ledpanel.cpp` (istemci görevi), `src/data_task.cpp` (haber/hava/deprem), kalanı ev-lamba'dan taşındı (aynı API, aynı web sayfası `include/relay_webpage_multi.h`).
Telegram bot/chat ve hava konumu Ayar sayfasından girilir (NVS / hava ayarı dosyası).
Sıradaki: kullanıcı cevapları (aşağıdaki sorular), LedPanel gerçek test, lamba ekleme testi, Telegram/hava konumunu ekrandan girme, yapay zeka efekti sayfası, web arayüzüne LedPanel bölümü. Ardından Nextion projesi (`C:\Kodlar\ozcan-NX4832k035_011`).

## Güncelleme (2026-10-13)
- Efekt gösterimi **çift tampon**: gizli PSRAM tamponuna çizilip vsync'te LovyanGFX DMA işaretçileri çevrilir (`src/efekt.cpp`, `-fno-access-control` build_src_flags). Ham framebuffer bayt sırası ters (bswap16). Yalnızca yatayda; dikeyde eski yol (akabilir). 24 MHz ekran saati denendi, titretti, geri alındı.
- Lambalar sekmesi web kartıyla aynı (dBm, R1/R2, zamanlayıcı özeti) + `src/ui_timer.cpp` zamanlayıcı penceresi (saat, gün, gündoğumu/batımı).
- Web: sayfalar Ana (saat/hava/haber/lamba/efekt), Lambalar, Efekt, Oto-Hava, Cihazlar, Sistem (parlaklık, yön, Telegram, bilgi), WiFi, Kayıt, Notlar. Yeni uçlar: `/api/news`, `/api/sistem*`, `/api/telegram`, `/api/efekt/*`, `/api/aikey` (`src/web_extra.cpp`, `src/efekt.cpp`).
- PC kurulumu: `araclar/KURULUM.bat`.

## 2026-10-13 ogleden sonra - "hava-ilk" dali (GUNCEL CALISAN SURUM)
Kullanici isteviyle sabahki ilk hava efekti kodunun (commit ab81947, LVGL goruntu olcekli gokyuzu) ustune donuldu; `core3-esp-lcd` dalindaki Arduino core 3 / esp_lcd / cift tampon / karo mantigi KULLANILMIYOR (referans icin duruyor).
Bu dalda: platform espressif32@7.0.1 (core 2.0.17, LovyanGFX); ekran saati 12 MHz (15 MHz'de hafif sallanti vardi; 12 MHz'de parazit/titreme kullanici tarafindan gitti diye dogrulandi; 21/24 MHz bozuyor, yukseltme); kayan haber seridi (alt, tam genislik); sehir + surum ust satirda; canli renkler (UI + gokyuzu paleti doygunluk x1.95); ince cerceveler; LVGL bellegi PSRAM'de (src/lv_mem_psram.cpp; yoksa haber/hava TLS "Memory allocation failed"); hubOnWifiUp ayri gorevde (NTP/mDNS/web kurulumu arayuzu dondurmasin).
Olcum bulgusu (dal wip-olcum-2026-10-13): bounce-buffer'li esp_lcd'de ISR'nin her 495 us'de yaptigi 12800 bayt PSRAM okumasi ana sayfada 574 us suruyordu (butce 495) -> parazit. Gokyuzu yukunu azaltmadan ekran saatini yukseltme.
Yukleme: ana sayfa yavasken OTA %10'da takiliyor -> USB (COM3) ile yukle: `pio run -t upload --upload-port COM3`.

## 2026-10-13 aksam - firmware dagitimi
- `araclar/firmware/esp01-role`: role dugumu firmware'i, 8 kart (ESP-01/07, NodeMCU, D1 mini, Sonoff Basic, ESP32 DevKit, C3 SuperMini, S3); hazir imajlar `bin/`; `derle_hepsi.py` hepsini derler, `../birlestir.py` ESP32 ailesi icin tek parcali "factory" imaj uretir (PlatformIO'nun gercek esptool bayraklariyla, flas basligi 4MB).
- Teslim/release imaji: `pio run -e release` (DEV_SCREENSHOT kapali) -> `.pio/build/release/firmware.bin` (guncelleme) ve `birlestir.py . release <cikis>` ile factory imaj.
- Web yukleyici sitesi ayri depo: `C:\Kodlar\esp-firmware-yukleyici` (ESP Web Tools gomulu, `cihazlar.json` + `tools/uret.py`). YEREL commit hazir; GitHub'a gonderilmedi (herkese acik depo + Pages izni bekliyor).
- GitHub release v7.2.1 ve depoyu herkese acma izin sistemince reddedildi (2026-10-13); kullanici onayi/izin kurali bekleniyor. Cihazdan "GitHub'dan guncelle" yalniz HERKESE ACIK depo + release `firmware.bin` ile calisir; gercek depoyla henuz denenmedi.

## 2026-10-14 gece - yayin ve kisisel bilgi temizligi
- **Herkese acik depo:** `Nedu41/crowpanel-hub` (temiz, tek gecmis, noreply kimlik). Bu (ozel) depodan uretilir: `python tools/yayin_hazirla.py` (varsayilan hedef `C:\Kodlar\crowpanel-hub-public`; CLAUDE.md ve bu betik yayina girmez; tarama kisisel bilgi bulursa DURUR), sonra hedefte `git push`. Release: `gh release create vX.Y.Z --repo Nedu41/crowpanel-hub firmware.bin crowpanel-7in.factory.bin` (`pio run -e release`, factory imaj: `python araclar/firmware/birlestir.py . release <cikis> esp32s3`).
- **GITHUB GUNCELLEME UCTAN UCA DENENDI** (7.2.0 -> 7.2.1, kaynak `Nedu41/crowpanel-hub`). Kartta artik **release imaji 7.2.1** calisiyor: `/screen.bmp` ve `/dev/*` YOK, Telegram kapali (secrets.h imaja girmez; Ayar > Telegram ile yeniden girilir). Gelistirme icin USB ile `pio run -e crowpanel -t upload --upload-port COM3` (OTA ana sayfa yavasken takilir).
- **Imajlarda kisisel bilgi bulundu ve giderildi:** (1) release imajina Telegram token+chat gomuluydu -> `[env:release]` `-DSECRETS_DISABLED`; (2) ESP32 imajlarinda derleme yolundaki Windows kullanici adi -> `tools/gizlilik.py` (-ffile-prefix-map). Imaj yayinlamadan once mutlaka tara (Python ile bayt arama; `strings` komutu yok).
- **Yukleyici sitesi** ev-lamba projesinde: `C:\Kodlar\ev-lamba\yukleyici\` (model adlariyla cihaz listesi: CrowPanel 7", ESP32+ILI9341 TFT, ESP-01/07, NodeMCU, D1 mini, Sonoff, ESP32, C3, S3; COM port secimi aciklamasi). `C:\Kodlar\esp-firmware-yukleyici` eski yerel kopya (artik gereksiz, silinebilir). ev-lamba commit'leri YEREL, push edilmedi (deposunda ayrica 35 gonderilmemis commit var). ev-lamba GitHub deposu zaten HERKESE ACIK.
- **ev-lamba acik depo temizlendi (2026-10-14):** kaynaktaki kisisel varsayilanlar (MQTT `evhub/`, genel konum, demo metinleri, e-posta) degistirildi; commit gecmisi yeniden yazildi (kimlik noreply) ve GitHub'a force-push edildi. Eski gecmis yedegi `C:\Kodlar\yedek\ev-lamba-oncesi.bundle` (kisisel bilgi icerir, gereksizse sil). Tum projeler artik `C:\Kodlar` altinda.

## Durum / Bekleyen (2026-10-27)
- **v7.5.0** derlendi + commit'li: Hatırlatma (günde 1-6), Mutfak (yumurta timer'ı), Kocaeli haberleri, I2S ses, gökyüzünde martılar + maskot, rüzgâra göre sallanan ağaç. Cihaza gönderilmedi (cihaz Özcan abide; OTA / GitHub release ile).
- **Doğrulanmadı:** I2S ses (pinler belgeden: SDIN17 LRCLK18 BCLK42; kartta denenmedi). Nem göstergesi HUB35'ten henüz taşınmadı. Mutfak ekranı LVGL'de yok, yalnız web sekmesi var.
- Kaynak (HUB35): `C:\Kodlar\ESP32-3248S035` (reminders.cpp, mutfak.cpp, relay_webpage_multi.h).
