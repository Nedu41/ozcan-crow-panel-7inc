"""CrowPanel HUB icin yapay zeka efekt koprusu (bilgisayarda calisir, HTTP :8767).
Cihazin Efekt sekmesi buraya baglanir: efekt cumlesini yapay zekaya urettirir (Gemini), calistirir ve
kareleri UDP 4048 ile cihaza yollar (RGB565). Cihaz tam ekranda gosterir.
  python araclar/efekt_koprusu.py
Gerekli: pip install numpy pillow.  Gemini anahtari cihazdan (Ayar > Yapay zeka anahtari) gelir ya da
araclar/.ai_key dosyasi / GEMINI_API_KEY ortam degiskeni. Yetki: her istek ?k=<anahtar> ister (araclar/.kopru_anahtar).
Uyari: model kodu bu bilgisayarda calistirilir (yalniz np/math, tam bir kum havuzu degil)."""
import http.server, os, re, sys, json, math, time, socket, struct, secrets, threading, traceback, urllib.parse, urllib.request, urllib.error
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
SAVE = os.path.join(HERE, 'efektler')
KF = os.path.join(HERE, '.kopru_anahtar')
AIKF = os.path.join(HERE, '.ai_key')
PORT, UDP_PORT, FPS = 8767, 4048, 15
MODELS = ('gemini-3.5-flash', 'gemini-3.6-flash', 'gemini-3-flash-preview', 'gemini-3.1-flash-lite', 'gemini-3.5-flash-lite', 'gemini-flash-lite-latest')
if os.path.exists(KF):
    KEY = open(KF).read().strip()
else:
    KEY = secrets.token_urlsafe(9)
    open(KF, 'w').write(KEY)

SYSTEM = """Sen bir ekran efekt yazarisin. Ekran %dx%d piksel (genislik x yukseklik), tam renk RGB. Kullanicinin istedigi efekt icin SADECE Python kodu yaz (aciklama yok, markdown yok).
Kural:
- Tam olarak su fonksiyonu tanimla:  def frame(t, W, H):  ve (H, W, 3) sekilli, dtype uint8 bir numpy dizisi (RGB) dondur. t = saniye (float).
- Yalnizca 'np' (numpy) ve 'math' kullanabilirsin; import yazma, dosya/ag erisimi yok. Her kare yaklasik 15 kez/sn cagrilir: dongu yerine numpy vektor islemi kullan (np.mgrid/np.meshgrid), kare basina ~20 ms altinda kal.
- Kalici durum gerekiyorsa fonksiyon disinda modul seviyesinde degisken/dict tanimla (ornegin yagmur damlalari), her cagrida guncelle.
- Koyu zemin, doygun/net renkler, 1-2 pikselden kucuk detay kullanma. Ince gri tonlardan kacin.
- Uzun sureli, akici animasyon olsun (t ile surekli degissin)."""


def api_key():
    k = os.environ.get('GEMINI_API_KEY', '')
    if not k and os.path.exists(AIKF):
        k = open(AIKF).read().strip()
    return k


def post(req):
    try:
        return json.loads(urllib.request.urlopen(req, timeout=60).read())
    except urllib.error.HTTPError as e:
        raise RuntimeError('HTTP %d' % e.code)
    except Exception as e:
        raise RuntimeError(type(e).__name__)


GOOD = [None]


def ask(prompt, W, H, feedback=None):
    if not api_key():
        raise RuntimeError('anahtar yok')
    msgs = prompt if not feedback else prompt + '\n\nOnceki kodun hatasi, duzelt:\n' + feedback
    body = {'systemInstruction': {'parts': [{'text': SYSTEM % (W, H)}]}, 'contents': [{'role': 'user', 'parts': [{'text': msgs}]}]}
    for mdl in ([GOOD[0]] if GOOD[0] else []) + [m for m in MODELS if m != GOOD[0]]:
        url = 'https://generativelanguage.googleapis.com/v1beta/models/%s:generateContent?key=%s' % (mdl, api_key())
        req = urllib.request.Request(url, json.dumps(body).encode(), {'Content-Type': 'application/json'})
        try:
            r = post(req); GOOD[0] = mdl; break
        except RuntimeError as e:
            print('%s kullanilamadi (%s), siradaki model...' % (mdl, e), flush=True)
    else:
        raise RuntimeError('tum modellerin kotasi dolu veya mesgul')
    txt = r['candidates'][0]['content']['parts'][0]['text']
    m = re.search(r'```(?:python)?\s*(.*?)```', txt, re.S)
    return (m.group(1) if m else txt).strip()


