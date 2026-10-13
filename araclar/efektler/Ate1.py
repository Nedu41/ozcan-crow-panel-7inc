# Ateş
def frame(t, W, H):
    y, x = np.mgrid[0:H, 0:W]
    
    # Ates hareketi: Yukari dogru kayan sin dalgalari
    v = np.sin(x * 0.15 + np.cos(t * 0.8) * 2.0)
    v += np.sin(x * 0.1 - y * 0.15 - t * 25.0)
    v += np.sin(x * 0.05 + y * 0.05 - t * 10.0)
    v = (v + 3.0) / 6.0 # 0-1 arasi normalize
    
    # Sekillendirme: Alt taraf daha parlak, yanlar daha sonuk
    # y=0 ust, y=H-1 alt.
    y_norm = (y / (H - 1))
    dist_center = np.abs(x - W / 2) / (W / 2)
    shape = (y_norm ** 1.5) * (1.1 - dist_center * 0.8)
    
    intensity = v * shape * 2.5
    intensity = np.clip(intensity, 0, 1)
    
    # Renk paleti: Siyah -> Kirmizi -> Turuncu -> Sari -> Beyaz
    img = np.zeros((H, W, 3), dtype=np.uint8)
    
    img[..., 0] = np.clip(intensity * 1000, 0, 255) # Kirmizi kanali hizli dolsun
    img[..., 1] = np.clip(intensity * 1000 - 200, 0, 255) # Yesil (Sari icin)
    img[..., 2] = np.clip(intensity * 1000 - 500, 0, 255) # Mavi (Beyaz merkez icin)
    
    # Alt kisma sabit kor efekti
    ember = np.clip((y_norm ** 8) * 200, 0, 255)
    img[..., 0] = np.maximum(img[..., 0], ember.astype(np.uint8))
    
    return img