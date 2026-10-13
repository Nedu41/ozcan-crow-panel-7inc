# disko
def frame(t, W, H):
    y, x = np.mgrid[0:H, 0:W]
    f = np.zeros((H, W, 3), dtype=np.float32)
    for i in range(5):
        a = t * (1.1 + i * 0.2) + i * 1.5
        cx = W / 2 + math.cos(a) * (W / 2.8)
        cy = H / 2 + math.sin(a * 0.7) * (H / 2.8)
        rad = 14 + 6 * math.sin(t * 7 + i)
        dist = (x - cx)**2 + (y - cy)**2
        mask = np.exp(-dist / (2 * rad**2))
        f[:, :, 0] += mask * (128 + 127 * math.sin(t * 2 + i))
        f[:, :, 1] += mask * (128 + 127 * math.sin(t * 2.5 + i * 2))
        f[:, :, 2] += mask * (128 + 127 * math.sin(t * 1.8 + i * 4))
    beat = (math.sin(t * 11) * 0.5 + 0.5)**8
    grid = (x % 13 == 0) | (y % 13 == 0)
    f[grid] = (f[grid] + 120) * (0.4 + 0.6 * beat)
    strobe = 1.2 if (t * 11) % 1.0 > 0.85 else 0.7
    return np.clip(f * strobe, 0, 255).astype(np.uint8)