SAFE = ('range', 'len', 'min', 'max', 'abs', 'int', 'float', 'sum', 'round', 'enumerate', 'zip', 'list', 'dict', 'tuple', 'bool', 'pow', 'divmod', 'sorted', 'reversed', 'map', 'any', 'all', 'isinstance', 'set')


def compile_effect(code, W, H):
    """Kodu derler, iki deneme karesiyle dogrular; (fonksiyon, hata)."""
    code = re.sub(r'^\s*(import (numpy|math)( as \w+)?|from (numpy|math) import .*)\s*$', '', code, flags=re.M)
    bad = re.search(r'^\s*(import|from)\s.*$', code, re.M) or re.search(r'__\w*', code) or re.search(r'\b(open|exec|eval|compile|globals|locals|getattr|setattr|input)\s*\(', code)
    if bad:
        return None, 'yasak ifade: %r (yalniz np ve math)' % bad.group(0).strip()[:60]
    import builtins
    ns = {'np': np, 'math': math, '__builtins__': {k: getattr(builtins, k) for k in SAFE}}
    try:
        exec(code, ns)
        fn = ns['frame']
        t0 = time.time()
        for tt in (0.0, 1.7):
            out = np.asarray(fn(tt, W, H))
            if out.shape != (H, W, 3):
                return None, 'frame() sekli %s, beklenen %s' % (out.shape, (H, W, 3))
        if (time.time() - t0) / 2 > 0.12:
            return None, 'frame() cok yavas; numpy vektor islemi kullan'
        return fn, None
    except Exception:
        return None, traceback.format_exc(limit=3)


