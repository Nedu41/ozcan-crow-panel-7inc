"""Herkese acik yayin deposunu (temiz gecmis, kisisel bilgi YOK) hazirlar.
  python tools/yayin_hazirla.py [hedef_dizin]      (varsayilan: ..\\crowpanel-hub-public)
Ne yapar:
  1. Hedefte .git varsa korur (yayin gecmisi surer), icerigi sifirdan yazar.
  2. git ls-files ile TAKIPTEKI dosyalari kopyalar; CLAUDE.md ve .claude/ gibi ozel dosyalari ALMAZ.
  3. Metinlerde bilinen kisisel ifadeleri nötrlestirir.
  4. Metin ve ikili dosyalarda kisisel bilgi taramasi yapar; bulursa DURUR (commit/push yok).
  5. Degisiklik varsa noreply kimligiyle commit eder (push elle: git push).
Gizli kalmasi gerekenler (secrets.h, anahtarlar) zaten git disidir; taramada da aranir.
"""
import os
import re
import shutil
import subprocess
import sys

KOK = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HEDEF = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.path.join(os.path.dirname(KOK), 'crowpanel-hub-public')

DISLA = ('CLAUDE.md', '.claude/', 'tools/yayin_hazirla.py')  # ozel devir notlari ve bu betik (aranan kaliplari icerir) yayina girmez
IKILI = ('.bin', '.jpeg', '.jpg', '.png', '.webp', '.c')  # .c: font tablolari (metin ama buyuk, taranir)
DEGISTIR = [  # (regex, yerine) — yalnizca metin dosyalari
    (r"Özcan abinin", "teslim alanın"), (r"Özcan abiye", "teslim alana"), (r"Özcan abi", "teslim alan kişi"),
    (r"\(kaynak: [A-Za-z]:\\[^)]*\)", "(kaynak: ev-lamba projesi)"),
]
# Taramada aranan kisisel kaliplar (ikili dosyalarda da)
KALIP = [rb'Emiliya', rb'EncanA01', rb'durmus', rb'nejat', rb'Nejat', rb'C:/Users', rb'C:\\Users', rb'D:/Kodlar', rb'D:\\Kodlar', rb'C:/Kodlar', rb'C:\\Kodlar',
         rb'[0-9]{8,10}:[A-Za-z0-9_-]{30,}', rb'AIza[0-9A-Za-z_-]{25,}', rb'6032215718', rb'Bahce_kapi', rb'Kalburcu']


def calistir(*a, cwd=None):
    return subprocess.run(a, cwd=cwd, check=True, capture_output=True, text=True, errors='replace').stdout


def main():
    os.makedirs(HEDEF, exist_ok=True)
    for ad in os.listdir(HEDEF):
        if ad == '.git':
            continue
        p = os.path.join(HEDEF, ad)
        shutil.rmtree(p) if os.path.isdir(p) else os.remove(p)
    dosyalar = calistir('git', 'ls-files', cwd=KOK).splitlines()
    for f in dosyalar:
        if any(f == d or f.startswith(d) for d in DISLA):
            continue
        k, h = os.path.join(KOK, f), os.path.join(HEDEF, f)
        os.makedirs(os.path.dirname(h), exist_ok=True)
        if f.lower().endswith(IKILI):
            shutil.copy(k, h)
            continue
        s = open(k, 'rb').read().decode('utf-8', errors='replace')
        for rx, yer in DEGISTIR:
            s = re.sub(rx, yer, s)
        open(h, 'wb').write(s.encode('utf-8'))
    # tarama
    bulgu = []
    for dp, dn, fn in os.walk(HEDEF):
        dn[:] = [d for d in dn if d != '.git']
        for f in fn:
            p = os.path.join(dp, f)
            d = open(p, 'rb').read()
            for k in KALIP:
                if re.search(k, d):
                    bulgu.append((os.path.relpath(p, HEDEF), k.decode()))
    if bulgu:
        print('!!! KISISEL BILGI BULUNDU, yayin durduruldu:')
        for f, k in bulgu:
            print('  ', f, '<-', k)
        sys.exit(1)
    print('tarama temiz:', len(dosyalar), 'dosya')
    if not os.path.isdir(os.path.join(HEDEF, '.git')):
        calistir('git', 'init', '-q', '-b', 'main', cwd=HEDEF)
    kim = ['-c', 'user.name=Nedu41', '-c', 'user.email=Nedu41@users.noreply.github.com', '-c', 'core.autocrlf=false']
    calistir('git', *kim, 'add', '-A', cwd=HEDEF)
    if calistir('git', 'status', '--porcelain', cwd=HEDEF).strip():
        surum = re.search(r'FW_VERSION "([^"]+)"', open(os.path.join(KOK, 'include', 'config.h'), encoding='utf-8').read()).group(1)
        calistir('git', *kim, 'commit', '-q', '-m', 'Surum ' + surum, cwd=HEDEF)
        print('commit yapildi: Surum', surum)
    else:
        print('degisiklik yok')
    print('hedef:', HEDEF)


if __name__ == '__main__':
    main()
