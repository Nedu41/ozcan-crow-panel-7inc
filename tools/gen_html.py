# Derleme oncesi: include/relay_webpage_multi.h icindeki HTML'i gzip'ler -> include/html_gz.h (flash tasarrufu ~75 KB).
Import("env")  # noqa: F821 (PlatformIO)
import gzip, os, re

proj = env.subst("$PROJECT_DIR")  # noqa: F821
src = os.path.join(proj, "include", "relay_webpage_multi.h")
dst = os.path.join(proj, "include", "html_gz.h")
txt = open(src, encoding="utf-8").read()
m = re.search(r'R"rawliteral\((.*)\)rawliteral"', txt, re.S)
data = gzip.compress(m.group(1).encode("utf-8"), 9, mtime=0)
out = "// OTOMATIK URETILDI (tools/gen_html.py) - elle duzenleme\n#pragma once\n#include <Arduino.h>\nconst size_t HTML_GZ_LEN = %d;\nconst uint8_t HTML_GZ[] PROGMEM = {\n" % len(data)
out += ",".join("0x%02x" % b for b in data) + "\n};\n"
if not os.path.exists(dst) or open(dst).read() != out:
    open(dst, "w").write(out)
print("html_gz.h: %d bayt gzip" % len(data))