def loading_frame(W, H, el, lines=('LUTFEN', 'BEKLEYIN')):
    from PIL import Image, ImageDraw, ImageFont
    im = Image.new('RGB', (W, H), (0, 0, 0)); d = ImageDraw.Draw(im)
    f = ImageFont.load_default(size=max(8, H // 7))
    cy = H // 2
    for i, t in enumerate(lines):
        w = d.textlength(t, font=f); d.text(((W - w) / 2, cy - H // 4 + i * (H // 7 + 4)), t, font=f, fill=(0, 255, 255))
    bw = W - 16; by = cy + H // 4
    d.rectangle([8, by, 8 + bw - 1, by + 7], outline=(255, 255, 255))
    fw = int((bw - 4) * min(0.95, el / 35.0))
    if fw > 0: d.rectangle([10, by + 2, 10 + fw - 1, by + 5], fill=(255, 128, 0))
    return np.asarray(im)


def rgb565(im):
    im = im.astype(np.uint16)
    return (((im[:, :, 0] & 0xF8) << 8) | ((im[:, :, 1] & 0xFC) << 3) | (im[:, :, 2] >> 3)).astype('<u2').tobytes()


class Player:
    def __init__(self):
        self.ip, self.W, self.H = None, 160, 96
        self.fn = self.code = None
        self.loading = None            # uretim baslangic zamani
        self.msg = None
        self.durum, self.ad, self.prompt = 'bos', '', ''
        self.ctl = {'hiz': 1.0, 'isik': 1.0}
        self.te, self.last, self.sizechk = 0.0, time.time(), 0.0
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.lock = threading.Lock()
        threading.Thread(target=self.run, daemon=True).start()

    def set(self, fn, code, ad=''):
        with self.lock:
            self.fn, self.code, self.ad = fn, code, ad
            self.te, self.last = 0.0, time.time()
            self.durum = 'oynuyor' if fn else 'bos'

    def stop(self):
        self.set(None, None); self.loading = None; self.msg = None

    def send(self, buf):
        n, off = len(buf), 0
        while off < n:
            ch = buf[off:off + 1440]
            push = 1 if off + 1440 >= n else 0
            self.sock.sendto(struct.pack('>BBBBIH', 0x41 if push else 0x40, 0, 1, 1, off, len(ch)) + ch, (self.ip, UDP_PORT))
            off += 1440
            t = time.perf_counter() + 0.002
            while time.perf_counter() < t: pass                          # Windows sleep(0.001) ~15 ms; bekleme dongusu

    def device_size(self):
        try:
            d = json.loads(urllib.request.urlopen('http://%s/api/efekt' % self.ip, timeout=2).read())
            return int(d['w']), int(d['h'])
        except Exception:
            return None

    def run(self):
        err = 0
        while True:
            if not self.ip:
                time.sleep(0.3); continue
            if time.time() - self.sizechk > 4 and (self.fn or self.loading or self.msg):      # ekran yonu degisti mi?
                self.sizechk = time.time()
                sz = self.device_size()
                if sz and sz != (self.W, self.H):
                    self.W, self.H = sz
                    if self.code:
                        fn2, e2 = compile_effect(self.code, self.W, self.H)
                        if fn2: self.set(fn2, self.code, self.ad)
            with self.lock:
                fn = self.fn
            try:
                if not fn:
                    if self.msg or self.loading:
                        el = time.time() - self.loading if self.loading else 0
                        self.send(rgb565(loading_frame(self.W, self.H, el, self.msg or ('LUTFEN', 'BEKLEYIN'))))
                        time.sleep(0.12)
                    else:
                        time.sleep(0.1)
                    continue
                st = time.time()
                c = self.ctl
                self.te += (st - self.last) * c['hiz']; self.last = st
                im = np.asarray(fn(self.te, self.W, self.H)).astype(np.float32) * c['isik']
                self.send(rgb565(np.clip(im, 0, 255).astype(np.uint8))); err = 0
                rest = 1.0 / FPS - (time.time() - st)           # Windows sleep ~15 ms adimli: kalan sureyi bekleme dongusuyle tut
                if rest > 0.02: time.sleep(rest - 0.016)
                while time.time() - st < 1.0 / FPS: pass
            except Exception:
                err += 1
                if err == 1: print('calisma hatasi:', traceback.format_exc(limit=2), flush=True)
                if err > 20: self.stop()
                time.sleep(0.1)


P = Player()


def names():
    try:
        return sorted(f[:-3] for f in os.listdir(SAVE) if f.endswith('.py') and not f.startswith('_'))
    except Exception:
        return []


def generate(prompt, ip):
    """Arka planda: yapay zekadan efekt kodu al, dogrula, oynat."""
    P.ip = ip
    P.stop(); P.durum, P.loading, P.prompt = 'uretiyor', time.time(), prompt
    fb, why = None, ('HATA', 'TEKRAR', 'DENE')
    for _ in range(3):
        print('uretiliyor: %s' % prompt, flush=True)
        try:
            code = ask(prompt, P.W, P.H, fb)
        except RuntimeError as e:
            print('API hatasi:', e, flush=True)
            why = ('ANAHTAR', 'YOK') if str(e) == 'anahtar yok' else ('KOTA', 'DOLDU')
            break
        fn, err = compile_effect(code, P.W, P.H)
        if fn:
            try:
                os.makedirs(SAVE, exist_ok=True)
                open(os.path.join(SAVE, '_son.py'), 'w', encoding='utf-8').write('# ' + prompt + '\n' + code)
            except Exception:
                pass
            P.loading = None; P.set(fn, code); return
        print('kod olmadi, tekrar:', err.strip().splitlines()[-1], flush=True)
        fb = err
    P.loading = None; P.durum = 'hata'; P.msg = why; time.sleep(6); P.msg = None


def lan_ip():
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); s.connect(('192.168.1.1', 80)); r = s.getsockname()[0]; s.close(); return r
    except Exception:
        return socket.gethostbyname(socket.gethostname())


class H(http.server.BaseHTTPRequestHandler):
    def _ok(self, obj, code=200):
        b = json.dumps(obj).encode()
        self.send_response(code)
        for k, v in (('Content-Type', 'application/json'), ('Content-Length', str(len(b)))):
            self.send_header(k, v)
        self.end_headers()
        self.wfile.write(b)

    def do_GET(self):
        u = urllib.parse.urlparse(self.path)
        q = {k: v[0] for k, v in urllib.parse.parse_qs(u.query).items()}
        if q.get('k') != KEY:
            return self._ok({'ok': 0, 'hata': 'anahtar'}, 403)
        ip = q.get('ip') or self.client_address[0]
        if P.ip != ip:
            P.ip = ip; sz = P.device_size()
            if sz: P.W, P.H = sz
        p = u.path
        if p == '/uret' and q.get('prompt'):
            threading.Thread(target=generate, args=(q['prompt'], ip), daemon=True).start()
        elif p == '/oynat' and q.get('ad'):
            f = os.path.join(SAVE, re.sub(r'\W+', '_', q['ad']) + '.py')
            if os.path.exists(f):
                code = open(f, encoding='utf-8').read(); fn, e = compile_effect(code, P.W, P.H)
                if fn: P.loading = None; P.set(fn, code, q['ad'])
                else: print('kayitli efekt hatasi:', e, flush=True)
        elif p == '/kaydet' and q.get('ad'):
            if P.code:
                os.makedirs(SAVE, exist_ok=True)
                open(os.path.join(SAVE, re.sub(r'\W+', '_', q['ad']) + '.py'), 'w', encoding='utf-8').write(P.code)
        elif p == '/sil' and q.get('ad'):
            try: os.remove(os.path.join(SAVE, re.sub(r'\W+', '_', q['ad']) + '.py'))
            except OSError: pass
        elif p == '/ayar':
            try:
                if 'hiz' in q: P.ctl['hiz'] = min(4.0, max(0.1, float(q['hiz'])))
                if 'isik' in q: P.ctl['isik'] = min(2.0, max(0.05, float(q['isik'])))
            except ValueError:
                pass
        elif p == '/anahtar' and q.get('g'):
            open(AIKF, 'w').write(q['g'].strip())
        elif p == '/durdur':
            P.stop()
        elif p != '/durum':
            return self._ok({'ok': 0, 'hata': 'bilinmeyen'}, 404)
        self._ok({'ok': 1, 'liste': names(), 'durum': P.durum, 'ad': P.ad})

    def log_message(self, *a):
        pass


def announce():
    """Kopru adresini cihaza bildirir (10 sn'de bir): crowhub.local, olmazsa agdaki /api/efekt yanit verenler."""
    import concurrent.futures as cf
    cache = [None]
    def chk(ip):
        try:
            return ip if b'"w"' in urllib.request.urlopen('http://%s/api/efekt' % ip, timeout=1.5).read() else None
        except Exception:
            return None
    while True:
        try:
            ip = cache[0] if cache[0] and chk(cache[0]) else None
            if not ip:
                try: ip = chk(socket.gethostbyname('crowhub.local'))
                except OSError: pass
            if not ip:
                base = lan_ip().rsplit('.', 1)[0]
                with cf.ThreadPoolExecutor(64) as ex:
                    ip = next((r for r in ex.map(chk, ['%s.%d' % (base, i) for i in range(1, 255)]) if r), None)
            cache[0] = ip
            if ip:
                u = 'http://%s:%d/?k=%s' % (lan_ip(), PORT, KEY)
                urllib.request.urlopen('http://%s/api/aibr?u=%s' % (ip, urllib.parse.quote(u, safe='')), timeout=4).read()
        except Exception:
            pass
        time.sleep(10)


if __name__ == '__main__':
    print('Efekt koprusu hazir: http://%s:%d/?k=%s' % (lan_ip(), PORT, KEY))
    print('Cihaz (crowhub.local) otomatik bulunur; Efekt sekmesinde "Koprü bagli" yazmali.', flush=True)
    threading.Thread(target=announce, daemon=True).start()
    try:
        http.server.ThreadingHTTPServer.allow_reuse_address = False      # Windows'ta ayni portu ikinci kopru da alabiliyor: acik hata ver
        http.server.ThreadingHTTPServer(('0.0.0.0', PORT), H).serve_forever()
    except KeyboardInterrupt:
        pass
