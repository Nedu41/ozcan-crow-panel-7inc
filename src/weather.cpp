#include "weather.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>
#include "app_state.h"
#include "storage.h"
#include "telegram.h"  // urlEncode
#include "alerts.h"

// =================== Nominatim geocode ===================
bool geocodeAddress(const String& address, String& outLat, String& outLon, String& displayName) {
  if (address.length() < 2) return false;
  HTTPClient http;
  String q = urlEncode(address);
  String url = "https://nominatim.openstreetmap.org/search?format=json&limit=1&q=" + q;
  http.begin(url);
  http.setTimeout(3000);
  http.setConnectTimeout(2000);
  http.addHeader("User-Agent", "MerkeziHUB/1.0");
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    return false;
  }
  String payload = http.getString();
  http.end();
  DynamicJsonDocument j(4096);
  DeserializationError err = deserializeJson(j, payload);
  if (err) {
    Serial.printf("geocodeAddress: JSON parse error: %s\n", err.c_str());
    return false;
  }
  if (!j.is<JsonArray>() || j.size() == 0) return false;
  JsonObject first = j[0];
  if (first.containsKey("lat") && first.containsKey("lon")) {
    outLat = String((const char*)first["lat"].as<const char*>());
    outLon = String((const char*)first["lon"].as<const char*>());
    displayName = first["display_name"] | address;
    Serial.printf("geocodeAddress: found %s => %s,%s\n", displayName.c_str(), outLat.c_str(), outLon.c_str());
    return true;
  }
  return false;
}

