# disko
def frame(t, W, H):
    y, x = np.ogrid[:H, :W]
    
    # Ritim ve Stroboskop
    beat = np.abs(np.sin(t * 8))
    strobe = (np.sin(t * 24) > 0.8).astype(float) * 50
    
    # Disko Zemini (4x8 kareler)
    tile_w, tile_h = 13, 13
    tx, ty = x // tile_w, y // tile_h
    
    # Karelerin renkleri (zamana gore degisken seed)
    c_phase = np.floor(t * 4)
    r_tile = (np.sin(tx * 0.5 + ty * 0.8 + c_phase) * 127 + 128) * beat
    g_tile = (np.sin(tx * 1.2 - ty * 0.4 + c_phase + 2) * 127 + 128) * beat
    b_tile = (np.sin(-tx * 0.7 + ty * 1.1 + c_phase + 4) * 127 + 128) * beat
    
    # Donen Isik Huzmeleri (Ust merkezden)
    cx, cy = W // 2, 5
    dx, dy = x - cx, y - cy
    dist = np.sqrt(dx**2 + dy**2) + 0.1
    angle = np.arctan2(dy, dx)
    
    ray_count = 7
    ray_val = np.sin(angle * ray_count + t * 10) * 0.5 + 0.5
    ray_val = np.power(ray_val, 12) * (300 / dist) # Mesafe ile azalan keskin huzme
    
    # Renkleri birlestir
    r = np.clip(r_tile * 0.4 + ray_val * 200 + strobe, 0, 255)
    g = np.clip(g_tile * 0.4 + ray_val * 255 + strobe, 0, 255)
    b = np.clip(b_tile * 0.4 + ray_val * 100 + strobe, 0, 255)
    
    # Isik patlamalari (parilti)
    glitter = (np.sin(x * 0.5 + t * 20) * np.cos(y * 0.5 - t * 15) > 0.98).astype(float) * 200
    r = np.clip(r + glitter, 0, 255)
    g = np.clip(g + glitter, 0, 255)
    b = np.clip(b + glitter, 0, 255)
    
    return np.stack([r, g, b], axis=-1).astype(np.uint8)