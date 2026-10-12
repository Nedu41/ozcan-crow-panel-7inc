# ozcan-crow-panel-7inc — devir notu

Elecrow CrowPanel 7" (ESP32-S3 dokunmatik ekran) ile **LedPanel-Pico kumandası**. Kullanıcı Türkçe konuşur, kısa cevap ister, kanıtsız deneme önerme. Hedef LED panel projesi: `D:\Kodlar\LedPanel-Pico` (ona dokunma; yalnızca API'sini kullan).

## Çalışma kuralları
- Önce kanıt/ölçüm, sonra kod. Pinler/ekran sürücüsü **doğrulanmadan** yazılmaz (global CLAUDE.md pin kontrol listesi: strapping, touch, ADC2, JTAG/USB, flash/PSRAM).
- Doğrulanmamış bir şeyi kesinmiş gibi sunma. `docs/DONANIM.md`'de yalnızca ölçülen/belgeden teyit edilen bilgi durur.
- Arayüz: koyu/gotik tema, siyah çerçeve, pastel renk; açık üstüne açık renk yok. Bkz. global CLAUDE.md tercihleri.
- Commit sonuna: `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. `src/secrets.h` commit'e girmez.

## LedPanel API (hedef cihaz, HTTP, kimlik doğrulamasız, aynı WiFi)
Panel IP'si DHCP ile değişir (son görülen 192.168.1.140; mDNS `ledpanel.local`). Üzerinden kumanda:
- `GET /api/state` — durum JSON (pat, rot, bp, fxs, fps, fxh, aibr …)
- `GET /api/set?rot=&mirx=&miry=&bp=(4..30)&pat=` — yön, ışık (tarama tekrarı), ekran
- `GET /api/fx?id=<31..48>&hiz=<10..300 ms>` — yerleşik efekt + hız (küçük = hızlı)
- `GET /api/fxhide?id=N | ?restore=1` — efekt listesinden kaldır/geri getir
- `GET /api/ddp?on=1|off=<sn>` — dış yayın aç/engelle
- `GET /api/walk?mode=0|1|2&col&grp&chip&step` — donanım test (yürüyüş / blok numaraları)
- Yapay zeka efekti: PC'de `araclar/ai_koprusu.py` (:8766, anahtar `?k=`); adresi panele kendisi bildirir → `/api/state` içindeki `aibr`. Uçlar: `/uret?prompt=`, `/oynat?ad=`, `/liste`, `/kaydet?ad=`, `/sil?ad=`, `/ayar?hiz=&isik=&fx=&fy=`, `/durdur` (hepsi `&ip=<panel>&k=<anahtar>`).
- Yerleşik efekt no: 31 Alev, 35 Plazma, 36 Gökkuşağı, 34 Matrix, 37 Yıldızlı gece, 41 Kar, 42 Yağmur, 43 Fırtına, 44 Doğa manzarası, 45 Okyanus, 46 Havai fişek, 47 Radar, 48 Kalp atışı.

## Durum (2026-10-12)
İskelet oluşturuldu; henüz kod yok. Sıradaki: donanımı teyit et (tam model/revizyon), `docs/DONANIM.md` doldur, ekran+dokunmatik "merhaba" testi, sonra WiFi + LedPanel istemcisi + arayüz.
