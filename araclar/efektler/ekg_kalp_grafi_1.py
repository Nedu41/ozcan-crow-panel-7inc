# canlı ekg kalp grafi hasta ekranı
def frame(t, W, H):
    y, x = np.mgrid[0:H, 0:W]
    xn = x / W
    p = 0.12 * np.exp(-((xn - 0.2)**2) / 0.0015)
    q = -0.15 * np.exp(-((xn - 0.32)**2) / 0.0003)
    r = 0.9 * np.exp(-((xn - 0.35)**2) / 0.0002)
    s = -0.25 * np.exp(-((xn - 0.38)**2) / 0.0005)
    tw = 0.2 * np.exp(-((xn - 0.6)**2) / 0.006)
    sig = p + q + r + s + tw
    head = (t * 40) % W
    age = (head - x) % W
    fade = np.exp(-age / 18.0) * (age < (W - 1))
    y_l = (H * 0.65) - sig * (H * 0.2)
    line = np.exp(-np.abs(y - y_l) / 1.1) * fade
    hr = (math.sin(t * 8) + 1) * 0.5
    heart = (np.sqrt((y - H*0.2)**2 + (x - W*0.35)**2) < (5 + hr * 2))
    grid = ((x % 10 == 0) | (y % 10 == 0)).astype(float) * 0.06
    res = np.zeros((H, W, 3), dtype=np.float32)
    res[:, :, 1] = grid * 80 + line * 255
    res[:, :, 0] = grid * 15 + line * 150
    res[:, :, 2] = grid * 15 + line * 150
    res[heart, 0] = 255
    res[heart, 1] = 10
    res[heart, 2] = 10
    return np.clip(res, 0, 255).astype(np.uint8)