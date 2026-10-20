#pragma once
#include <Arduino.h>

// Web arayuzu basliginda ve TFT ust seridinde gosterilir - tek kaynak burasi
#define FW_VERSION "7.5.0-crow"  // artirinca web Donanim > Surum gecmisi (HW_CHANGELOG) guncellenir

// ---- Donanim ----
// Pinler: board.h / docs/DONANIM.md

// ---- Ozellikler ----
#define TELEGRAM_ENABLED true

// ---- Zamanlama ----
#define MUTEX_TIMEOUT pdMS_TO_TICKS(100)
const int STATUS_CHECK_INTERVAL = 10000;           // cihaz durum turu
const int HTTP_TIMEOUT_MS = 1000;                  // ESP01 kontrol istegi
const uint16_t STATUS_PUSH_PORT = 4211;             // ESP01 v2.8 UDP durum push (firmware ile ayni)
const unsigned long DEBOUNCE_DELAY_MS = 600000;    // manuel kontrolden sonra otomasyon beklemesi
const unsigned long NEWS_UPDATE_INTERVAL = 900000; // 15 dk
// Anadolu Ajansi dunya haberleri: ~15-22KB, yonlendirme yok, duz <title> (2026-09-27 dogrulandi)
#define NEWS_RSS_URL "https://www.aa.com.tr/tr/rss/default?cat=dunya"
const int NEWS_MAX_ITEMS = 12;
// Bolge haberleri: Google Haberler "Kocaeli" aramasi (bircok kaynak, one cikanlar ustte; yonlendirmesiz 200, 2026-10-20)
#define NEWS_LOCAL_RSS_URL "https://news.google.com/rss/search?q=Kocaeli&hl=tr&gl=TR&ceid=TR:tr"
const int NEWS_LOCAL_ITEMS = 4;

// ---- Uyarilar (hava + deprem, bkz. alerts.cpp) ----
#define ALERT_WIND_KMH 50            // gunluk max ruzgar hamlesi esigi (km/s)
#define ALERT_RAIN_MM 25             // gunluk toplam yagis esigi (mm)
#define ALERT_EQ_NEAR_KM 100         // "yakin" deprem yaricapi (km)
#define ALERT_EQ_NEAR_MAG 3.5f       // yakin: bu buyukluk ve ustu
#define ALERT_EQ_FAR_MAG 5.0f        // Turkiye geneli: bu buyukluk ve ustu
#define ALERT_EQ_WINDOW_H 12         // AFAD'dan son kac saatlik kayit sorgulansin
const unsigned long ALERT_EQ_INTERVAL_MS = 300000UL;  // deprem sorgusu 5 dk
const unsigned long ALERT_TICKER_MS = 3600000UL;      // alt yazida (haber bandi) 60 dk gorunur
#define ALERT_ACK_FILE "/alerts_ack.json"

// ---- MQTT ----
#define MQTT_BROKER "broker.hivemq.com"
#define MQTT_PORT 1883

// ---- Cihaz/role limitleri ----
#define MAX_DEVICES 10
#define MAX_RELAYS 4  // LC Technology 4 kanal modulu icin; role sayisi cihazdan (status "relays") gelir, yoksa 2

// ---- LittleFS dosyalari ----
#define DEVICES_FILE "/devices.json"
#define SCHEDULE_FILE "/devices_schedule.json"
#define WEATHER_CONFIG_FILE "/weather_config.json"
#define NOTES_FILE "/notes.json"

