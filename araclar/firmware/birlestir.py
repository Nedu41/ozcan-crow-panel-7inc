"""PlatformIO'nun gercek yukleme komutundan (esptool write_flash ...) tek parcali "factory" imaj uretir.
Ayni bayraklar ve offsetler kullanilir; boylece tarayicidan 0x0'a yazilan imaj, pio upload ile ayni sonucu verir.
Kullanim:  python birlestir.py <proje_dizini> <ortam> <cikis.bin>
"""
import os
import shlex
import subprocess
import sys

PIO_HOME = os.path.join(os.path.expanduser('~'), '.platformio')


def yukleme_komutu(proje, ortam):
    # Sahte porta yuklemeyi dene: komut -v ile yazilir, port acilamayinca hata verir (bize yalniz komut lazim)
    r = subprocess.run(['pio', 'run', '-e', ortam, '-t', 'upload', '--upload-port', 'COM99', '-v'],
                       cwd=proje, capture_output=True, text=True, errors='replace')
    for satir in (r.stdout + r.stderr).splitlines():
        if 'write_flash' in satir and 'esptool' in satir:
            return shlex.split(satir, posix=False)
    raise RuntimeError('esptool write_flash komutu bulunamadi: ' + ortam)


def birlestir(proje, ortam, cikis):
    t = [x.strip('"') for x in yukleme_komutu(proje, ortam)]
    chip = t[t.index('--chip') + 1]
    i = t.index('write_flash') + 1
    bayrak, parcalar = [], []
    while i < len(t):
        if t[i].startswith('--flash_') or t[i] in ('-z',):
            if t[i] == '-z':
                i += 1
                continue
            bayrak += [t[i], t[i + 1]]
            i += 2
        else:
            parcalar += [t[i], os.path.normpath(os.path.join(proje, t[i + 1]))]
            i += 2
    # flas boyutu basligi 4MB'a sabitlenir: 8/16MB'lik kartta da calisir, 4MB'lik kartta 8MB basligi onyuklemeyi bozabilir
    for k in range(0, len(bayrak), 2):
        if bayrak[k] == '--flash_size':
            bayrak[k + 1] = '4MB'
    py = os.path.join(PIO_HOME, 'penv', 'Scripts', 'python.exe')
    esptool = os.path.join(PIO_HOME, 'packages', 'tool-esptoolpy', 'esptool.py')
    subprocess.run([py, esptool, '--chip', chip, 'merge_bin', '-o', cikis] + bayrak + parcalar, check=True)
    print('birlestirildi:', cikis, os.path.getsize(cikis), 'bayt  (bayraklar:', ' '.join(bayrak), ')')


if __name__ == '__main__':
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    birlestir(os.path.abspath(sys.argv[1]), sys.argv[2], os.path.abspath(sys.argv[3]))
