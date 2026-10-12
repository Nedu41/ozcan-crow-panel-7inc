#include "storage.h"
#include <LittleFS.h>
#include <vector>
#include "app_state.h"

// ============== LittleFS operations ==============
void loadDevices() {
  if (LittleFS.exists(DEVICES_FILE)) {
    File f = LittleFS.open(DEVICES_FILE, "r");
    if (f) {
      DEVICE_LIST_JSON = f.readString();
      f.close();
      Serial.println("Cihaz listesi yüklendi.");
      deserializeJson(doc, DEVICE_LIST_JSON);
      return;
    }
  }
  DEVICE_LIST_JSON = "[]";  // cihazlar web > Cihazlar sayfasindan eklenir
  deserializeJson(doc, DEVICE_LIST_JSON);
  Serial.println("Varsayılan cihaz listesi yüklendi.");
}

// sunriseAction/sunsetAction RAM dizileri cihazin LISTEDEKI SIRASIYLA tutuluyor.
// Liste degisince (silme/ekleme/siralama) bastan kurulmazsa ayarlar kayip yanlis
// lambaya uygulanirdi. Once tum konumlari temizle, sonra ada gore doldur.
static void rebuildSunActions() {
  for (int i = 0; i < MAX_DEVICES; i++)
    for (int r = 0; r < MAX_RELAYS; r++) { sunriseAction[i][r] = ""; sunsetAction[i][r] = ""; }
  int devIdx = 0;
  for (JsonObject device : doc.as<JsonArray>()) {
    if (devIdx >= MAX_DEVICES) break;
    String name = device["name"] | "";
    if (name == "") { devIdx++; continue; }
    for (int r = 0; r < MAX_RELAYS; r++) {
      String rKey = "r" + String(r + 1);
      sunriseAction[devIdx][r] = SCHEDULE_CONFIG_DOC[name][rKey + "_sunrise_act"] | String("");
      sunsetAction[devIdx][r]  = SCHEDULE_CONFIG_DOC[name][rKey + "_sunset_act"]  | String("");
    }
    devIdx++;
  }
}

void saveDevices(String json) {
  // Ad degisikligini yakalamak icin eski MAC -> ad eslemesi
  std::vector<std::pair<String, String>> oldMacName;
  for (JsonObject d : doc.as<JsonArray>()) {
    String mac = d["mac"] | "";
    if (mac.length()) oldMacName.push_back({mac, d["name"] | String("")});
  }

  File f = LittleFS.open(DEVICES_FILE, "w");
  if (f) {
    f.print(json);
    f.close();
    DEVICE_LIST_JSON = json;
    deserializeJson(doc, DEVICE_LIST_JSON);

    // Yeniden adlandirilan cihaz (ayni MAC, yeni ad): zamanlayici ve bulut
    // otomasyonu ada bagli - eskiden ad degisince baglanti sessizce kopuyordu.
    for (JsonObject d : doc.as<JsonArray>()) {
      String mac = d["mac"] | "";
      String newName = d["name"] | "";
      if (!mac.length() || !newName.length()) continue;
      for (auto& mn : oldMacName) {
        if (mn.first != mac || mn.second == newName || !mn.second.length()) continue;
        const String& oldName = mn.second;
        if (xSemaphoreTake(scheduleMutex, MUTEX_TIMEOUT) == pdTRUE) {
          if (SCHEDULE_CONFIG_DOC[oldName].is<JsonObject>()) {
            SCHEDULE_CONFIG_DOC[newName].set(SCHEDULE_CONFIG_DOC[oldName]);
            SCHEDULE_CONFIG_DOC.remove(oldName);
            saveScheduleConfig();
          }
          xSemaphoreGive(scheduleMutex);
        }
        if (xSemaphoreTake(weatherMutex, MUTEX_TIMEOUT) == pdTRUE) {
          bool renamed = false;
          for (JsonObject t : WEATHER_CONFIG_DOC["targets"].as<JsonArray>()) {
            if ((t["device"] | String("")) == oldName) { t["device"] = newName; renamed = true; }
          }
          if (renamed) saveWeatherConfig();
          xSemaphoreGive(weatherMutex);
        }
        Serial.printf("[Cihaz] yeniden adlandirildi: %s -> %s (ayarlar tasindi)\n", oldName.c_str(), newName.c_str());
      }
    }
    if (xSemaphoreTake(scheduleMutex, MUTEX_TIMEOUT) == pdTRUE) {
      // Listede olmayan (silinmis) cihazlarin zamanlayici kaydini temizle
      std::vector<String> staleSched;
      for (JsonPair kv : SCHEDULE_CONFIG_DOC.as<JsonObject>()) {
        bool inList = false;
        for (JsonObject d : doc.as<JsonArray>()) {
          if ((d["name"] | String("")) == kv.key().c_str()) { inList = true; break; }
        }
        if (!inList) staleSched.push_back(kv.key().c_str());
      }
      for (const String& k : staleSched) SCHEDULE_CONFIG_DOC.remove(k);
      if (!staleSched.empty()) saveScheduleConfig();
      rebuildSunActions();
      xSemaphoreGive(scheduleMutex);
    }

    // Silinen/yeniden adlandirilan cihazlarin durum kaydini temizle - yoksa
    // eski ad "hayalet" OFFLINE kayit olarak durum verisinde kaliyordu
    if (xSemaphoreTake(statusMutex, MUTEX_TIMEOUT) == pdTRUE) {
      JsonObject st = CURRENT_STATUS_DOC.as<JsonObject>();
      std::vector<String> stale;
      for (JsonPair kv : st) {
        bool inList = false;
        for (JsonObject d : doc.as<JsonArray>()) {
          if ((d["name"] | String("")) == kv.key().c_str()) { inList = true; break; }
        }
        if (!inList) stale.push_back(kv.key().c_str());
      }
      for (const String& k : stale) st.remove(k);
      xSemaphoreGive(statusMutex);
    }
    Serial.println("Cihazlar kaydedildi.");
  } else {
    Serial.println("HATA: devices.json yazılamadı.");
  }
}

