"""Tum kartlari derler, hazir imajlari bin/ altina koyar.
  bin/<kart>.bin          uygulama imaji (ArduinoOTA / web guncelleme / ESP8266 icin 0x0)
  bin/<kart>.factory.bin  yalniz ESP32 ailesi: bootloader + bolum tablosu + uygulama (0x0'dan yazilir; web yukleyici bunu kullanir)
Kullanim:  python derle_hepsi.py [kart ...]   (bos = hepsi)
"""
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
# kart: (cip, bootloader ofseti)  cip None = ESP8266 (birlestirme yok)
KARTLAR = {
    'esp01_1m': None, 'esp07_gpio45': None, 'nodemcu': None, 'd1_mini': None, 'sonoff_basic': None,
    'c3_supermini': ('esp32c3', 0x0),
    'esp32_devkit': ('esp32', 0x1000),
    'esp32_s3': ('esp32s3', 0x0),
}


def birlestir(kart, cip, boot_ofs, build):
    # PlatformIO'nun gercek yukleme komutundaki bayrak ve offsetlerle (bkz. ../birlestir.py)
    sys.path.insert(0, os.path.dirname(HERE))
    from birlestir import birlestir as _b
    _b(HERE, kart, os.path.join(HERE, 'bin', kart + '.factory.bin'), cip)


def main():
    hedef = sys.argv[1:] or list(KARTLAR)
    os.makedirs(os.path.join(HERE, 'bin'), exist_ok=True)
    for kart in hedef:
        print('=== derleniyor:', kart, flush=True)
        r = subprocess.run(['pio', 'run', '-e', kart], cwd=HERE)
        if r.returncode != 0:
            print('!!! BASARISIZ:', kart, flush=True)
            continue
        build = os.path.join(HERE, '.pio', 'build', kart)
        shutil.copy(os.path.join(build, 'firmware.bin'), os.path.join(HERE, 'bin', kart + '.bin'))
        if KARTLAR[kart]:
            birlestir(kart, KARTLAR[kart][0], KARTLAR[kart][1], build)
        print('--- tamam:', kart, flush=True)


if __name__ == '__main__':
    main()
