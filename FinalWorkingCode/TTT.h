#ifndef __TTT_H__
#define __TTT_H__

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include "OLED_Display.h"

String urlEncode_ttt(String str);
String translateText(String text, String sourceLang, String targetLang);

#endif 