// YENİ: Schedule config
void loadScheduleConfig() {
  scheduleMutex = xSemaphoreCreateMutex();
  if (LittleFS.exists(SCHEDULE_FILE)) {
    File f = LittleFS.open(SCHEDULE_FILE, "r");
    if (f) {
      DeserializationError err = deserializeJson(SCHEDULE_CONFIG_DOC, f);
      f.close();
      if (!err) {
        Serial.println("Schedule ayarları yüklendi.");

        // RAM dizileri (sunriseAction/sunsetAction) restart sonrası sıfırlanıyor — JSON'dan geri yükle
        rebuildSunActions();
        return;
      }
    }
  }
  SCHEDULE_CONFIG_DOC.clear();
  saveScheduleConfig();
  Serial.println("Varsayılan schedule oluşturuldu.");
}

void saveScheduleConfig() {
  File f = LittleFS.open(SCHEDULE_FILE, "w");
  if (f) {
    serializeJson(SCHEDULE_CONFIG_DOC, f);
    f.close();
    Serial.println("Schedule ayarları kaydedildi.");
  }
}

// Notlar sayfasi: [{t: metin, d: yapildi, c: olusturma (unix)}] - HUB'da
// kalici, hangi telefondan acilirsa acilsin ayni liste
String readNotes() {
  if (!LittleFS.exists(NOTES_FILE)) return "[]";
  File f = LittleFS.open(NOTES_FILE, "r");
  if (!f) return "[]";
  String s = f.readString();
  f.close();
  return s.length() ? s : "[]";
}

bool saveNotes(const String& json) {
  if (json.length() > 8192) return false;
  JsonDocument check;
  if (deserializeJson(check, json) || !check.is<JsonArray>()) return false;
  File f = LittleFS.open(NOTES_FILE, "w");
  if (!f) return false;
  f.print(json);
  f.close();
  return true;
}

void saveWeatherConfig() {
  File f = LittleFS.open(WEATHER_CONFIG_FILE, "w");
  if (f) {
    serializeJson(WEATHER_CONFIG_DOC, f);
    f.close();
    Serial.println("WEATHER CONFIG -> Başarıyla LittleFS'e kaydedildi.");
  } else {
    Serial.println("HATA: LittleFS'e WEATHER CONFIG yazılamadı.");
  }
}

