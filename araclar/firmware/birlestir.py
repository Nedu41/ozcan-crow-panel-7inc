"""Derlenmis ESP32 ailesi imajindan tek parcali "factory" imaj uretir (0x0'a yazilir).
Bayraklar/offsetler `pio run -t upload -v` ciktisindan dogrulanmis sabit degerlerdir (cip basina).
Kullanim:  python birlestir.py <proje_dizini> <ortam> <cikis.bin> <cip>     cip: esp32 | esp32c3 | esp32s3
Onkosul: `pio run -e <ortam>` calismis olmali (.pio/build/<ortam>/ icinde bootloader/partitions/firmware).
"""
import os
import subprocess
import sys

PIO_HOME = os.path.join(os.path.expanduser('~'), '.platformio')
# cip: (flas modu, flas frekansi, bootloader ofseti)
BILINEN = {'esp32': ('dio', '40m', 0x1000), 'esp32c3': ('dio', '80m', 0x0), 'esp32s3': ('dio', '80m', 0x0)}


def birlestir(proje, ortam, cikis, cip):
    if cip not in BILINEN:
        raise RuntimeError('bilinmeyen cip: ' + str(cip))
    py = os.path.join(PIO_HOME, 'penv', 'Scripts', 'python.exe')
    if not os.path.exists(py):
        py = sys.executable
    esptool = os.path.join(PIO_HOME, 'packages', 'tool-esptoolpy', 'esptool.py')
    build = os.path.join(proje, '.pio', 'build', ortam)
    boot_app0 = os.path.join(PIO_HOME, 'packages', 'framework-arduinoespressif32', 'tools', 'partitions', 'boot_app0.bin')
    mod, frek, boot = BILINEN[cip]
    parcalar = [hex(boot), os.path.join(build, 'bootloader.bin'), '0x8000', os.path.join(build, 'partitions.bin'),
                '0xe000', boot_app0, '0x10000', os.path.join(build, 'firmware.bin')]
    for p in parcalar[1::2]:
        if not os.path.exists(p):
            raise FileNotFoundError(p + '  (once: pio run -e ' + ortam + ')')
    # flas boyutu basligi 4MB: 8/16MB'lik kartta da calisir, 4MB'lik kartta 8MB basligi onyuklemeyi bozabilir
    bayrak = ['--flash_mode', mod, '--flash_freq', frek, '--flash_size', '4MB']
    subprocess.run([py, esptool, '--chip', cip, 'merge_bin', '-o', cikis] + bayrak + parcalar, check=True, capture_output=True)
    print('birlestirildi:', cikis, os.path.getsize(cikis), 'bayt', ' '.join(bayrak))


if __name__ == '__main__':
    if len(sys.argv) != 5:
        sys.exit(__doc__)
    birlestir(os.path.abspath(sys.argv[1]), sys.argv[2], os.path.abspath(sys.argv[3]), sys.argv[4])
