#pragma once
#include <Arduino.h>

void loadDevices();
void saveDevices(String json);
void loadScheduleConfig();
void saveScheduleConfig();
void loadWeatherConfig();
void saveWeatherConfig();
String readNotes();
bool saveNotes(const String& json);
