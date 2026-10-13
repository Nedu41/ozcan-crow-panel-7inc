"""PlatformIO extra_script: derlenen imajlara kullanici adi / klasor yollari gomulmesin.
ESP32 derlemelerinde __FILE__ (ESP_LOG, assert) mutlak yol tasir: "<surucu>:/<kullanici-klasoru>/.platformio/...".
Bu betik yollari nötr kokler ile degistirir (-ffile-prefix-map). Yolu koda YAZMAZ, derleme sirasinda hesaplar.
Kullanim: platformio.ini -> extra_scripts = pre:tools/gizlilik.py   (relative yol proje kokune gore)
"""
Import("env")
import os


def varyantlar(p):
    p = os.path.normpath(p)
    return {p, p.replace("\\", "/")}


esleme = []
for kaynak, hedef in (
    (env.subst("$PROJECT_PACKAGES_DIR"), "/pio/packages"),
    (env.subst("$PROJECT_CORE_DIR"), "/pio"),
    (env.subst("$PROJECT_DIR"), "/proje"),
    (os.path.expanduser("~"), "/home/kullanici"),
):
    if kaynak:
        for v in varyantlar(kaynak):
            esleme.append((v, hedef))

# uzun yollar once: paket klasoru, ev dizininden once eslesmeli
esleme.sort(key=lambda x: -len(x[0]))
for k, h in esleme:
    env.Append(CCFLAGS=["-ffile-prefix-map=%s=%s" % (k, h)])
