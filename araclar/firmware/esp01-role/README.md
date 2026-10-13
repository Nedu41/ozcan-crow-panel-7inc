# Role dugumu firmware'i (ESP-01 / ESP-07 / ESP32-C3)

HUB'a (crowhub.local) eklenen lamba/priz dugumleri bu firmware'i calistirir. Kaynak ev-lamba projesinden alinmistir.

## Yeni cihaz kurulumu
1. Firmware'i yukle (asagida). Kod icinde WiFi sifresi yoktur.
2. Cihaz acilinca **ESP01_SETUP** (C3 icin **C3_SETUP**) adli WiFi agini acar. Telefondan baglan, acilan sayfada evin WiFi'sini sec, sifreyi gir.
3. Cihaz ag'a baglanir ve `esp01-xxxxxx.local` adiyla duyurulur (xxxxxx = MAC'in son 6 hanesi).
4. HUB'da (ekran: Lambalar bos ise web arayuzu > Cihazlar) **Cihaz Tara** ile bulunur, isim verilip eklenir.

## Yukleme (PlatformIO)
```
cd araclar/firmware/esp01-role
pio run -e esp01_1m -t upload --upload-port COMx
```
ESP-01'i yuklemek icin USB-seri programlayici gerekir (flas modu: GPIO0 -> GND, sonra reset). Sonraki guncellemeler kablosuz (OTA, ArduinoOTA) yapilabilir.

## Hazir imaj (PlatformIO'suz)
`esp01_1m.bin` varsa: `python -m esptool --port COMx --baud 115200 write_flash 0x0 esp01_1m.bin`

## Pinler
ESP-01: role1=GPIO0, role2=GPIO2. ESP-07 (`esp07_gpio45`): GPIO4/GPIO5. C3: GPIO3/GPIO4.
