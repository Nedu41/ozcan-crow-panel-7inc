#pragma once
// Hatirlatici (ilac, yapilacak is): web'den olusturulur, zamani gelince uyari banner'i (alerts.cpp) acar.
// Kayit /reminders.json: [{"i":1,"t":"Ilac","n":"Tansiyon","s":[[9,0],[14,0],[21,0]],"r":"g","d":"2026-10-21","w":62,"on":true}]
//   s: gunluk saatler (yoksa tek saat "h"/"m"); r: "t" tek sefer (d tarihinde), "g" her gun,
//   "h" secili gunler (w: bit0 = Pazar ... bit6 = Cumartesi)
// Uyari kimligi "rm:<i>:<YYYYMMDD>:<HHMM>": o saatin hatirlatmasi onaylaninca o gun tekrar cikmaz.
#include <Arduino.h>

void remindersInit();                    // LittleFS acildiktan sonra
String remindersJson();                  // web: liste
bool remindersSave(const String& json);  // web: tum liste (dogrulanir)
void remindersTick();                    // zamani gelenleri uyariya cevirir (birkac sn'de bir)