// =================== Open-Meteo fetch ===================
bool fetchOpenMeteo(const String& lat, const String& lon, JsonObject& out, DynamicJsonDocument& scratch) {
  if (lat.length() < 2 || lon.length() < 2) return false;

  HTTPClient http;
  // DEGISTI: model acikca ecmwf_ifs025'e sabitlendi. SuDepoSis_v2 projesinde
  // sahada 4 model karsilastirildi (2026-09-01): varsayilan "best_match" bu
  // bolgede gercekte yagan yagmuru hep %0-3 gosterip kacirdi, ecmwf_ifs025
  // (Avrupa Orta Vadeli Tahmin Merkezi) ayni gunler icin %35-46 verip telefon
  // hava uygulamalariyla birebir ortustu - bkz. SuDepoSis_v2/esp32_master/
  // include/config.h WEATHER_FORECAST_MODEL yorumu.
  // forecast_days=1: sadece bugunun o anki saati kullaniliyor. Eskiden 7 gun
  // (168 saat x 4 alan) isteniyordu - ekran sprite'lari heap'i azaltinca
  // deserializeJson "NoMemory" ile basarisiz oldu (2026-09-27 seri log).
  // Saatlik ruzgar alanlari da kaldirildi: ruzgar current blogundan geliyor.
  // Bulut/nem artik "current" blogundan - saatlik dizi 7 gunde bellege sigmiyordu.
  // Gunluk: past_days=1 + forecast_days=6 -> 7 gun, indeks 1 = bugun (ekranda 2. sira).
  String url = "https://api.open-meteo.com/v1/forecast?latitude=" + lat + "&longitude=" + lon + "&daily=weather_code,temperature_2m_max,temperature_2m_min,sunrise,sunset,wind_gusts_10m_max,precipitation_sum" + "&current=temperature_2m,relative_humidity_2m,cloud_cover,wind_speed_10m,wind_direction_10m,weather_code,is_day,shortwave_radiation&timezone=auto&past_days=1&forecast_days=6&models=ecmwf_ifs025";

  Serial.printf("DEBUG: Open-Meteo URL: %s\n", url.c_str());

  http.begin(url);
  http.setTimeout(3000);
  http.setConnectTimeout(2000);
  int code = http.GET();

  if (code != HTTP_CODE_OK) {
    Serial.printf("fetchOpenMeteo: HTTP code %d\n", code);
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  // DEBUG: Ham JSON'u görelim
  Serial.println("DEBUG: Open-Meteo RAW JSON (ilk 500 karakter):");
  Serial.println(payload.substring(0, 500));

  scratch.clear();
  DeserializationError err = deserializeJson(scratch, payload);

  if (err) {
    Serial.printf("fetchOpenMeteo: JSON parse error %s\n", err.c_str());
    return false;
  }

  // Anlik degerler: yeni "current" blogu. (Eski current_weather=true ile
  // birlikte istenirse Open-Meteo current= parametresini yok sayiyor - isinim
  // hic gelmiyordu, 2026-09-28 dogrulandi.)
  if (scratch["current"].is<JsonObject>()) {
    JsonObject cw = scratch["current"].as<JsonObject>();
    float temp = cw["temperature_2m"] | 0.0f;
    float windSpeed = cw["wind_speed_10m"] | 0.0f;
    int windDir = cw["wind_direction_10m"] | 0;
    int weathercode = cw["weather_code"] | 0;

    Serial.printf("DEBUG: current -> temp: %.1f, wind_speed: %.1f, wind_dir: %d, weathercode: %d\n",
                  temp, windSpeed, windDir, weathercode);

    out["temp"] = temp;
    out["wind_speed"] = windSpeed;
    out["wind_dir"] = windDir;
    out["weathercode"] = weathercode;
    out["is_day"] = cw["is_day"] | 1;  // ekran: gunes/ay, gunduz/gece gokyuzu
    // Gercek isik: yere ulasan gunes isinimi (W/m2). Bulut yuzdesinden tahmin
    // etmek yerine bulut otomasyonu bununla karar verir (-1: veri yok).
    out["radiation"] = cw["shortwave_radiation"] | -1.0f;
    out["clouds"] = cw["cloud_cover"] | 0;
    out["humidity"] = cw["relative_humidity_2m"] | 0;
  } else {
    Serial.println("DEBUG: current anahtarı yok!");
    out["temp"] = 0.0f;
    out["wind_speed"] = 0.0f;
    out["wind_dir"] = 0.0f;
  }

  // Haftalik tahmin: [[kod, max, min], ...] - indeks 1 bugun
  JsonArray fcOut = out["fc"].to<JsonArray>();
  JsonArray dCode = scratch["daily"]["weather_code"].as<JsonArray>();
  JsonArray dMax = scratch["daily"]["temperature_2m_max"].as<JsonArray>();
  JsonArray dMin = scratch["daily"]["temperature_2m_min"].as<JsonArray>();
  for (size_t i = 0; i < dCode.size() && i < 7; i++) {
    JsonArray d = fcOut.add<JsonArray>();
    d.add(dCode[i] | -1);
    d.add((int)roundf(dMax[i] | 0.0f));
    d.add((int)roundf(dMin[i] | 0.0f));
  }

  // Firtina/ruzgar/yagis uyarilari (bugun + yarin) - bkz. alerts.cpp
  if (scratch["daily"]["wind_gusts_10m_max"].is<JsonArray>()) {
    alertsEvaluateWeather(scratch["daily"]["time"].as<JsonArray>(), dCode,
                          scratch["daily"]["wind_gusts_10m_max"].as<JsonArray>(),
                          scratch["daily"]["precipitation_sum"].as<JsonArray>());
  }

  // Gün doğumu ve batımı (bugun = indeks 1, past_days=1 yuzunden)
  if (scratch.containsKey("daily") && scratch["daily"].containsKey("sunrise") && scratch["daily"].containsKey("sunset")) {
    JsonArray sunriseArr = scratch["daily"]["sunrise"].as<JsonArray>();
    JsonArray sunsetArr = scratch["daily"]["sunset"].as<JsonArray>();

    if (sunriseArr.size() > 1) {
      const char* sunriseStr = sunriseArr[1];
      String sunrise = String(sunriseStr);
      if (sunrise.length() >= 16) {
        out["sunrise_time"] = sunrise.substring(11, 16);  // Saat:dakika kısmını al
      }
    }

    if (sunsetArr.size() > 1) {
      const char* sunsetStr = sunsetArr[1];
      String sunset = String(sunsetStr);
      if (sunset.length() >= 16) {
        out["sunset_time"] = sunset.substring(11, 16);  // Saat:dakika kısmını al
      }
    }
  }

  return true;
}

// =================== Wind direction helper ===================
String getWindDirection(int degrees) {
  if (degrees >= 337.5 || degrees < 22.5) return "Kuzey";
  if (degrees >= 22.5 && degrees < 67.5) return "KuzeyDoğu";
  if (degrees >= 67.5 && degrees < 112.5) return "Doğu";
  if (degrees >= 112.5 && degrees < 157.5) return "GüneyDoğu";
  if (degrees >= 157.5 && degrees < 202.5) return "Güney";
  if (degrees >= 202.5 && degrees < 247.5) return "GüneyBatı";
  if (degrees >= 247.5 && degrees < 292.5) return "Batı";
  if (degrees >= 292.5 && degrees < 337.5) return "KuzeyBatı";
  return "-";
}


// =================== updateWeatherFromConfig ===================
void updateWeatherFromConfig() {
  Serial.println("DEBUG: updateWeatherFromConfig başladı");

  String city, district, neighbourhood, lat, lon, displayName;

  if (xSemaphoreTake(weatherMutex, MUTEX_TIMEOUT) == pdTRUE) {
    city = (const char*)(WEATHER_CONFIG_DOC["city"] | "");
    district = (const char*)(WEATHER_CONFIG_DOC["district"] | "");
    neighbourhood = (const char*)(WEATHER_CONFIG_DOC["neighbourhood"] | "");
    lat = (const char*)(WEATHER_CONFIG_DOC["latitude"] | "");
    lon = (const char*)(WEATHER_CONFIG_DOC["longitude"] | "");
    displayName = (const char*)(WEATHER_CONFIG_DOC["city_name"] | "");
    xSemaphoreGive(weatherMutex);
  } else {
    Serial.println("ERROR: Mutex alınamadı");
    return;
  }

  // Eğer adres alanları doluysa ve koordinatlar boşsa, geocode yap
  if ((city.length() > 0 || district.length() > 0 || neighbourhood.length() > 0) && (lat.length() < 2 || lon.length() < 2)) {
    String address = neighbourhood + (neighbourhood.length() > 0 && district.length() > 0 ? ", " : "") + district + (district.length() > 0 && city.length() > 0 ? ", " : "") + city;
    String outLat, outLon, disp;
    if (geocodeAddress(address, outLat, outLon, disp)) {
      if (xSemaphoreTake(weatherMutex, MUTEX_TIMEOUT) == pdTRUE) {
        WEATHER_CONFIG_DOC["latitude"] = outLat;
        WEATHER_CONFIG_DOC["longitude"] = outLon;
        WEATHER_CONFIG_DOC["city_name"] = disp;
        saveWeatherConfig();
        xSemaphoreGive(weatherMutex);
      }
      lat = outLat;
      lon = outLon;
      displayName = disp;
    } else {
      if (xSemaphoreTake(weatherMutex, MUTEX_TIMEOUT) == pdTRUE) {
        if (city.length() > 0 || district.length() > 0) {
          WEATHER_CONFIG_DOC["city_name"] = city + (district.length() > 0 ? ", " + district : "");
          saveWeatherConfig();
        }
        xSemaphoreGive(weatherMutex);
      }
    }
  }

  // Konum yoksa (ilk kurulum) ag istegi yapma
  if (lat.length() < 2 || lon.length() < 2) {
    if (xSemaphoreTake(weatherMutex, MUTEX_TIMEOUT) == pdTRUE) {
      CURRENT_WEATHER_DOC.clear();
      CURRENT_WEATHER_DOC["description"] = "NO_LOCATION";
      CURRENT_WEATHER_DOC["city_name"] = "";
      xSemaphoreGive(weatherMutex);
    }
    return;
  }

  // Hava durumu verilerini çek
  DynamicJsonDocument scratch(12288);
  DynamicJsonDocument outDoc(2048);
  JsonObject out = outDoc.to<JsonObject>();

  bool fetchSuccess = fetchOpenMeteo(lat, lon, out, scratch);
  if (!fetchSuccess) {
    Serial.println("ERROR: Open-Meteo fetch başarısız");
    if (xSemaphoreTake(weatherMutex, MUTEX_TIMEOUT) == pdTRUE) {
      CURRENT_WEATHER_DOC.clear();
      CURRENT_WEATHER_DOC["description"] = "FETCH_ERROR";
      CURRENT_WEATHER_DOC["city_name"] = displayName.length() > 0 ? displayName : (city.length() > 0 ? city : "Bilinmeyen");
      xSemaphoreGive(weatherMutex);
    }
    weatherDisplayDirty = true;
    return;
  }

  // Hava durumu verilerini işle ve mutex içinde kaydet
  if (xSemaphoreTake(weatherMutex, MUTEX_TIMEOUT) == pdTRUE) {
    CURRENT_WEATHER_DOC.clear();
    CURRENT_WEATHER_DOC["temp"]       = out["temp"] | 0.0f;
    CURRENT_WEATHER_DOC["wind_speed"] = out["wind_speed"] | 0.0f;
    CURRENT_WEATHER_DOC["wind_dir"]   = out["wind_dir"] | 0;
    CURRENT_WEATHER_DOC["clouds"]     = out["clouds"] | 0;
    CURRENT_WEATHER_DOC["humidity"]   = out["humidity"] | 0;

    int windDirDegrees = out["wind_dir"] | 0;
    CURRENT_WEATHER_DOC["wind_direction"] = getWindDirection(windDirDegrees);

    if (displayName.length() > 0) {
      CURRENT_WEATHER_DOC["city_name"] = displayName;
    } else if (city.length() > 0) {
      CURRENT_WEATHER_DOC["city_name"] = city;
    } else {
      CURRENT_WEATHER_DOC["city_name"] = "";
    }

    CURRENT_WEATHER_DOC["description"]  = "OK";
    CURRENT_WEATHER_DOC["sunrise_time"] = out["sunrise_time"] | "";
    CURRENT_WEATHER_DOC["sunset_time"]  = out["sunset_time"] | "";
    CURRENT_WEATHER_DOC["fc"].set(out["fc"]);

    int clouds = CURRENT_WEATHER_DOC["clouds"] | 0;
    int lightnessScore = 0;
    if      (clouds <= 25) lightnessScore = 1;
    else if (clouds <= 99) lightnessScore = 2;
    else                   lightnessScore = 3;
    int weathercode = out["weathercode"] | 0;
    if ((weathercode >= 51 && weathercode <= 99) || (weathercode >= 200 && weathercode < 700)) {
      lightnessScore = 3;
    }
    CURRENT_WEATHER_DOC["lightness_score"] = lightnessScore;
    CURRENT_WEATHER_DOC["weathercode"] = weathercode;   // WMO kodu - ekran animasyonu
    CURRENT_WEATHER_DOC["is_day"] = out["is_day"] | 1;
    CURRENT_WEATHER_DOC["radiation"] = out["radiation"] | -1.0f;

    Serial.printf("DEBUG: Hava durumu güncellendi - Şehir: %s, Sıcaklık: %.1f°C, Rüzgar: %.1f Km/s, Skor: %d\n",
                  CURRENT_WEATHER_DOC["city_name"].as<String>().c_str(),
                  CURRENT_WEATHER_DOC["temp"].as<float>(),
                  CURRENT_WEATHER_DOC["wind_speed"].as<float>(), lightnessScore);

    xSemaphoreGive(weatherMutex);
  }

  weatherDisplayDirty = true;  // cizimi displayTask yapar (tek SPI sahibi)
}

// ============== NTP ==============
void forceNtpSync() {
  Serial.println("DEBUG: NTP Senkronizasyonu zorlaniyor...");
  configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov", "0.tr.pool.ntp.org");
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 5000)) {
    Serial.printf("DEBUG: NTP Guncellendi: %02d:%02d:%02d\n", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  } else {
    Serial.println("DEBUG: NTP Guncelleme Basarisiz.");
  }
}
