#pragma once
#include <Arduino.h>

// Open-Meteo (ecmwf_ifs025) + Nominatim geocode -> CURRENT_WEATHER_DOC
void updateWeatherFromConfig();
String getWindDirection(int degrees);
void forceNtpSync();
