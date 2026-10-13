# Smile
def frame(t, W, H):
    img = np.zeros((H, W, 3), dtype=np.uint8)
    
    # Animasyonlu merkez noktasi (yukari-asagi ziplama)
    cy = H // 2 + int(12 * math.sin(t * 3))
    cx = W // 2
    
    # Grid olusturma
    yy, xx = np.mgrid[:H, :W]
    
    # Yuz dairesi (Sari)
    dist_face = np.sqrt((xx - cx)**2 + (yy - cy)**2)
    face_mask = dist_face < 23
    img[face_mask] = [255, 220, 0]
    
    # Gozler (Siyah, goz kirpma animasyonu)
    eye_y = cy - 7
    eye_x_offset = 8
    eye_r = 4.5
    
    # Goz kirpma carpani (0.1 ile 1.0 arasi)
    blink = 0.1 + 0.9 * abs(math.sin(t * 2))
    if abs(math.sin(t * 2)) < 0.1: blink = 0.05 # Tam kapanma ani
    
    # Sol ve sag goz mesafesi (Y ekseni goz kirpma icin olcekli)
    dist_eye_l = np.sqrt((xx - (cx - eye_x_offset))**2 + ((yy - eye_y) / blink)**2)
    dist_eye_r = np.sqrt((xx - (cx + eye_x_offset))**2 + ((yy - eye_y) / blink)**2)
    
    eye_mask = (dist_eye_l < eye_r) | (dist_eye_r < eye_r)
    img[eye_mask] = [0, 0, 0]
    
    # Agiz (Gulen yuz - yay seklinde)
    # Merkezden asagida bir cemberin parcasini aliyoruz
    smile_cy = cy + 2
    dist_smile = np.sqrt((xx - cx)**2 + (yy - smile_cy)**2)
    
    # Agiz kalinligi ve acisi
    smile_mask = (dist_smile > 10) & (dist_smile < 14) & (yy > smile_cy + 4)
    
    # Kenarlari yumusatmak icin acisal kisitlama (istege bagli ama netlik icin onemli)
    # dx = xx - cx, dy = yy - smile_cy; dy/dx egimi kontrol edilebilir
    
    img[smile_mask] = [0, 0, 0]
    
    # Yanaklar (Pembe - hafif parlama)
    blush_y = cy + 4
    blush_x_offset = 14
    dist_blush_l = np.sqrt((xx - (cx - blush_x_offset))**2 + (yy - blush_y)**2)
    dist_blush_r = np.sqrt((xx - (cx + blush_x_offset))**2 + (yy - blush_y)**2)
    
    blush_intensity = 0.5 + 0.5 * math.sin(t * 3)
    blush_mask = (dist_blush_l < 4) | (dist_blush_r < 4)
    img[blush_mask & face_mask] = [255, 100, 100]

    return img