#pragma once
#include <Arduino.h>

// Turkce UTF-8 -> ASCII (yalniz ASCII fontlu yerler icin; LVGL'de gerekmez)
String trAscii(const String& in);
String decodeEntities(String s);

// AA dunya RSS -> NEWS_TICKER_TEXT (blocking HTTPS, veri gorevinden cagrilir)
bool fetchNews();
String newsStatus();  // son haber cekme sonucu (uzaktan teshis)