void loadWeatherConfig() {
  bool needSave = false;
  if (LittleFS.exists(WEATHER_CONFIG_FILE)) {
    File f = LittleFS.open(WEATHER_CONFIG_FILE, "r");
    if (f) {
      DeserializationError err = deserializeJson(WEATHER_CONFIG_DOC, f);
      f.close();
      if (err) {
        Serial.println("Config JSON bozuk, varsayılanlar yükleniyor.");
        WEATHER_CONFIG_DOC.clear();
        needSave = true;
      }
    }
  } else {
    Serial.println("Weather Config dosyası bulunamadı, varsayılanlar ayarlanıyor.");
    needSave = true;
  }

  if (!WEATHER_CONFIG_DOC.containsKey("active")) WEATHER_CONFIG_DOC["active"] = false;
  // GOC (2026-09-28): tek cihaz/role (device_name+relay_id) -> coklu hedef listesi
  // ("targets": [{device,relay},...]). Web arayuzu artik dropdown degil tikli
  // kutular - ayni otomasyona birden fazla lamba baglanabiliyor.
  if (!WEATHER_CONFIG_DOC.containsKey("targets")) {
    JsonArray targets = WEATHER_CONFIG_DOC["targets"].to<JsonArray>();
    String oldDevice = WEATHER_CONFIG_DOC["device_name"] | "";
    int oldRelay = WEATHER_CONFIG_DOC["relay_id"] | 0;
    if (oldDevice.length() && oldDevice != "Yok" && oldRelay >= 1 && oldRelay <= MAX_RELAYS) {
      JsonObject t = targets.add<JsonObject>();
      t["device"] = oldDevice;
      t["relay"] = oldRelay;
    }
    needSave = true;
  }
  if (!WEATHER_CONFIG_DOC.containsKey("threshold")) WEATHER_CONFIG_DOC["threshold"] = 3;
  // Isik esigi (W/m2): isinim bunun altina dusunce ac. Olcum 2026-09-28: yagmurlu
  // kapali 11:30'da 88 W/m2 ("cok karanlik degil"), acik ogle ~600-700.
  if (!WEATHER_CONFIG_DOC.containsKey("light_threshold")) WEATHER_CONFIG_DOC["light_threshold"] = 60;
  if (!WEATHER_CONFIG_DOC.containsKey("action")) WEATHER_CONFIG_DOC["action"] = "ON";
  // Karanlik otomasyonu sadece gun dogumu+1sa .. gun batimi-1sa arasinda calissin
  if (!WEATHER_CONFIG_DOC.containsKey("daylight_window")) WEATHER_CONFIG_DOC["daylight_window"] = true;

  // YENİ EKLEME: Rüzgar Hızı Eşiği (Km/s)
  if (!WEATHER_CONFIG_DOC.containsKey("wind_threshold")) WEATHER_CONFIG_DOC["wind_threshold"] = 15.0f;

  if (!WEATHER_CONFIG_DOC.containsKey("city")) WEATHER_CONFIG_DOC["city"] = "";
  if (!WEATHER_CONFIG_DOC.containsKey("district")) WEATHER_CONFIG_DOC["district"] = "";
  if (!WEATHER_CONFIG_DOC.containsKey("neighbourhood")) WEATHER_CONFIG_DOC["neighbourhood"] = "";
  if (!WEATHER_CONFIG_DOC.containsKey("city_name")) WEATHER_CONFIG_DOC["city_name"] = "";

  // Konum varsayilani YOK: cihaz her evde kendi konumuyla (web > Hava ayarlari) kurulur
  if (!WEATHER_CONFIG_DOC.containsKey("latitude")) { WEATHER_CONFIG_DOC["latitude"] = ""; needSave = true; }
  if (!WEATHER_CONFIG_DOC.containsKey("longitude")) { WEATHER_CONFIG_DOC["longitude"] = ""; needSave = true; }

  if (needSave) {
    saveWeatherConfig();
    Serial.println("Varsayılan hava ayarları kaydedildi.");
  }
}
