#pragma once
// ESP8266'daki Hash.h sha1(String) karsiligi (ESP32'de yok). Ayri baslikta:
// .ino'da tanimlanirsa Arduino'nun prototip uretimi bozuluyor.
#include <Arduino.h>
#include <mbedtls/sha1.h>

static inline String sha1(const String& s) {
    uint8_t h[20];
    mbedtls_sha1((const unsigned char*)s.c_str(), s.length(), h);
    char hex[41];
    for (int i = 0; i < 20; i++) sprintf(hex + i * 2, "%02x", h[i]);
    return String(hex);
}
